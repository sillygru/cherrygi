#include "DiffModel.h"

namespace Cherry {

DiffModel::DiffModel(IGitService *service, QObject *parent)
    : QAbstractListModel(parent)
    , m_service(service)
{
}

int DiffModel::rowCount(const QModelIndex &parent) const
{
    if (parent.isValid()) return 0;
    return m_lines.size();
}

QVariant DiffModel::data(const QModelIndex &index, int role) const
{
    if (!index.isValid() || index.row() < 0 || index.row() >= m_lines.size()) {
        return QVariant();
    }

    const auto &line = m_lines[index.row()];

    switch (role) {
    case OldLineNumRole:
        return line.oldLineNumber;
    case NewLineNumRole:
        return line.newLineNumber;
    case OldLineNumStrRole:
        return (line.oldLineNumber > 0) ? QString::number(line.oldLineNumber) : QString();
    case NewLineNumStrRole:
        return (line.newLineNumber > 0) ? QString::number(line.newLineNumber) : QString();
    case LineTypeRole:
        return static_cast<int>(line.type);
    case LineTypeStrRole: {
        switch (line.type) {
        case DiffLineType::Context: return "context";
        case DiffLineType::Addition: return "addition";
        case DiffLineType::Deletion: return "deletion";
        case DiffLineType::HunkHeader: return "hunkHeader";
        }
        return "context";
    }
    case MarkerRole: {
        switch (line.type) {
        case DiffLineType::Context: return " ";
        case DiffLineType::Addition: return "+";
        case DiffLineType::Deletion: return "-";
        case DiffLineType::HunkHeader: return "@@";
        }
        return " ";
    }
    case ContentRole:
        return line.content;
    default:
        return QVariant();
    }
}

QHash<int, QByteArray> DiffModel::roleNames() const
{
    return {
        {OldLineNumRole, "oldLineNum"},
        {NewLineNumRole, "newLineNum"},
        {OldLineNumStrRole, "oldLineNumStr"},
        {NewLineNumStrRole, "newLineNumStr"},
        {LineTypeRole, "lineType"},
        {LineTypeStrRole, "lineTypeStr"},
        {MarkerRole, "marker"},
        {ContentRole, "content"}
    };
}

void DiffModel::setFilePath(const QString &path)
{
    if (m_filePath == path) return;
    m_filePath = path;
    emit filePathChanged();
    loadDiffForFile(m_filePath);
}

void DiffModel::loadDiffForFile(const QString &filePath)
{
    m_filePath = filePath;
    m_commitSha.clear();
    emit filePathChanged();

    if (!m_service || filePath.isEmpty()) {
        clear();
        return;
    }

    auto lines = m_service->getDiffForFile(filePath);
    setDiffLines(lines);
}

void DiffModel::loadDiffForCommit(const QString &commitSha, const QString &filePath)
{
    m_filePath = filePath;
    m_commitSha = commitSha;
    emit filePathChanged();

    if (!m_service || filePath.isEmpty() || commitSha.isEmpty()) {
        clear();
        return;
    }

    auto lines = m_service->getDiffForCommitFile(commitSha, filePath);
    setDiffLines(lines);
}

void DiffModel::loadDiffForStash(const QString &stashId, const QString &filePath)
{
    m_filePath = filePath;
    m_commitSha.clear();
    emit filePathChanged();

    if (!m_service || filePath.isEmpty()) {
        clear();
        return;
    }

    auto lines = m_service->getDiffForStashFile(stashId, filePath);
    setDiffLines(lines);
}

void DiffModel::clear()
{
    beginResetModel();
    m_lines.clear();
    m_additions = 0;
    m_deletions = 0;
    endResetModel();
    emit countChanged();
    emit statsChanged();
}

void DiffModel::setDiffLines(const QList<DiffLine> &lines)
{
    beginResetModel();
    m_lines = lines;
    m_additions = 0;
    m_deletions = 0;
    for (const auto &l : m_lines) {
        if (l.type == DiffLineType::Addition) m_additions++;
        else if (l.type == DiffLineType::Deletion) m_deletions++;
    }
    endResetModel();
    emit countChanged();
    emit statsChanged();
}

void DiffModel::setService(IGitService *service)
{
    m_service = service;
    if (!m_filePath.isEmpty()) {
        loadDiffForFile(m_filePath);
    } else {
        clear();
    }
}

} // namespace Cherry
