#include "RepositoryListModel.h"

namespace Cherry {

RepositoryListModel::RepositoryListModel(IGitService *service, QObject *parent)
    : QAbstractListModel(parent)
    , m_service(service)
{
    if (m_service) {
        connect(m_service, &IGitService::repositoryChanged, this, &RepositoryListModel::reload);
        reload();
    }
}

int RepositoryListModel::rowCount(const QModelIndex &parent) const
{
    if (parent.isValid()) return 0;
    return m_repos.size();
}

QVariant RepositoryListModel::data(const QModelIndex &index, int role) const
{
    if (!index.isValid() || index.row() < 0 || index.row() >= m_repos.size()) {
        return QVariant();
    }

    const auto &repo = m_repos[index.row()];

    switch (role) {
    case IdRole:
        return repo.id;
    case NameRole:
        return repo.name;
    case PathRole:
        return repo.path;
    case IsCurrentRole:
        return (repo.id == m_currentRepoId);
    case CurrentBranchRole:
        return repo.currentBranch;
    case ChangedFilesCountRole:
        return repo.changedFilesCount;
    case AheadCountRole:
        return repo.aheadCount;
    case BehindCountRole:
        return repo.behindCount;
    case LastFetchedTimeRole:
        return repo.lastFetchedTime;
    case IsMissingRole:
        return repo.isMissing;
    case RemoteUrlRole:
        return repo.remoteUrl;
    default:
        return QVariant();
    }
}

QHash<int, QByteArray> RepositoryListModel::roleNames() const
{
    return {
        {IdRole, "repoId"},
        {NameRole, "name"},
        {PathRole, "path"},
        {IsCurrentRole, "isCurrent"},
        {CurrentBranchRole, "currentBranch"},
        {ChangedFilesCountRole, "changedFilesCount"},
        {AheadCountRole, "aheadCount"},
        {BehindCountRole, "behindCount"},
        {LastFetchedTimeRole, "lastFetchedTime"},
        {IsMissingRole, "isMissing"},
        {RemoteUrlRole, "remoteUrl"}
    };
}

void RepositoryListModel::reload()
{
    if (!m_service) {
        beginResetModel();
        m_repos.clear();
        m_currentRepoId.clear();
        endResetModel();
        emit countChanged();
        return;
    }

    beginResetModel();
    m_repos = m_service->getRepositories();
    auto cur = m_service->getCurrentRepository();
    m_currentRepoId = cur ? cur->id : QString();
    endResetModel();

    emit countChanged();
}

QString RepositoryListModel::findFirstMatchingRepoId(const QString &filter) const
{
    if (m_repos.isEmpty()) {
        return QString();
    }

    const QString trimmed = filter.trimmed();
    if (trimmed.isEmpty()) {
        return m_repos.first().id;
    }

    // 1. Exact name match (case-insensitive)
    for (const auto &repo : m_repos) {
        if (repo.name.compare(trimmed, Qt::CaseInsensitive) == 0) {
            return repo.id;
        }
    }

    // 2. Starts with name match (case-insensitive)
    for (const auto &repo : m_repos) {
        if (repo.name.startsWith(trimmed, Qt::CaseInsensitive)) {
            return repo.id;
        }
    }

    // 3. Name contains match (case-insensitive)
    for (const auto &repo : m_repos) {
        if (repo.name.contains(trimmed, Qt::CaseInsensitive)) {
            return repo.id;
        }
    }

    // 4. Path contains match (case-insensitive)
    for (const auto &repo : m_repos) {
        if (repo.path.contains(trimmed, Qt::CaseInsensitive)) {
            return repo.id;
        }
    }

    return QString();
}

void RepositoryListModel::setService(IGitService *service)
{
    if (m_service) {
        disconnect(m_service, nullptr, this, nullptr);
    }
    m_service = service;
    if (m_service) {
        connect(m_service, &IGitService::repositoryChanged, this, &RepositoryListModel::reload);
    }
    reload();
}

} // namespace Cherry
