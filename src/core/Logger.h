#pragma once

#include <QLoggingCategory>

Q_DECLARE_LOGGING_CATEGORY(appLog)

// Install a file-backed Qt message handler so background errors remain diagnosable after startup.
class Logger
{
public:
    // Register the process-wide message handler and create the log directory if needed.
    static void installMessageHandler();
};
