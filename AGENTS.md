# AGENTS.md — cherrygi

Native KDE Plasma Git client (Kirigami 6 / Qt Quick) mirroring GitHub Desktop's UI and workflow. This file is instructions for AI coding agents, not documentation for humans. Keep it factual, current, and under 150 lines; update it in the same change that changes a convention.

## Stack

- C++20, Qt 6.5+, KF6 (Kirigami, CoreAddons, I18n, IconThemes, ColorScheme, Config), CMake 3.20+, zlib.
- QML/Quick module `org.kde.cherrygi`; `CherryStyle.qml` is a QML singleton (see `QT_QML_SINGLETON_TYPE` in CMakeLists).
- Backend behind `IGitService`: `GitCliService` (hybrid — CLI mutations plus direct `.git` reader for reads).
- Git I/O is local via the `git` CLI or `GitRepositoryReader`; optional GitHub avatar metadata is fetched from the repository `mentionables/users` API using an in-memory `gh auth token` when available, with anonymous fallback.

## Commands

- Configure: `cmake -B build`
- Build (incremental, default validation gate): `cmake --build build`
- Clean build (only when deleting files, modifying CMakeLists, or encountering stale cache issues): `rm -rf build && cmake -B build && cmake --build build`
- There is no test target and no `ctest`. Do not invent one. Build success is the validation gate.
- Do **not** launch/execute the UI unless explicitly asked.

## Code style — C++

- 4-space indent, `#pragma once`, all project code in `namespace Cherry`.
- Members prefixed `m_`; local functions camelCase; `enum class` and plain structs in `Types.h`.
- New structs used as model data must be registered with `Q_DECLARE_METATYPE` and use member-init defaults (`int count{0};`).
- Models are `QAbstractListModel` subclasses exposing roles as camelCase names.

```cpp
// Data-flow pattern (RepositoryListModel.cpp)
QVariant RepositoryListModel::data(const QModelIndex &index, int role) const
{
    if (!index.isValid() || index.row() >= m_repos.size()) return QVariant();
    const auto &repo = m_repos[index.row()];
    switch (role) {
    case NameRole:   return repo.name;
    case IsMissingRole: return repo.isMissing;
    default:         return QVariant();
    }
}
```

## Code style — QML

- Import order: `QtQuick`, `QtQuick.Controls as QQC2`, `QtQuick.Layouts`, `org.kde.kirigami as Kirigami`, `org.kde.cherrygi`, then relative `../style`.
- Every component gets `id: root`, lives in `src/qml/components/`, and talks to the app only via the `appController` singleton.
- Use `CherryStyle` tokens and `Kirigami.Theme` for all colors/dimensions — never hardcode color values (see `UndoToast.qml` for the pattern).

## Architecture

```
IGitService → AppController → Qt Item Models → QML
GitHubAvatarService → AppController → Qt Item Models/QML
```

- QML never calls `IGitService` or Git directly; only `AppController` talks to the service.
- Renames carry `oldFilePath` on `FileChange`; pass it through (`getDiffForFile(path, oldPath)`, `getDiffForCommitFile(sha, path, oldPath)`).
- Image diffs render via `GitImageProvider` + `ImageDiffViewer.qml`; keep blob reads on the service layer.

## State invariants (keep these true)

- **Commit**: stage selected files → snapshot for undo → update ahead count → clear summary/description.
- **Undo**: restore uncommitted files → pop commit → restore summary/description → decrement ahead count.
- **Stash**: selecting a stash sets `selectedStashId` and shows `StashInspector`; popping/dropping clears the selection and restores/drops the snapshot in the service.

## Boundaries — do not

- Auto-stage or `git add .`; changed files are staged explicitly per-file from the UI.
- Hardcode colors, sizes, or strings that already exist as `CherryStyle` tokens or `Kirigami.Theme`.
- Push to remotes, or rewrite history, unless explicitly requested.
- Touch generated/install artifacts (`build/`, `.desktop` metadata, icon assets) except when the change is intentional.

## Git workflow

- Make logical incremental local commits when implementing features or fixes; never push unless asked.
- Commit messages follow the existing log: short imperative summary, optional `feat:`/`fix:`/`refactor:`/`docs:` prefix, one topic per commit.

## Key files

- `src/core/IGitService.h` — service interface; all Git capabilities live here.
- `src/core/AppController.h` — central QObject; QML-facing state and actions.
- `src/core/GitHubAvatarService.h` — repository-scoped GitHub identity/avatar lookup and cache.
- `src/core/GitCliService.cpp` — hybrid backend (CLI + direct `.git` reader).
- `src/qml/Main.qml` — root `Kirigami.ApplicationWindow`.
- `docs/architecture.md`, `docs/git_service_transition.md` — deeper context when needed.
