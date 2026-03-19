#pragma once

#include <QObject>
#include <QString>

#include "ClipboardEntry.h"
#include "HistoryListModel.h"

class ClipboardDatabase;
class ClipboardListener;
class InputSimulator;
class SettingsManager;

// Coordinate clipboard capture, persistence, filtering, activation, and the popup list model.
class HistoryManager : public QObject
{
    Q_OBJECT

public:
    // Construct the history manager around the storage, clipboard, and settings services.
    HistoryManager(SettingsManager *settingsManager,
                   ClipboardDatabase *database,
                   ClipboardListener *clipboardListener,
                   InputSimulator *inputSimulator,
                   QObject *parent = nullptr);

    // Load the initial model state from SQLite during application startup.
    bool initialize();

    // Expose the list model consumed by the QML popup.
    HistoryListModel *model();

    // Refresh the popup contents from the database using the current search filter.
    Q_INVOKABLE void refreshModel();

    // Update the search filter applied to the popup list.
    Q_INVOKABLE void setSearchQuery(const QString &searchQuery);

    // Restore the selected entry to the live clipboard and attempt to paste it.
    Q_INVOKABLE void activateEntry(qint64 id);

    // Toggle the pin state of the selected entry.
    Q_INVOKABLE void togglePinned(qint64 id);

    // Toggle the favorite state of the selected entry.
    Q_INVOKABLE void toggleFavorite(qint64 id);

    // Clear all non-pinned history items.
    Q_INVOKABLE void clearHistory();

    // Return recent non-sensitive entries for the file-based sync engine.
    QVector<ClipboardEntry> syncEntries(int limit) const;

    // Merge synchronized entries imported from other devices into the local history store.
    void importEntries(const QVector<ClipboardEntry> &entries);

public slots:
    // Persist a freshly captured clipboard entry and update the popup model.
    void handleCapturedEntry(const ClipboardEntry &entry);

signals:
    // Report a non-fatal activation issue, such as a platform that cannot synthesize paste natively.
    void activationNotice(const QString &message);

private:
    // Build a QMimeData payload that faithfully restores a stored entry to the live clipboard.
    class QMimeData *mimeDataForEntry(const ClipboardEntry &entry) const;

    // Return the effective runtime settings snapshot used by the manager.
    SettingsManager *m_settingsManager = nullptr;

    // Keep the SQLite history store available for fetch and update operations.
    ClipboardDatabase *m_database = nullptr;

    // Suppress self-originated clipboard updates when restoring an entry for paste.
    ClipboardListener *m_clipboardListener = nullptr;

    // Simulate the native paste gesture when the current platform supports it.
    InputSimulator *m_inputSimulator = nullptr;

    // Expose the current history list to QML.
    HistoryListModel m_model;

    // Track the live search query so model refreshes remain consistent.
    QString m_searchQuery;
};
