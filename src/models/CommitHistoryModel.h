#pragma once

#include <QAbstractListModel>
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
        ChangedFilesCountRole
    };

    explicit CommitHistoryModel(IGitService *service, QObject *parent = nullptr);

    int rowCount(const QModelIndex &parent = QModelIndex()) const override;
    QVariant data(const QModelIndex &index, int role = Qt::DisplayRole) const override;
    QHash<int, QByteArray> roleNames() const override;

    QString filterText() const { return m_filterText; }
    void setFilterText(const QString &text);

    Q_INVOKABLE QString getSha(int index) const;
    Q_INVOKABLE void reload();

signals:
    void countChanged();
    void filterTextChanged();

private:
    void applyFilter();

    IGitService *m_service;
    QList<CommitItem> m_allCommits;
    QList<CommitItem> m_filteredCommits;
    QString m_filterText;
};

} // namespace Cherry
