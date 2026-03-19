#include "TrayController.h"

#include <QAction>
#include <QApplication>
#include <QDir>
#include <QFile>
#include <QIcon>
#include <QMenu>
#include <QStandardPaths>

#include "core/HistoryManager.h"
#include "core/SettingsManager.h"
#include "ui/PopupController.h"
#include "ui/SettingsDialog.h"

TrayController::TrayController(QObject *parent)
    : QObject(parent)
{
}

void TrayController::initialize(PopupController *popupController, HistoryManager *historyManager, SettingsManager *settingsManager)
{
    // Keep the settings manager available for the tray settings action.
    m_settingsManager = settingsManager;

    // Use the installed icon theme name so the tray integrates with Linux desktops naturally.
    m_trayIcon.setIcon(QIcon::fromTheme(QStringLiteral("clip-stacker"), QIcon(QStringLiteral(":/icons/clip-stacker.svg"))));
    m_trayIcon.setToolTip(QStringLiteral("clip-stacker"));

    auto *menu = new QMenu();

    // Provide the primary action that toggles the popup history UI.
    QAction *showHistoryAction = menu->addAction(QStringLiteral("Show Clipboard History"));
    connect(showHistoryAction, &QAction::triggered, popupController, &PopupController::togglePopup);

    // Provide a quick action for clearing non-pinned history entries.
    QAction *clearHistoryAction = menu->addAction(QStringLiteral("Clear Unpinned History"));
    connect(clearHistoryAction, &QAction::triggered, historyManager, &HistoryManager::clearHistory);

    // Provide a native settings dialog for retention, sync, hotkey, and privacy configuration.
    QAction *settingsAction = menu->addAction(QStringLiteral("Settings"));
    connect(settingsAction, &QAction::triggered, this, &TrayController::showSettings);

    // Provide a first-run convenience action for enabling per-user autostart.
    QAction *autostartAction = menu->addAction(QStringLiteral("Enable Auto-Start"));
    connect(autostartAction, &QAction::triggered, this, &TrayController::enableAutostart);

    menu->addSeparator();

    // Provide a standard quit action for the resident background service.
    QAction *quitAction = menu->addAction(QStringLiteral("Quit"));
    connect(quitAction, &QAction::triggered, qApp, &QApplication::quit);

    // Attach the menu and show the tray indicator.
    m_trayIcon.setContextMenu(menu);
    m_trayIcon.show();

    // Support left-click activation as a quick popup toggle.
    connect(&m_trayIcon, &QSystemTrayIcon::activated, this, [popupController](QSystemTrayIcon::ActivationReason reason) {
        if (reason == QSystemTrayIcon::Trigger) {
            popupController->togglePopup();
        }
    });
}

void TrayController::showMessage(const QString &title, const QString &message)
{
    // Surface lightweight runtime notices such as hotkey fallback behavior and paste limitations.
    m_trayIcon.showMessage(title, message, QSystemTrayIcon::Information, 5000);
}

void TrayController::showSettings()
{
    // Open the settings dialog modally so changes are applied intentionally and atomically.
    SettingsDialog dialog(m_settingsManager);
    dialog.exec();
}

void TrayController::enableAutostart()
{
    const QString autostartDirectory = QStandardPaths::writableLocation(QStandardPaths::ConfigLocation) + QStringLiteral("/autostart");
    QDir().mkpath(autostartDirectory);
    const QString desktopFilePath = autostartDirectory + QStringLiteral("/clip-stacker.desktop");

    QFile desktopFile(desktopFilePath);
    if (desktopFile.open(QIODevice::WriteOnly | QIODevice::Truncate | QIODevice::Text)) {
        const QString executablePath = QCoreApplication::applicationFilePath();
        const QByteArray content = QStringLiteral(
                                      "[Desktop Entry]\n"
                                      "Type=Application\n"
                                      "Name=clip-stacker\n"
                                      "Exec=%1\n"
                                      "Icon=clip-stacker\n"
                                      "Terminal=false\n"
                                      "Categories=Utility;\n"
                                      "X-GNOME-Autostart-enabled=true\n")
                                      .arg(executablePath)
                                      .toUtf8();
        desktopFile.write(content);
        desktopFile.close();
        showMessage(QStringLiteral("clip-stacker"), QStringLiteral("Autostart enabled for the current user."));
    } else {
        showMessage(QStringLiteral("clip-stacker"), QStringLiteral("Failed to write the autostart desktop entry."));
    }
}
