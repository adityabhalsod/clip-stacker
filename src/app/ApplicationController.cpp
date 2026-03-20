#include "ApplicationController.h"

#include <QLocalServer>
#include <QLocalSocket>

#include "core/ClipboardDatabase.h"
#include "core/ClipboardListener.h"
#include "core/HistoryManager.h"
#include "core/HotkeyManager.h"
#include "core/InputSimulator.h"
#include "core/Logger.h"
#include "core/SettingsManager.h"
#include "core/SyncEngine.h"
#include "ui/PopupController.h"
#include "ui/TrayController.h"

ApplicationController::ApplicationController(QObject *parent)
    : QObject(parent)
{
}

ApplicationController::~ApplicationController() = default;

bool ApplicationController::initialize()
{
    // Start the local command server first so duplicate launches can fail cleanly.
    if (!startCommandServer()) {
        qWarning(appLog) << "Another clip-stacker instance is already running.";
        return false;
    }

    // Construct the shared service objects in dependency order.
    m_settingsManager = std::make_unique<SettingsManager>();
    m_database = std::make_unique<ClipboardDatabase>();
    m_clipboardListener = std::make_unique<ClipboardListener>();
    m_inputSimulator = std::make_unique<InputSimulator>();

    // Open the persistent history store before the dependent services start.
    if (!m_database->initialize()) {
        return false;
    }

    // Construct the services that depend on settings, storage, and clipboard access.
    m_historyManager = std::make_unique<HistoryManager>(m_settingsManager.get(),
                                                        m_database.get(),
                                                        m_clipboardListener.get(),
                                                        m_inputSimulator.get());
    m_hotkeyManager = std::make_unique<HotkeyManager>();
    m_popupController = std::make_unique<PopupController>();
    m_trayController = std::make_unique<TrayController>();
    m_syncEngine = std::make_unique<SyncEngine>(m_settingsManager.get(), m_historyManager.get());

    // Load the initial popup model from SQLite before the QML window binds to it.
    if (!m_historyManager->initialize()) {
        return false;
    }

    // Load the popup UI and expose the history objects to QML.
    if (!m_popupController->initialize(m_historyManager->model(), m_historyManager.get())) {
        qCritical(appLog) << "Failed to initialize popup UI.";
        return false;
    }

    // Start the tray icon and connect user-visible runtime notices.
    m_trayController->initialize(m_popupController.get(), m_historyManager.get(), m_settingsManager.get());
    connect(m_historyManager.get(), &HistoryManager::activationNotice, m_trayController.get(), [this](const QString &message) {
        m_trayController->showMessage(QStringLiteral("clip-stacker"), message);
    });

    // Toggle the popup globally with Ctrl+Super+V when the current session supports native hotkeys.
    connect(m_hotkeyManager.get(), &HotkeyManager::activated, m_popupController.get(), &PopupController::togglePopup);
    connect(m_hotkeyManager.get(), &HotkeyManager::availabilityChanged, m_trayController.get(), [this](bool available, const QString &reason) {
        if (!available && !reason.isEmpty()) {
            m_trayController->showMessage(QStringLiteral("clip-stacker"), reason);
            qWarning(appLog) << reason;
        }
    });

    // Persist new clipboard items as they arrive from the live clipboard monitor.
    connect(m_clipboardListener.get(), &ClipboardListener::entryCaptured, m_historyManager.get(), &HistoryManager::handleCapturedEntry);

    // Refresh platform-sensitive services when settings change.
    connect(m_settingsManager.get(), &SettingsManager::settingsChanged, m_historyManager.get(), &HistoryManager::refreshModel);

    // Re-register the global hotkey whenever the user saves a new key combination in settings.
    connect(m_settingsManager.get(), &SettingsManager::settingsChanged, this, [this]() {
        m_hotkeyManager->registerHotkey(m_settingsManager->snapshot().hotkey);
    });

    // Register the hotkey from persistent settings on startup (defaults to Ctrl+Meta+V).
    m_hotkeyManager->registerHotkey(m_settingsManager->snapshot().hotkey);

    // Start monitoring the clipboard with the tray menu and control socket as the popup entry points.
    m_clipboardListener->start();

    // Start background synchronization after the main services are live.
    m_syncEngine->start();
    return true;
}

bool ApplicationController::startCommandServer()
{
    m_commandServer = std::make_unique<QLocalServer>();

    // Remove a stale local socket left behind by an unclean shutdown when no live server is reachable.
    QLocalSocket probe;
    probe.connectToServer(QStringLiteral("clip-stacker-control"));
    if (!probe.waitForConnected(100)) {
        QLocalServer::removeServer(QStringLiteral("clip-stacker-control"));
    }

    // Start the single-instance control socket used by the CLI popup toggle command.
    if (!m_commandServer->listen(QStringLiteral("clip-stacker-control"))) {
        return false;
    }

    connect(m_commandServer.get(), &QLocalServer::newConnection, this, [this]() {
        QLocalSocket *socket = m_commandServer->nextPendingConnection();
        connect(socket, &QLocalSocket::readyRead, this, [this, socket]() {
            const QByteArray command = socket->readAll().trimmed();
            if (command == QByteArrayLiteral("toggle") && m_popupController) {
                m_popupController->togglePopup();
            }
            socket->disconnectFromServer();
            socket->deleteLater();
        });
    });
    return true;
}
