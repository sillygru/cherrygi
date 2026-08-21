#pragma once

#include <QAbstractListModel>
#include "../core/IGitService.h"

namespace Cherry {

class RepositoryListModel : public QAbstractListModel {
    Q_OBJECT
    Q_PROPERTY(int count READ rowCount NOTIFY countChanged)

public:
    enum RepoRoles {
        IdRole = Qt::UserRole + 1,
        NameRole,
        PathRole,
        IsCurrentRole,
        CurrentBranchRole,
        ChangedFilesCountRole,
        AheadCountRole,
        BehindCountRole,
        LastFetchedTimeRole,
        IsMissingRole,
        RemoteUrlRole
    };

    explicit RepositoryListModel(IGitService *service, QObject *parent = nullptr);

    int rowCount(const QModelIndex &parent = QModelIndex()) const override;
    QVariant data(const QModelIndex &index, int role = Qt::DisplayRole) const override;
    QHash<int, QByteArray> roleNames() const override;

    Q_INVOKABLE void reload();
    Q_INVOKABLE QString findFirstMatchingRepoId(const QString &filter) const;
    void setService(IGitService *service);

signals:
    void countChanged();

private:
    IGitService *m_service;
    QList<RepositoryInfo> m_repos;
    QString m_currentRepoId;
};

} // namespace Cherry
