#include "SyncEngine.h"

#include <QCryptographicHash>
#include <QDateTime>
#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QSaveFile>
#include <QStandardPaths>
#include <QSysInfo>

#include "ClipboardEntry.h"
#include "HistoryManager.h"
#include "Logger.h"
#include "SettingsManager.h"

SyncEngine::SyncEngine(SettingsManager *settingsManager, HistoryManager *historyManager, QObject *parent)
    : QObject(parent)
    , m_settingsManager(settingsManager)
    , m_historyManager(historyManager)
{
    // Sync periodically in the background while keeping the interval conservative for low resource usage.
    m_timer.setInterval(60'000);
    connect(&m_timer, &QTimer::timeout, this, &SyncEngine::triggerSync);
    connect(m_settingsManager, &SettingsManager::settingsChanged, this, &SyncEngine::triggerSync);
}

void SyncEngine::start()
{
    // Start the periodic timer and run an initial sync pass if shared-folder sync is enabled.
    m_timer.start();
    triggerSync();
}

void SyncEngine::triggerSync()
{
    const QString directoryPath = syncDirectory();
    if (directoryPath.isEmpty()) {
        return;
    }

    QDir directory(directoryPath);
    if (!directory.exists()) {
        directory.mkpath(QStringLiteral("."));
    }

    const QString localFilePath = directory.filePath(deviceId() + QStringLiteral(".json"));
    QJsonArray entriesArray;

    // Export a bounded snapshot of recent non-sensitive history items for this device.
    for (const ClipboardEntry &entry : m_historyManager->syncEntries(200)) {
        entriesArray.append(clipboardEntryToJson(entry));
    }

    QJsonObject rootObject;
    rootObject.insert(QStringLiteral("deviceId"), deviceId());
    rootObject.insert(QStringLiteral("updatedAt"), QDateTime::currentDateTimeUtc().toString(Qt::ISODateWithMs));
    rootObject.insert(QStringLiteral("entries"), entriesArray);

    QSaveFile saveFile(localFilePath);
    if (saveFile.open(QIODevice::WriteOnly | QIODevice::Truncate)) {
        saveFile.write(QJsonDocument(rootObject).toJson(QJsonDocument::Indented));
        saveFile.commit();
        QFile::setPermissions(localFilePath, QFileDevice::ReadOwner | QFileDevice::WriteOwner);
    } else {
        qWarning(appLog) << "Failed to write sync file:" << localFilePath;
    }

    // Import snapshots from other devices that have changed since the last successful pass.
    const QFileInfoList files = directory.entryInfoList(QStringList{QStringLiteral("*.json")}, QDir::Files | QDir::Readable, QDir::Time);
    for (const QFileInfo &fileInfo : files) {
        if (fileInfo.absoluteFilePath() == localFilePath) {
            continue;
        }

        if (m_lastImportedTimes.value(fileInfo.absoluteFilePath()) >= fileInfo.lastModified()) {
            continue;
        }

        QFile file(fileInfo.absoluteFilePath());
        if (!file.open(QIODevice::ReadOnly)) {
            continue;
        }

        const QJsonDocument document = QJsonDocument::fromJson(file.readAll());
        const QJsonArray remoteEntriesArray = document.object().value(QStringLiteral("entries")).toArray();
        QVector<ClipboardEntry> importedEntries;
        importedEntries.reserve(remoteEntriesArray.size());

        for (const QJsonValue &value : remoteEntriesArray) {
            importedEntries.append(clipboardEntryFromJson(value.toObject()));
        }

        if (!importedEntries.isEmpty()) {
            m_historyManager->importEntries(importedEntries);
        }
        m_lastImportedTimes.insert(fileInfo.absoluteFilePath(), fileInfo.lastModified());
    }
}

QString SyncEngine::deviceId() const
{
    // Derive a stable but non-human-readable device id from the machine identifier or hostname fallback.
    QByteArray source = QSysInfo::machineUniqueId();
    if (source.isEmpty()) {
        source = QSysInfo::machineHostName().toUtf8();
    }
    return QString::fromLatin1(QCryptographicHash::hash(source, QCryptographicHash::Sha256).toHex().left(16));
}

QString SyncEngine::syncDirectory() const
{
    const SettingsManager::SettingsSnapshot settings = m_settingsManager->snapshot();

    // Disable synchronization cleanly when the user has not enabled it or selected a folder.
    if (!settings.syncEnabled || settings.syncDirectory.trimmed().isEmpty()) {
        return QString();
    }
    return settings.syncDirectory;
}
