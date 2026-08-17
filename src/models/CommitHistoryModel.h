#pragma once

#include <QAbstractListModel>
#include <QHash>
#include "../core/IGitService.h"

namespace Cherry {

class CommitHistoryModel : public QAbstractListModel {
    Q_OBJECT
    Q_PROPERTY(int count READ rowCount NOTIFY countChanged)
    Q_PROPERTY(QString filterText READ filterText WRITE setFilterText NOTIFY filterTextChanged)

public:
    enum CommitRoles {
        ShaRole = Qt::UserRole + 1,
        ShortShaRole,
        SummaryRole,
        DescriptionRole,
        AuthorNameRole,
        AuthorEmailRole,
        AuthorAvatarUrlRole,
        RelativeTimeRole,
        TimestampRole,
        CoAuthorsRole,
        CoAuthorsTextRole,
        ChangedFilesCountRole,
        IsLocalRole,
        AuthorColorRole,
        AuthorInitialRole,
        TagsRole
    };

    explicit CommitHistoryModel(IGitService *service, QObject *parent = nullptr);

    int rowCount(const QModelIndex &parent = QModelIndex()) const override;
    QVariant data(const QModelIndex &index, int role = Qt::DisplayRole) const override;
    QHash<int, QByteArray> roleNames() const override;

    QString filterText() const { return m_filterText; }
    void setFilterText(const QString &text);

    void setAheadCount(int count);
    void setAvatarOverrides(const QHash<QString, QString> &overrides);

    Q_INVOKABLE QString getSha(int index) const;
    Q_INVOKABLE void reload();
    void clear();
    void setUpdatesSuspended(bool suspended);
    void setService(IGitService *service);

signals:
    void countChanged();
    void filterTextChanged();
    void aheadCountChanged();

private:
    void applyFilter();

    IGitService *m_service;
    QList<CommitItem> m_allCommits;
    QList<CommitItem> m_filteredCommits;
    QString m_filterText;
    QString m_currentAuthorName;
    QString m_currentAuthorEmail;
    QString m_remoteUrl;
    QHash<QString, QString> m_avatarOverrides;
    int m_aheadCount{0};
    bool m_updatesSuspended{false};
};

} // namespace Cherry
