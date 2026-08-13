#include "StashModel.h"

namespace Cherry {

StashModel::StashModel(IGitService *service, QObject *parent)
    : QAbstractListModel(parent)
    , m_service(service)
{
    if (m_service) {
        connect(m_service, &IGitService::stashesUpdated, this, &StashModel::reload);
        reload();
    }
}

int StashModel::rowCount(const QModelIndex &parent) const
{
    if (parent.isValid()) return 0;
    return m_stashes.size();
}

QVariant StashModel::data(const QModelIndex &index, int role) const
{
    if (!index.isValid() || index.row() < 0 || index.row() >= m_stashes.size()) {
        return QVariant();
    }

    const auto &stash = m_stashes[index.row()];

    switch (role) {
    case IdRole:
        return stash.id;
    case MessageRole:
        return stash.message;
    case BranchNameRole:
        return stash.branchName;
    case TimestampRole:
        return stash.timestamp.toString("yyyy-MM-dd hh:mm");
    case FileCountRole:
        return stash.files.size();
    default:
        return QVariant();
    }
}

QHash<int, QByteArray> StashModel::roleNames() const
{
    return {
        {IdRole, "stashId"},
        {MessageRole, "message"},
        {BranchNameRole, "branchName"},
        {TimestampRole, "timestamp"},
        {FileCountRole, "fileCount"}
    };
}

void StashModel::reload()
{
    if (!m_service) {
        beginResetModel();
        m_stashes.clear();
        endResetModel();
        emit countChanged();
        return;
    }

    beginResetModel();
    m_stashes = m_service->getStashes();
    endResetModel();

    emit countChanged();
}

void StashModel::setService(IGitService *service)
{
    if (m_service) {
        disconnect(m_service, nullptr, this, nullptr);
    }
    m_service = service;
    if (m_service) {
        connect(m_service, &IGitService::stashesUpdated, this, &StashModel::reload);
    }
    reload();
}

} // namespace Cherry
