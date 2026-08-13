#include "CommitHistoryModel.h"

namespace Cherry {

CommitHistoryModel::CommitHistoryModel(IGitService *service, QObject *parent)
    : QAbstractListModel(parent)
    , m_service(service)
{
    if (m_service) {
        connect(m_service, &IGitService::commitHistoryUpdated, this, &CommitHistoryModel::reload);
        reload();
    }
}

int CommitHistoryModel::rowCount(const QModelIndex &parent) const
{
    if (parent.isValid()) return 0;
    return m_filteredCommits.size();
}

QVariant CommitHistoryModel::data(const QModelIndex &index, int role) const
{
    if (!index.isValid() || index.row() < 0 || index.row() >= m_filteredCommits.size()) {
        return QVariant();
    }

    const auto &commit = m_filteredCommits[index.row()];

    switch (role) {
    case ShaRole:
        return commit.sha;
    case ShortShaRole:
        return commit.shortSha;
    case SummaryRole:
        return commit.summary;
    case DescriptionRole:
        return commit.description;
    case AuthorNameRole:
        return commit.authorName;
    case AuthorEmailRole:
        return commit.authorEmail;
    case AuthorAvatarUrlRole:
        return commit.authorAvatarUrl;
    case RelativeTimeRole:
        return commit.relativeTime;
    case TimestampRole:
        return commit.timestamp.toString("yyyy-MM-dd hh:mm");
    case CoAuthorsRole:
        return commit.coAuthors;
    case CoAuthorsTextRole:
        return commit.coAuthors.join(", ");
    case ChangedFilesCountRole:
        return commit.changedFiles.size();
    case IsLocalRole:
        return index.row() < m_aheadCount;
    default:
        return QVariant();
    }
}

QHash<int, QByteArray> CommitHistoryModel::roleNames() const
{
    return {
        {ShaRole, "sha"},
        {ShortShaRole, "shortSha"},
        {SummaryRole, "summary"},
        {DescriptionRole, "description"},
        {AuthorNameRole, "authorName"},
        {AuthorEmailRole, "authorEmail"},
        {AuthorAvatarUrlRole, "authorAvatarUrl"},
        {RelativeTimeRole, "relativeTime"},
        {TimestampRole, "timestamp"},
        {CoAuthorsRole, "coAuthors"},
        {CoAuthorsTextRole, "coAuthorsText"},
        {ChangedFilesCountRole, "changedFilesCount"},
        {IsLocalRole, "isLocal"}
    };
}

void CommitHistoryModel::setAheadCount(int count)
{
    if (m_aheadCount == count) return;
    m_aheadCount = count;
    if (!m_filteredCommits.isEmpty()) {
        emit dataChanged(index(0, 0), index(qMin(m_aheadCount, (int)m_filteredCommits.size() - 1), 0), {IsLocalRole});
    }
    emit aheadCountChanged();
}

void CommitHistoryModel::setFilterText(const QString &text)
{
    if (m_filterText == text) return;
    m_filterText = text;
    applyFilter();
    emit filterTextChanged();
}

QString CommitHistoryModel::getSha(int index) const
{
    if (index < 0 || index >= m_filteredCommits.size()) return QString();
    return m_filteredCommits[index].sha;
}

void CommitHistoryModel::reload()
{
    if (!m_service) return;
    m_allCommits = m_service->getCommitHistory();
    applyFilter();
}

void CommitHistoryModel::applyFilter()
{
    beginResetModel();
    if (m_filterText.trimmed().isEmpty()) {
        m_filteredCommits = m_allCommits;
    } else {
        m_filteredCommits.clear();
        const QString lower = m_filterText.trimmed().toLower();
        for (const auto &c : m_allCommits) {
            if (c.summary.toLower().contains(lower) ||
                c.description.toLower().contains(lower) ||
                c.authorName.toLower().contains(lower) ||
                c.sha.toLower().startsWith(lower) ||
                c.shortSha.toLower().startsWith(lower)) {
                m_filteredCommits.append(c);
            }
        }
    }
    endResetModel();
    emit countChanged();
}

} // namespace Cherry
