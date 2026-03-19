#include "ClipboardDatabase.h"

#include <QDir>
#include <QDateTime>
#include <QJsonArray>
#include <QJsonDocument>
#include <QSqlError>
#include <QSqlQuery>
#include <QStandardPaths>

#include "Logger.h"

namespace {

// Keep a stable connection name so the application can reopen the same SQLite database cleanly.
constexpr auto kConnectionName = "clip-stacker-sqlite";

// Serialize file URI lists as JSON to preserve order and survive SQLite round-trips safely.
QString fileUrisToJson(const QStringList &fileUris)
{
    QJsonArray array;
    for (const QString &uri : fileUris) {
        array.append(uri);
    }
    return QString::fromUtf8(QJsonDocument(array).toJson(QJsonDocument::Compact));
}

// Restore serialized file URI lists from their JSON storage form.
QStringList fileUrisFromJson(const QString &json)
{
    QStringList fileUris;
    const QJsonDocument document = QJsonDocument::fromJson(json.toUtf8());
    for (const QJsonValue &value : document.array()) {
        fileUris.append(value.toString());
    }
    return fileUris;
}

} // namespace

ClipboardDatabase::ClipboardDatabase(QObject *parent)
    : QObject(parent)
{
}

bool ClipboardDatabase::initialize()
{
    // Create the per-user application data directory that will hold the SQLite database file.
    const QString dataDirectory = QStandardPaths::writableLocation(QStandardPaths::AppDataLocation);
    QDir().mkpath(dataDirectory);

    // Open a dedicated SQLite connection for the clipboard history store.
    QSqlDatabase database = QSqlDatabase::contains(kConnectionName)
                                ? QSqlDatabase::database(kConnectionName)
                                : QSqlDatabase::addDatabase(QStringLiteral("QSQLITE"), kConnectionName);
    database.setDatabaseName(dataDirectory + QStringLiteral("/history.sqlite"));

    // Fail fast when the SQLite driver or file permissions prevent the history store from opening.
    if (!database.open()) {
        qCritical(appLog) << "Failed to open clipboard database:" << database.lastError().text();
        return false;
    }

    // Create the schema used to store normalized clipboard content and metadata.
    QSqlQuery query(database);
    if (!query.exec(QStringLiteral(
            "CREATE TABLE IF NOT EXISTS clipboard_entries ("
            "id INTEGER PRIMARY KEY AUTOINCREMENT,"
            "content_hash TEXT NOT NULL UNIQUE,"
            "content_type TEXT NOT NULL,"
            "plain_text TEXT,"
            "html_text TEXT,"
            "rtf_data BLOB,"
            "image_png BLOB,"
            "file_uris TEXT,"
            "preview_text TEXT NOT NULL,"
            "created_at TEXT NOT NULL,"
            "last_used_at TEXT NOT NULL,"
            "source_application TEXT,"
            "sensitive INTEGER NOT NULL DEFAULT 0,"
            "pinned INTEGER NOT NULL DEFAULT 0,"
            "favorite INTEGER NOT NULL DEFAULT 0,"
            "use_count INTEGER NOT NULL DEFAULT 0"
            ")"))) {
        qCritical(appLog) << "Failed to create clipboard table:" << query.lastError().text();
        return false;
    }

    // Add indexes that keep search and sort operations fast as the history grows.
    query.exec(QStringLiteral("CREATE INDEX IF NOT EXISTS idx_clipboard_last_used ON clipboard_entries(last_used_at DESC)"));
    query.exec(QStringLiteral("CREATE INDEX IF NOT EXISTS idx_clipboard_pinned ON clipboard_entries(pinned DESC, favorite DESC)"));
    query.exec(QStringLiteral("CREATE INDEX IF NOT EXISTS idx_clipboard_source_app ON clipboard_entries(source_application)"));
    return true;
}

