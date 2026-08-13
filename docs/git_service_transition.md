# Transitioning from Mock to Real Git Service

`cherrygi` has been built with clean architectural separation. The QML UI and Qt Item Models interact exclusively with the `IGitService` interface via `AppController`.

To replace the `MockGitService` with a real Git backend (such as `libgit2` or a `QProcess`-based Git CLI service), no QML files or list models need to be modified.

---

## 1. Implementing `LibGit2Service`

Create `src/core/LibGit2Service.h` and `src/core/LibGit2Service.cpp` implementing `IGitService`:

```cpp
// src/core/LibGit2Service.h
#pragma once

#include "IGitService.h"
#include <git2.h>

namespace Cherry {

class LibGit2Service : public IGitService {
    Q_OBJECT
public:
    explicit LibGit2Service(QObject *parent = nullptr);
    ~LibGit2Service() override;

    // Repositories
    QList<RepositoryInfo> getRepositories() override;
    std::optional<RepositoryInfo> getCurrentRepository() override;
    bool openRepository(const QString &pathOrId) override;
    bool addRepository(const QString &name, const QString &path) override;

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

    // Commit & History
    QList<CommitItem> getCommitHistory(int limit = 100) override;
    std::optional<CommitItem> getCommitDetails(const QString &sha) override;
    bool createCommit(const QString &summary, const QString &description, const QStringList &coAuthors) override;
    bool undoLastCommit() override;
    bool revertCommit(const QString &sha) override;

    // Remote Operations
    RemoteStatus getRemoteStatus() override;
    void fetchOrigin() override;
    void pullOrigin() override;
    void pushOrigin() override;

    // Stashing
    QList<StashItem> getStashes() override;
    bool stashChanges(const QString &message = QString()) override;
    bool popStash(const QString &stashId = QString()) override;
    bool dropStash(const QString &stashId) override;

private:
    git_repository *m_repo{nullptr};
    QString m_repoPath;
};

} // namespace Cherry
```

---

## 2. CMake Integration

In `CMakeLists.txt`, find and link `libgit2`:

```cmake
find_package(PkgConfig REQUIRED)
pkg_check_modules(LIBGIT2 REQUIRED libgit2)

target_link_libraries(cherrygi PRIVATE
    ${LIBGIT2_LIBRARIES}
    ...
)
```

---

## 3. Switching Backend in `AppController`

In `src/core/AppController.cpp`, instantiate `LibGit2Service` (or choose between Mock and Real Git via an environment variable / setting):

```cpp
AppController::AppController(QObject *parent)
    : QObject(parent)
{
    const bool useMock = qEnvironmentVariableIsSet("CHERRYGI_USE_MOCK");
    if (useMock) {
        m_service = std::make_unique<MockGitService>();
    } else {
        m_service = std::make_unique<LibGit2Service>();
    }

    m_repoModel = new RepositoryListModel(m_service.get(), this);
    m_branchModel = new BranchListModel(m_service.get(), this);
    m_changedFilesModel = new ChangedFilesModel(m_service.get(), this);
    m_commitHistoryModel = new CommitHistoryModel(m_service.get(), this);
    m_diffModel = new DiffModel(m_service.get(), this);
    m_stashModel = new StashModel(m_service.get(), this);

    connectServiceSignals();
    updateCurrentState();
}
```

---

## 4. Signal Contract Checklist

When implementing `IGitService` methods, ensure the following Qt signals are emitted on state changes:
- `repositoryChanged(const RepositoryInfo &repo)`: Emitted whenever the active repository changes.
- `branchListChanged()`: Emitted when branches are created, deleted, or listed.
- `currentBranchChanged(const BranchInfo &branch)`: Emitted when active branch changes.
- `changedFilesUpdated()`: Emitted when files are staged, modified, discarded, or committed.
- `commitHistoryUpdated()`: Emitted when new commits are created or pulled.
- `remoteStatusUpdated(const RemoteStatus &status)`: Emitted during and after fetch/pull/push operations.
- `stashesUpdated()`: Emitted when changes are stashed or restored.
- `operationSucceeded(const QString &msg)`: Triggers feedback toast in UI.
- `operationFailed(const QString &errMsg)`: Triggers error toast in UI.
