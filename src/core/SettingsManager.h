#pragma once

#include <QObject>
#include <QRegularExpression>
#include <QSettings>
#include <QString>
#include <QStringList>

// Centralize persistent user settings so the service, UI, and sync engine stay consistent.
class SettingsManager : public QObject
{
    Q_OBJECT

public:
    // Expose the key configuration values used throughout the application.
    struct SettingsSnapshot {
        int historyLimit = 250;
        bool syncEnabled = false;
        QString syncDirectory;
        // Fixed global shortcut — not user-configurable; both lowercase and uppercase V are accepted.
        QString hotkey = QStringLiteral("Ctrl+Alt+V");
        bool storeSensitiveData = false;
        QStringList blockedApplications;
        QStringList sensitivePatterns;
    };

    // Construct the settings wrapper around Qt's native platform settings backend.
    explicit SettingsManager(QObject *parent = nullptr);

    // Return an immutable snapshot suitable for non-UI components that only need current values.
    SettingsSnapshot snapshot() const;

    // Persist a full settings snapshot after the settings UI or migration layer changes it.
    void setSnapshot(const SettingsSnapshot &snapshot);

    // Decide whether a clipboard item should be treated as sensitive based on source-app and text patterns.
    bool shouldTreatAsSensitive(const QString &sourceApplication, const QString &plainText) const;

signals:
    // Notify long-running services when configuration changes require a runtime refresh.
    void settingsChanged();

private:
    // Keep the settings backend alive for the lifetime of the process.
    QSettings m_settings;
};
