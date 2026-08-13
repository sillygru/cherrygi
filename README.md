# cherrygi 🍒

> A beautiful, native **KDE Plasma 6** Git client modeled on the intuitive layout and workflow of **GitHub Desktop**, built with **C++20, Kirigami, Qt 6, and CMake**.

---

## ✨ Features

- **GitHub Desktop Structure, KDE Plasma Soul**:
  - Distraction-free, modern top header bar with 3 segments:
    - **Current Repository**: Switch repositories with search filter, view repository paths and active local changes.
    - **Current Branch**: Switch branches, view Pull Request status badges (`#17192 ✓`), and create new branches.
    - **Remote Actions & Sync**: Real-time ahead/behind badges (`3 ↓`, `1 ↑`), fetch origin, pull origin, and push origin with animated spinners.
- **Left Sidebar Workflow**:
  - **Changes Tab**:
    - Select all / Deselect all master control.
    - List of changed files with checkboxes and status badges (**[M]** Modified, **[+]** Added, **[-]** Deleted, **[R]** Renamed).
    - Additions (`+10`) and Deletions (`-2`) badges per file.
    - Stashed Changes foldout with 1-click restore or drop.
  - **Commit Box**:
    - Author avatar identicon.
    - Commit title / summary single-line field.
    - Multi-line description textarea.
    - Co-author tag chips (`@sergiou87 [x]`, `@tidy-dev [x]`) with interactive addition popup.
    - Prominent **"Commit to <current_branch>"** accent action button.
    - **1-Click Undo Commit**: Instant undo banner to restore changes and commit metadata.
  - **History Tab**:
    - Search commits by message, author, or SHA.
    - Chronological list with author avatar, summary, relative timestamp, and short commit SHA badges.
- **Rich Diff Viewer & Commit Inspector**:
  - **Unified & Split Modes**: Seamlessly toggle between unified stacked diff and side-by-side split view.
  - Line numbers gutter for old and new line numbers.
  - Hunk headers (`@@ -137,10 +137,19 @@`) with subtle accent tinting.
  - Addition and deletion row highlights with Breeze positive/negative semantic colors.
  - Whitespace change visibility toggle.
  - **Commit Inspector**: View full commit metadata, copy commit SHA, list changed files in that commit, and view historical diffs.
- **Modular & Maintainable Architecture**:
  - Fully decoupled `IGitService` abstract interface.
  - Stateful `MockGitService` with rich sample data mirroring GitHub Desktop's UI.
  - Ready for drop-in transition to `libgit2` or Git CLI backends.

---

## 🛠️ Requirements & Dependencies

- **CMake** >= 3.20
- **C++ Compiler** with C++20 support (GCC 12+, Clang 15+)
- **Qt 6** (Core, Gui, Qml, Quick, QuickControls2, Widgets) >= 6.5
- **KDE Frameworks 6 (KF6)**:
  - `KF6Kirigami`
  - `KF6CoreAddons`
  - `KF6I18n`
  - `KF6IconThemes`
  - `KF6ColorScheme`
  - `KF6Config`
- **Extra CMake Modules (ECM)**

On Arch Linux / Fedora / openSUSE / Ubuntu:
```bash
# Arch Linux
sudo pacman -S cmake extra-cmake-modules qt6-base qt6-declarative kirigami

# Fedora
sudo dnf install cmake extra-cmake-modules qt6-qtbase-devel qt6-qtdeclarative-devel kf6-kirigami-devel

# Ubuntu 24.04+
sudo apt install cmake extra-cmake-modules qt6-base-dev qt6-declarative-dev libkf6kirigami-dev
```

---

## 🚀 Building cherrygi

```bash
# Clone the repository
git clone https://github.com/your-org/cherrygi.git
cd cherrygi

# Configure with CMake
cmake -B build

# Build the executable
cmake --build build
```

---

## 📂 Project Structure

```
cherrygi/
├── CMakeLists.txt                 # CMake build configuration
├── AGENTS.md                      # Guide for developers & AI agents
├── README.md                      # User manual and overview
├── docs/
│   ├── architecture.md            # C++ service layer and QML architecture
│   ├── ui_guide.md                # Mapping GitHub Desktop to KDE Plasma Breeze
│   └── git_service_transition.md  # Transitioning from Mock to real Git backends
└── src/
    ├── main.cpp                   # App entry point
    ├── core/
    │   ├── Types.h                # Core Git data structures
    │   ├── IGitService.h          # Abstract Git service interface
    │   ├── MockGitService.h/.cpp  # Stateful in-memory Git simulation
    │   └── AppController.h/.cpp   # Central coordinator for UI and models
    ├── models/
    │   ├── RepositoryListModel    # List of repositories
    │   ├── BranchListModel        # Branch list with filter
    │   ├── ChangedFilesModel      # Staged and modified files
    │   ├── CommitHistoryModel     # Searchable commit history
    │   ├── DiffModel              # Diff lines and gutter line numbers
    │   └── StashModel             # Stashed changes
    └── qml/
        ├── Main.qml               # Root application window
        ├── style/
        │   └── CherryStyle.qml    # Theme metrics, Breeze colors, typography
        └── components/
            ├── HeaderBar.qml      # 3-segmented header
            ├── RepoDropdown.qml   # Repository switcher
            ├── BranchDropdown.qml # Branch switcher and creator
            ├── RemoteDropdown.qml # Remote sync and actions
            ├── SidebarPanel.qml   # Changes / History tabs & commit box
            ├── ChangesTab.qml     # Changes list with checkboxes
            ├── HistoryTab.qml     # Commit history list
            ├── CommitBox.qml      # Commit message inputs and co-authors
            ├── MainContentArea.qml# Diff viewer & commit inspector container
            ├── DiffHeader.qml     # Diff controls and navigation
            ├── DiffViewer.qml     # Unified & Split diff viewer
            ├── CommitInspector.qml# Detailed historical commit view
            └── UndoToast.qml      # Floating feedback banner
```

---

## 📖 Documentation

- [Architecture Overview](docs/architecture.md)
- [UI & Theming Guide](docs/ui_guide.md)
- [Git Backend Transition Guide](docs/git_service_transition.md)
- [Agent & Developer Guide](AGENTS.md)

---

## 📄 License

Licensed under the LGPLv3 or GPLv3 to align with KDE Frameworks and Qt Open Source licensing.
