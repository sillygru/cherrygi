#pragma once

#include "IGitService.h"
#include <QSettings>
#include <QMap>
#include <QDateTime>
#include <functional>

namespace Cherry {

struct GitResult {
    int exitCode{-1};
    QString stdOut;
    QString stdErr;
    bool success{false};
};

class GitCliService : public IGitService {
    Q_OBJECT
public:
    explicit GitCliService(QObject *parent = nullptr);
    ~GitCliService() override = default;

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
    bool canUndoCommit() const override;
    QString getLastUndoCommitSha() const override { return m_lastUndoCommitSha; }
    QString getLastUndoCommitSummary() const override { return m_lastUndoCommitSummary; }
    QString getLastUndoCommitDescription() const override { return m_lastUndoCommitDescription; }
    QStringList getLastUndoCommitCoAuthors() const override { return m_lastUndoCommitCoAuthors; }

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
    QString getGlobalAuthorName() const override;
    QString getGlobalAuthorEmail() const override;
    bool setAuthorInfo(const QString &name, const QString &email, bool global = false) override;

    // Helper utilities
    GitResult runGit(const QStringList &args, const QString &workingDir = QString(), int timeoutMs = 30000);
    void runGitAsync(const QStringList &args, std::function<void(const GitResult &)> callback);

    QString activeRepoPath() const { return m_repoPath; }

private:
    void loadSavedRepositories();
    void saveRepositories();
    void discoverInitialRepository();
    QList<DiffLine> parseDiffOutput(const QString &diffText);
    QString formatRelativeTime(const QDateTime &dt) const;

    QString m_repoPath;
    QString m_repoName;
    QMap<QString, QString> m_knownRepos; // path -> name
    QMap<QString, bool> m_fileSelection; // filePath -> isSelected

    RemoteStatus m_remoteStatus;
    QDateTime m_lastFetchTime;

    QString m_lastUndoCommitSha;
    QString m_lastUndoCommitSummary;
    QString m_lastUndoCommitDescription;
    QStringList m_lastUndoCommitCoAuthors;
};

} // namespace Cherry
