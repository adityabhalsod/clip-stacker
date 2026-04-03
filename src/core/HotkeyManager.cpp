#include "HotkeyManager.h"

#include <QCoreApplication>
#include <QGuiApplication>
#include <QProcess>
#include <QRegularExpression>
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

// Parse a hotkey string like Ctrl+Super+V into an X11 modifier mask and keysym.
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
    // Use the X11 passive key grab when the current session runs on X11.
    if (QGuiApplication::platformName() == QStringLiteral("xcb")) {
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

        // Tell the user when another application or the desktop shell already owns the shortcut.
        if (g_hotkeyGrabFailed) {
            emit availabilityChanged(false, QStringLiteral("The hotkey is already reserved by another application or the desktop shell."));
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
    }

    // On Wayland, fall back to registering via the desktop environment's custom keybinding system.
    return registerViaDesktopKeybinding(sequence);
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

    // Explicitly release the keyboard so the passive grab can fire again on the next key press.
    if (shouldActivate) {
        XAllowEvents(m_display, AsyncKeyboard, CurrentTime);
        XFlush(m_display);
    }

    // Emit the activation signal directly — we are already deferred via the QTimer poll callback.
    if (shouldActivate) {
        emit activated();
    }
#endif
}

void HotkeyManager::unregisterHotkey()
{
#if defined(Q_OS_LINUX)
    // Release the X11 passive key grab only when one is active on an X11 session.
    if (m_registered && m_display && QGuiApplication::platformName() == QStringLiteral("xcb")) {
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
    }
    // Desktop keybindings persist intentionally so the shortcut survives app restarts.
#endif

    // Reset the registration state regardless of which backend was used.
    m_registered = false;
    m_rootWindow = 0;
    m_keyCode = 0;
    m_modifiers = 0;
}

// Convert a portable hotkey string like "Ctrl+Alt+V" to GNOME binding format "<Control><Alt>v".
QString HotkeyManager::toGnomeBinding(const QString &sequence)
{
    const QStringList tokens = sequence.split(QLatin1Char('+'), Qt::SkipEmptyParts);
    if (tokens.isEmpty()) {
        return QString();
    }

    QString result;
    // Map each modifier token to its GNOME accelerator tag.
    for (int i = 0; i < tokens.size() - 1; ++i) {
        const QString mod = tokens.at(i).trimmed().toLower();
        if (mod == QStringLiteral("ctrl") || mod == QStringLiteral("control")) {
            result += QStringLiteral("<Control>");
        } else if (mod == QStringLiteral("alt")) {
            result += QStringLiteral("<Alt>");
        } else if (mod == QStringLiteral("shift")) {
            result += QStringLiteral("<Shift>");
        } else if (mod == QStringLiteral("meta") || mod == QStringLiteral("super") || mod == QStringLiteral("win")) {
            result += QStringLiteral("<Super>");
        }
    }
    // Append the trailing key name in lowercase as GNOME expects.
    result += tokens.last().trimmed().toLower();
    return result;
}

// Run a dconf write command and return true on success.
static bool dconfWrite(const QString &path, const QString &gvariantValue)
{
    QProcess proc;
    proc.start(QStringLiteral("dconf"), {QStringLiteral("write"), path, gvariantValue});
    return proc.waitForFinished(3000) && proc.exitCode() == 0;
}

// Run a dconf read command and return the trimmed output.
static QString dconfRead(const QString &path)
{
    QProcess proc;
    proc.start(QStringLiteral("dconf"), {QStringLiteral("read"), path});
    if (proc.waitForFinished(3000)) {
        return QString::fromUtf8(proc.readAllStandardOutput()).trimmed();
    }
    return QString();
}

