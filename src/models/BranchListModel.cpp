#include "BranchListModel.h"

namespace Cherry {

BranchListModel::BranchListModel(IGitService *service, QObject *parent)
    : QAbstractListModel(parent)
    , m_service(service)
{
    if (m_service) {
        connect(m_service, &IGitService::branchListChanged, this, &BranchListModel::reload);
        reload();
    }
}

int BranchListModel::rowCount(const QModelIndex &parent) const
{
    if (parent.isValid()) return 0;
    return m_filteredBranches.size();
}

QVariant BranchListModel::data(const QModelIndex &index, int role) const
{
    if (!index.isValid() || index.row() < 0 || index.row() >= m_filteredBranches.size()) {
        return QVariant();
    }

    const auto &branch = m_filteredBranches[index.row()];

    switch (role) {
    case NameRole:
        return branch.name;
    case IsCurrentRole:
        return branch.isCurrent;
    case IsDefaultRole:
        return branch.isDefault;
    case IsRemoteRole:
        return branch.isRemote;
    case PrNumberRole:
        return branch.prNumber;
    case PrMergedOrActiveRole:
        return branch.prMergedOrActive;
    case TipCommitShaRole:
        return branch.tipCommitSha;
    default:
        return QVariant();
    }
}

QHash<int, QByteArray> BranchListModel::roleNames() const
{
    return {
        {NameRole, "name"},
        {IsCurrentRole, "isCurrent"},
        {IsDefaultRole, "isDefault"},
        {IsRemoteRole, "isRemote"},
        {PrNumberRole, "prNumber"},
        {PrMergedOrActiveRole, "prMergedOrActive"},
        {TipCommitShaRole, "tipCommitSha"}
    };
}

void BranchListModel::setFilterText(const QString &text)
{
    if (m_filterText == text) return;
    m_filterText = text;
    applyFilter();
    emit filterTextChanged();
}

void BranchListModel::reload()
{
    if (!m_service) {
        m_allBranches.clear();
        applyFilter();
        return;
    }
    m_allBranches = m_service->getBranches();
    applyFilter();
}

void BranchListModel::setService(IGitService *service)
{
    if (m_service) {
        disconnect(m_service, nullptr, this, nullptr);
    }
    m_service = service;
    if (m_service) {
        connect(m_service, &IGitService::branchListChanged, this, &BranchListModel::reload);
    }
    reload();
}

void BranchListModel::applyFilter()
{
    beginResetModel();
    if (m_filterText.trimmed().isEmpty()) {
        m_filteredBranches = m_allBranches;
    } else {
        m_filteredBranches.clear();
        const QString filter = m_filterText.trimmed();
        m_filteredBranches.reserve(m_allBranches.size());
        for (const auto &b : m_allBranches) {
            if (b.name.contains(filter, Qt::CaseInsensitive) || (!b.prNumber.isEmpty() && b.prNumber.contains(filter, Qt::CaseInsensitive))) {
                m_filteredBranches.append(b);
            }
        }
    }
    endResetModel();
    emit countChanged();
}

} // namespace Cherry
