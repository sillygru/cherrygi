#pragma once

#include <QObject>
#include <QString>
#include <QStringList>
#include <memory>

#include "IGitService.h"
#include "MockGitService.h"
#include "../models/RepositoryListModel.h"
#include "../models/BranchListModel.h"
#include "../models/ChangedFilesModel.h"
#include "../models/CommitHistoryModel.h"
#include "../models/DiffModel.h"
#include "../models/StashModel.h"

namespace Cherry {

class AppController : public QObject {
    Q_OBJECT

    // Models
    Q_PROPERTY(Cherry::RepositoryListModel* repositories READ repositories CONSTANT)
    Q_PROPERTY(Cherry::BranchListModel* branches READ branches CONSTANT)
    Q_PROPERTY(Cherry::ChangedFilesModel* changedFiles READ changedFiles CONSTANT)
    Q_PROPERTY(Cherry::CommitHistoryModel* commitHistory READ commitHistory CONSTANT)
    Q_PROPERTY(Cherry::DiffModel* diffModel READ diffModel CONSTANT)
    Q_PROPERTY(Cherry::StashModel* stashes READ stashes CONSTANT)

    // Current State
    Q_PROPERTY(QString currentRepoName READ currentRepoName NOTIFY currentRepoChanged)
    Q_PROPERTY(QString currentRepoPath READ currentRepoPath NOTIFY currentRepoChanged)
    Q_PROPERTY(QString currentBranchName READ currentBranchName NOTIFY currentBranchChanged)
    Q_PROPERTY(QString currentBranchPr READ currentBranchPr NOTIFY currentBranchChanged)
    Q_PROPERTY(bool currentBranchPrActive READ currentBranchPrActive NOTIFY currentBranchChanged)

    // Remote Sync State
    Q_PROPERTY(int aheadCount READ aheadCount NOTIFY remoteStatusChanged)
    Q_PROPERTY(int behindCount READ behindCount NOTIFY remoteStatusChanged)
    Q_PROPERTY(QString lastFetchedText READ lastFetchedText NOTIFY remoteStatusChanged)
    Q_PROPERTY(bool isFetching READ isFetching NOTIFY remoteStatusChanged)
    Q_PROPERTY(bool isPulling READ isPulling NOTIFY remoteStatusChanged)
    Q_PROPERTY(bool isPushing READ isPushing NOTIFY remoteStatusChanged)

    // Selection & Navigation
    Q_PROPERTY(QString activeTab READ activeTab WRITE setActiveTab NOTIFY activeTabChanged)
    Q_PROPERTY(QString selectedFilePath READ selectedFilePath WRITE setSelectedFilePath NOTIFY selectedFilePathChanged)
    Q_PROPERTY(QString selectedCommitSha READ selectedCommitSha WRITE setSelectedCommitSha NOTIFY selectedCommitShaChanged)
    Q_PROPERTY(QVariant selectedCommitData READ selectedCommitData NOTIFY selectedCommitShaChanged)

    // Diff Options
    Q_PROPERTY(QString diffViewMode READ diffViewMode WRITE setDiffViewMode NOTIFY diffViewModeChanged)
    Q_PROPERTY(bool showWhitespace READ showWhitespace WRITE setShowWhitespace NOTIFY showWhitespaceChanged)

    // Undo State
    Q_PROPERTY(bool hasUndoCommit READ hasUndoCommit NOTIFY undoStateChanged)
    Q_PROPERTY(QString lastUndoCommitSummary READ lastUndoCommitSummary NOTIFY undoStateChanged)
    Q_PROPERTY(QString lastUndoCommitDescription READ lastUndoCommitDescription NOTIFY undoStateChanged)

    // Notification / Toast
    Q_PROPERTY(QString toastMessage READ toastMessage NOTIFY toastChanged)
    Q_PROPERTY(bool toastIsError READ toastIsError NOTIFY toastChanged)
    Q_PROPERTY(bool toastVisible READ toastVisible NOTIFY toastChanged)

public:
    explicit AppController(QObject *parent = nullptr);
    ~AppController() override;

