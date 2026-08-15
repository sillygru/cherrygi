#pragma once

#include <QAbstractListModel>
#include "../core/IGitService.h"

namespace Cherry {

class BranchListModel : public QAbstractListModel {
    Q_OBJECT
    Q_PROPERTY(int count READ rowCount NOTIFY countChanged)
    Q_PROPERTY(QString filterText READ filterText WRITE setFilterText NOTIFY filterTextChanged)

public:
    enum BranchRoles {
        NameRole = Qt::UserRole + 1,
        IsCurrentRole,
        IsDefaultRole,
        IsRemoteRole,
        PrNumberRole,
        PrMergedOrActiveRole,
        TipCommitShaRole
    };

    explicit BranchListModel(IGitService *service, QObject *parent = nullptr);

    int rowCount(const QModelIndex &parent = QModelIndex()) const override;
    QVariant data(const QModelIndex &index, int role = Qt::DisplayRole) const override;
    QHash<int, QByteArray> roleNames() const override;

    QString filterText() const { return m_filterText; }
    void setFilterText(const QString &text);

    Q_INVOKABLE void reload();
    void clear();
    void setUpdatesSuspended(bool suspended);
    void setService(IGitService *service);

signals:
    void countChanged();
    void filterTextChanged();

private:
    void applyFilter();

    IGitService *m_service;
    QList<BranchInfo> m_allBranches;
    QList<BranchInfo> m_filteredBranches;
    QString m_filterText;
    bool m_updatesSuspended{false};
};

} // namespace Cherry
