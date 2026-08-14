#pragma once

#include <QAbstractListModel>
#include <functional>
#include "../core/IGitService.h"

namespace Cherry {

class DiffModel : public QAbstractListModel {
    Q_OBJECT
    Q_PROPERTY(int count READ rowCount NOTIFY countChanged)
    Q_PROPERTY(QString filePath READ filePath WRITE setFilePath NOTIFY filePathChanged)
    Q_PROPERTY(int additions READ additions NOTIFY statsChanged)
    Q_PROPERTY(int deletions READ deletions NOTIFY statsChanged)
    Q_PROPERTY(bool isLoading READ isLoading NOTIFY isLoadingChanged)
    Q_PROPERTY(bool metadataOnly READ metadataOnly NOTIFY metadataOnlyChanged)

    Q_PROPERTY(bool isImage READ isImage NOTIFY isImageChanged)
    Q_PROPERTY(QString oldImageUrl READ oldImageUrl NOTIFY imageUrlsChanged)
    Q_PROPERTY(QString newImageUrl READ newImageUrl NOTIFY imageUrlsChanged)
    Q_PROPERTY(QString oldImageDimensions READ oldImageDimensions NOTIFY imageInfoChanged)
    Q_PROPERTY(QString newImageDimensions READ newImageDimensions NOTIFY imageInfoChanged)
    Q_PROPERTY(qint64 oldImageSize READ oldImageSize NOTIFY imageInfoChanged)
    Q_PROPERTY(qint64 newImageSize READ newImageSize NOTIFY imageInfoChanged)
    Q_PROPERTY(QString imageDiffMode READ imageDiffMode WRITE setImageDiffMode NOTIFY imageDiffModeChanged)

public:
    enum DiffRoles {
        OldLineNumRole = Qt::UserRole + 1,
        NewLineNumRole,
        OldLineNumStrRole,
        NewLineNumStrRole,
        LineTypeRole,
        LineTypeStrRole,
        MarkerRole,
        ContentRole
    };

    explicit DiffModel(IGitService *service, QObject *parent = nullptr);

    int rowCount(const QModelIndex &parent = QModelIndex()) const override;
    QVariant data(const QModelIndex &index, int role = Qt::DisplayRole) const override;
    QHash<int, QByteArray> roleNames() const override;

    QString filePath() const { return m_filePath; }
    void setFilePath(const QString &path);

    int additions() const { return m_additions; }
    int deletions() const { return m_deletions; }
    bool isLoading() const { return m_isLoading; }
    bool metadataOnly() const { return m_metadataOnly; }

    bool isImage() const { return m_isImage; }
    QString oldImageUrl() const { return m_oldImageUrl; }
    QString newImageUrl() const { return m_newImageUrl; }
    QString oldImageDimensions() const { return m_oldImageDimensions; }
    QString newImageDimensions() const { return m_newImageDimensions; }
    qint64 oldImageSize() const { return m_oldImageSize; }
    qint64 newImageSize() const { return m_newImageSize; }
    QString imageDiffMode() const { return m_imageDiffMode; }
    void setImageDiffMode(const QString &mode);

    Q_INVOKABLE void loadDiffForFile(const QString &filePath, const QString &oldFilePath = QString());
    Q_INVOKABLE void loadDiffForCommit(const QString &commitSha, const QString &filePath, const QString &oldFilePath = QString());
    Q_INVOKABLE void loadDiffForStash(const QString &stashId, const QString &filePath, const QString &oldFilePath = QString());
    Q_INVOKABLE void clear();
    void setService(IGitService *service);

signals:
    void countChanged();
    void filePathChanged();
    void statsChanged();
    void isLoadingChanged();
    void metadataOnlyChanged();
    void isImageChanged();
    void imageUrlsChanged();
    void imageInfoChanged();
    void imageDiffModeChanged();

private:
    struct DiffLoadResult {
        QList<DiffLine> lines;
        bool metadataOnly{false};
        bool isImage{false};
        QString oldImageUrl;
        QString newImageUrl;
        QString oldImageDimensions;
        QString newImageDimensions;
        qint64 oldImageSize{0};
        qint64 newImageSize{0};
    };

    void setDiffLines(const QList<DiffLine> &lines);
    void loadDiffAsync(std::function<DiffLoadResult()> loader);
    static void populateImageInfo(IGitService *service, const QString &oldPath, const QString &newPath, const QString &oldRef, const QString &newRef, DiffLoadResult &result);

    IGitService *m_service;
    QString m_filePath;
    QString m_commitSha;
    QList<DiffLine> m_lines;
    int m_additions{0};
    int m_deletions{0};
    bool m_isLoading{false};
    bool m_metadataOnly{false};
    bool m_isImage{false};
    QString m_oldImageUrl;
    QString m_newImageUrl;
    QString m_oldImageDimensions;
    QString m_newImageDimensions;
    qint64 m_oldImageSize{0};
    qint64 m_newImageSize{0};
    QString m_imageDiffMode{"2-up"};
    quint64 m_loadGeneration{0};
};

} // namespace Cherry
