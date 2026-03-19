#include "Logger.h"

#include <QDateTime>
#include <QDir>
#include <QFile>
#include <QMutex>
#include <QMutexLocker>
#include <QStandardPaths>
#include <QTextStream>

Q_LOGGING_CATEGORY(appLog, "clipstacker.app")

namespace {

// Serialize concurrent log writes because Qt can emit messages from multiple threads.
QMutex &logMutex()
{
    static QMutex mutex;
    return mutex;
}

// Open the append-only log file stored in the standard per-user application data directory.
QFile &logFile()
{
    static QFile file;
    if (!file.isOpen()) {
        const QString logDirectoryPath = QStandardPaths::writableLocation(QStandardPaths::AppDataLocation) + QStringLiteral("/logs");
        QDir().mkpath(logDirectoryPath);
        file.setFileName(logDirectoryPath + QStringLiteral("/clip-stacker.log"));
        file.open(QIODevice::WriteOnly | QIODevice::Append | QIODevice::Text);
    }
    return file;
}

// Map the Qt message type to a short severity token that remains easy to scan in plain text logs.
QString severityForType(QtMsgType type)
{
    switch (type) {
    case QtDebugMsg:
        return QStringLiteral("DEBUG");
    case QtInfoMsg:
        return QStringLiteral("INFO");
    case QtWarningMsg:
        return QStringLiteral("WARN");
    case QtCriticalMsg:
        return QStringLiteral("ERROR");
    case QtFatalMsg:
        return QStringLiteral("FATAL");
    }
    return QStringLiteral("INFO");
}

// Write one formatted line to the log file and stderr so both packaged and local builds are diagnosable.
void messageHandler(QtMsgType type, const QMessageLogContext &context, const QString &message)
{
    QMutexLocker locker(&logMutex());
    QTextStream stream(&logFile());
    const QString line = QStringLiteral("%1 [%2] %3 (%4:%5, %6)\n")
                             .arg(QDateTime::currentDateTime().toString(Qt::ISODateWithMs),
                                  severityForType(type),
                                  message,
                                  QString::fromUtf8(context.file ? context.file : "?"),
                                  QString::number(context.line),
                                  QString::fromUtf8(context.function ? context.function : "?"));
    stream << line;
    stream.flush();
    fprintf(stderr, "%s", line.toLocal8Bit().constData());
}

} // namespace

void Logger::installMessageHandler()
{
    // Install the process-wide log sink once near startup so all subsystems share the same diagnostics path.
    qInstallMessageHandler(messageHandler);
}
