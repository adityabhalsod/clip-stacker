#include "PopupController.h"

#include <QDialog>
#include <QEvent>
#include <QKeyEvent>
#include <QLineEdit>
#include <QListView>
#include <QModelIndex>
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
    // Behave like a natural top-level OS tooltip or interactive list widget without system border decorations.
    m_dialog->setWindowFlags(Qt::Popup | Qt::FramelessWindowHint);
    // Declare that this unmanaged hierarchy can process keyboard events eagerly.
    m_dialog->setFocusPolicy(Qt::StrongFocus);
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
    // Request strong focus policy so the list view can intercept keyboard events.
    m_listView->setFocusPolicy(Qt::StrongFocus);
    m_listView->installEventFilter(this);
    layout->addWidget(m_listView);

    // Refresh the history list immediately whenever the search text changes.
    connect(m_searchLineEdit, &QLineEdit::textChanged, historyManager, &HistoryManager::setSearchQuery);

    // Provide single-click activation, skipping the traditional double-click requirement.
    connect(m_listView, &QListView::clicked, this, [this](const QModelIndex &index) {
        if (!index.isValid() || !m_historyManager) {
            return;
        }

        // Keep a copy of the index identifier so we can safely activate it outside the lambda scope.
        const qint64 entryId = index.data(HistoryListModel::IdRole).toLongLong();

        // Ensure the screen structure repaints immediately so input goes back to the desktop shell.
        hidePopup();

        // Introduce a short timeout to let the desktop window manager return focus to the previous active window.
        QTimer::singleShot(150, this, [this, entryId]() {
            if (m_historyManager) {
                // Command the history layer to apply the clipboard selection natively.
                m_historyManager->activateEntry(entryId);
            }
        });
    });

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
    showPopup();
}

void PopupController::hidePopup()
{
    // Hide the transient popup window without destroying the widget state.
    if (m_dialog) {
        m_dialog->hide();
    }
}

bool PopupController::eventFilter(QObject *watched, QEvent *event)
{
    // Hide the popup when the top-level dialog loses activation to match transient panel behavior.
    if (watched == m_dialog && event->type() == QEvent::WindowDeactivate) {
        hidePopup();
        return false;
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

    // Handle popup dismissal and activation from the history list itself.
    if (watched == m_listView && event->type() == QEvent::KeyPress) {
        auto *keyEvent = static_cast<QKeyEvent *>(event);
        if (keyEvent->key() == Qt::Key_Escape) {
            hidePopup();
            return true;
        }

        // Bridge Return/Enter keystrokes into the new single-click signal handler.
        if ((keyEvent->key() == Qt::Key_Return || keyEvent->key() == Qt::Key_Enter) && m_listView->currentIndex().isValid()) {
            emit m_listView->clicked(m_listView->currentIndex());
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
    
    // Spawn exactly near the cursor with a slight translation so the cursor itself remains perfectly visible.
    const int x = qBound(availableGeometry.left() + 12,
                         cursorPosition.x() + 8,
                         availableGeometry.right() - width - 12);
    const int y = qBound(availableGeometry.top() + 12,
                         cursorPosition.y() + 8,
                         availableGeometry.bottom() - height - 12);

    // Reset the popup filter so the latest history entries are visible every time the popup opens.
    if (m_historyManager) {
        // Clear existing filter buffers before painting the full collection model again.
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
    if (m_searchLineEdit) {
        m_searchLineEdit->setFocus();
    }
}