bool ClipboardDatabase::upsertEntry(const ClipboardEntry &entry)
{
    QSqlQuery query(database());

    // Insert new clipboard payloads and update duplicates in place using the content hash as the natural key.
    query.prepare(QStringLiteral(
        "INSERT INTO clipboard_entries ("
        "content_hash, content_type, plain_text, html_text, rtf_data, image_png, file_uris, preview_text, "
        "created_at, last_used_at, source_application, sensitive, pinned, favorite, use_count"
        ") VALUES (?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?)"
        "ON CONFLICT(content_hash) DO UPDATE SET "
        "content_type = excluded.content_type,"
        "plain_text = excluded.plain_text,"
        "html_text = excluded.html_text,"
        "rtf_data = excluded.rtf_data,"
        "image_png = excluded.image_png,"
        "file_uris = excluded.file_uris,"
        "preview_text = excluded.preview_text,"
        "last_used_at = excluded.last_used_at,"
        "source_application = excluded.source_application,"
        "sensitive = excluded.sensitive,"
        "pinned = MAX(clipboard_entries.pinned, excluded.pinned),"
        "favorite = MAX(clipboard_entries.favorite, excluded.favorite),"
        "use_count = MAX(clipboard_entries.use_count, excluded.use_count)"));
    query.addBindValue(entry.contentHash);
    query.addBindValue(contentTypeToString(entry.type));
    query.addBindValue(entry.plainText);
    query.addBindValue(entry.htmlText);
    query.addBindValue(entry.rtfData);
    query.addBindValue(entry.imagePng);
    query.addBindValue(fileUrisToJson(entry.fileUris));
    query.addBindValue(entry.previewText);
    query.addBindValue(entry.createdAt.toString(Qt::ISODateWithMs));
    query.addBindValue(entry.lastUsedAt.toString(Qt::ISODateWithMs));
    query.addBindValue(entry.sourceApplication);
    query.addBindValue(entry.sensitive);
    query.addBindValue(entry.pinned);
    query.addBindValue(entry.favorite);
    query.addBindValue(entry.useCount);

    // Report persistence failures so startup diagnostics and packaged logs surface the root cause.
    if (!query.exec()) {
        qCritical(appLog) << "Failed to upsert clipboard entry:" << query.lastError().text();
        return false;
    }
    return true;
}

QVector<ClipboardEntry> ClipboardDatabase::fetchEntries(const QString &searchQuery, int limit) const
{
    QVector<ClipboardEntry> entries;
    QSqlQuery query(database());

    // Query the popup list with pinned items first and newest usage times after that.
    QString sql = QStringLiteral(
        "SELECT id, content_hash, content_type, plain_text, html_text, rtf_data, image_png, file_uris, preview_text, "
        "created_at, last_used_at, source_application, sensitive, pinned, favorite, use_count "
        "FROM clipboard_entries ");

    // Apply the search term across the preview, plain text, and source application fields.
    if (!searchQuery.trimmed().isEmpty()) {
        sql += QStringLiteral("WHERE preview_text LIKE ? OR plain_text LIKE ? OR source_application LIKE ? ");
    }

    sql += QStringLiteral("ORDER BY pinned DESC, favorite DESC, last_used_at DESC LIMIT ?");
    query.prepare(sql);

    if (!searchQuery.trimmed().isEmpty()) {
        const QString pattern = QStringLiteral("%") + searchQuery.trimmed() + QStringLiteral("%");
        query.addBindValue(pattern);
        query.addBindValue(pattern);
        query.addBindValue(pattern);
    }
    query.addBindValue(limit);

    // Materialize the result set into value objects that the history model can own safely.
    if (query.exec()) {
        while (query.next()) {
            entries.append(entryFromCurrentQuery(query));
        }
    } else {
        qWarning(appLog) << "Failed to fetch clipboard entries:" << query.lastError().text();
    }

    return entries;
}

QVector<ClipboardEntry> ClipboardDatabase::fetchSyncEntries(int limit) const
{
    QVector<ClipboardEntry> entries;
    QSqlQuery query(database());

    // Export only non-sensitive entries so shared-folder sync does not leak obvious secrets by default.
    query.prepare(QStringLiteral(
        "SELECT id, content_hash, content_type, plain_text, html_text, rtf_data, image_png, file_uris, preview_text, "
        "created_at, last_used_at, source_application, sensitive, pinned, favorite, use_count "
        "FROM clipboard_entries WHERE sensitive = 0 ORDER BY pinned DESC, favorite DESC, last_used_at DESC LIMIT ?"));
    query.addBindValue(limit);

    if (query.exec()) {
        while (query.next()) {
            entries.append(entryFromCurrentQuery(query));
        }
    } else {
        qWarning(appLog) << "Failed to fetch sync entries:" << query.lastError().text();
    }
    return entries;
}

