#include "SettingsManager.h"

#include <QDir>
#include <QStandardPaths>

SettingsManager::SettingsManager(QObject *parent)
    : QObject(parent)
    , m_settings(QStringLiteral("Aditya"), QStringLiteral("clip-stacker"))
{
    // Seed the blocked-application list with common password managers to avoid accidental persistence.
    if (!m_settings.contains(QStringLiteral("privacy/blockedApplications"))) {
        m_settings.setValue(QStringLiteral("privacy/blockedApplications"),
                            QStringList{QStringLiteral("KeePassXC"), QStringLiteral("1Password"), QStringLiteral("Bitwarden")});
    }

    // Seed a conservative set of sensitive-text patterns that catch obvious secrets without being too noisy.
    if (!m_settings.contains(QStringLiteral("privacy/sensitivePatterns"))) {
        m_settings.setValue(QStringLiteral("privacy/sensitivePatterns"),
                            QStringList{QStringLiteral("BEGIN RSA PRIVATE KEY"),
                                        QStringLiteral("BEGIN OPENSSH PRIVATE KEY"),
                                        QStringLiteral("ghp_"),
                                        QStringLiteral("AIza")});
    }
}

SettingsManager::SettingsSnapshot SettingsManager::snapshot() const
{
    // Return the effective settings with sensible defaults for first-run sessions.
    SettingsSnapshot snapshot;
    snapshot.historyLimit = m_settings.value(QStringLiteral("history/limit"), 250).toInt();
    snapshot.syncEnabled = m_settings.value(QStringLiteral("sync/enabled"), false).toBool();
    snapshot.syncDirectory = m_settings.value(QStringLiteral("sync/directory"), QString()).toString();
    snapshot.hotkey = m_settings.value(QStringLiteral("hotkey/sequence"), QStringLiteral("Super+V")).toString();
    snapshot.storeSensitiveData = m_settings.value(QStringLiteral("privacy/storeSensitiveData"), false).toBool();
    snapshot.blockedApplications = m_settings.value(QStringLiteral("privacy/blockedApplications")).toStringList();
    snapshot.sensitivePatterns = m_settings.value(QStringLiteral("privacy/sensitivePatterns")).toStringList();
    return snapshot;
}

void SettingsManager::setSnapshot(const SettingsManager::SettingsSnapshot &snapshot)
{
    // Persist the history retention ceiling used by the database pruning routine.
    m_settings.setValue(QStringLiteral("history/limit"), snapshot.historyLimit);

    // Persist the sync settings used by the file-based multi-device synchronization engine.
    m_settings.setValue(QStringLiteral("sync/enabled"), snapshot.syncEnabled);
    m_settings.setValue(QStringLiteral("sync/directory"), snapshot.syncDirectory);

    // Persist the hotkey string even when the current platform falls back to a manual shortcut setup.
    m_settings.setValue(QStringLiteral("hotkey/sequence"), snapshot.hotkey);

    // Persist the privacy controls that gate sensitive-entry storage and sync.
    m_settings.setValue(QStringLiteral("privacy/storeSensitiveData"), snapshot.storeSensitiveData);
    m_settings.setValue(QStringLiteral("privacy/blockedApplications"), snapshot.blockedApplications);
    m_settings.setValue(QStringLiteral("privacy/sensitivePatterns"), snapshot.sensitivePatterns);

    // Flush the settings backend so background-service restarts do not lose a just-saved update.
    m_settings.sync();

    // Notify the running services that they should refresh any cached settings.
    emit settingsChanged();
}

bool SettingsManager::shouldTreatAsSensitive(const QString &sourceApplication, const QString &plainText) const
{
    const SettingsSnapshot currentSnapshot = snapshot();

    // Treat clipboard items from blocked applications as sensitive to avoid storing secret material.
    for (const QString &blockedApplication : currentSnapshot.blockedApplications) {
        if (!blockedApplication.isEmpty() && sourceApplication.contains(blockedApplication, Qt::CaseInsensitive)) {
            return true;
        }
    }

    // Treat clipboard items matching obvious secret markers as sensitive.
    for (const QString &pattern : currentSnapshot.sensitivePatterns) {
        const QRegularExpression regularExpression(QRegularExpression::escape(pattern), QRegularExpression::CaseInsensitiveOption);
        if (regularExpression.match(plainText).hasMatch()) {
            return true;
        }
    }

    return false;
}
