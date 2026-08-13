#include "ChangedFilesModel.h"
#include <QFileInfo>

namespace Cherry {

ChangedFilesModel::ChangedFilesModel(IGitService *service, QObject *parent)
    : QAbstractListModel(parent)
    , m_service(service)
{
    if (m_service) {
        connect(m_service, &IGitService::changedFilesUpdated, this, &ChangedFilesModel::reload);
        reload();
    }
}

int ChangedFilesModel::rowCount(const QModelIndex &parent) const
{
    if (parent.isValid()) return 0;
    return m_files.size();
}

QVariant ChangedFilesModel::data(const QModelIndex &index, int role) const
{
    if (!index.isValid() || index.row() < 0 || index.row() >= m_files.size()) {
        return QVariant();
    }

    const auto &file = m_files[index.row()];
    QFileInfo fi(file.filePath);

    switch (role) {
    case IdRole:
        return file.id;
    case FilePathRole:
        return file.filePath;
    case FileNameRole:
        return fi.fileName();
    case FileDirRole:
        return fi.path();
    case StatusRole:
        return static_cast<int>(file.status);
    case StatusTextRole: {
        switch (file.status) {
        case FileChangeType::Modified: return "Modified";
        case FileChangeType::Added: return "Added";
        case FileChangeType::Deleted: return "Deleted";
        case FileChangeType::Renamed: return "Renamed";
        case FileChangeType::Untracked: return "Untracked";
        }
        return "Modified";
    }
    case StatusIconRole: {
        switch (file.status) {
        case FileChangeType::Modified: return "document-edit";
        case FileChangeType::Added: return "list-add";
        case FileChangeType::Deleted: return "list-remove";
        case FileChangeType::Renamed: return "edit-rename";
        case FileChangeType::Untracked: return "document-new";
        }
        return "document-edit";
    }
    case StatusColorRole: {
        switch (file.status) {
        case FileChangeType::Modified: return "#e5a50a"; // Amber/Orange
        case FileChangeType::Added: return "#2ec27e";    // Green
        case FileChangeType::Deleted: return "#e01b24";  // Red
        case FileChangeType::Renamed: return "#3584e4";  // Blue
        case FileChangeType::Untracked: return "#865e3c";
        }
        return "#e5a50a";
    }
    case IsSelectedRole:
        return file.isSelected;
    case AdditionsRole:
        return file.additions;
    case DeletionsRole:
        return file.deletions;
    default:
        return QVariant();
    }
}

bool ChangedFilesModel::setData(const QModelIndex &index, const QVariant &value, int role)
{
    if (!index.isValid() || index.row() < 0 || index.row() >= m_files.size()) {
        return false;
    }

    if (role == IsSelectedRole) {
        bool selected = value.toBool();
        if (m_files[index.row()].isSelected != selected) {
            m_files[index.row()].isSelected = selected;
            if (m_service) {
                m_service->setFileSelected(m_files[index.row()].filePath, selected);
            }
            emit dataChanged(index, index, {IsSelectedRole});
            emit selectionChanged();
            return true;
        }
    }
    return false;
}

QHash<int, QByteArray> ChangedFilesModel::roleNames() const
{
    return {
        {IdRole, "fileId"},
        {FilePathRole, "filePath"},
        {FileNameRole, "fileName"},
        {FileDirRole, "fileDir"},
        {StatusRole, "status"},
        {StatusTextRole, "statusText"},
        {StatusIconRole, "statusIcon"},
        {StatusColorRole, "statusColor"},
        {IsSelectedRole, "isSelected"},
        {AdditionsRole, "additions"},
        {DeletionsRole, "deletions"}
    };
}

int ChangedFilesModel::selectedCount() const
{
    int count = 0;
    for (const auto &f : m_files) {
        if (f.isSelected) count++;
    }
    return count;
}

bool ChangedFilesModel::allSelected() const
{
    if (m_files.isEmpty()) return false;
    for (const auto &f : m_files) {
        if (!f.isSelected) return false;
    }
    return true;
}

bool ChangedFilesModel::hasPartialSelection() const
{
    int count = selectedCount();
    return count > 0 && count < m_files.size();
}

void ChangedFilesModel::toggleSelected(int index)
{
    if (index < 0 || index >= m_files.size()) return;
    setData(this->index(index, 0), !m_files[index].isSelected, IsSelectedRole);
}

void ChangedFilesModel::selectAll(bool select)
{
    if (!m_service) return;
    m_service->setAllFilesSelected(select);
    for (int i = 0; i < m_files.size(); ++i) {
        m_files[i].isSelected = select;
    }
    emit dataChanged(index(0, 0), index(m_files.size() - 1, 0), {IsSelectedRole});
    emit selectionChanged();
}

QString ChangedFilesModel::getFilePath(int index) const
{
    if (index < 0 || index >= m_files.size()) return QString();
    return m_files[index].filePath;
}

void ChangedFilesModel::reload()
{
    if (!m_service) return;

    beginResetModel();
    m_files = m_service->getChangedFiles();
    endResetModel();

    emit countChanged();
    emit selectionChanged();
}

} // namespace Cherry
