#include "MockGitService.h"
#include <QUuid>
#include <QRandomGenerator>

namespace Cherry {

MockGitService::MockGitService(QObject *parent)
    : IGitService(parent)
{
    initializeMockData();
}

void MockGitService::initializeMockData()
{
    // ==========================================
    // REPOSITORY 1: desktop (The GitHub Desktop Mock)
    // ==========================================
    RepoState desktopRepo;
    desktopRepo.info = {
        "repo-desktop",
        "desktop",
        "/home/developer/workspace/desktop",
        "file-status-tooltip",
        3, // changedFilesCount
        0, // aheadCount
        3, // behindCount
        "Last fetched 8 minutes ago"
    };

    desktopRepo.currentBranch = "file-status-tooltip";

    desktopRepo.branches = {
        {"file-status-tooltip", true, false, false, "#17192", true, "a91fc42"},
        {"main", false, true, false, "", false, "8e23bb1"},
        {"feat/diff-split-view", false, false, false, "#17180", false, "39f01ab"},
        {"fix/stash-recovery", false, false, false, "", false, "d4128f6"},
        {"refactor/git-service", false, false, false, "#17155", true, "76e31c0"},
        {"release/3.4.1", false, false, false, "", false, "104ab72"}
    };

    desktopRepo.remoteStatus = {
        0, // ahead
        3, // behind
        "Last fetched 8 minutes ago",
        false, false, false
    };

    // --- Changed File 1: section-list.tsx ---
    FileChange change1;
    change1.id = "fc-1";
    change1.filePath = "app/src/ui/lib/list/section-list.tsx";
    change1.status = FileChangeType::Modified;
    change1.isSelected = true;
    change1.additions = 10;
    change1.deletions = 0;
    change1.diffLines = {
        {-1, -1, DiffLineType::HunkHeader, "@@ -137,10 +137,19 @@ interface ISectionListProps {"},
        {137, 137, DiffLineType::Context, "    source: IMouseClickSource"},
        {138, 138, DiffLineType::Context, "  ) => void"},
        {139, 139, DiffLineType::Context, "}"},
        {-1, 140, DiffLineType::Addition, "  /** This function will be called when a row obtains focus, no matter how */"},
        {140, 141, DiffLineType::Context, "  readonly onRowFocus?: ("},
        {141, 142, DiffLineType::Context, "    indexPath: RowIndexPath,"},
        {142, 143, DiffLineType::Context, "    source: React.FocusEvent<HTMLDivElement>"},
        {143, 144, DiffLineType::Context, "  ) => void"},
        {-1, 145, DiffLineType::Addition, "  /** This function will be called only when a row obtains focus via keyboard */"},
        {-1, 146, DiffLineType::Addition, "  readonly onRowKeyboardFocus?: ("},
        {-1, 147, DiffLineType::Addition, "    indexPath: RowIndexPath,"},
        {-1, 148, DiffLineType::Addition, "    source: React.KeyboardEvent<any>"},
        {-1, 149, DiffLineType::Addition, "  ) => void"},
        {-1, 150, DiffLineType::Addition, ""},
        {-1, 151, DiffLineType::Addition, "  /** This function will be called only when a row loses focus */"},
        {-1, 152, DiffLineType::Addition, "  readonly onRowBlur?: ("},
        {144, 153, DiffLineType::Context, "    indexPath: RowIndexPath,"},
        {145, 154, DiffLineType::Context, "    source: React.FocusEvent<HTMLDivElement>"},
        {146, 155, DiffLineType::Context, "  ) => void"}
    };

    // --- Changed File 2: tooltip.tsx ---
    FileChange change2;
    change2.id = "fc-2";
    change2.filePath = "app/src/ui/lib/tooltip.tsx";
    change2.status = FileChangeType::Modified;
    change2.isSelected = true;
    change2.additions = 6;
    change2.deletions = 2;
    change2.diffLines = {
        {-1, -1, DiffLineType::HunkHeader, "@@ -45,8 +45,12 @@ export class Tooltip extends React.Component<ITooltipProps> {"},
        {45, 45, DiffLineType::Context, "  public render() {"},
        {46, 46, DiffLineType::Context, "    const { target, text, direction, isVisible } = this.props"},
        {47, -1, DiffLineType::Deletion, "-   if (!isVisible || !text) {"},
        {48, -1, DiffLineType::Deletion, "-     return null"},
        {-1, 47, DiffLineType::Addition, "+   if (!isVisible || !text || text.trim().length === 0) {"},
        {-1, 48, DiffLineType::Addition, "+     return null"},
        {-1, 49, DiffLineType::Addition, "+   }"},
        {-1, 50, DiffLineType::Addition, "+   const ariaRole = this.props.ariaRole || 'tooltip'"},
        {49, 51, DiffLineType::Context, "    return ("},
        {50, 52, DiffLineType::Context, "      <div className=\"tooltip-popup\" role={ariaRole}>"},
        {51, 53, DiffLineType::Context, "        {text}"},
        {52, 54, DiffLineType::Context, "      </div>"}
    };

    // --- Changed File 3: tooltipped-content.tsx ---
    FileChange change3;
    change3.id = "fc-3";
    change3.filePath = "app/src/ui/lib/tooltipped-content.tsx";
    change3.status = FileChangeType::Modified;
    change3.isSelected = true;
    change3.additions = 4;
    change3.deletions = 1;
    change3.diffLines = {
        {-1, -1, DiffLineType::HunkHeader, "@@ -88,7 +88,10 @@ export const TooltippedContent: React.FC<Props> = (props) => {"},
        {88, 88, DiffLineType::Context, "  const onMouseEnter = () => {"},
        {89, -1, DiffLineType::Deletion, "-   setShowTooltip(true)"},
        {-1, 89, DiffLineType::Addition, "+   if (shouldShowOnHover(props)) {"},
        {-1, 90, DiffLineType::Addition, "+     setShowTooltip(true)"},
        {-1, 91, DiffLineType::Addition, "+   }"},
        {90, 92, DiffLineType::Context, "  }"},
        {91, 93, DiffLineType::Context, "  return <span onMouseEnter={onMouseEnter}>{props.children}</span>"}
    };

    desktopRepo.changedFiles = {change1, change2, change3};

    // Stashed Changes
    StashItem stash1;
    stash1.id = "stash-0";
    stash1.message = "WIP: Experimental virtualized list scroll optimizations";
    stash1.branchName = "file-status-tooltip";
    stash1.timestamp = QDateTime::currentDateTime().addDays(-1);
    stash1.files = {change1};
    desktopRepo.stashes = {stash1};

    // Commit History
    CommitItem c1;
    c1.sha = "a91fc42b781e9f0d453896503c836bb731a54109";
    c1.shortSha = "a91fc42";
    c1.summary = "Fix section list row key navigation boundary check";
    c1.description = "Ensures index path doesn't overflow when pressing Down arrow on last group row.\n\nCloses #17190.";
    c1.authorName = "Sergio Gómez";
    c1.authorEmail = "sergiou87@github.com";
    c1.authorAvatarUrl = "qrc:/assets/avatar1.png";
    c1.timestamp = QDateTime::currentDateTime().addSecs(-3600 * 2);
    c1.relativeTime = "2 hours ago";
    c1.coAuthors = {"@tidy-dev"};
    c1.changedFiles = {change1};

    CommitItem c2;
    c2.sha = "e57c10db054117ec8423fef6173bb4624da6e001";
    c2.shortSha = "e57c10d";
    c2.summary = "Add accessible tooltip triggers for file tree nodes";
    c2.description = "Improves screen reader compatibility across git status trees.\nCo-authored with @tidy-dev.";
    c2.authorName = "Markus Olsson";
    c2.authorEmail = "markus@github.com";
    c2.authorAvatarUrl = "qrc:/assets/avatar2.png";
    c2.timestamp = QDateTime::currentDateTime().addSecs(-3600 * 7);
    c2.relativeTime = "7 hours ago";
    c2.coAuthors = {"@sergiou87", "@tidy-dev"};
    c2.changedFiles = {change2, change3};

    CommitItem c3;
    c3.sha = "8e23bb19a4e3752e861d856fcb498425d0234cf7";
    c3.shortSha = "8e23bb1";
    c3.summary = "Merge pull request #17180 from desktop/feat/diff-split-view";
    c3.description = "Split diff viewer enhancements with side-by-side gutter alignment.";
    c3.authorName = "GitHub Automation";
    c3.authorEmail = "noreply@github.com";
    c3.authorAvatarUrl = "qrc:/assets/avatar3.png";
    c3.timestamp = QDateTime::currentDateTime().addDays(-1);
    c3.relativeTime = "Yesterday at 18:42";
    c3.changedFiles = {change1, change2};

    CommitItem c4;
    c4.sha = "d4128f60c042971eb0579e001ca2f7902bbecaa4";
    c4.shortSha = "d4128f6";
    c4.summary = "Refactor diff tokenization to support syntax styling";
    c4.description = "Pre-calculates lexer highlights for TSX, C++, Rust, and Python diff chunks.";
    c4.authorName = "Sergio Gómez";
    c4.authorEmail = "sergiou87@github.com";
    c4.authorAvatarUrl = "qrc:/assets/avatar1.png";
    c4.timestamp = QDateTime::currentDateTime().addDays(-2);
    c4.relativeTime = "2 days ago";
    c4.changedFiles = {change1};

    CommitItem c5;
    c5.sha = "39f01ab738491c953508ec3b2a2610d4812fb6b8";
    c5.shortSha = "39f01ab";
    c5.summary = "Optimize branch switcher popup filter performance";
    c5.description = "Switches to fuzzy matching debounce for repos with >1,000 remote tracking branches.";
    c5.authorName = "KDE Plasma Dev";
    c5.authorEmail = "plasma-dev@kde.org";
    c5.authorAvatarUrl = "qrc:/assets/avatar4.png";
    c5.timestamp = QDateTime::currentDateTime().addDays(-4);
    c5.relativeTime = "4 days ago";
    c5.changedFiles = {change3};

    desktopRepo.commitHistory = {c1, c2, c3, c4, c5};
    m_repositories.insert(desktopRepo.info.id, desktopRepo);

    // ==========================================
    // REPOSITORY 2: cherrygi-core
    // ==========================================
    RepoState cherryRepo;
    cherryRepo.info = {
        "repo-cherrygi",
        "cherrygi-core",
        "/run/media/gru/fatkingston/code/gitclone",
        "main",
        1, 1, 0,
        "Last fetched 2 minutes ago"
    };
    cherryRepo.currentBranch = "main";
    cherryRepo.branches = {
        {"main", true, true, false, "", false, "44a19bc"},
        {"feat/kirigami-breeze-theme", false, false, false, "#2", true, "90df12c"},
        {"feat/diff-split-mode", false, false, false, "#5", false, "bc41029"}
    };
    cherryRepo.remoteStatus = {1, 0, "Last fetched 2 minutes ago", false, false, false};

    FileChange cfile;
    cfile.id = "cg-1";
    cfile.filePath = "src/core/MockGitService.cpp";
    cfile.status = FileChangeType::Modified;
    cfile.isSelected = true;
    cfile.additions = 15;
    cfile.deletions = 2;
    cfile.diffLines = {
        {-1, -1, DiffLineType::HunkHeader, "@@ -1,10 +1,15 @@ namespace Cherry {"},
        {1, 1, DiffLineType::Context, "#include \"MockGitService.h\""},
        {2, 2, DiffLineType::Context, ""},
        {-1, 3, DiffLineType::Addition, "// High-fidelity Kirigami GitHub Desktop clone"},
        {-1, 4, DiffLineType::Addition, "namespace Cherry {"},
        {3, 5, DiffLineType::Context, "MockGitService::MockGitService(QObject *parent)"}
    };
    cherryRepo.changedFiles = {cfile};
    cherryRepo.commitHistory = {c1, c3};
    m_repositories.insert(cherryRepo.info.id, cherryRepo);

    // ==========================================
    // REPOSITORY 3: plasma-workspace
    // ==========================================
    RepoState plasmaRepo;
    plasmaRepo.info = {
        "repo-plasma",
        "plasma-workspace",
        "/home/developer/kde/plasma-workspace",
        "master",
        2, 0, 12,
        "Last fetched 1 hour ago"
    };
    plasmaRepo.currentBranch = "master";
    plasmaRepo.branches = {
        {"master", true, true, false, "", false, "ff82910"},
        {"plasma/6.2", false, false, false, "", false, "ee10294"}
    };
    plasmaRepo.remoteStatus = {0, 12, "Last fetched 1 hour ago", false, false, false};
    plasmaRepo.changedFiles = {change2, change3};
    plasmaRepo.commitHistory = {c4, c5};
    m_repositories.insert(plasmaRepo.info.id, plasmaRepo);

    m_currentRepoId = "repo-desktop";
}

RepoState* MockGitService::activeState()
{
    if (!m_repositories.contains(m_currentRepoId)) {
        if (!m_repositories.isEmpty()) {
            m_currentRepoId = m_repositories.firstKey();
        } else {
            return nullptr;
        }
    }
    return &m_repositories[m_currentRepoId];
}

QList<RepositoryInfo> MockGitService::getRepositories()
{
    QList<RepositoryInfo> list;
    for (const auto &state : m_repositories) {
        list.append(state.info);
    }
    return list;
}

std::optional<RepositoryInfo> MockGitService::getCurrentRepository()
{
    auto *state = activeState();
    if (!state) return std::nullopt;
    return state->info;
}

bool MockGitService::openRepository(const QString &pathOrId)
{
    for (auto it = m_repositories.begin(); it != m_repositories.end(); ++it) {
        if (it.key() == pathOrId || it.value().info.path == pathOrId || it.value().info.name == pathOrId) {
            m_currentRepoId = it.key();
            emit repositoryChanged(it.value().info);
            emit branchListChanged();
            if (auto b = getCurrentBranch()) {
                emit currentBranchChanged(*b);
            }
            emit changedFilesUpdated();
            emit commitHistoryUpdated();
            emit remoteStatusUpdated(it.value().remoteStatus);
            emit stashesUpdated();
            return true;
        }
    }
    return false;
}

bool MockGitService::addRepository(const QString &name, const QString &path)
{
    QString id = "repo-" + QString::number(QRandomGenerator::global()->generate());
    RepoState newRepo;
    newRepo.info = {id, name, path, "main", 0, 0, 0, "Just now"};
    newRepo.currentBranch = "main";
    newRepo.branches = {{"main", true, true, false, "", false, "init123"}};
    newRepo.remoteStatus = {0, 0, "Just now", false, false, false};
    m_repositories.insert(id, newRepo);
    openRepository(id);
    return true;
}

bool MockGitService::removeRepository(const QString &repoIdOrPath)
{
    for (auto it = m_repositories.begin(); it != m_repositories.end(); ++it) {
        if (it.key() == repoIdOrPath || it.value().info.path == repoIdOrPath || it.value().info.id == repoIdOrPath) {
            QString id = it.key();
            m_repositories.remove(id);
            if (m_currentRepoId == id) {
                if (!m_repositories.isEmpty()) {
                    openRepository(m_repositories.firstKey());
                } else {
                    m_currentRepoId.clear();
                    emit repositoryChanged({});
                }
            } else {
                emit repositoryChanged(activeState() ? activeState()->info : RepositoryInfo{});
            }
            emit operationSucceeded("Repository removed from list");
            return true;
        }
    }
    return false;
}

QList<BranchInfo> MockGitService::getBranches()
{
    auto *state = activeState();
    if (!state) return {};
    return state->branches;
}

std::optional<BranchInfo> MockGitService::getCurrentBranch()
{
    auto *state = activeState();
    if (!state) return std::nullopt;
    for (const auto &b : state->branches) {
        if (b.name == state->currentBranch) {
            return b;
        }
    }
    return std::nullopt;
}

bool MockGitService::switchBranch(const QString &branchName)
{
    auto *state = activeState();
    if (!state) return false;

    bool found = false;
    for (auto &b : state->branches) {
        if (b.name == branchName) {
            b.isCurrent = true;
            found = true;
        } else {
            b.isCurrent = false;
        }
    }

    if (found) {
        state->currentBranch = branchName;
        state->info.currentBranch = branchName;
        emit branchListChanged();
        if (auto b = getCurrentBranch()) {
            emit currentBranchChanged(*b);
        }
        emit operationSucceeded(QString("Switched to branch '%1'").arg(branchName));
        return true;
    }
    return false;
}

bool MockGitService::createBranch(const QString &branchName, const QString &sourceBranch)
{
    Q_UNUSED(sourceBranch);
    auto *state = activeState();
    if (!state || branchName.trimmed().isEmpty()) return false;

    // Check if branch already exists
    for (const auto &b : state->branches) {
        if (b.name == branchName.trimmed()) {
            emit operationFailed(QString("Branch '%1' already exists").arg(branchName));
            return false;
        }
    }

    BranchInfo newBranch;
    newBranch.name = branchName.trimmed();
    newBranch.isCurrent = false;
    newBranch.isDefault = false;
    newBranch.isRemote = false;
    newBranch.tipCommitSha = state->commitHistory.isEmpty() ? "0000000" : state->commitHistory.first().shortSha;

    state->branches.append(newBranch);
    switchBranch(newBranch.name);
    emit operationSucceeded(QString("Created and switched to branch '%1'").arg(branchName));
    return true;
}

bool MockGitService::deleteBranch(const QString &branchName)
{
    auto *state = activeState();
    if (!state || state->currentBranch == branchName) {
        emit operationFailed("Cannot delete current active branch");
        return false;
    }

    for (int i = 0; i < state->branches.size(); ++i) {
        if (state->branches[i].name == branchName) {
            state->branches.removeAt(i);
            emit branchListChanged();
            emit operationSucceeded(QString("Deleted branch '%1'").arg(branchName));
            return true;
        }
    }
    return false;
}

QList<FileChange> MockGitService::getChangedFiles()
{
    auto *state = activeState();
    if (!state) return {};
    return state->changedFiles;
}

void MockGitService::setFileSelected(const QString &filePath, bool selected)
{
    auto *state = activeState();
    if (!state) return;

    for (auto &f : state->changedFiles) {
        if (f.filePath == filePath) {
            f.isSelected = selected;
            break;
        }
    }
    emit changedFilesUpdated();
}

void MockGitService::setAllFilesSelected(bool selected)
{
    auto *state = activeState();
    if (!state) return;

    for (auto &f : state->changedFiles) {
        f.isSelected = selected;
    }
    emit changedFilesUpdated();
}

bool MockGitService::discardFileChanges(const QString &filePath)
{
    auto *state = activeState();
    if (!state) return false;

    for (int i = 0; i < state->changedFiles.size(); ++i) {
        if (state->changedFiles[i].filePath == filePath) {
            state->changedFiles.removeAt(i);
            state->info.changedFilesCount = state->changedFiles.size();
            emit changedFilesUpdated();
            emit operationSucceeded(QString("Discarded changes in '%1'").arg(filePath));
            return true;
        }
    }
    return false;
}

bool MockGitService::discardAllChanges()
{
    auto *state = activeState();
    if (!state) return false;

    state->changedFiles.clear();
    state->info.changedFilesCount = 0;
    emit changedFilesUpdated();
    emit operationSucceeded("Discarded all uncommitted changes");
    return true;
}

QList<DiffLine> MockGitService::getDiffForFile(const QString &filePath)
{
    auto *state = activeState();
    if (!state) return {};

    for (const auto &f : state->changedFiles) {
        if (f.filePath == filePath) {
            return f.diffLines;
        }
    }
    return {};
}

QList<DiffLine> MockGitService::getDiffForCommitFile(const QString &commitSha, const QString &filePath)
{
    auto *state = activeState();
    if (!state) return {};

    for (const auto &c : state->commitHistory) {
        if (c.sha == commitSha || c.shortSha == commitSha) {
            for (const auto &f : c.changedFiles) {
                if (f.filePath == filePath) {
                    return f.diffLines;
                }
            }
        }
    }
    return {};
}

QList<DiffLine> MockGitService::getDiffForStashFile(const QString &stashId, const QString &filePath)
{
    auto *state = activeState();
    if (!state) return {};

    for (const auto &s : state->stashes) {
        if (stashId.isEmpty() || s.id == stashId) {
            for (const auto &f : s.files) {
                if (filePath.isEmpty() || f.filePath == filePath) {
                    return f.diffLines;
                }
            }
        }
    }
    return {};
}

QList<CommitItem> MockGitService::getCommitHistory(int limit)
{
    auto *state = activeState();
    if (!state) return {};
    return state->commitHistory.mid(0, limit);
}

std::optional<CommitItem> MockGitService::getCommitDetails(const QString &sha)
{
    auto *state = activeState();
    if (!state) return std::nullopt;

    for (const auto &c : state->commitHistory) {
        if (c.sha == sha || c.shortSha == sha) {
            return c;
        }
    }
    return std::nullopt;
}

bool MockGitService::createCommit(const QString &summary, const QString &description, const QStringList &coAuthors)
{
    auto *state = activeState();
    if (!state || summary.trimmed().isEmpty()) {
        emit operationFailed("Commit summary cannot be empty");
        return false;
    }

    QList<FileChange> committedFiles;
    QList<FileChange> remainingFiles;

    for (const auto &f : state->changedFiles) {
        if (f.isSelected) {
            committedFiles.append(f);
        } else {
            remainingFiles.append(f);
        }
    }

    if (committedFiles.isEmpty()) {
        emit operationFailed("No files selected to commit");
        return false;
    }

    // Generate Mock SHA
    QString hex = QString::number(QRandomGenerator::global()->generate64(), 16) +
                  QString::number(QRandomGenerator::global()->generate64(), 16);
    while (hex.length() < 40) hex.append('0');
    hex = hex.left(40);

    CommitItem newCommit;
    newCommit.sha = hex;
    newCommit.shortSha = hex.left(7);
    newCommit.summary = summary.trimmed();
    newCommit.description = description;
    newCommit.authorName = "You (Local User)";
    newCommit.authorEmail = "user@kde-plasma.local";
    newCommit.authorAvatarUrl = "";
    newCommit.timestamp = QDateTime::currentDateTime();
    newCommit.relativeTime = "Just now";
    newCommit.coAuthors = coAuthors;
    newCommit.changedFiles = committedFiles;

    // Push to undo stack
    UndoSnapshot snap;
    snap.commit = newCommit;
    snap.restoredFiles = committedFiles;
    m_undoStack.push(snap);

    // Update state
    state->commitHistory.prepend(newCommit);
    state->changedFiles = remainingFiles;
    state->info.changedFilesCount = remainingFiles.size();
    state->remoteStatus.ahead += 1;
    state->info.aheadCount = state->remoteStatus.ahead;

    emit commitHistoryUpdated();
    emit changedFilesUpdated();
    emit remoteStatusUpdated(state->remoteStatus);
    emit operationSucceeded(QString("Committed %1: %2").arg(newCommit.shortSha, newCommit.summary));
    return true;
}

bool MockGitService::undoLastCommit()
{
    auto *state = activeState();
    if (!state || m_undoStack.isEmpty()) {
        emit operationFailed("No commit available to undo");
        return false;
    }

    UndoSnapshot snap = m_undoStack.pop();

    // Remove from history
    for (int i = 0; i < state->commitHistory.size(); ++i) {
        if (state->commitHistory[i].sha == snap.commit.sha) {
            state->commitHistory.removeAt(i);
            break;
        }
    }

    // Restore files
    for (const auto &f : snap.restoredFiles) {
        state->changedFiles.append(f);
    }
    state->info.changedFilesCount = state->changedFiles.size();

    if (state->remoteStatus.ahead > 0) {
        state->remoteStatus.ahead -= 1;
        state->info.aheadCount = state->remoteStatus.ahead;
    }

    emit commitHistoryUpdated();
    emit changedFilesUpdated();
    emit remoteStatusUpdated(state->remoteStatus);
    emit operationSucceeded(QString("Undid commit %1").arg(snap.commit.shortSha));
    return true;
}

bool MockGitService::revertCommit(const QString &sha)
{
    auto *state = activeState();
    if (!state) return false;

    auto opt = getCommitDetails(sha);
    if (!opt) {
        emit operationFailed("Commit not found for revert");
        return false;
    }

    return createCommit(QString("Revert \"%1\"").arg(opt->summary),
                        QString("This reverts commit %1.").arg(opt->sha),
                        {});
}

RemoteStatus MockGitService::getRemoteStatus()
{
    auto *state = activeState();
    if (!state) return {};
    return state->remoteStatus;
}

void MockGitService::fetchOrigin()
{
    auto *state = activeState();
    if (!state) return;

    state->remoteStatus.isFetching = true;
    emit remoteStatusUpdated(state->remoteStatus);

    QTimer::singleShot(600, this, [this, state]() {
        state->remoteStatus.isFetching = false;
        state->remoteStatus.lastFetchedText = "Last fetched just now";
        state->info.lastFetchedTime = "Just now";
        emit remoteStatusUpdated(state->remoteStatus);
        emit operationSucceeded("Fetched latest changes from origin");
    });
}

void MockGitService::pullOrigin()
{
    auto *state = activeState();
    if (!state) return;

    state->remoteStatus.isPulling = true;
    emit remoteStatusUpdated(state->remoteStatus);

    QTimer::singleShot(800, this, [this, state]() {
        state->remoteStatus.isPulling = false;
        int pulledCount = state->remoteStatus.behind;
        state->remoteStatus.behind = 0;
        state->info.behindCount = 0;
        state->remoteStatus.lastFetchedText = "Last fetched just now";
        emit remoteStatusUpdated(state->remoteStatus);
        emit commitHistoryUpdated();
        emit operationSucceeded(QString("Successfully pulled %1 commits from origin").arg(pulledCount));
    });
}

void MockGitService::pushOrigin()
{
    auto *state = activeState();
    if (!state) return;

    state->remoteStatus.isPushing = true;
    emit remoteStatusUpdated(state->remoteStatus);

    QTimer::singleShot(800, this, [this, state]() {
        state->remoteStatus.isPushing = false;
        int pushedCount = state->remoteStatus.ahead;
        state->remoteStatus.ahead = 0;
        state->info.aheadCount = 0;
        emit remoteStatusUpdated(state->remoteStatus);
        emit operationSucceeded(QString("Successfully pushed %1 commits to origin").arg(pushedCount));
    });
}

QList<StashItem> MockGitService::getStashes()
{
    auto *state = activeState();
    if (!state) return {};
    return state->stashes;
}

std::optional<StashItem> MockGitService::getStashDetails(const QString &stashId)
{
    auto *state = activeState();
    if (!state) return std::nullopt;

    for (const auto &s : state->stashes) {
        if (stashId.isEmpty() || s.id == stashId) {
            return s;
        }
    }
    return std::nullopt;
}

bool MockGitService::stashChanges(const QString &message)
{
    auto *state = activeState();
    if (!state || state->changedFiles.isEmpty()) {
        emit operationFailed("No changes available to stash");
        return false;
    }

    StashItem s;
    s.id = "stash-" + QString::number(state->stashes.size());
    s.message = message.isEmpty() ? QString("WIP on %1: %2").arg(state->currentBranch, QDateTime::currentDateTime().toString()) : message;
    s.branchName = state->currentBranch;
    s.timestamp = QDateTime::currentDateTime();
    s.files = state->changedFiles;

    state->stashes.prepend(s);
    state->changedFiles.clear();
    state->info.changedFilesCount = 0;

    emit stashesUpdated();
    emit changedFilesUpdated();
    emit operationSucceeded(QString("Stashed changes as '%1'").arg(s.message));
    return true;
}

bool MockGitService::popStash(const QString &stashId)
{
    auto *state = activeState();
    if (!state || state->stashes.isEmpty()) {
        emit operationFailed("No stashes to restore");
        return false;
    }

    int targetIndex = 0;
    if (!stashId.isEmpty()) {
        for (int i = 0; i < state->stashes.size(); ++i) {
            if (state->stashes[i].id == stashId) {
                targetIndex = i;
                break;
            }
        }
    }

    StashItem popped = state->stashes.takeAt(targetIndex);
    for (const auto &f : popped.files) {
        state->changedFiles.append(f);
    }
    state->info.changedFilesCount = state->changedFiles.size();

    emit stashesUpdated();
    emit changedFilesUpdated();
    emit operationSucceeded(QString("Restored stash '%1'").arg(popped.message));
    return true;
}

bool MockGitService::dropStash(const QString &stashId)
{
    auto *state = activeState();
    if (!state || state->stashes.isEmpty()) return false;

    for (int i = 0; i < state->stashes.size(); ++i) {
        if (state->stashes[i].id == stashId) {
            state->stashes.removeAt(i);
            emit stashesUpdated();
            emit operationSucceeded("Dropped stash");
            return true;
        }
    }
    return false;
}

} // namespace Cherry
