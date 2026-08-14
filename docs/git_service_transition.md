# Git service architecture

`cherrygi` keeps Git operations behind `IGitService`, so QML and models do not depend on a Git implementation. The real implementation is `GitCliService`, with a small read-only `GitRepositoryReader` embedded inside it.

## Why the backend is hybrid

Git's command line remains the compatibility boundary for operations that touch the index, working tree, remotes, credentials, or repository state transitions:

- status and index/worktree inspection
- diffs and metadata-only file checks
- staging, committing, undo, revert, checkout, and branch mutations
- stash creation, pop, and drop
- configuration changes
- fetch, pull, push, and GitHub CLI publishing

Those operations are still delegated to the existing `git`/`gh` processes, including their mature handling of ignore rules, filters, hooks, conflicts, SSH, and credentials.

Read-only metadata does not need a process for every request. `GitRepositoryReader` reads the repository database directly for:

- `.git` discovery, including linked-worktree `.git` files
- `HEAD`, loose refs, and `packed-refs`
- loose Git objects and pack indexes/packfiles, including ref/ofs deltas
- commit headers, messages, authors, parents, and history traversal
- commit tree contents and changed-file names for commit inspection
- stash reflog entries and stash metadata

This removes repeated process startup and text parsing from repository loading, branch dropdowns, history refreshes, and stash listings while leaving mutation semantics unchanged.

## Build dependency

The reader uses zlib to decompress Git's object format. CMake locates it with `find_package(ZLIB REQUIRED)` and links `ZLIB::ZLIB`.

```bash
# Configure and build
cmake -B build
cmake --build build
```

On Linux, install the platform's zlib development package (`zlib-devel` on Fedora, `zlib1g-dev` on Debian/Ubuntu, or `zlib` on Arch).

## Service contract

The UI continues to use only `IGitService` and its signals:

- `repositoryChanged(const RepositoryInfo &repo)` when the active repository changes
- `branchListChanged()` when refs change
- `currentBranchChanged(const BranchInfo &branch)` when `HEAD` changes
- `changedFilesUpdated()` for index/worktree changes
- `commitHistoryUpdated()` for history changes
- `remoteStatusUpdated(const RemoteStatus &status)` for remote operations
- `stashesUpdated()` for stash changes
- `operationSucceeded` / `operationFailed` for user-visible feedback

When a CLI mutation completes, `GitCliService` invalidates the direct reader's object/ref caches before emitting the normal model update signals. External changes are detected by the existing filesystem watcher and trigger the same refresh path.