    // Getters
    RepositoryListModel* repositories() const { return m_repoModel; }
    BranchListModel* branches() const { return m_branchModel; }
    ChangedFilesModel* changedFiles() const { return m_changedFilesModel; }
    CommitHistoryModel* commitHistory() const { return m_commitHistoryModel; }
    DiffModel* diffModel() const { return m_diffModel; }
    StashModel* stashes() const { return m_stashModel; }

    QString currentRepoName() const;
    QString currentRepoPath() const;
    QString currentBranchName() const;
    QString currentBranchPr() const;
    bool currentBranchPrActive() const;

    int aheadCount() const { return m_remoteStatus.ahead; }
    int behindCount() const { return m_remoteStatus.behind; }
    QString lastFetchedText() const { return m_remoteStatus.lastFetchedText; }
    bool isFetching() const { return m_remoteStatus.isFetching; }
    bool isPulling() const { return m_remoteStatus.isPulling; }
    bool isPushing() const { return m_remoteStatus.isPushing; }

    QString activeTab() const { return m_activeTab; }
    void setActiveTab(const QString &tab);

    QString selectedFilePath() const { return m_selectedFilePath; }
    void setSelectedFilePath(const QString &path);

    QString selectedCommitSha() const { return m_selectedCommitSha; }
    void setSelectedCommitSha(const QString &sha);
    QVariant selectedCommitData() const;

    QString diffViewMode() const { return m_diffViewMode; }
    void setDiffViewMode(const QString &mode);

    bool showWhitespace() const { return m_showWhitespace; }
    void setShowWhitespace(bool show);

    bool hasUndoCommit() const;
    QString lastUndoCommitSummary() const;
    QString lastUndoCommitDescription() const;

    QString toastMessage() const { return m_toastMessage; }
    bool toastIsError() const { return m_toastIsError; }
    bool toastVisible() const { return m_toastVisible; }

    // Invocable Actions for QML
    Q_INVOKABLE void switchRepository(const QString &repoIdOrPath);
    Q_INVOKABLE void addRepository(const QString &name, const QString &path);

    Q_INVOKABLE void switchBranch(const QString &branchName);
    Q_INVOKABLE void createBranch(const QString &branchName);
    Q_INVOKABLE void deleteBranch(const QString &branchName);

    Q_INVOKABLE bool commit(const QString &summary, const QString &description, const QStringList &coAuthors);
    Q_INVOKABLE bool undoLastCommit();

    Q_INVOKABLE void fetchOrigin();
    Q_INVOKABLE void pullOrigin();
    Q_INVOKABLE void pushOrigin();

    Q_INVOKABLE void discardFileChanges(const QString &filePath);
    Q_INVOKABLE void discardAllChanges();

    Q_INVOKABLE void stashChanges(const QString &message = QString());
    Q_INVOKABLE void popStash(const QString &stashId = QString());
    Q_INVOKABLE void dropStash(const QString &stashId);

    Q_INVOKABLE void selectFileForDiff(const QString &filePath);
    Q_INVOKABLE void selectCommit(const QString &sha);
    Q_INVOKABLE void revertCommit(const QString &sha);

    Q_INVOKABLE void hideToast();
    Q_INVOKABLE void showToast(const QString &message, bool isError = false);

signals:
    void currentRepoChanged();
    void currentBranchChanged();
    void remoteStatusChanged();
    void activeTabChanged();
    void selectedFilePathChanged();
    void selectedCommitShaChanged();
    void diffViewModeChanged();
    void showWhitespaceChanged();
    void undoStateChanged();
    void toastChanged();

private:
    void updateCurrentState();
    void connectServiceSignals();

    std::unique_ptr<MockGitService> m_service;
    RepositoryListModel *m_repoModel{nullptr};
    BranchListModel *m_branchModel{nullptr};
    ChangedFilesModel *m_changedFilesModel{nullptr};
    CommitHistoryModel *m_commitHistoryModel{nullptr};
    DiffModel *m_diffModel{nullptr};
    StashModel *m_stashModel{nullptr};

    QString m_activeTab{"changes"}; // "changes" | "history"
    QString m_selectedFilePath;
    QString m_selectedCommitSha;
    QString m_diffViewMode{"unified"}; // "unified" | "split"
    bool m_showWhitespace{true};

    RemoteStatus m_remoteStatus;

    QString m_toastMessage;
    bool m_toastIsError{false};
    bool m_toastVisible{false};
};

} // namespace Cherry
