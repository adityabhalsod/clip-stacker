#include "ClipboardListener.h"

#include <QBuffer>
#include <QClipboard>
#include <QCryptographicHash>
#include <QDateTime>
#include <QGuiApplication>
#include <QImage>
#include <QMimeData>
#include <QUrl>

#if defined(Q_OS_LINUX)
#include <QGuiApplication>
#include <X11/Xatom.h>
#include <X11/Xlib.h>
#include <X11/Xutil.h>
#endif

namespace {

// Resolve the active X11 window class when the application runs under X11.
QString activeWindowClassX11()
{
#if defined(Q_OS_LINUX)
    if (QGuiApplication::platformName() != QStringLiteral("xcb")) {
        return QString();
    }

    Display *display = XOpenDisplay(nullptr);
    if (!display) {
        return QString();
    }

    const Atom activeWindowAtom = XInternAtom(display, "_NET_ACTIVE_WINDOW", True);
    const Atom windowType = AnyPropertyType;
    Atom actualType = None;
    int actualFormat = 0;
    unsigned long itemCount = 0;
    unsigned long bytesAfter = 0;
    unsigned char *propertyData = nullptr;
    Window window = 0;

    if (XGetWindowProperty(display,
                           DefaultRootWindow(display),
                           activeWindowAtom,
                           0,
                           (~0L),
                           False,
                           windowType,
                           &actualType,
                           &actualFormat,
                           &itemCount,
                           &bytesAfter,
                           &propertyData) == Success
        && propertyData) {
        window = *reinterpret_cast<Window *>(propertyData);
        XFree(propertyData);
    }

    if (!window) {
        XCloseDisplay(display);
        return QString();
    }

    XClassHint classHint;
    QString className;
    if (XGetClassHint(display, window, &classHint)) {
        className = QString::fromLocal8Bit(classHint.res_class ? classHint.res_class : "");
        if (classHint.res_name) {
            XFree(classHint.res_name);
        }
        if (classHint.res_class) {
            XFree(classHint.res_class);
        }
    }
    XCloseDisplay(display);
    return className;
#else
    return QString();
#endif
}

} // namespace

ClipboardListener::ClipboardListener(QObject *parent)
    : QObject(parent)
    , m_clipboard(QGuiApplication::clipboard())
{
    // Use a short single-shot timer to coalesce repeated clipboard notifications into one capture pass.
    m_debounceTimer.setInterval(80);
    m_debounceTimer.setSingleShot(true);
    connect(&m_debounceTimer, &QTimer::timeout, this, &ClipboardListener::processClipboardChange);
}

void ClipboardListener::start()
{
    // Watch for clipboard changes emitted by the active desktop session.
    connect(m_clipboard, &QClipboard::dataChanged, &m_debounceTimer, qOverload<>(&QTimer::start));
}

void ClipboardListener::suppressNextChange()
{
    // Ignore the next clipboard notification because it originated from a history activation.
    m_suppressNextChange = true;
}

void ClipboardListener::processClipboardChange()
{
    // Drop self-originated clipboard updates so reusing an item does not duplicate it in history.
    if (m_suppressNextChange) {
        m_suppressNextChange = false;
        return;
    }

    const QMimeData *mimeData = m_clipboard->mimeData();
    if (!mimeData) {
        return;
    }

    ClipboardEntry entry;
    entry.createdAt = QDateTime::currentDateTimeUtc();
    entry.lastUsedAt = entry.createdAt;
    entry.sourceApplication = currentSourceApplication();

    // Normalize file-copy payloads before generic text because URL lists often also expose plain text.
    if (mimeData->hasUrls()) {
        entry.type = ClipboardContentType::FileList;
        const QList<QUrl> urls = mimeData->urls();
        for (const QUrl &url : urls) {
            entry.fileUris.append(url.toString());
        }
        entry.previewText = compactPreview(entry.fileUris.join(QStringLiteral(" • ")));
    } else if (mimeData->hasImage()) {
        entry.type = ClipboardContentType::Image;
        QImage image = qvariant_cast<QImage>(mimeData->imageData());
        QBuffer buffer(&entry.imagePng);
        buffer.open(QIODevice::WriteOnly);
        image.save(&buffer, "PNG");
        entry.previewText = QStringLiteral("Image %1 x %2").arg(image.width()).arg(image.height());
    } else if (mimeData->hasHtml()) {
        entry.type = ClipboardContentType::Html;
        entry.htmlText = mimeData->html();
        entry.plainText = mimeData->text();
        entry.previewText = compactPreview(entry.plainText);
    } else if (mimeData->hasFormat(QStringLiteral("text/rtf"))) {
        entry.type = ClipboardContentType::Rtf;
        entry.rtfData = mimeData->data(QStringLiteral("text/rtf"));
        entry.plainText = mimeData->text();
        entry.previewText = compactPreview(entry.plainText);
    } else if (mimeData->hasText()) {
        entry.type = ClipboardContentType::Text;
        entry.plainText = mimeData->text();
        entry.previewText = compactPreview(entry.plainText);
    } else {
        return;
    }

    // Mark sensitive payloads later in the history manager after settings are available.
    entry.contentHash = contentHashForEntry(entry);

    // Ignore empty payloads that do not give the user a useful history entry.
    if (entry.contentHash.isEmpty() || entry.previewText.isEmpty()) {
        return;
    }

    emit entryCaptured(entry);
}

QString ClipboardListener::currentSourceApplication() const
{
    // Return the active X11 window class when available and an empty string on unsupported platforms.
    return activeWindowClassX11();
}

QString ClipboardListener::contentHashForEntry(const ClipboardEntry &entry) const
{
    QCryptographicHash hash(QCryptographicHash::Sha256);

    // Fold the normalized content type into the hash so visually similar values from different payload types stay distinct.
    hash.addData(contentTypeToString(entry.type).toUtf8());
    hash.addData("|");

    // Hash the payload fields that define logical clipboard identity for deduplication.
    hash.addData(entry.plainText.toUtf8());
    hash.addData("|");
    hash.addData(entry.htmlText.toUtf8());
    hash.addData("|");
    hash.addData(entry.rtfData);
    hash.addData("|");
    hash.addData(entry.imagePng);
    hash.addData("|");
    hash.addData(entry.fileUris.join(QStringLiteral("\n")).toUtf8());
    return QString::fromLatin1(hash.result().toHex());
}

QString ClipboardListener::compactPreview(const QString &text) const
{
    // Collapse whitespace and trim long values so the popup remains readable and lightweight.
    QString preview = text.simplified();
    if (preview.size() > 140) {
        preview = preview.left(137) + QStringLiteral("...");
    }
    return preview;
}
