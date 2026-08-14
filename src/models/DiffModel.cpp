#include "DiffModel.h"
#include <QPointer>
#include <QThread>
#include <QDateTime>
#include <QImage>

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

void DiffModel::setImageDiffMode(const QString &mode)
{
    if (m_imageDiffMode == mode) return;
    m_imageDiffMode = mode;
    emit imageDiffModeChanged();
}

void DiffModel::populateImageInfo(IGitService *service, const QString &filePath, const QString &oldRef, const QString &newRef, DiffLoadResult &result)
{
    QByteArray oldBlob = service->getFileBlob(filePath, oldRef);
    QByteArray newBlob = service->getFileBlob(filePath, newRef);

    result.oldImageSize = oldBlob.size();
    result.newImageSize = newBlob.size();

    if (!oldBlob.isEmpty()) {
        QImage oldImg;
        if (oldImg.loadFromData(oldBlob)) {
            result.oldImageDimensions = QString("%1 × %2 px").arg(oldImg.width()).arg(oldImg.height());
        }
    }

    if (!newBlob.isEmpty()) {
        QImage newImg;
        if (newImg.loadFromData(newBlob)) {
            result.newImageDimensions = QString("%1 × %2 px").arg(newImg.width()).arg(newImg.height());
        }
    }
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

    IGitService *service = m_service;
    loadDiffAsync([service, filePath]() {
        DiffLoadResult result;
        if (service->isImageFile(filePath)) {
            result.isImage = true;
            const qint64 ts = QDateTime::currentMSecsSinceEpoch();
            result.oldImageUrl = QString("image://gitimage/working/old/%1?t=%2").arg(filePath).arg(ts);
            result.newImageUrl = QString("image://gitimage/working/new/%1?t=%2").arg(filePath).arg(ts);
            populateImageInfo(service, filePath, "HEAD", "", result);
        } else {
            result.lines = service->getDiffForFile(filePath);
            result.metadataOnly = service->isFileMetadataOnly(filePath);
        }
        return result;
    });
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

    IGitService *service = m_service;
    loadDiffAsync([service, commitSha, filePath]() {
        DiffLoadResult result;
        if (service->isImageFile(filePath)) {
            result.isImage = true;
            result.oldImageUrl = QString("image://gitimage/commit/%1/old/%2").arg(commitSha, filePath);
            result.newImageUrl = QString("image://gitimage/commit/%1/new/%2").arg(commitSha, filePath);
            populateImageInfo(service, filePath, QString("%1~1").arg(commitSha), commitSha, result);
        } else {
            result.lines = service->getDiffForCommitFile(commitSha, filePath);
        }
        return result;
    });
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

    IGitService *service = m_service;
    loadDiffAsync([service, stashId, filePath]() {
        DiffLoadResult result;
        if (service->isImageFile(filePath)) {
            result.isImage = true;
            result.oldImageUrl = QString("image://gitimage/stash/%1/old/%2").arg(stashId, filePath);
            result.newImageUrl = QString("image://gitimage/stash/%1/new/%2").arg(stashId, filePath);
            populateImageInfo(service, filePath, QString("%1^1").arg(stashId), stashId, result);
        } else {
            result.lines = service->getDiffForStashFile(stashId, filePath);
        }
        return result;
    });
}

void DiffModel::loadDiffAsync(std::function<DiffLoadResult()> loader)
{
    const quint64 generation = ++m_loadGeneration;
    if (m_metadataOnly) {
        m_metadataOnly = false;
        emit metadataOnlyChanged();
    }
    m_isLoading = true;
    emit isLoadingChanged();

    QPointer<DiffModel> self(this);
    QThread *thread = QThread::create([self, generation, loader = std::move(loader)]() mutable {
        const DiffLoadResult result = loader();
        QMetaObject::invokeMethod(self, [self, generation, result]() {
            if (!self || self->m_loadGeneration != generation) return;

            self->m_metadataOnly = result.metadataOnly;
            emit self->metadataOnlyChanged();

            bool imageChanged = (self->m_isImage != result.isImage);
            self->m_isImage = result.isImage;
            self->m_oldImageUrl = result.oldImageUrl;
            self->m_newImageUrl = result.newImageUrl;
            self->m_oldImageDimensions = result.oldImageDimensions;
            self->m_newImageDimensions = result.newImageDimensions;
            self->m_oldImageSize = result.oldImageSize;
            self->m_newImageSize = result.newImageSize;

            if (imageChanged) emit self->isImageChanged();
            emit self->imageUrlsChanged();
            emit self->imageInfoChanged();

            self->setDiffLines(result.isImage || result.metadataOnly ? QList<DiffLine>() : result.lines);
            self->m_isLoading = false;
            emit self->isLoadingChanged();
        }, Qt::QueuedConnection);
    });
    connect(thread, &QThread::finished, thread, &QObject::deleteLater);
    thread->start();
}

void DiffModel::clear()
{
    ++m_loadGeneration;
    if (m_metadataOnly) {
        m_metadataOnly = false;
        emit metadataOnlyChanged();
    }
    if (m_isImage) {
        m_isImage = false;
        emit isImageChanged();
    }
    m_oldImageUrl.clear();
    m_newImageUrl.clear();
    m_oldImageDimensions.clear();
    m_newImageDimensions.clear();
    m_oldImageSize = 0;
    m_newImageSize = 0;
    emit imageUrlsChanged();
    emit imageInfoChanged();

    beginResetModel();
    m_lines.clear();
    m_additions = 0;
    m_deletions = 0;
    endResetModel();
    m_isLoading = false;
    emit isLoadingChanged();
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

