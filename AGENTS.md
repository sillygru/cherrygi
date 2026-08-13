# AGENTS.md - Developer & Agent Guide for cherrygi

Welcome to the `cherrygi` repository. This file serves as an architectural briefing, codebase roadmap, and development handbook for AI agents and human contributors.

---

## 1. Project Overview

- **Name**: `cherrygi`
- **Purpose**: Native KDE Plasma (Kirigami / Qt 6 / Breeze) Git client matching GitHub Desktop's user interface structure and workflow conventions.
- **Languages**: C++20, QML (QtQuick / Kirigami 6), CMake.
- **Execution Policy**: Do **not** execute/launch the UI directly unless explicitly requested; build validation must be done via `cmake --build build`.

---

## 2. Directory Layout

```
cherrygi/
├── CMakeLists.txt                 # Main CMake project configuration (KF6 + Qt6)
├── AGENTS.md                      # Agent & Developer briefing (this document)
├── README.md                      # User-facing manual and features overview
├── docs/
│   ├── architecture.md            # Deep-dive on C++ architecture, models, and QML integration
│   ├── ui_guide.md                # Mapping GitHub Desktop components to KDE Plasma / Breeze
│   └── git_service_transition.md  # Guide for replacing MockGitService with LibGit2 / CLI
└── src/
    ├── main.cpp                   # Application entry point, QML engine initialization
    ├── core/
    │   ├── Types.h                # Data structures: FileChange, CommitItem, DiffLine, StashItem, etc.
    │   ├── IGitService.h          # Abstract interface defining Git backend operations
    │   ├── GitCliService.h/.cpp   # Native Git CLI backend with QProcess & QSettings
    │   ├── MockGitService.h/.cpp  # Stateful in-memory mock implementation
    │   └── AppController.h/.cpp   # Central QObject coordinating state, models, and actions
    ├── models/
    │   ├── RepositoryListModel    # QAbstractListModel for repositories
    │   ├── BranchListModel        # QAbstractListModel with branch search filtering
    │   ├── ChangedFilesModel      # QAbstractListModel for staged/changed files
    │   ├── CommitHistoryModel     # QAbstractListModel for searchable commit history
    │   ├── DiffModel              # QAbstractListModel for line-by-line diff rendering
    │   └── StashModel             # QAbstractListModel for stashed changes
    └── qml/
        ├── Main.qml               # Root Kirigami.ApplicationWindow
        ├── style/
        │   └── CherryStyle.qml    # Theme metrics, Breeze colors, typography singleton
        └── components/
            ├── HeaderBar.qml      # 3-segmented header (Repo, Branch, Remote, Backend Switcher)
            ├── BackendSelectionDialog.qml # Startup mode chooser (Real Git vs Mock Demo)
            ├── RepoDropdown.qml   # Repository switcher popup with folder picker & context menu
            ├── BranchDropdown.qml # Branch switcher and inline creator
            ├── RemoteDropdown.qml # Fetch, Pull, Push, PR, Terminal, File Manager actions
            ├── SidebarPanel.qml   # Changes / History tab switcher
            ├── ChangesTab.qml     # Changed files list with checkboxes, status badges, stash row
            ├── HistoryTab.qml     # Searchable commit history list
            ├── CommitBox.qml      # Commit summary, description, co-authors, undo banner
            ├── MainContentArea.qml# Right content container (Diff vs Stash vs Commit Inspector)
            ├── DiffHeader.qml     # Diff navigation and display controls
            ├── DiffViewer.qml     # Unified and Split (side-by-side) diff viewer
            ├── CommitInspector.qml# Detailed historical commit viewer
            ├── StashInspector.qml # Stashed changes viewer with restore and discard actions
            └── UndoToast.qml      # Floating feedback banner
```

---

## 3. Build & Test Commands

### Configure & Build
```bash
cmake -B build
cmake --build build
```

### Clean Build
```bash
rm -rf build
cmake -B build
cmake --build build
```

---

## 4. Architectural Rules & Guidelines

1. **Clean Service Decoupling**:
   - Never couple QML components directly to Git engine details.
   - All Git data flows through `IGitService` -> `AppController` -> Qt Item Models -> QML.
2. **KDE Plasma & Breeze Aesthetics**:
   - Use `Kirigami.Theme` and `CherryStyle` for all colors and dimensions.
   - Never hardcode arbitrary colors; use semantic tokens (`positiveTextColor`, `negativeTextColor`, `highlightColor`).
3. **State Invariants**:
   - Committing files must stage selected changes, generate an undo snapshot, update ahead count, and clear input fields.
   - Undoing a commit must restore uncommitted files, pop the commit from history, restore summary/description text, and decrement ahead count.
   - Selecting a stash updates `selectedStashId` and displays `StashInspector` in the main content area with its stashed file diffs.
   - Popping or dropping a stash clears the stash selection and restores/drops the snapshot in the service.
4. **Git Commits**:
   - Make logical incremental commits locally when implementing new features or bug fixes.
   - Do not push commits to remote repositories unless requested.
