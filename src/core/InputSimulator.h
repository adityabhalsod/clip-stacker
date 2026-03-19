#pragma once

#include <QObject>

// Trigger a native paste operation after the chosen history item has been restored to the clipboard.
class InputSimulator : public QObject
{
    Q_OBJECT

public:
    // Construct the paste simulator for the current desktop session.
    explicit InputSimulator(QObject *parent = nullptr);

    // Attempt to synthesize the platform paste gesture and report whether it succeeded.
    bool simulatePaste() const;

    // Describe the currently available paste strategy for diagnostics and tray notifications.
    QString strategyDescription() const;
};
