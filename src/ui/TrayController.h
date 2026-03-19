#pragma once

#include <QObject>
#include <QSystemTrayIcon>

class HistoryManager;
class PopupController;
class SettingsManager;

// Own the persistent tray icon, menu actions, and lightweight desktop notifications.
class TrayController : public QObject
{
    Q_OBJECT

public:
    // Construct the tray integration controller.
    explicit TrayController(QObject *parent = nullptr);

    // Create the tray icon, bind menu actions, and show the resident background-service indicator.
    void initialize(PopupController *popupController, HistoryManager *historyManager, SettingsManager *settingsManager);

public slots:
    // Display a lightweight informational notification from other subsystems.
    void showMessage(const QString &title, const QString &message);

private:
    // Open the native settings dialog so users can change runtime behavior without editing files.
    void showSettings();

    // Enable autostart by writing a per-user desktop entry into ~/.config/autostart.
    void enableAutostart();

    // Keep the shared runtime settings available for the settings dialog.
    SettingsManager *m_settingsManager = nullptr;

    // Keep the tray icon alive for the life of the background service.
    QSystemTrayIcon m_trayIcon;
};
