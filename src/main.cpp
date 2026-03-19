#include <QApplication>
#include <QCommandLineParser>
#include <QCommandLineOption>
#include <QLocalSocket>

#include "app/ApplicationController.h"
#include "core/Logger.h"

int main(int argc, char *argv[])
{
    // Create the GUI application because the clipboard, tray icon, and QML popup all require a full GUI stack.
    QApplication application(argc, argv);

    // Publish stable application metadata for settings, logs, DBus names, and installed desktop files.
    application.setApplicationName(QStringLiteral("clip-stacker"));
    application.setApplicationDisplayName(QStringLiteral("clip-stacker"));
    application.setOrganizationName(QStringLiteral("Aditya"));
    application.setDesktopFileName(QStringLiteral("clip-stacker"));
    application.setQuitOnLastWindowClosed(false);

    // Install the structured log sink before the rest of the application starts emitting messages.
    Logger::installMessageHandler();

    // Define the CLI used for a single-instance popup toggle and for optional diagnostics later on.
    QCommandLineParser parser;
    parser.setApplicationDescription(QStringLiteral("Clipboard history manager for Linux desktops."));
    parser.addHelpOption();
    parser.addVersionOption();

    // Expose a CLI toggle command so Wayland users can wire the popup to a desktop shortcut.
    QCommandLineOption togglePopupOption(QStringList{QStringLiteral("toggle-popup")},
                                         QStringLiteral("Ask the running instance to toggle the popup window."));
    parser.addOption(togglePopupOption);
    parser.process(application);

    // Forward the popup toggle request to an existing instance and exit early if one is already running.
    if (parser.isSet(togglePopupOption)) {
        QLocalSocket socket;
        socket.connectToServer(QStringLiteral("clip-stacker-control"));
        if (socket.waitForConnected(250)) {
            socket.write("toggle");
            socket.flush();
            socket.waitForBytesWritten(250);
            return 0;
        }
    }

    // Construct the main application coordinator that wires together storage, UI, sync, and platform services.
    ApplicationController controller;

    // Abort startup cleanly if a required subsystem such as SQLite or QML fails to initialize.
    if (!controller.initialize()) {
        return 1;
    }

    // Enter the Qt event loop for the long-running clipboard monitoring service.
    return application.exec();
}
