#pragma once

#include <QByteArray>
#include <QDateTime>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QString>
#include <QStringList>

// Classify each clipboard payload so the UI and storage layer can render and persist it correctly.
enum class ClipboardContentType {
    Text,
    Html,
    Image,
    FileList,
    Rtf,
    Unknown,
};

// Represent one clipboard-history record together with the metadata needed by the UI and sync engine.
struct ClipboardEntry {
    // Store the database identity so the UI can act on a stable record even after model refreshes.
    qint64 id = -1;

    // Record the normalized content type used for preview rendering and activation logic.
    ClipboardContentType type = ClipboardContentType::Unknown;

    // Keep plain text separately because it is searchable and useful as a universal fallback.
    QString plainText;

    // Preserve HTML markup for rich text paste workflows.
    QString htmlText;

    // Preserve RTF payloads for applications that advertise text/rtf instead of HTML.
    QByteArray rtfData;

    // Store image data as PNG bytes for a stable database representation.
    QByteArray imagePng;

    // Persist file references as URIs so file-copy clipboard entries survive restarts.
    QStringList fileUris;

    // Cache a compact preview string to avoid recomputing list labels on every paint.
    QString previewText;

    // Track when the item entered history for sorting and retention policies.
    QDateTime createdAt;

    // Track the latest usage time so frequently reused entries can be surfaced later.
    QDateTime lastUsedAt;

    // Remember the source application when the platform exposes it.
    QString sourceApplication;

    // Flag entries that should not be synced or retained aggressively.
    bool sensitive = false;

    // Keep pinned items immune from automatic history pruning.
    bool pinned = false;

    // Let users star important items without making them fully pinned.
    bool favorite = false;

    // Count reuse operations for future ranking and diagnostics.
    int useCount = 0;

    // Store a stable content hash used for deduplication across sessions and sync merges.
    QString contentHash;
};

// Convert the enum to a stable string for SQLite storage, JSON sync payloads, and QML display logic.
inline QString contentTypeToString(ClipboardContentType type)
{
    switch (type) {
    case ClipboardContentType::Text:
        return QStringLiteral("text");
    case ClipboardContentType::Html:
        return QStringLiteral("html");
    case ClipboardContentType::Image:
        return QStringLiteral("image");
    case ClipboardContentType::FileList:
        return QStringLiteral("files");
    case ClipboardContentType::Rtf:
        return QStringLiteral("rtf");
    case ClipboardContentType::Unknown:
    default:
        return QStringLiteral("unknown");
    }
}

// Convert persisted strings back into the enum used internally by the history service.
inline ClipboardContentType contentTypeFromString(const QString &value)
{
    if (value == QStringLiteral("text")) {
        return ClipboardContentType::Text;
    }
    if (value == QStringLiteral("html")) {
        return ClipboardContentType::Html;
    }
    if (value == QStringLiteral("image")) {
        return ClipboardContentType::Image;
    }
    if (value == QStringLiteral("files")) {
        return ClipboardContentType::FileList;
    }
    if (value == QStringLiteral("rtf")) {
        return ClipboardContentType::Rtf;
    }
    return ClipboardContentType::Unknown;
}

// Serialize a clipboard entry into JSON for file-based multi-device synchronization.
inline QJsonObject clipboardEntryToJson(const ClipboardEntry &entry)
{
    QJsonObject object;
    object.insert(QStringLiteral("id"), QString::number(entry.id));
    object.insert(QStringLiteral("type"), contentTypeToString(entry.type));
    object.insert(QStringLiteral("plainText"), entry.plainText);
    object.insert(QStringLiteral("htmlText"), entry.htmlText);
    object.insert(QStringLiteral("rtfData"), QString::fromLatin1(entry.rtfData.toBase64()));
    object.insert(QStringLiteral("imagePng"), QString::fromLatin1(entry.imagePng.toBase64()));
    object.insert(QStringLiteral("previewText"), entry.previewText);
    object.insert(QStringLiteral("createdAt"), entry.createdAt.toString(Qt::ISODateWithMs));
    object.insert(QStringLiteral("lastUsedAt"), entry.lastUsedAt.toString(Qt::ISODateWithMs));
    object.insert(QStringLiteral("sourceApplication"), entry.sourceApplication);
    object.insert(QStringLiteral("sensitive"), entry.sensitive);
    object.insert(QStringLiteral("pinned"), entry.pinned);
    object.insert(QStringLiteral("favorite"), entry.favorite);
    object.insert(QStringLiteral("useCount"), entry.useCount);
    object.insert(QStringLiteral("contentHash"), entry.contentHash);

    QJsonArray uris;
    for (const QString &uri : entry.fileUris) {
        uris.append(uri);
    }
    object.insert(QStringLiteral("fileUris"), uris);
    return object;
}

// Rehydrate a synchronized JSON payload into the strongly typed record used by the history manager.
inline ClipboardEntry clipboardEntryFromJson(const QJsonObject &object)
{
    ClipboardEntry entry;
    entry.id = object.value(QStringLiteral("id")).toString().toLongLong();
    entry.type = contentTypeFromString(object.value(QStringLiteral("type")).toString());
    entry.plainText = object.value(QStringLiteral("plainText")).toString();
    entry.htmlText = object.value(QStringLiteral("htmlText")).toString();
    entry.rtfData = QByteArray::fromBase64(object.value(QStringLiteral("rtfData")).toString().toLatin1());
    entry.imagePng = QByteArray::fromBase64(object.value(QStringLiteral("imagePng")).toString().toLatin1());
    entry.previewText = object.value(QStringLiteral("previewText")).toString();
    entry.createdAt = QDateTime::fromString(object.value(QStringLiteral("createdAt")).toString(), Qt::ISODateWithMs);
    entry.lastUsedAt = QDateTime::fromString(object.value(QStringLiteral("lastUsedAt")).toString(), Qt::ISODateWithMs);
    entry.sourceApplication = object.value(QStringLiteral("sourceApplication")).toString();
    entry.sensitive = object.value(QStringLiteral("sensitive")).toBool();
    entry.pinned = object.value(QStringLiteral("pinned")).toBool();
    entry.favorite = object.value(QStringLiteral("favorite")).toBool();
    entry.useCount = object.value(QStringLiteral("useCount")).toInt();
    entry.contentHash = object.value(QStringLiteral("contentHash")).toString();

    const QJsonArray uris = object.value(QStringLiteral("fileUris")).toArray();
    for (const QJsonValue &uriValue : uris) {
        entry.fileUris.append(uriValue.toString());
    }
    return entry;
}
