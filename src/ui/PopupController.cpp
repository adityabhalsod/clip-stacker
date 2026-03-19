#include "PopupController.h"

#include <QDialog>
#include <QEvent>
#include <QKeyEvent>
#include <QLineEdit>
#include <QListView>
#include <QMouseEvent>
#include <QTimer>
#include <QVBoxLayout>
#include <QCursor>
#include <QGuiApplication>
#include <QScreen>

#include "core/HistoryListModel.h"
#include "core/HistoryManager.h"

PopupController::PopupController(QObject *parent)
    : QObject(parent)
{
}

bool PopupController::initialize(HistoryListModel *historyListModel, HistoryManager *historyManager)
{
    // Keep the history manager available for popup search updates and item activation.
    m_historyManager = historyManager;

    // Create the lightweight fallback popup dialog using Qt Widgets so startup does not depend on missing QML runtime modules.
    m_dialog = new QDialog();
    m_dialog->setWindowTitle(QStringLiteral("Clipboard History"));
    m_dialog->setModal(false);
    m_dialog->setWindowFlags(Qt::Tool | Qt::FramelessWindowHint | Qt::WindowStaysOnTopHint);
    m_dialog->resize(460, 520);
    m_dialog->installEventFilter(this);

    // Lay out the popup vertically with a search field on top and a history list below.
    auto *layout = new QVBoxLayout(m_dialog);
    layout->setContentsMargins(12, 12, 12, 12);
    layout->setSpacing(8);

    // Create the live search field used to refresh the history model as the user types.
    m_searchLineEdit = new QLineEdit(m_dialog);
    m_searchLineEdit->setPlaceholderText(QStringLiteral("Search clipboard history"));
    m_searchLineEdit->installEventFilter(this);
    layout->addWidget(m_searchLineEdit);

    // Create the history list view and bind it directly to the shared history model.
    m_listView = new QListView(m_dialog);
    m_listView->setModel(historyListModel);
    m_listView->setSelectionMode(QAbstractItemView::SingleSelection);
    m_listView->setEditTriggers(QAbstractItemView::NoEditTriggers);
    m_listView->setUniformItemSizes(true);
    m_listView->installEventFilter(this);
    // Also watch the viewport so mouse presses bypass the window-manager activation delay.
    m_listView->viewport()->installEventFilter(this);
    layout->addWidget(m_listView);

    // Refresh the history list immediately whenever the search text changes.
    connect(m_searchLineEdit, &QLineEdit::textChanged, historyManager, &HistoryManager::setSearchQuery);

    // Hide the popup automatically when the dialog loses activation.
    connect(m_dialog, &QDialog::finished, this, [this]() {
        if (m_historyManager) {
            m_historyManager->setSearchQuery(QString());
        }
    });
    return true;
}

void PopupController::togglePopup()
{
    // Toggle visibility while keeping the positioning logic centralized.
    if (m_dialog && m_dialog->isVisible()) {
        hidePopup();
        return;
    }
    // Suppress reopening when the popup was hidden within the last 500 ms to avoid the
    // WindowDeactivate / hotkey race where the WM hides the popup an instant before the
    // hotkey event fires togglePopup, causing it to immediately reopen.
    if (m_hideTimer.isValid() && m_hideTimer.elapsed() < 500) {
        return;
    }
    showPopup();
}

void PopupController::hidePopup()
{
    // Hide the transient popup window without destroying the widget state.
    if (m_dialog) {
        m_dialog->hide();
    }
    // Record the hide timestamp so togglePopup can suppress accidental reopens from the WM race.
    m_hideTimer.start();
    // Reset the search filter so the full history is visible the next time the popup opens.
    if (m_historyManager) {
        m_historyManager->setSearchQuery(QString());
    }
    // Clear the search field so the next popup show starts fresh.
    if (m_searchLineEdit) {
        m_searchLineEdit->clear();
    }
}

