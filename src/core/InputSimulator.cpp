#include "InputSimulator.h"

#include <QGuiApplication>

#if defined(Q_OS_LINUX)
#include <X11/keysym.h>
#include <X11/Xlib.h>
#include <X11/extensions/XTest.h>
#endif

InputSimulator::InputSimulator(QObject *parent)
    : QObject(parent)
{
}

bool InputSimulator::simulatePaste() const
{
#if defined(Q_OS_LINUX)
    // Use the native XTest extension when the application runs under X11.
    if (QGuiApplication::platformName() == QStringLiteral("xcb")) {
        Display *display = XOpenDisplay(nullptr);
        if (!display) {
            return false;
        }

        const KeyCode controlKeyCode = XKeysymToKeycode(display, XK_Control_L);
        const KeyCode vKeyCode = XKeysymToKeycode(display, XK_V);
        if (!controlKeyCode || !vKeyCode) {
            XCloseDisplay(display);
            return false;
        }

        XTestFakeKeyEvent(display, controlKeyCode, True, CurrentTime);
        XTestFakeKeyEvent(display, vKeyCode, True, CurrentTime);
        XTestFakeKeyEvent(display, vKeyCode, False, CurrentTime);
        XTestFakeKeyEvent(display, controlKeyCode, False, CurrentTime);
        XFlush(display);
        XCloseDisplay(display);
        return true;
    }
#endif

    // Return false on platforms without a safe built-in paste backend so the caller can fall back gracefully.
    return false;
}

QString InputSimulator::strategyDescription() const
{
    // Describe whether paste automation is natively available in the current session.
    if (QGuiApplication::platformName() == QStringLiteral("xcb")) {
        return QStringLiteral("X11 XTest");
    }
    return QStringLiteral("Clipboard-only fallback");
}
