#include "HistoryListModel.h"

HistoryListModel::HistoryListModel(QObject *parent)
    : QAbstractListModel(parent)
{
}

int HistoryListModel::rowCount(const QModelIndex &parent) const
{
    // Suppress child rows because the popup exposes a flat history list.
    if (parent.isValid()) {
        return 0;
    }
    return m_entries.size();
}

QVariant HistoryListModel::data(const QModelIndex &index, int role) const
{
    // Ignore invalid indexes so QML delegates do not read past the current list contents.
    if (!index.isValid() || index.row() < 0 || index.row() >= m_entries.size()) {
        return QVariant();
    }

    const ClipboardEntry &entry = m_entries.at(index.row());

    // Provide a default display string so Qt Widgets views can render the same model without a custom delegate.
    if (role == Qt::DisplayRole) {
        return QStringLiteral("[%1] %2")
            .arg(contentTypeToString(entry.type), entry.previewText);
    }

    // Return the role-specific value consumed by the QML popup delegate.
    switch (role) {
    case IdRole:
        return entry.id;
    case PreviewRole:
        return entry.previewText;
    case TypeRole:
        return contentTypeToString(entry.type);
    case SourceRole:
        return entry.sourceApplication;
    case CreatedAtRole:
        return entry.createdAt.toLocalTime().toString(QStringLiteral("MMM d, hh:mm"));
    case LastUsedAtRole:
        return entry.lastUsedAt.toLocalTime().toString(QStringLiteral("MMM d, hh:mm"));
    case PinnedRole:
        return entry.pinned;
    case FavoriteRole:
        return entry.favorite;
    case SensitiveRole:
        return entry.sensitive;
    default:
        return QVariant();
    }
}

QHash<int, QByteArray> HistoryListModel::roleNames() const
{
    // Publish stable role names for the QML popup delegates and keyboard handlers.
    return {
        {IdRole, "entryId"},
        {PreviewRole, "previewText"},
        {TypeRole, "contentType"},
        {SourceRole, "sourceApplication"},
        {CreatedAtRole, "createdAt"},
        {LastUsedAtRole, "lastUsedAt"},
        {PinnedRole, "pinned"},
        {FavoriteRole, "favorite"},
        {SensitiveRole, "sensitive"},
    };
}

void HistoryListModel::setEntries(const QVector<ClipboardEntry> &entries)
{
    // Reset the model in one operation because the history list is refreshed from the database as a snapshot.
    beginResetModel();
    m_entries = entries;
    endResetModel();
}