bool PopupController::eventFilter(QObject *watched, QEvent *event)
{
    // Hide the popup when the top-level dialog loses activation to match transient panel behavior.
    if (watched == m_dialog && event->type() == QEvent::WindowDeactivate) {
        hidePopup();
        return false;
    }

    // Handle single-click activation directly from the list viewport to bypass window-manager focus delays.
    if (m_listView && watched == m_listView->viewport() && event->type() == QEvent::MouseButtonPress) {
        auto *mouseEvent = static_cast<QMouseEvent *>(event);
        if (mouseEvent->button() == Qt::LeftButton) {
            // Resolve which item the user clicked using viewport-local coordinates.
            const QModelIndex index = m_listView->indexAt(mouseEvent->position().toPoint());
            if (index.isValid() && m_historyManager) {
                const qint64 entryId = index.data(HistoryListModel::IdRole).toLongLong();
                // Close the popup and paste after a short delay so the window manager can restore focus.
                hidePopup();
                QTimer::singleShot(200, this, [this, entryId]() {
                    if (m_historyManager) {
                        m_historyManager->activateEntry(entryId);
                    }
                });
                return true;
            }
        }
    }

    // Handle search-field keyboard shortcuts used for fast navigation into the list.
    if (watched == m_searchLineEdit && event->type() == QEvent::KeyPress) {
        auto *keyEvent = static_cast<QKeyEvent *>(event);
        if (keyEvent->key() == Qt::Key_Down && m_listView) {
            m_listView->setFocus();
            if (m_listView->model() && m_listView->model()->rowCount() > 0) {
                m_listView->setCurrentIndex(m_listView->model()->index(0, 0));
            }
            return true;
        }
        if (keyEvent->key() == Qt::Key_Escape) {
            hidePopup();
            return true;
        }
    }

    // Handle popup dismissal and keyboard activation from the history list itself.
    if (watched == m_listView && event->type() == QEvent::KeyPress) {
        auto *keyEvent = static_cast<QKeyEvent *>(event);
        // Close the popup when the user presses Escape from the list.
        if (keyEvent->key() == Qt::Key_Escape) {
            hidePopup();
            return true;
        }
        // Activate the highlighted entry on Enter/Return so the keyboard flow matches single-click.
        if ((keyEvent->key() == Qt::Key_Return || keyEvent->key() == Qt::Key_Enter) && m_listView->currentIndex().isValid()) {
            const qint64 entryId = m_listView->currentIndex().data(HistoryListModel::IdRole).toLongLong();
            // Close popup and paste after the same delay used by single-click activation.
            hidePopup();
            QTimer::singleShot(200, this, [this, entryId]() {
                if (m_historyManager) {
                    m_historyManager->activateEntry(entryId);
                }
            });
            return true;
        }
    }

    // Defer all other events to the base QObject implementation.
    return QObject::eventFilter(watched, event);
}

void PopupController::showPopup()
{
    if (!m_dialog) {
        return;
    }

    const QPoint cursorPosition = QCursor::pos();
    QScreen *screen = QGuiApplication::screenAt(cursorPosition);
    if (!screen) {
        screen = QGuiApplication::primaryScreen();
    }
    const QRect availableGeometry = screen ? screen->availableGeometry() : QRect(0, 0, 1280, 720);

    // Keep the popup close to the cursor while clamping it fully inside the current screen.
    const int width = m_dialog->width() > 0 ? m_dialog->width() : 460;
    const int height = m_dialog->height() > 0 ? m_dialog->height() : 520;
    // Center the popup horizontally around the cursor and clamp to visible screen edges.
    const int x = qBound(availableGeometry.left() + 12,
                         cursorPosition.x() - (width / 2),
                         availableGeometry.right() - width - 12);
    // Prefer showing the popup below the cursor like a context menu; flip above if not enough room.
    int y;
    if (cursorPosition.y() + 16 + height <= availableGeometry.bottom() - 12) {
        // Enough room below the cursor.
        y = cursorPosition.y() + 16;
    } else {
        // Fall back to above the cursor when the lower edge would be clipped.
        y = qMax(availableGeometry.top() + 12, cursorPosition.y() - height - 16);
    }

    // Remember the currently focused application before the popup takes focus away.
    if (m_historyManager) {
        m_historyManager->capturePasteTarget();
        // Reset the popup filter so the latest history entries are visible every time the popup opens.
        m_historyManager->setSearchQuery(QString());
    }
    if (m_searchLineEdit) {
        m_searchLineEdit->clear();
    }

    // Position, show, and focus the popup near the cursor.
    m_dialog->move(x, y);
    m_dialog->show();
    m_dialog->raise();
    m_dialog->activateWindow();
    // Pre-select the first history item so Enter can paste immediately without arrow-key navigation.
    if (m_listView && m_listView->model() && m_listView->model()->rowCount() > 0) {
        m_listView->setCurrentIndex(m_listView->model()->index(0, 0));
        m_listView->setFocus();
    } else if (m_searchLineEdit) {
        m_searchLineEdit->setFocus();
    }
}
