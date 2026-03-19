#pragma once

#include <QDateTime>
#include <QHash>
#include <QObject>
#include <QTimer>

class HistoryManager;
class SettingsManager;

// Synchronize recent clipboard history through a user-controlled shared folder such as Syncthing or Nextcloud.
class SyncEngine : public QObject
{
    Q_OBJECT

public:
    // Construct the sync engine around the settings and history services.
    SyncEngine(SettingsManager *settingsManager, HistoryManager *historyManager, QObject *parent = nullptr);

    // Start the periodic sync timer and immediately honor the current runtime settings.
    void start();

public slots:
    // Export the local device history and import remote device histories from the shared folder.
    void triggerSync();

private:
    // Return a stable per-device id used as the sync file name in the shared folder.
    QString deviceId() const;

    // Return the current shared-folder path, or an empty string when sync is disabled.
    QString syncDirectory() const;

    // Keep the settings available for runtime sync reconfiguration.
    SettingsManager *m_settingsManager = nullptr;

    // Read and write normalized history records through the history service.
    HistoryManager *m_historyManager = nullptr;

    // Drive periodic synchronization without blocking the UI thread.
    QTimer m_timer;

    // Track per-file timestamps to avoid re-importing unchanged remote snapshots.
    QHash<QString, QDateTime> m_lastImportedTimes;
};
