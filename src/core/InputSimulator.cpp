#include "InputSimulator.h"

#include <QGuiApplication>

#include <cstring>

#if defined(Q_OS_LINUX)
#include <X11/Xatom.h>
#include <X11/keysym.h>
#include <X11/Xlib.h>
#include <X11/extensions/XTest.h>
#endif

InputSimulator::InputSimulator(QObject *parent)
    : QObject(parent)
{
}

void InputSimulator::capturePasteTarget()
{
#if defined(Q_OS_LINUX)
    // Reset the cached target before trying to read the active window from the display.
    m_targetWindow = 0;

    // Only X11 exposes the EWMH active-window property for reliable focus tracking.
    if (QGuiApplication::platformName() != QStringLiteral("xcb")) {
        return;
    }

    // Open a short-lived connection to query the root window property.
    Display *display = XOpenDisplay(nullptr);
    if (!display) {
        return;
    }

    // Resolve the EWMH atom that points at the currently active top-level window.
    const Atom activeWindowAtom = XInternAtom(display, "_NET_ACTIVE_WINDOW", True);
    if (activeWindowAtom == None) {
        XCloseDisplay(display);
        return;
    }

    // Read the window id stored in the root window property.
    Atom actualType = None;
    int actualFormat = 0;
    unsigned long itemCount = 0;
    unsigned long bytesAfter = 0;
    unsigned char *propertyData = nullptr;

    if (XGetWindowProperty(display,
                           DefaultRootWindow(display),
                           activeWindowAtom,
                           0, 1, False,
                           XA_WINDOW,
                           &actualType, &actualFormat,
                           &itemCount, &bytesAfter,
                           &propertyData) == Success
        && propertyData && itemCount == 1) {
        // Store the active window so it can be restored after the popup closes.
        m_targetWindow = *reinterpret_cast<unsigned long *>(propertyData);
        XFree(propertyData);
    }

    // Close the query connection immediately after reading the property.
    XCloseDisplay(display);
#endif
}

bool InputSimulator::restorePasteTarget() const
{
#if defined(Q_OS_LINUX)
    // Skip the restoration attempt when no target window was captured or the session is not X11.
    if (m_targetWindow == 0 || QGuiApplication::platformName() != QStringLiteral("xcb")) {
        return false;
    }

    // Open a short-lived connection to send the EWMH activation request.
    Display *display = XOpenDisplay(nullptr);
    if (!display) {
        return false;
    }

    // Resolve the EWMH atom used to ask the window manager to focus a window.
    const Atom activeWindowAtom = XInternAtom(display, "_NET_ACTIVE_WINDOW", False);
    if (activeWindowAtom == None) {
        XCloseDisplay(display);
        return false;
    }

    // Build a client message that asks the window manager to activate the previously focused window.
    XEvent event;
    std::memset(&event, 0, sizeof(event));
    event.xclient.type = ClientMessage;
    event.xclient.window = static_cast<Window>(m_targetWindow);
    event.xclient.message_type = activeWindowAtom;
    event.xclient.format = 32;
    // Source indication: 1 = normal application request.
    event.xclient.data.l[0] = 1;
    event.xclient.data.l[1] = CurrentTime;

    // Deliver the activation request through the root window so the window manager processes it.
    XSendEvent(display,
               DefaultRootWindow(display),
               False,
               SubstructureRedirectMask | SubstructureNotifyMask,
               &event);
    // Flush and sync to ensure the window manager receives the request before paste events follow.
    XSync(display, False);
    XCloseDisplay(display);
    return true;
#else
    return false;
#endif
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
        // Sync to ensure all fake key events are delivered before closing the connection.
        XSync(display, False);
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
