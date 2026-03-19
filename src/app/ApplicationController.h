#pragma once

#include <QObject>
#include <memory>

class ClipboardDatabase;
class ClipboardListener;
class HistoryManager;
class HotkeyManager;
class InputSimulator;
class QLocalServer;
class SettingsManager;
class SyncEngine;
class PopupController;
class TrayController;

// Wire together the service layer, popup UI, tray integration, sync loop, and single-instance command socket.
class ApplicationController : public QObject
{
    Q_OBJECT

public:
    // Construct the top-level application coordinator.
    explicit ApplicationController(QObject *parent = nullptr);

    // Destroy the coordinator after all owned subsystem types are complete in the implementation file.
    ~ApplicationController() override;

    // Initialize all subsystems and start the resident clipboard service.
    bool initialize();

private:
    // Start the single-instance local socket server used by the CLI popup toggle command.
    bool startCommandServer();

    // Own the persistent runtime settings.
    std::unique_ptr<SettingsManager> m_settingsManager;

    // Own the SQLite history store.
    std::unique_ptr<ClipboardDatabase> m_database;

    // Own the live clipboard monitor.
    std::unique_ptr<ClipboardListener> m_clipboardListener;

    // Own the paste simulation backend.
    std::unique_ptr<InputSimulator> m_inputSimulator;

    // Own the history coordination layer and QML list model.
    std::unique_ptr<HistoryManager> m_historyManager;

    // Own the native global hotkey backend used to toggle the popup on supported sessions.
    std::unique_ptr<HotkeyManager> m_hotkeyManager;

    // Own the QML popup bridge.
    std::unique_ptr<PopupController> m_popupController;

    // Own the persistent tray icon and user-facing menu.
    std::unique_ptr<TrayController> m_trayController;

    // Own the shared-folder sync engine.
    std::unique_ptr<SyncEngine> m_syncEngine;

    // Own the local command socket server for single-instance control.
    std::unique_ptr<QLocalServer> m_commandServer;
};