bool HotkeyManager::registerViaDesktopKeybinding(const QString &sequence)
{
    // Only GNOME exposes the custom keybinding gsettings schema used below.
    const QString desktop = qEnvironmentVariable("XDG_CURRENT_DESKTOP");
    if (!desktop.contains(QStringLiteral("GNOME"), Qt::CaseInsensitive)) {
        emit availabilityChanged(false,
                                 QStringLiteral("Global hotkeys require X11 or GNOME on Wayland. "
                                                "Use clip-stacker --toggle-popup as a desktop shortcut fallback."));
        return false;
    }

    // Convert the portable hotkey string to the GNOME accelerator format.
    const QString gnomeBinding = toGnomeBinding(sequence);
    if (gnomeBinding.isEmpty()) {
        emit availabilityChanged(false, QStringLiteral("Invalid hotkey sequence."));
        return false;
    }

    // Build the command that GNOME will execute when the shortcut is pressed.
    const QString appPath = QCoreApplication::applicationFilePath();
    const QString command = appPath + QStringLiteral(" --toggle-popup");
    const QString basePath = QStringLiteral("/org/gnome/settings-daemon/plugins/media-keys/custom-keybindings/");

    // Read the current list of custom keybinding paths from gsettings.
    QProcess listProc;
    listProc.start(QStringLiteral("gsettings"),
                   {QStringLiteral("get"),
                    QStringLiteral("org.gnome.settings-daemon.plugins.media-keys"),
                    QStringLiteral("custom-keybindings")});
    if (!listProc.waitForFinished(3000)) {
        emit availabilityChanged(false, QStringLiteral("Failed to read GNOME keybinding settings."));
        return false;
    }
    const QString listOutput = QString::fromUtf8(listProc.readAllStandardOutput()).trimmed();

    // Scan existing custom keybinding slots for a pre-existing clip-stacker entry.
    int targetSlot = -1;
    bool alreadyInList = false;

    for (int i = 0; i < 50; ++i) {
        const QString slotPath = basePath + QStringLiteral("custom%1/").arg(i);
        const QString existingCmd = dconfRead(slotPath + QStringLiteral("command"));

        // Reuse the existing clip-stacker slot rather than creating a duplicate.
        if (existingCmd.contains(QStringLiteral("clip-stacker"))) {
            targetSlot = i;
            alreadyInList = listOutput.contains(slotPath);

            // Skip the write if the binding and command are already correct.
            const QString existingBinding = dconfRead(slotPath + QStringLiteral("binding"));
            if (existingBinding == QStringLiteral("'%1'").arg(gnomeBinding)
                && existingCmd.contains(appPath)) {
                m_registered = true;
                emit availabilityChanged(true, QStringLiteral("Global hotkey registered via GNOME custom keybinding."));
                return true;
            }
            break;
        }
    }

    // Find the first unused slot when no existing entry was found.
    if (targetSlot == -1) {
        for (int i = 0; i < 50; ++i) {
            const QString slotPath = basePath + QStringLiteral("custom%1/").arg(i);
            const QString existingName = dconfRead(slotPath + QStringLiteral("name"));
            // An empty read means the slot is not yet used.
            if (existingName.isEmpty()) {
                targetSlot = i;
                break;
            }
        }
    }

    if (targetSlot == -1) {
        emit availabilityChanged(false, QStringLiteral("Could not find a free GNOME custom keybinding slot."));
        return false;
    }

    // Write the three dconf keys that define one custom keybinding entry.
    const QString slotPath = basePath + QStringLiteral("custom%1/").arg(targetSlot);
    bool ok = true;
    ok = ok && dconfWrite(slotPath + QStringLiteral("name"), QStringLiteral("'clip-stacker'"));
    ok = ok && dconfWrite(slotPath + QStringLiteral("command"), QStringLiteral("'%1'").arg(command));
    ok = ok && dconfWrite(slotPath + QStringLiteral("binding"), QStringLiteral("'%1'").arg(gnomeBinding));

    if (!ok) {
        emit availabilityChanged(false, QStringLiteral("Failed to write GNOME keybinding settings."));
        return false;
    }

    // Append the new slot path to the gsettings list when it is not already registered.
    if (!alreadyInList) {
        // Parse existing slot paths from the gsettings output (e.g. ['/path/custom0/', '/path/custom1/']).
        QStringList paths;
        const QRegularExpression pathRegex(QStringLiteral("'([^']+)'"));
        auto matchIterator = pathRegex.globalMatch(listOutput);
        while (matchIterator.hasNext()) {
            paths.append(matchIterator.next().captured(1));
        }
        paths.append(slotPath);

        // Rebuild the GVariant list string expected by gsettings set.
        QStringList quotedPaths;
        for (const QString &p : std::as_const(paths)) {
            quotedPaths.append(QStringLiteral("'%1'").arg(p));
        }
        const QString newList = QStringLiteral("[%1]").arg(quotedPaths.join(QStringLiteral(", ")));

        QProcess setProc;
        setProc.start(QStringLiteral("gsettings"),
                      {QStringLiteral("set"),
                       QStringLiteral("org.gnome.settings-daemon.plugins.media-keys"),
                       QStringLiteral("custom-keybindings"),
                       newList});
        if (!setProc.waitForFinished(3000) || setProc.exitCode() != 0) {
            emit availabilityChanged(false, QStringLiteral("Failed to update GNOME custom keybindings list."));
            return false;
        }
    }

    // Mark registration as successful so the settings-changed handler knows it worked.
    m_registered = true;
    emit availabilityChanged(true, QStringLiteral("Global hotkey registered via GNOME custom keybinding."));
    return true;
}
