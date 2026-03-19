#pragma once

#include <QObject>
#include <QString>

class QTimer;

// Forward declare the opaque X11 display type so the header does not leak X11 macros into Qt's MOC step.
struct _XDisplay;

// Register the global popup hotkey when the current desktop session exposes a supported native API.
class HotkeyManager : public QObject
{
    Q_OBJECT

public:
    // Construct the native hotkey manager.
    explicit HotkeyManager(QObject *parent = nullptr);

    // Release any native hotkey registration before the service exits.
    ~HotkeyManager() override;

    // Register the configured hotkey string and report whether the registration succeeded.
    bool registerHotkey(const QString &sequence);

signals:
    // Fire when the registered hotkey has been pressed globally.
    void activated();

    // Report whether global hotkey registration is available on the current session.
    void availabilityChanged(bool available, const QString &reason);

private:
    // Drain the dedicated X11 connection and emit the activation signal for matching key presses.
    void processX11Events();

    // Release any previous native key grab before re-registering or shutting down.
    void unregisterHotkey();

    // Track whether a native grab is currently active.
    bool m_registered = false;

    // Keep a dedicated X11 connection alive so the global key grab remains active for the app lifetime.
    _XDisplay *m_display = nullptr;

    // Poll the dedicated X11 connection at regular intervals to detect hotkey presses.
    QTimer *m_pollTimer = nullptr;

    // Cache the root window used for X11 passive key grabs.
    unsigned long m_rootWindow = 0;

    // Store the keycode for the currently registered hotkey.
    unsigned int m_keyCode = 0;

    // Store the X11 modifier mask for the currently registered hotkey.
    unsigned int m_modifiers = 0;
};
