#pragma once

#include <QObject>

// Trigger a native paste operation after the chosen history item has been restored to the clipboard.
class InputSimulator : public QObject
{
    Q_OBJECT

public:
    // Construct the paste simulator for the current desktop session.
    explicit InputSimulator(QObject *parent = nullptr);

    // Record the currently focused top-level window before the popup steals focus.
    void capturePasteTarget();

    // Ask the window manager to re-activate the captured target window.
    bool restorePasteTarget() const;

    // Attempt to synthesize the platform paste gesture and report whether it succeeded.
    bool simulatePaste() const;

    // Describe the currently available paste strategy for diagnostics and tray notifications.
    QString strategyDescription() const;

private:
    // Cache the X11 window id that held focus before the popup opened.
    unsigned long m_targetWindow = 0;
};
