#include "HistoryManager.h"

#include <memory>

#include <QClipboard>
#include <QGuiApplication>
#include <QImage>
#include <QMimeData>
#include <QTimer>
#include <QUrl>

#include "ClipboardDatabase.h"
#include "ClipboardListener.h"
#include "InputSimulator.h"
#include "SettingsManager.h"

HistoryManager::HistoryManager(SettingsManager *settingsManager,
                               ClipboardDatabase *database,
                               ClipboardListener *clipboardListener,
                               InputSimulator *inputSimulator,
                               QObject *parent)
    : QObject(parent)
    , m_settingsManager(settingsManager)
    , m_database(database)
    , m_clipboardListener(clipboardListener)
    , m_inputSimulator(inputSimulator)
    , m_model(this)
{
}

bool HistoryManager::initialize()
{
    // Populate the initial popup model from the existing SQLite history.
    refreshModel();
    return true;
}

HistoryListModel *HistoryManager::model()
{
    // Return the model used directly by QML.
    return &m_model;
}

void HistoryManager::refreshModel()
{
    // Reload the visible history using the current search filter and configured retention ceiling.
    const int limit = m_settingsManager->snapshot().historyLimit;
    m_model.setEntries(m_database->fetchEntries(m_searchQuery, limit));
}

void HistoryManager::setSearchQuery(const QString &searchQuery)
{
    // Refresh the popup immediately when the user changes the search text.
    m_searchQuery = searchQuery;
    refreshModel();
}

void HistoryManager::capturePasteTarget()
{
    // Forward to InputSimulator to remember the X11 window that should receive the paste event.
    if (m_inputSimulator) {
        m_inputSimulator->capturePasteTarget();
    }
}

void HistoryManager::activateEntry(qint64 id)
{
    const ClipboardEntry entry = m_database->fetchEntryById(id);
    if (entry.id < 0) {
        return;
    }

    std::unique_ptr<QMimeData> mimeData(mimeDataForEntry(entry));
    if (!mimeData) {
        return;
    }

    // Prevent the restored clipboard item from being re-captured as a new history record.
    m_clipboardListener->suppressNextChange();

    // Restore the selected history item to the live system clipboard.
    QGuiApplication::clipboard()->setMimeData(mimeData.release());

    // Record the activation so ordering and usage stats stay up to date.
    m_database->markUsed(id);
    refreshModel();

    // Restore focus to the target application that was active before the popup opened.
    m_inputSimulator->restorePasteTarget();

    // Wait for the window manager to finish the focus switch before injecting the paste shortcut.
    QTimer::singleShot(300, this, [this]() {
        // Attempt a native paste and report a graceful fallback notice if unavailable.
        if (!m_inputSimulator->simulatePaste()) {
            emit activationNotice(QStringLiteral("Clipboard updated. Native paste automation is unavailable in this session, so paste manually with Ctrl+V."));
        }
    });
}

void HistoryManager::togglePinned(qint64 id)
{
    const ClipboardEntry entry = m_database->fetchEntryById(id);
    if (entry.id < 0) {
        return;
    }

    // Flip the pinned state and refresh the list ordering.
    m_database->setPinned(id, !entry.pinned);
    refreshModel();
}

void HistoryManager::toggleFavorite(qint64 id)
{
    const ClipboardEntry entry = m_database->fetchEntryById(id);
    if (entry.id < 0) {
        return;
    }

    // Flip the favorite state and refresh the list ordering.
    m_database->setFavorite(id, !entry.favorite);
    refreshModel();
}

void HistoryManager::clearHistory()
{
    // Delete all non-pinned entries and refresh the popup.
    m_database->clearUnpinnedEntries();
    refreshModel();
}

QVector<ClipboardEntry> HistoryManager::syncEntries(int limit) const
{
    // Export recent non-sensitive entries for the shared-folder sync engine.
    return m_database->fetchSyncEntries(limit);
}

void HistoryManager::importEntries(const QVector<ClipboardEntry> &entries)
{
    // Merge synchronized entries from other devices into the local store using normal deduplication rules.
    for (const ClipboardEntry &entry : entries) {
        m_database->upsertEntry(entry);
    }
    refreshModel();
}

void HistoryManager::handleCapturedEntry(const ClipboardEntry &entry)
{
    ClipboardEntry mutableEntry = entry;
    const SettingsManager::SettingsSnapshot settings = m_settingsManager->snapshot();

    // Infer whether the captured payload looks sensitive before deciding to persist it.
    mutableEntry.sensitive = m_settingsManager->shouldTreatAsSensitive(mutableEntry.sourceApplication, mutableEntry.plainText);

    // Skip sensitive entries entirely unless the user has explicitly opted into storing them.
    if (mutableEntry.sensitive && !settings.storeSensitiveData) {
        return;
    }

    // Persist the entry, deduplicate by content hash, and then prune the database to the configured size limit.
    if (m_database->upsertEntry(mutableEntry)) {
        m_database->pruneToLimit(settings.historyLimit);
        refreshModel();
    }
}

QMimeData *HistoryManager::mimeDataForEntry(const ClipboardEntry &entry) const
{
    auto *mimeData = new QMimeData();

    // Restore the payload type-specific clipboard formats needed by target applications.
    switch (entry.type) {
    case ClipboardContentType::Text:
        mimeData->setText(entry.plainText);
        break;
    case ClipboardContentType::Html:
        mimeData->setHtml(entry.htmlText);
        mimeData->setText(entry.plainText);
        break;
    case ClipboardContentType::Image:
        mimeData->setImageData(QImage::fromData(entry.imagePng, "PNG"));
        break;
    case ClipboardContentType::FileList: {
        QList<QUrl> urls;
        for (const QString &uri : entry.fileUris) {
            urls.append(QUrl(uri));
        }
        mimeData->setUrls(urls);
        mimeData->setText(entry.fileUris.join(QStringLiteral("\n")));
        break;
    }
    case ClipboardContentType::Rtf:
        mimeData->setData(QStringLiteral("text/rtf"), entry.rtfData);
        mimeData->setText(entry.plainText);
        break;
    case ClipboardContentType::Unknown:
    default:
        delete mimeData;
        return nullptr;
    }

    return mimeData;
}
