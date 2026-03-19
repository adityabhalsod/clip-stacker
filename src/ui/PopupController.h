#pragma once

#include <QObject>

class HistoryListModel;
class HistoryManager;
class QDialog;
class QLineEdit;
class QListView;

// Bridge a lightweight popup window to the backend services and position it near the current cursor.
class PopupController : public QObject
{
    Q_OBJECT

public:
    // Construct the popup controller that owns the popup widgets.
    explicit PopupController(QObject *parent = nullptr);

    // Create the popup UI and bind it to the history model and actions.
    bool initialize(HistoryListModel *historyListModel, HistoryManager *historyManager);

    // Toggle the popup visibility from the tray or the global hotkey.
    Q_INVOKABLE void togglePopup();

    // Hide the popup when focus is lost or the user dismisses it explicitly.
    Q_INVOKABLE void hidePopup();

protected:
    // Intercept widget events used for popup dismissal and keyboard navigation.
    bool eventFilter(QObject *watched, QEvent *event) override;

private:
    // Show and place the popup near the current cursor position while keeping it on screen.
    void showPopup();

    // Keep the history manager available for search updates and item activation.
    HistoryManager *m_historyManager = nullptr;

    // Own the fallback popup dialog used on systems without the required Qt Quick runtime modules.
    QDialog *m_dialog = nullptr;

    // Own the popup search field used to filter history entries in real time.
    QLineEdit *m_searchLineEdit = nullptr;

    // Own the list view used to render the clipboard history model.
    QListView *m_listView = nullptr;
};
