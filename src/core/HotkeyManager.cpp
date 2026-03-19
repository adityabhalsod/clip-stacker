#include "HotkeyManager.h"

#include <QGuiApplication>
#include <QStringList>
#include <QTimer>

#include <array>

#if defined(Q_OS_LINUX)
#include <X11/Xlib.h>
#include <X11/Xutil.h>
#endif

namespace {

#if defined(Q_OS_LINUX)

// Track whether the last X11 grab attempt failed because another client already owns the shortcut.
bool g_hotkeyGrabFailed = false;

// Capture X11 grab failures synchronously so registration can report a useful error.
int hotkeyErrorHandler(Display *, XErrorEvent *event)
{
    // Mark the grab as failed when another client already reserved the same accelerator.
    if (event->error_code == BadAccess) {
        g_hotkeyGrabFailed = true;
    }

    // Return zero because the handler only records the failure state.
    return 0;
}

#endif

// Parse a simple hotkey string like Super+V into an X11 modifier mask and keysym.
bool parseSequence(const QString &sequence, unsigned int *modifiers, QString *keyToken)
{
    const QStringList tokens = sequence.split(QLatin1Char('+'), Qt::SkipEmptyParts);
    if (tokens.isEmpty()) {
        return false;
    }

    *modifiers = 0;
    for (int index = 0; index < tokens.size() - 1; ++index) {
        const QString token = tokens.at(index).trimmed().toLower();
        if (token == QStringLiteral("shift")) {
            *modifiers |= ShiftMask;
        } else if (token == QStringLiteral("ctrl") || token == QStringLiteral("control")) {
            *modifiers |= ControlMask;
        } else if (token == QStringLiteral("alt")) {
            *modifiers |= Mod1Mask;
        } else if (token == QStringLiteral("super") || token == QStringLiteral("meta") || token == QStringLiteral("win")) {
            *modifiers |= Mod4Mask;
        }
    }

    *keyToken = tokens.last().trimmed().toUpper();
    return !keyToken->isEmpty();
}

} // namespace

HotkeyManager::HotkeyManager(QObject *parent)
    : QObject(parent)
{
}

HotkeyManager::~HotkeyManager()
{
    // Release any global key grab before shutdown.
    unregisterHotkey();

#if defined(Q_OS_LINUX)
    // Close the dedicated X11 connection after all grabs have been released.
    if (m_display) {
        XCloseDisplay(m_display);
        m_display = nullptr;
    }
#endif
}

bool HotkeyManager::registerHotkey(const QString &sequence)
{
    unregisterHotkey();

#if defined(Q_OS_LINUX)
    // Only X11 exposes a portable global key-grab API to regular applications.
    if (QGuiApplication::platformName() != QStringLiteral("xcb")) {
        emit availabilityChanged(false,
                                 QStringLiteral("Global hotkeys require compositor or portal support on Wayland. Use clip-stacker --toggle-popup as a desktop shortcut fallback."));
        return false;
    }

    unsigned int modifiers = 0;
    QString keyToken;
    if (!parseSequence(sequence, &modifiers, &keyToken)) {
        emit availabilityChanged(false, QStringLiteral("Invalid hotkey sequence."));
        return false;
    }

    // Open one persistent X11 connection so the passive key grab survives after registration.
    if (!m_display) {
        m_display = XOpenDisplay(nullptr);
    }

    // Abort when the application cannot reach the current X11 display.
    if (!m_display) {
        emit availabilityChanged(false, QStringLiteral("Failed to open the X11 display."));
        return false;
    }

    // Resolve the trailing key token to the X11 keysym that will be grabbed globally.
    const KeySym keySym = XStringToKeysym(keyToken.toLatin1().constData());

    // Convert the keysym to the keycode understood by the current X11 server.
    const unsigned int keyCode = XKeysymToKeycode(m_display, keySym);

    // Stop when X11 cannot map the requested key token to a usable keycode.
    if (!keyCode) {
        emit availabilityChanged(false, QStringLiteral("Unsupported hotkey key."));
        return false;
    }

    // Remember the root window used for both registration and later cleanup.
    m_rootWindow = DefaultRootWindow(m_display);

    // Try the grab against the common Caps Lock and Num Lock modifier combinations as well.
    const std::array<unsigned int, 4> extraModifierMasks{0U, LockMask, Mod2Mask, static_cast<unsigned int>(LockMask | Mod2Mask)};

    // Install a temporary X11 error handler so grab conflicts become a controlled runtime message.
    g_hotkeyGrabFailed = false;
    const auto previousErrorHandler = XSetErrorHandler(hotkeyErrorHandler);

    // Register the passive key grab for each lock-key combination that users may have enabled.
    for (const unsigned int extraMask : extraModifierMasks) {
        XGrabKey(m_display,
                 static_cast<int>(keyCode),
                 modifiers | extraMask,
                 m_rootWindow,
                 False,
                 GrabModeAsync,
                 GrabModeAsync);
    }

    // Flush grab requests immediately so X11 reports any conflicts while registration is in progress.
    XSync(m_display, False);

    // Restore the previous X11 error handler now that registration has completed.
    XSetErrorHandler(previousErrorHandler);

    // Tell the user when another application or the desktop shell already owns Super+V.
    if (g_hotkeyGrabFailed) {
        emit availabilityChanged(false, QStringLiteral("Super+V is already reserved by another application or desktop shortcut."));
        return false;
    }

    // Create a polling timer so the dedicated X11 connection is checked at regular intervals.
    if (!m_pollTimer) {
        m_pollTimer = new QTimer(this);
        // Poll every 50 ms — responsive enough for a keyboard shortcut, cheap for the event loop.
        m_pollTimer->setInterval(50);
        connect(m_pollTimer, &QTimer::timeout, this, &HotkeyManager::processX11Events);
    }

    // Start polling after successful registration.
    m_pollTimer->start();

    // Persist the registration state used by cleanup and event matching.
    m_registered = true;
    m_keyCode = keyCode;
    m_modifiers = modifiers;
    emit availabilityChanged(true, QStringLiteral("Global hotkey registered."));
    return true;
#else
    Q_UNUSED(sequence)
    emit availabilityChanged(false, QStringLiteral("Global hotkeys are not available on this platform."));
    return false;
#endif
}

