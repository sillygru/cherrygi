#pragma once

#include "IGitService.h"
#include "GitRepositoryReader.h"
#include <QSettings>
#include <QMap>
#include <QDateTime>
#include <QFileSystemWatcher>
#include <QTimer>
#include <QRecursiveMutex>
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
    void refreshRepository() override;

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

    // Diff & Blobs
    QList<DiffLine> getDiffForFile(const QString &filePath) override;
    bool isFileMetadataOnly(const QString &filePath) override;
    QList<DiffLine> getDiffForCommitFile(const QString &commitSha, const QString &filePath) override;
    QList<DiffLine> getDiffForStashFile(const QString &stashId, const QString &filePath) override;
    QByteArray getFileBlob(const QString &filePath, const QString &ref = QString()) override;
    bool isImageFile(const QString &filePath) const override;

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

    // Git configuration
    bool ignoreFileModeChanges(bool global) override;
    bool setIgnoreFileModeChanges(bool ignored, bool global) override;

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
    void clearUndoState();

private:
    void loadSavedRepositories();
    void saveRepositories();
    void discoverInitialRepository();
    void setupFileSystemWatcher();
    void invalidateRepositoryCaches();
    QList<DiffLine> parseDiffOutput(const QString &diffText);
    QString formatRelativeTime(const QDateTime &dt) const;
    QString formatGitError(const QString &rawError, const QString &fallbackContext) const;

    // Per-repo last-fetch timestamps, persisted to app data.
    void loadFetchTimes();
    void saveFetchTimes();

    bool m_suppressRefreshSignals{false};
    bool m_refreshInProgress{false};
    bool m_changedFilesCacheValid{false};
    bool m_branchesCacheValid{false};
    bool m_currentBranchCacheValid{false};
    bool m_commitHistoryCacheValid{false};
    bool m_stashesCacheValid{false};
    bool m_remoteStatusCacheValid{false};
    QList<FileChange> m_changedFilesCache;
    QList<BranchInfo> m_branchesCache;
    std::optional<BranchInfo> m_currentBranchCache;
    QList<CommitItem> m_commitHistoryCache;
    QList<StashItem> m_stashesCache;

    bool m_fileDiffCacheValid{false};
    QString m_fileDiffCachePath;
    QList<DiffLine> m_fileDiffCache;

    void preloadRepositoryCaches();
    void autoStageChanges();
    void emitRepositoryRefreshSignals(bool changedFilesChanged = true);

    QString m_repoPath;
    QString m_repoName;
    // Native read-only view of .git. Mutating and network operations still use git CLI.
    GitRepositoryReader m_repositoryReader;
    QMap<QString, QString> m_knownRepos; // path -> name
    QMap<QString, bool> m_fileSelection; // filePath -> isSelected

    RemoteStatus m_remoteStatus;
    QDateTime m_lastFetchTime;
    QMap<QString, QDateTime> m_repoFetchTimes; // repoPath -> last fetch time

    QString m_lastUndoCommitSha;
    QString m_lastUndoCommitSummary;
    QString m_lastUndoCommitDescription;
    QStringList m_lastUndoCommitCoAuthors;

    QFileSystemWatcher *m_fsWatcher{nullptr};
    QTimer *m_fsDebounceTimer{nullptr};
    mutable QRecursiveMutex m_cacheMutex;
};

} // namespace Cherry
