#pragma once

#include <QAbstractListModel>
#include <QVector>

#include "ClipboardEntry.h"

// Adapt the clipboard history records into a QML-friendly list model for the popup UI.
class HistoryListModel : public QAbstractListModel
{
    Q_OBJECT

public:
    // Define the QML roles exposed by each history row.
    enum Roles {
        IdRole = Qt::UserRole + 1,
        PreviewRole,
        TypeRole,
        SourceRole,
        CreatedAtRole,
        LastUsedAtRole,
        PinnedRole,
        FavoriteRole,
        SensitiveRole,
    };

    // Construct the list model used by the popup window.
    explicit HistoryListModel(QObject *parent = nullptr);

    // Return the number of rows currently visible in the popup.
    int rowCount(const QModelIndex &parent = QModelIndex()) const override;

    // Return the data backing one visible row in the popup history list.
    QVariant data(const QModelIndex &index, int role) const override;

    // Publish the role names needed by QML delegates.
    QHash<int, QByteArray> roleNames() const override;

    // Replace the model contents after a database refresh or sync import.
    void setEntries(const QVector<ClipboardEntry> &entries);

private:
    // Keep the visible entries in the current popup order.
    QVector<ClipboardEntry> m_entries;
};