void HotkeyManager::processX11Events()
{
#if defined(Q_OS_LINUX)
    // Stop early when no hotkey is registered or the dedicated X11 connection is unavailable.
    if (!m_registered || !m_display) {
        return;
    }

    // Track whether a matching key press was found so the signal can be emitted outside the Xlib loop.
    bool shouldActivate = false;

    // Read and handle each queued X11 event generated for the passive key grab connection.
    while (XPending(m_display) > 0) {
        XEvent event;
        XNextEvent(m_display, &event);

        // Check if the queued key press matches the configured hotkey.
        if (event.type == KeyPress) {
            const auto &keyEvent = event.xkey;
            // Strip lock-state bits and compare only the meaningful accelerator modifiers.
            const unsigned int relevantState = keyEvent.state & (ShiftMask | ControlMask | Mod1Mask | Mod4Mask);

            // Record a match so the activation signal can be deferred safely.
            if (static_cast<unsigned int>(keyEvent.keycode) == m_keyCode && relevantState == m_modifiers) {
                shouldActivate = true;
            }
        }
    }

    // Flush any outgoing requests so the connection stays clean for the next grab cycle.
    XFlush(m_display);

    // Defer the activation signal to the next event-loop pass to prevent Xlib reentrancy from Qt widget operations.
    if (shouldActivate) {
        QTimer::singleShot(0, this, [this]() { emit activated(); });
    }
#endif
}

void HotkeyManager::unregisterHotkey()
{
#if defined(Q_OS_LINUX)
    // Release the previous X11 key grab when the registration changes.
    if (!m_registered || !m_display || QGuiApplication::platformName() != QStringLiteral("xcb")) {
        return;
    }

    // Stop polling while the passive grab is being removed.
    if (m_pollTimer) {
        m_pollTimer->stop();
    }

    // Remove the grab for the base modifier combination and the common lock-key variants.
    const std::array<unsigned int, 4> extraModifierMasks{0U, LockMask, Mod2Mask, static_cast<unsigned int>(LockMask | Mod2Mask)};
    for (const unsigned int extraMask : extraModifierMasks) {
        XUngrabKey(m_display, static_cast<int>(m_keyCode), m_modifiers | extraMask, m_rootWindow);
    }

    // Flush the ungrab requests so the X server releases the shortcut immediately.
    XSync(m_display, False);
#endif

    m_registered = false;
    m_rootWindow = 0;
    m_keyCode = 0;
    m_modifiers = 0;
}
