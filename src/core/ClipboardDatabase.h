#pragma once

#include <QObject>
#include <QSqlDatabase>
#include <QString>
#include <QVector>

#include "ClipboardEntry.h"

// Persist clipboard history in SQLite and expose query/update primitives to the service layer.
class ClipboardDatabase : public QObject
{
    Q_OBJECT

public:
    // Construct the storage wrapper around a dedicated SQLite connection.
    explicit ClipboardDatabase(QObject *parent = nullptr);

    // Open the database, create the schema, and prepare the indexes required for responsive queries.
    bool initialize();

    // Insert or update one clipboard entry using its content hash for deduplication.
    bool upsertEntry(const ClipboardEntry &entry);

    // Fetch history entries for the popup model, optionally filtered by a search query.
    QVector<ClipboardEntry> fetchEntries(const QString &searchQuery, int limit) const;

    // Fetch sync-safe entries that can be exported to a shared folder without sensitive payloads.
    QVector<ClipboardEntry> fetchSyncEntries(int limit) const;

    // Look up a single entry by id for activation or toggle operations.
    ClipboardEntry fetchEntryById(qint64 id) const;

    // Update the pinned state when the user locks or unlocks an item.
    bool setPinned(qint64 id, bool pinned);

    // Update the favorite state when the user stars or unstars an item.
    bool setFavorite(qint64 id, bool favorite);

    // Update the last-used fields after the user re-pastes an item from history.
    bool markUsed(qint64 id);

    // Delete all non-pinned entries during an explicit user clear-history operation.
    bool clearUnpinnedEntries();

    // Trim the database to the configured maximum while preserving pinned items.
    bool pruneToLimit(int limit);

private:
    // Return the shared SQLite connection used by all methods in this single-threaded service.
    QSqlDatabase database() const;

    // Convert the current query row into the strongly typed record used by the rest of the application.
    ClipboardEntry entryFromCurrentQuery(class QSqlQuery &query) const;
};
