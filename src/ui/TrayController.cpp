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

    // Provide a toggle action for enabling or disabling per-user autostart.
    m_autostartAction = menu->addAction(QString());
    refreshAutostartAction();
    connect(m_autostartAction, &QAction::triggered, this, [this]() {
        // Toggle autostart based on whether it is currently enabled.
        if (isAutostartEnabled()) {
            disableAutostart();
        } else {
            enableAutostart();
        }
        // Refresh the label so the menu reflects the new state immediately.
        refreshAutostartAction();
    });

    menu->addSeparator();

    // Provide a standard quit action for the resident background service.
    QAction *quitAction = menu->addAction(QStringLiteral("Quit"));
    connect(quitAction, &QAction::triggered, qApp, &QApplication::quit);

    // Attach the menu and show the tray indicator.
    m_trayIcon.setContextMenu(menu);
    m_trayIcon.show();

    // Support left-click and double-click activation as quick popup toggles.
    connect(&m_trayIcon, &QSystemTrayIcon::activated, this, [popupController](QSystemTrayIcon::ActivationReason reason) {
        // Open the clipboard history popup on single-click or double-click.
        if (reason == QSystemTrayIcon::Trigger || reason == QSystemTrayIcon::DoubleClick) {
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
    // Build the standard per-user autostart directory path.
    const QString autostartDirectory = QStandardPaths::writableLocation(QStandardPaths::ConfigLocation) + QStringLiteral("/autostart");
    QDir().mkpath(autostartDirectory);
    const QString desktopFilePath = autostartDirectory + QStringLiteral("/clip-stacker.desktop");

    QFile desktopFile(desktopFilePath);
    if (desktopFile.open(QIODevice::WriteOnly | QIODevice::Truncate | QIODevice::Text)) {
        // Resolve the actual executable path so the autostart entry always points to the installed binary.
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

void TrayController::disableAutostart()
{
    // Resolve the autostart desktop file path for the current user.
    const QString desktopFilePath = QStandardPaths::writableLocation(QStandardPaths::ConfigLocation)
                                    + QStringLiteral("/autostart/clip-stacker.desktop");

    // Remove the desktop entry file to stop the session from launching clip-stacker on login.
    if (QFile::exists(desktopFilePath)) {
        if (QFile::remove(desktopFilePath)) {
            showMessage(QStringLiteral("clip-stacker"), QStringLiteral("Autostart disabled for the current user."));
        } else {
            showMessage(QStringLiteral("clip-stacker"), QStringLiteral("Failed to remove the autostart desktop entry."));
        }
    } else {
        showMessage(QStringLiteral("clip-stacker"), QStringLiteral("Autostart is already disabled."));
    }
}

void TrayController::refreshAutostartAction()
{
    // Update the menu action text so the user always sees the correct toggle option.
    if (m_autostartAction) {
        m_autostartAction->setText(isAutostartEnabled()
                                      ? QStringLiteral("Disable Auto-Start")
                                      : QStringLiteral("Enable Auto-Start"));
    }
}

bool TrayController::isAutostartEnabled() const
{
    // Check for the per-user autostart desktop file to determine current state.
    const QString desktopFilePath = QStandardPaths::writableLocation(QStandardPaths::ConfigLocation)
                                    + QStringLiteral("/autostart/clip-stacker.desktop");
    return QFile::exists(desktopFilePath);
}
