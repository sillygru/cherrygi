#include "CommitHistoryModel.h"
#include "AvatarResolver.h"
#include <QColor>

namespace Cherry {

namespace {
QColor computeAuthorColor(const QString &name)
{
    if (name.isEmpty()) return QColor("#7f8c8d");
    quint32 hash = 0;
    for (int i = 0; i < name.size(); ++i) {
        hash = static_cast<quint32>(name[i].unicode()) + ((hash << 5) - hash);
    }
    int hue = static_cast<int>(hash % 360);
    return QColor::fromHsl(hue, 140, 115);
}
}

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
        return !commit.authorAvatarUrl.isEmpty() ? commit.authorAvatarUrl : AvatarResolver::resolve(commit.authorName, commit.authorEmail);
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
    case AuthorColorRole:
        return computeAuthorColor(commit.authorName);
    case AuthorInitialRole:
        return commit.authorName.isEmpty() ? QString("G") : QString(commit.authorName.at(0).toUpper());
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
        {IsLocalRole, "isLocal"},
        {AuthorColorRole, "authorColor"},
        {AuthorInitialRole, "authorInitial"}
    };
}

void CommitHistoryModel::setAheadCount(int count)
{
    if (m_aheadCount == count) return;
    m_aheadCount = count;
    if (!m_filteredCommits.isEmpty()) {
        emit dataChanged(index(0, 0), index(m_filteredCommits.size() - 1, 0), {IsLocalRole});
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
    if (!m_service) {
        if (!m_allCommits.isEmpty()) {
            m_allCommits.clear();
            applyFilter();
        }
        return;
    }
    auto newCommits = m_service->getCommitHistory();
    if (newCommits.size() == m_allCommits.size()) {
        bool identical = true;
        for (int i = 0; i < newCommits.size(); ++i) {
            if (newCommits[i].sha != m_allCommits[i].sha ||
                newCommits[i].summary != m_allCommits[i].summary ||
                newCommits[i].relativeTime != m_allCommits[i].relativeTime) {
                identical = false;
                break;
            }
        }
        if (identical) {
            return;
        }
    }
    m_allCommits = std::move(newCommits);
    applyFilter();
}

void CommitHistoryModel::setService(IGitService *service)
{
    if (m_service) {
        disconnect(m_service, nullptr, this, nullptr);
    }
    m_service = service;
    if (m_service) {
        connect(m_service, &IGitService::commitHistoryUpdated, this, &CommitHistoryModel::reload);
    }
    reload();
}

void CommitHistoryModel::applyFilter()
{
    beginResetModel();
    if (m_filterText.trimmed().isEmpty()) {
        m_filteredCommits = m_allCommits;
    } else {
        m_filteredCommits.clear();
        const QString filter = m_filterText.trimmed();
        m_filteredCommits.reserve(m_allCommits.size() / 2);
        for (const auto &c : m_allCommits) {
            if (c.summary.contains(filter, Qt::CaseInsensitive) ||
                c.description.contains(filter, Qt::CaseInsensitive) ||
                c.authorName.contains(filter, Qt::CaseInsensitive) ||
                c.sha.startsWith(filter, Qt::CaseInsensitive) ||
                c.shortSha.startsWith(filter, Qt::CaseInsensitive)) {
                m_filteredCommits.append(c);
            }
        }
    }
    endResetModel();
    emit countChanged();
}

} // namespace Cherry
