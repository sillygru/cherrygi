#pragma once

#include "Types.h"
#include <QObject>
#include <QList>
#include <optional>

namespace Cherry {

class IGitService : public QObject {
    Q_OBJECT
public:
    explicit IGitService(QObject *parent = nullptr) : QObject(parent) {}
    ~IGitService() override = default;

    // Repositories
    virtual QList<RepositoryInfo> getRepositories() = 0;
    virtual std::optional<RepositoryInfo> getCurrentRepository() = 0;
    virtual bool openRepository(const QString &pathOrId) = 0;
    virtual bool addRepository(const QString &name, const QString &path) = 0;
    virtual bool removeRepository(const QString &repoIdOrPath) = 0;
    virtual void refreshRepository() = 0;

    // Branches
    virtual QList<BranchInfo> getBranches() = 0;
    virtual std::optional<BranchInfo> getCurrentBranch() = 0;
    virtual bool switchBranch(const QString &branchName) = 0;
    virtual bool createBranch(const QString &branchName, const QString &sourceBranch = QString()) = 0;
    virtual bool deleteBranch(const QString &branchName) = 0;

    // Working Directory / Changes
    virtual QList<FileChange> getChangedFiles() = 0;
    virtual void setFileSelected(const QString &filePath, bool selected) = 0;
    virtual void setAllFilesSelected(bool selected) = 0;
    virtual bool discardFileChanges(const QString &filePath) = 0;
    virtual bool discardAllChanges() = 0;

    // Diff & Blobs
    virtual QList<DiffLine> getDiffForFile(const QString &filePath) = 0;
    virtual bool isFileMetadataOnly(const QString &filePath) = 0;
    virtual QList<DiffLine> getDiffForCommitFile(const QString &commitSha, const QString &filePath) = 0;
    virtual QList<DiffLine> getDiffForStashFile(const QString &stashId, const QString &filePath) = 0;
    virtual QByteArray getFileBlob(const QString &filePath, const QString &ref = QString()) = 0;
    virtual bool isImageFile(const QString &filePath) const = 0;

    // Commit & History
    virtual QList<CommitItem> getCommitHistory(int limit = 100) = 0;
    virtual std::optional<CommitItem> getCommitDetails(const QString &sha) = 0;
    virtual bool createCommit(const QString &summary, const QString &description, const QStringList &coAuthors) = 0;
    virtual bool undoLastCommit() = 0;
    virtual bool revertCommit(const QString &sha) = 0;
    virtual bool canUndoCommit() const = 0;
    virtual QString getLastUndoCommitSha() const = 0;
    virtual QString getLastUndoCommitSummary() const = 0;
    virtual QString getLastUndoCommitDescription() const = 0;
    virtual QStringList getLastUndoCommitCoAuthors() const = 0;

    // Remote Operations & Publishing
    virtual RemoteStatus getRemoteStatus() = 0;
    virtual bool hasRemote() const = 0;
    virtual QString getRemoteUrl(const QString &remoteName = "origin") const = 0;
    virtual bool setRemoteUrl(const QString &url, const QString &remoteName = "origin") = 0;
    virtual bool removeRemote(const QString &remoteName = "origin") = 0;
    virtual bool publishRepository(const QString &name, const QString &description, bool isPrivate) = 0;
    virtual void fetchOrigin() = 0;
    virtual void pullOrigin() = 0;
    virtual void pushOrigin() = 0;

    // Stashing
    virtual QList<StashItem> getStashes() = 0;
    virtual std::optional<StashItem> getStashDetails(const QString &stashId) = 0;
    virtual bool stashChanges(const QString &message = QString()) = 0;
    virtual bool popStash(const QString &stashId = QString()) = 0;
    virtual bool dropStash(const QString &stashId) = 0;

    // Git configuration
    virtual bool ignoreFileModeChanges(bool global) = 0;
    virtual bool setIgnoreFileModeChanges(bool ignored, bool global) = 0;

    // User / Author Info
    virtual QString getAuthorName() const = 0;
    virtual QString getAuthorEmail() const = 0;
    virtual QString getGlobalAuthorName() const = 0;
    virtual QString getGlobalAuthorEmail() const = 0;
    virtual bool setAuthorInfo(const QString &name, const QString &email, bool global = false) = 0;

signals:
    void repositoryChanged(const RepositoryInfo &repo);
    void branchListChanged();
    void currentBranchChanged(const BranchInfo &branch);
    void changedFilesUpdated();
    void commitHistoryUpdated();
    void remoteStatusUpdated(const RemoteStatus &status);
    void stashesUpdated();
    void operationSucceeded(const QString &message);
    void operationFailed(const QString &errorMessage);
};

} // namespace Cherry
