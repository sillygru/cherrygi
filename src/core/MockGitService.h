#pragma once

#include "IGitService.h"
#include <QMap>
#include <QStack>
#include <QTimer>

namespace Cherry {

struct RepoState {
    RepositoryInfo info;
    QList<BranchInfo> branches;
    QString currentBranch;
    QList<FileChange> changedFiles;
    QList<CommitItem> commitHistory;
    QList<StashItem> stashes;
    RemoteStatus remoteStatus;
};

class MockGitService : public IGitService {
    Q_OBJECT
public:
    explicit MockGitService(QObject *parent = nullptr);
    ~MockGitService() override = default;

    // Repositories
    QList<RepositoryInfo> getRepositories() override;
    std::optional<RepositoryInfo> getCurrentRepository() override;
    bool openRepository(const QString &pathOrId) override;
    bool addRepository(const QString &name, const QString &path) override;
    bool removeRepository(const QString &repoIdOrPath) override;

    // Branches
    QList<BranchInfo> getBranches() override;
    std::optional<BranchInfo> getCurrentBranch() override;
    bool switchBranch(const QString &branchName) override;
    bool createBranch(const QString &branchName, const QString &sourceBranch = QString()) override;
    bool deleteBranch(const QString &branchName) override;

    // Working Directory / Changes
    QList<FileChange> getChangedFiles() override;
    void setFileSelected(const QString &filePath, bool selected) override;
    void setAllFilesSelected(bool selected) override;
    bool discardFileChanges(const QString &filePath) override;
    bool discardAllChanges() override;

    // Diff
    QList<DiffLine> getDiffForFile(const QString &filePath) override;
    QList<DiffLine> getDiffForCommitFile(const QString &commitSha, const QString &filePath) override;
    QList<DiffLine> getDiffForStashFile(const QString &stashId, const QString &filePath) override;

    // Commit & History
    QList<CommitItem> getCommitHistory(int limit = 100) override;
    std::optional<CommitItem> getCommitDetails(const QString &sha) override;
    bool createCommit(const QString &summary, const QString &description, const QStringList &coAuthors) override;
    bool undoLastCommit() override;
    bool revertCommit(const QString &sha) override;
    bool canUndoCommit() const override { return !m_undoStack.isEmpty(); }
    QString getLastUndoCommitSha() const override { return m_undoStack.isEmpty() ? QString() : m_undoStack.top().commit.sha; }
    QString getLastUndoCommitSummary() const override { return m_undoStack.isEmpty() ? QString() : m_undoStack.top().commit.summary; }
    QString getLastUndoCommitDescription() const override { return m_undoStack.isEmpty() ? QString() : m_undoStack.top().commit.description; }
    QStringList getLastUndoCommitCoAuthors() const override { return m_undoStack.isEmpty() ? QStringList() : m_undoStack.top().commit.coAuthors; }

    // Remote Operations & Publishing
    RemoteStatus getRemoteStatus() override;
    bool hasRemote() const override;
    QString getRemoteUrl(const QString &remoteName = "origin") const override;
    bool setRemoteUrl(const QString &url, const QString &remoteName = "origin") override;
    bool removeRemote(const QString &remoteName = "origin") override;
    bool publishRepository(const QString &name, const QString &description, bool isPrivate) override;
    void fetchOrigin() override;
    void pullOrigin() override;
    void pushOrigin() override;

    // Stashing
    QList<StashItem> getStashes() override;
    std::optional<StashItem> getStashDetails(const QString &stashId) override;
    bool stashChanges(const QString &message = QString()) override;
    bool popStash(const QString &stashId = QString()) override;
    bool dropStash(const QString &stashId) override;

    // User / Author Info
    QString getAuthorName() const override;
    QString getAuthorEmail() const override;
    QString getGlobalAuthorName() const override { return "Desktop Contributor"; }
    QString getGlobalAuthorEmail() const override { return "contributor@desktop.local"; }
    bool setAuthorInfo(const QString &name, const QString &email, bool global = false) override;

    // Undo commit snapshot
    struct UndoSnapshot {
        CommitItem commit;
        QList<FileChange> restoredFiles;
    };
    UndoSnapshot getLastUndoSnapshot() const { return m_undoStack.isEmpty() ? UndoSnapshot{} : m_undoStack.top(); }

private:
    void initializeMockData();
    RepoState* activeState();

    QMap<QString, RepoState> m_repositories;
    QString m_currentRepoId;
    QStack<UndoSnapshot> m_undoStack;
};

} // namespace Cherry
