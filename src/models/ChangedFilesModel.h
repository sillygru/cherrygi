#pragma once

#include <QAbstractListModel>
#include "../core/IGitService.h"

namespace Cherry {

class ChangedFilesModel : public QAbstractListModel {
    Q_OBJECT
    Q_PROPERTY(int count READ rowCount NOTIFY countChanged)
    Q_PROPERTY(int selectedCount READ selectedCount NOTIFY selectionChanged)
    Q_PROPERTY(bool allSelected READ allSelected NOTIFY selectionChanged)
    Q_PROPERTY(bool hasPartialSelection READ hasPartialSelection NOTIFY selectionChanged)

public:
    enum FileChangeRoles {
        IdRole = Qt::UserRole + 1,
        FilePathRole,
        FileNameRole,
        FileDirRole,
        StatusRole,
        StatusTextRole,
        StatusIconRole,
        StatusColorRole,
        IsSelectedRole,
        AdditionsRole,
        DeletionsRole
    };

    explicit ChangedFilesModel(IGitService *service, QObject *parent = nullptr);

    int rowCount(const QModelIndex &parent = QModelIndex()) const override;
    QVariant data(const QModelIndex &index, int role = Qt::DisplayRole) const override;
    bool setData(const QModelIndex &index, const QVariant &value, int role = Qt::EditRole) override;
    QHash<int, QByteArray> roleNames() const override;

    int selectedCount() const;
    bool allSelected() const;
    bool hasPartialSelection() const;

    Q_INVOKABLE void toggleSelected(int index);
    Q_INVOKABLE void selectAll(bool select);
    Q_INVOKABLE QString getFilePath(int index) const;
    Q_INVOKABLE void reload();
    void setService(IGitService *service);

signals:
    void countChanged();
    void selectionChanged();

private:
    IGitService *m_service;
    QList<FileChange> m_files;
};

} // namespace Cherry
