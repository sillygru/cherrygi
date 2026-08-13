#pragma once

#include <QAbstractListModel>
#include "../core/IGitService.h"

namespace Cherry {

class StashModel : public QAbstractListModel {
    Q_OBJECT
    Q_PROPERTY(int count READ rowCount NOTIFY countChanged)

public:
    enum StashRoles {
        IdRole = Qt::UserRole + 1,
        MessageRole,
        BranchNameRole,
        TimestampRole,
        FileCountRole
    };

    explicit StashModel(IGitService *service, QObject *parent = nullptr);

    int rowCount(const QModelIndex &parent = QModelIndex()) const override;
    QVariant data(const QModelIndex &index, int role = Qt::DisplayRole) const override;
    QHash<int, QByteArray> roleNames() const override;

    Q_INVOKABLE void reload();

signals:
    void countChanged();

private:
    IGitService *m_service;
    QList<StashItem> m_stashes;
};

} // namespace Cherry
