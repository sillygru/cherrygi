# cherrygi Architecture Documentation

`cherrygi` is a native KDE Plasma desktop client for Git, engineered with the visual structural conventions of GitHub Desktop and the styling, theming, and HIG (Human Interface Guidelines) of KDE Plasma 6 (Kirigami, Qt 6, Breeze).

```mermaid
graph TD
    subgraph UI ["QML Layer (Kirigami / Breeze)"]
        Main["Main.qml"]
        HeaderBar["HeaderBar.qml"]
        Sidebar["SidebarPanel.qml"]
        ChangesTab["ChangesTab.qml"]
        HistoryTab["HistoryTab.qml"]
        CommitBox["CommitBox.qml"]
        MainContentArea["MainContentArea.qml"]
        DiffViewer["DiffViewer.qml"]
        CommitInspector["CommitInspector.qml"]
        StashInspector["StashInspector.qml"]
    end

    subgraph Controller ["Controller & Models Layer (C++)"]
        AppCtrl["Cherry::AppController"]
        RepoModel["RepositoryListModel"]
        BranchModel["BranchListModel"]
        ChangesModel["ChangedFilesModel"]
        HistoryModel["CommitHistoryModel"]
        DiffMdl["DiffModel"]
        StashMdl["StashModel"]
    end

    subgraph Service ["Backend Service Layer (C++)"]
        IGit["IGitService (Abstract Interface)"]
        MockGit["MockGitService (Current Implementation)"]
        LibGit2["LibGit2Service (Future Target)"]
    end

    Main --> HeaderBar
    Main --> Sidebar
    Main --> MainContentArea
    Sidebar --> ChangesTab
    Sidebar --> HistoryTab
    Sidebar --> CommitBox
    MainContentArea --> DiffViewer
    MainContentArea --> CommitInspector
    MainContentArea --> StashInspector

    HeaderBar --> AppCtrl
    ChangesTab --> ChangesModel
    ChangesTab --> StashMdl
    HistoryTab --> HistoryModel
    CommitBox --> AppCtrl
    DiffViewer --> DiffMdl
    CommitInspector --> AppCtrl
    StashInspector --> AppCtrl

    AppCtrl --> RepoModel
    AppCtrl --> BranchModel
    AppCtrl --> ChangesModel
    AppCtrl --> HistoryModel
    AppCtrl --> DiffMdl
    AppCtrl --> StashMdl

    AppCtrl --> IGit
    RepoModel --> IGit
    BranchModel --> IGit
    ChangesModel --> IGit
    HistoryModel --> IGit
    DiffMdl --> IGit
    StashMdl --> IGit

    IGit <|-- MockGit
    IGit <|-- LibGit2
```

---

## 1. Domain Types (`src/core/Types.h`)

All data structures are strongly typed in C++:
- `RepositoryInfo`: Metadata for repositories (`id`, `name`, `path`, `currentBranch`, `changedFilesCount`, `aheadCount`, `behindCount`, `lastFetchedTime`).
- `BranchInfo`: Branch metadata (`name`, `isCurrent`, `isDefault`, `isRemote`, `prNumber`, `prMergedOrActive`, `tipCommitSha`).
- `FileChange`: File change representation (`id`, `filePath`, `status` [Modified, Added, Deleted, Renamed], `isSelected`, `additions`, `deletions`, `diffLines`).
- `CommitItem`: Full commit description (`sha`, `shortSha`, `summary`, `description`, `authorName`, `authorEmail`, `timestamp`, `relativeTime`, `coAuthors`, `changedFiles`).
- `DiffLine`: Individual diff entry (`oldLineNumber`, `newLineNumber`, `type` [Context, Addition, Deletion, HunkHeader], `content`).
- `StashItem`: Stashed snapshot (`id`, `message`, `branchName`, `timestamp`, `files`).
- `RemoteStatus`: Sync status (`ahead`, `behind`, `lastFetchedText`, `isFetching`, `isPulling`, `isPushing`).

