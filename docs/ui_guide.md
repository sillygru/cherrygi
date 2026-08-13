# cherrygi UI & Theming Guide

This document explains how GitHub Desktop's layout and interaction hierarchy are adapted for **KDE Plasma 6 / Breeze** using **Kirigami**.

---

## 1. GitHub Desktop Structure Mapping

| GitHub Desktop Component | `cherrygi` QML Component | KDE Plasma Breeze Adaptation |
| :--- | :--- | :--- |
| **Top Menu Bar** | *Omitted* (as requested) | Clean, distraction-free modern client look |
| **Repo Switcher (Top Left)** | `HeaderBar.qml` + `RepoDropdown.qml` | Breeze card styling, `folder-git` icon, search field, repository path label, changed badge |
| **Branch Switcher (Top Mid)** | `HeaderBar.qml` + `BranchDropdown.qml` | `vcs-branch` icon, PR badge chip (`#17192 ✓`), inline branch creation form, default tag |
| **Sync Action (Top Right)** | `HeaderBar.qml` + `RemoteDropdown.qml` | Spin animation during sync, `vcs-pull-symbolic`/`vcs-push-symbolic`, count badge (`3 ↓`), context menu |
| **Changes / History Tabs** | `SidebarPanel.qml` | Segmented Breeze tabs with count badge, highlight underline indicator |
| **Changed Files List** | `ChangesTab.qml` | Checkbox staging, semantic status badges (Yellow Modified, Green Added, Red Deleted), addition/deletion pills |
| **Stashed Changes Strip** | `ChangesTab.qml` | Interactive clickable row with direct **Restore** and **Discard** buttons and selection highlight |
| **Stash Inspector (Right Pane)** | `StashInspector.qml` | Stash summary card, branch & timestamp tags, Restore/Discard buttons, stashed file list, diff viewer |
| **Commit Box (Bottom Left)** | `CommitBox.qml` | Identicon avatar, Summary `TextField`, Description `TextArea`, Co-authors chip flow (`@user [x]`), "Commit to <branch>" accent button |
| **Undo Commit Banner** | `CommitBox.qml` + `UndoToast.qml` | Transient alert banner allowing 1-click commit undo with state restoration |
| **Diff Header Bar** | `DiffHeader.qml` | File path, prev/next file arrows, split vs unified toggle, whitespace toggle, gear menu |
| **Diff Viewer (Right Pane)** | `DiffViewer.qml` | Dual gutter (old/new line numbers), hunk header tint, soft green/red row backgrounds, monospace font |
| **Commit Inspector** | `CommitInspector.qml` | Full summary header, author card with SHA copy, changed files list in commit, historical diff viewer |

---

## 2. Visual Palette & KDE Plasma Tokens

`cherrygi` dynamically utilizes `Kirigami.Theme` and `CherryStyle.qml` tokens:

- **Backgrounds**:
  - Main background: `Kirigami.Theme.backgroundColor`
  - Cards & headers: `CherryStyle.cardBackground` (`rgba(textColor, 0.04)`)
  - Hover highlights: `CherryStyle.hoverBackground` (`rgba(textColor, 0.07)`)
  - Active selection: `CherryStyle.activeBackground` (`rgba(highlightColor, 0.15)`)
- **Semantic Git Accents**:
  - **Additions (`+`)**: `Kirigami.Theme.positiveTextColor` (`#2ec27e`) with 14% alpha row fill and 22% alpha gutter fill.
  - **Deletions (`-`)**: `Kirigami.Theme.negativeTextColor` (`#e01b24`) with 14% alpha row fill and 22% alpha gutter fill.
  - **Modifications (`[M]`)**: Amber (`#e5a50a`) with subtle badge border.
  - **Hunk Headers (`@@`)**: `Kirigami.Theme.highlightColor` with 10% alpha header fill.
- **Borders & Radii**:
  - Border color: `CherryStyle.borderColor` (`rgba(textColor, 0.12)`)
  - Corner radius: `CherryStyle.radiusSmall` (4px), `CherryStyle.radiusMedium` (6px).

---

## 3. Typography Hierarchy

- **UI Headings**: `Kirigami.Theme.defaultFont` (System Breeze Sans, 14–16px bold).
- **Code & Diff Text**: `Monospace, Source Code Pro, Hack, JetBrains Mono, Fira Code, monospace` (13px, crisp metrics).
- **Metadata & Badges**: System font (11–12px with muted disabled color).

---

## 4. Key Micro-Interactions

1. **Commit Workflow**:
   - Entering text in the summary field enables the accent "Commit to <branch>" button if changes are checked.
   - Clicking Commit clears the fields, updates history, increments ahead count, and displays the **Undo Banner**.
2. **Undo Commit**:
   - Clicking "Undo" immediately pops the commit from history, restores the uncommitted files, re-populates the summary and description in the input box, and decrements ahead count.
3. **Stashed Changes Inspection**:
   - Clicking on the Stashed Changes bar opens `StashInspector` with file list and diffs.
   - Clicking "Restore" or "Discard" pops/drops the stash with visual toast feedback.
4. **Diff View Modes**:
   - Easily toggle between **Unified** (stacked) and **Split** (side-by-side) with aligned line numbers and synced scrolling.
5. **Branch & Repository Filtering**:
   - Real-time search filter in both repository and branch dropdown sheets.
