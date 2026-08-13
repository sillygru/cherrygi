#pragma once

#include <QAbstractListModel>
#include "../core/IGitService.h"

namespace Cherry {

class DiffModel : public QAbstractListModel {
    Q_OBJECT
    Q_PROPERTY(int count READ rowCount NOTIFY countChanged)
    Q_PROPERTY(QString filePath READ filePath WRITE setFilePath NOTIFY filePathChanged)
    Q_PROPERTY(int additions READ additions NOTIFY statsChanged)
    Q_PROPERTY(int deletions READ deletions NOTIFY statsChanged)

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

    Q_INVOKABLE void loadDiffForFile(const QString &filePath);
    Q_INVOKABLE void loadDiffForCommit(const QString &commitSha, const QString &filePath);
    Q_INVOKABLE void loadDiffForStash(const QString &stashId, const QString &filePath);
    Q_INVOKABLE void clear();

signals:
    void countChanged();
    void filePathChanged();
    void statsChanged();

private:
    void setDiffLines(const QList<DiffLine> &lines);

    IGitService *m_service;
    QString m_filePath;
    QString m_commitSha;
    QList<DiffLine> m_lines;
    int m_additions{0};
    int m_deletions{0};
};

} // namespace Cherry