---

## 2. Service Interface (`src/core/IGitService.h`)

The `IGitService` interface acts as an abstraction barrier between the backend and UI layers. It declares pure virtual methods:
- **Repositories**: `getRepositories()`, `getCurrentRepository()`, `openRepository()`, `addRepository()`.
- **Branches**: `getBranches()`, `getCurrentBranch()`, `switchBranch()`, `createBranch()`, `deleteBranch()`.
- **Changes**: `getChangedFiles()`, `setFileSelected()`, `setAllFilesSelected()`, `discardFileChanges()`, `discardAllChanges()`.
- **Diffs**: `getDiffForFile()`, `getDiffForCommitFile()`, `getDiffForStashFile()`.
- **Commit & History**: `getCommitHistory()`, `getCommitDetails()`, `createCommit()`, `undoLastCommit()`, `revertCommit()`.
- **Remote Operations**: `getRemoteStatus()`, `fetchOrigin()`, `pullOrigin()`, `pushOrigin()`.
- **Stashing**: `getStashes()`, `getStashDetails()`, `stashChanges()`, `popStash()`, `dropStash()`.

---

## 3. Mock Service Implementation (`src/core/MockGitService.h` & `.cpp`)

The mock service provides an in-memory Git simulator with state transitions:
1. **Interactive Commits**: Calling `createCommit()` removes selected files from `changedFiles`, creates a historical commit, increments `aheadCount`, and pushes a snapshot to an internal `m_undoStack`.
2. **Undo Commit**: Calling `undoLastCommit()` removes the tip commit from history, restores previous staged files and commit summary/description, and decrements `aheadCount`.
3. **Stash Management**: Calling `stashChanges()`, `popStash()`, or `dropStash()` updates in-memory stash lists and emits reactive signals.
4. **Branch Switching & Creation**: Allows switching between mock branches (`file-status-tooltip`, `main`, `feat/diff-split-view`, etc.) or creating new branches seamlessly.
5. **Remote Synchronization**: Simulates network latency with `QTimer::singleShot` for `fetchOrigin()`, `pullOrigin()`, and `pushOrigin()`.

---

## 4. Models Layer (`src/models/`)

To provide reactive bindings in QML without overhead, specialized `QAbstractListModel` subclasses are used:
- `RepositoryListModel`: Exposes repository lists and active repo state.
- `BranchListModel`: Exposes branches with live fuzzy filtering (`filterText`).
- `ChangedFilesModel`: Exposes checkboxes, status icons, status colors, and addition/deletion counts.
- `CommitHistoryModel`: Exposes commit history with live text search across messages, authors, and SHAs.
- `DiffModel`: Exposes line-by-line diff chunks with gutter line numbers, markers, and line types for uncommitted files, historical commits, and stashes.
- `StashModel`: Exposes list of stashed changes.

---

## 5. QML UI Layer (`src/qml/`)

The QML structure implements GitHub Desktop's workflow using KDE Plasma Breeze design language:
- `Main.qml`: `Kirigami.ApplicationWindow` containing the top header and split workspace.
- `HeaderBar.qml`: 3-segmented header:
  - Repo Segment -> `RepoDropdown.qml`
  - Branch Segment -> `BranchDropdown.qml`
  - Sync Segment -> `RemoteDropdown.qml`
- `SidebarPanel.qml`: Left panel with segmented `Changes` / `History` buttons, `ChangesTab.qml`, `HistoryTab.qml`, and bottom `CommitBox.qml`.
- `MainContentArea.qml`: Right area switching dynamically between:
  - `DiffViewer.qml`: Working-tree diffs with Unified and Split side-by-side modes.
  - `CommitInspector.qml`: Historical commit inspection and commit file diffs.
  - `StashInspector.qml`: Stashed changes inspection with 1-click restore and discard actions.
- `UndoToast.qml`: Floating transient notification toast for operation feedback and instant commit undo.