ClipboardEntry ClipboardDatabase::fetchEntryById(qint64 id) const
{
    QSqlQuery query(database());

    // Retrieve the selected history item so it can be restored to the live clipboard.
    query.prepare(QStringLiteral(
        "SELECT id, content_hash, content_type, plain_text, html_text, rtf_data, image_png, file_uris, preview_text, "
        "created_at, last_used_at, source_application, sensitive, pinned, favorite, use_count "
        "FROM clipboard_entries WHERE id = ?"));
    query.addBindValue(id);

    if (query.exec() && query.next()) {
        return entryFromCurrentQuery(query);
    }
    return ClipboardEntry{};
}

bool ClipboardDatabase::setPinned(qint64 id, bool pinned)
{
    QSqlQuery query(database());

    // Persist the pin state so important entries survive pruning and appear at the top of the list.
    query.prepare(QStringLiteral("UPDATE clipboard_entries SET pinned = ? WHERE id = ?"));
    query.addBindValue(pinned);
    query.addBindValue(id);
    return query.exec();
}

bool ClipboardDatabase::setFavorite(qint64 id, bool favorite)
{
    QSqlQuery query(database());

    // Persist the favorite flag used by the popup UI for lightweight prioritization.
    query.prepare(QStringLiteral("UPDATE clipboard_entries SET favorite = ? WHERE id = ?"));
    query.addBindValue(favorite);
    query.addBindValue(id);
    return query.exec();
}

bool ClipboardDatabase::markUsed(qint64 id)
{
    QSqlQuery query(database());

    // Update the usage timestamp and counter so reused items float upward naturally.
    query.prepare(QStringLiteral(
        "UPDATE clipboard_entries SET last_used_at = ?, use_count = use_count + 1 WHERE id = ?"));
    query.addBindValue(QDateTime::currentDateTimeUtc().toString(Qt::ISODateWithMs));
    query.addBindValue(id);
    return query.exec();
}

bool ClipboardDatabase::clearUnpinnedEntries()
{
    QSqlQuery query(database());

    // Respect user-pinned entries when clearing history from the tray or popup actions.
    return query.exec(QStringLiteral("DELETE FROM clipboard_entries WHERE pinned = 0"));
}

bool ClipboardDatabase::pruneToLimit(int limit)
{
    QSqlQuery query(database());

    // Remove the oldest unpinned entries once the configured retention ceiling is exceeded.
    query.prepare(QStringLiteral(
        "DELETE FROM clipboard_entries WHERE id IN ("
        "SELECT id FROM clipboard_entries WHERE pinned = 0 ORDER BY last_used_at DESC LIMIT -1 OFFSET ?"
        ")"));
    query.addBindValue(limit);
    return query.exec();
}

QSqlDatabase ClipboardDatabase::database() const
{
    // Reuse the named connection created during initialization.
    return QSqlDatabase::database(kConnectionName);
}

ClipboardEntry ClipboardDatabase::entryFromCurrentQuery(QSqlQuery &query) const
{
    ClipboardEntry entry;

    // Rebuild the value object in the same column order used by the SELECT statements above.
    entry.id = query.value(0).toLongLong();
    entry.contentHash = query.value(1).toString();
    entry.type = contentTypeFromString(query.value(2).toString());
    entry.plainText = query.value(3).toString();
    entry.htmlText = query.value(4).toString();
    entry.rtfData = query.value(5).toByteArray();
    entry.imagePng = query.value(6).toByteArray();
    entry.fileUris = fileUrisFromJson(query.value(7).toString());
    entry.previewText = query.value(8).toString();
    entry.createdAt = QDateTime::fromString(query.value(9).toString(), Qt::ISODateWithMs);
    entry.lastUsedAt = QDateTime::fromString(query.value(10).toString(), Qt::ISODateWithMs);
    entry.sourceApplication = query.value(11).toString();
    entry.sensitive = query.value(12).toBool();
    entry.pinned = query.value(13).toBool();
    entry.favorite = query.value(14).toBool();
    entry.useCount = query.value(15).toInt();
    return entry;
}
