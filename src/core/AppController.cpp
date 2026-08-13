#include "AppController.h"
#include <QVariantMap>
#include <QFileDialog>
#include <QDesktopServices>
#include <QUrl>
#include <QProcess>
#include <QDir>
#include <QFileInfo>
#include <QRegularExpression>

namespace Cherry {

AppController::AppController(QObject *parent)
    : QObject(parent)
    , m_gitCliService(std::make_unique<GitCliService>())
    , m_mockService(std::make_unique<MockGitService>())
{
    // Default to real git, but isBackendDialogVisible is true on startup
    m_activeService = m_gitCliService.get();
    m_backendMode = "real";
    m_isBackendDialogVisible = true;

    m_repoModel = new RepositoryListModel(m_activeService, this);
    m_branchModel = new BranchListModel(m_activeService, this);
    m_changedFilesModel = new ChangedFilesModel(m_activeService, this);
    m_commitHistoryModel = new CommitHistoryModel(m_activeService, this);
    m_diffModel = new DiffModel(m_activeService, this);
    m_stashModel = new StashModel(m_activeService, this);

    connectServiceSignals();
    updateCurrentState();

    // Select initial file
    auto files = m_activeService->getChangedFiles();
    if (!files.isEmpty()) {
        setSelectedFilePath(files.first().filePath);
    }

    // Select initial commit
    auto commits = m_activeService->getCommitHistory(1);
    if (!commits.isEmpty()) {
        m_selectedCommitSha = commits.first().sha;
    }
}

AppController::~AppController() = default;

void AppController::setBackendMode(const QString &mode)
{
    QString normalized = (mode.toLower() == "mock") ? "mock" : "real";
    if (m_backendMode == normalized && m_activeService) return;

    m_backendMode = normalized;
    if (m_backendMode == "mock") {
        m_activeService = m_mockService.get();
    } else {
        m_activeService = m_gitCliService.get();
    }

    // Rebind all models to the newly active service
    m_repoModel->setService(m_activeService);
    m_branchModel->setService(m_activeService);
    m_changedFilesModel->setService(m_activeService);
    m_commitHistoryModel->setService(m_activeService);
    m_diffModel->setService(m_activeService);
    m_stashModel->setService(m_activeService);

    m_selectedFilePath.clear();
    m_selectedCommitSha.clear();
    m_selectedStashId.clear();
    emit selectedFilePathChanged();
    emit selectedCommitShaChanged();
    emit selectedStashIdChanged();

    connectServiceSignals();
    updateCurrentState();

    auto files = m_activeService->getChangedFiles();
    if (!files.isEmpty()) {
        setSelectedFilePath(files.first().filePath);
    } else {
        m_diffModel->clear();
    }

    auto commits = m_activeService->getCommitHistory(1);
    if (!commits.isEmpty()) {
        setSelectedCommitSha(commits.first().sha);
    }

    emit backendModeChanged();
}

void AppController::setBackendDialogVisible(bool visible)
{
    if (m_isBackendDialogVisible == visible) return;
    m_isBackendDialogVisible = visible;
    emit backendDialogVisibleChanged();
}

void AppController::showBackendSelectionDialog()
{
    setBackendDialogVisible(true);
}

void AppController::hideBackendSelectionDialog()
{
    setBackendDialogVisible(false);
}

void AppController::selectBackend(const QString &mode)
{
    setBackendMode(mode);
    hideBackendSelectionDialog();
    showToast(QString("Active Mode: %1").arg(m_backendMode == "mock" ? "Mock Demo" : "Real Git Backend"));
}

void AppController::connectServiceSignals()
{
    if (!m_activeService) return;

    disconnect(m_activeService, nullptr, this, nullptr);

    connect(m_activeService, &IGitService::repositoryChanged, this, [this]() {
        updateCurrentState();
        emit currentRepoChanged();
    });

    connect(m_activeService, &IGitService::currentBranchChanged, this, [this]() {
        emit currentBranchChanged();
    });

    connect(m_activeService, &IGitService::remoteStatusUpdated, this, [this](const RemoteStatus &status) {
        m_remoteStatus = status;
        m_commitHistoryModel->setAheadCount(m_remoteStatus.ahead);
        emit remoteStatusChanged();
    });

    connect(m_activeService, &IGitService::operationSucceeded, this, [this](const QString &msg) {
        showToast(msg, false);
    });

    connect(m_activeService, &IGitService::operationFailed, this, [this](const QString &msg) {
        showToast(msg, true);
    });

    connect(m_activeService, &IGitService::changedFilesUpdated, this, [this]() {
        if (!m_selectedFilePath.isEmpty()) {
            m_diffModel->loadDiffForFile(m_selectedFilePath);
        } else {
            auto files = m_activeService->getChangedFiles();
            if (!files.isEmpty()) {
                setSelectedFilePath(files.first().filePath);
            } else {
                m_diffModel->clear();
            }
        }
    });
}

void AppController::updateCurrentState()
{
    if (!m_activeService) return;

    m_remoteStatus = m_activeService->getRemoteStatus();
    m_commitHistoryModel->setAheadCount(m_remoteStatus.ahead);
    emit remoteStatusChanged();
    emit currentRepoChanged();
    emit currentBranchChanged();
    emit undoStateChanged();
}

QString AppController::currentRepoName() const
{
    if (!m_activeService) return "No repository";
    auto repo = m_activeService->getCurrentRepository();
    return repo ? repo->name : "No repository";
}

QString AppController::currentRepoPath() const
{
    if (!m_activeService) return "";
    auto repo = m_activeService->getCurrentRepository();
    return repo ? repo->path : "";
}

QString AppController::currentBranchName() const
{
    if (!m_activeService) return "main";
    auto b = m_activeService->getCurrentBranch();
    return b ? b->name : "main";
}

QString AppController::currentBranchPr() const
{
    if (!m_activeService) return "";
    auto b = m_activeService->getCurrentBranch();
    return b ? b->prNumber : "";
}

bool AppController::currentBranchPrActive() const
{
    if (!m_activeService) return false;
    auto b = m_activeService->getCurrentBranch();
    return b ? b->prMergedOrActive : false;
}

QString AppController::currentAuthorName() const
{
    if (!m_activeService) return "User";
    return m_activeService->getAuthorName();
}

QString AppController::currentAuthorInitial() const
{
    QString name = currentAuthorName().trimmed();
    if (name.isEmpty()) return "U";
    return name.left(1).toUpper();
}

void AppController::setActiveTab(const QString &tab)
{
    if (m_activeTab == tab) return;
    m_activeTab = tab;
    emit activeTabChanged();

    if (m_activeTab == "changes") {
        if (!m_selectedFilePath.isEmpty()) {
            m_diffModel->loadDiffForFile(m_selectedFilePath);
        }
    } else if (m_activeTab == "history") {
        if (!m_selectedCommitSha.isEmpty()) {
            selectCommit(m_selectedCommitSha);
        }
    }
}

void AppController::setSelectedFilePath(const QString &path)
{
    if (m_selectedFilePath == path) return;
    m_selectedFilePath = path;
    emit selectedFilePathChanged();
    m_diffModel->loadDiffForFile(m_selectedFilePath);
}

void AppController::setSelectedCommitSha(const QString &sha)
{
    if (m_selectedCommitSha == sha) return;
    m_selectedCommitSha = sha;
    emit selectedCommitShaChanged();
}

QVariant AppController::selectedCommitData() const
{
    if (!m_activeService) return QVariantMap();

    auto c = m_activeService->getCommitDetails(m_selectedCommitSha);
    if (!c) return QVariantMap();

    QVariantMap map;
    map["sha"] = c->sha;
    map["shortSha"] = c->shortSha;
    map["summary"] = c->summary;
    map["description"] = c->description;
    map["authorName"] = c->authorName;
    map["authorEmail"] = c->authorEmail;
    map["authorAvatarUrl"] = c->authorAvatarUrl;
    map["relativeTime"] = c->relativeTime;
    map["timestamp"] = c->timestamp.toString("yyyy-MM-dd hh:mm");
    map["coAuthors"] = c->coAuthors;

    QVariantList files;
    for (const auto &f : c->changedFiles) {
        QVariantMap fm;
        fm["filePath"] = f.filePath;
        fm["status"] = static_cast<int>(f.status);
        fm["additions"] = f.additions;
        fm["deletions"] = f.deletions;
        files.append(fm);
    }
    map["files"] = files;

    return map;
}

void AppController::setSelectedStashId(const QString &id)
{
    if (m_selectedStashId == id) return;
    m_selectedStashId = id;
    emit selectedStashIdChanged();
}

QVariant AppController::selectedStashData() const
{
    if (!m_activeService) return QVariantMap();

    auto s = m_activeService->getStashDetails(m_selectedStashId);
    if (!s) {
        auto stashes = m_activeService->getStashes();
        if (!stashes.isEmpty()) {
            s = stashes.first();
        } else {
            return QVariantMap();
        }
    }

    QVariantMap map;
    map["stashId"] = s->id;
    map["message"] = s->message;
    map["branchName"] = s->branchName;
    map["timestamp"] = s->timestamp.toString("yyyy-MM-dd hh:mm");

    QVariantList files;
    for (const auto &f : s->files) {
        QVariantMap fm;
        fm["filePath"] = f.filePath;
        fm["status"] = static_cast<int>(f.status);
        fm["statusText"] = (f.status == FileChangeType::Added) ? "Added" : ((f.status == FileChangeType::Deleted) ? "Deleted" : "Modified");
        fm["additions"] = f.additions;
        fm["deletions"] = f.deletions;
        files.append(fm);
    }
    map["files"] = files;
    return map;
}

void AppController::setDiffViewMode(const QString &mode)
{
    if (m_diffViewMode == mode) return;
    m_diffViewMode = mode;
    emit diffViewModeChanged();
}

void AppController::setShowWhitespace(bool show)
{
    if (m_showWhitespace == show) return;
    m_showWhitespace = show;
    emit showWhitespaceChanged();
}

bool AppController::hasUndoCommit() const
{
    return m_activeService ? m_activeService->hasUndoCommit() : false;
}

QString AppController::lastUndoCommitSummary() const
{
    return m_activeService ? m_activeService->getLastUndoCommitSummary() : QString();
}

QString AppController::lastUndoCommitDescription() const
{
    return m_activeService ? m_activeService->getLastUndoCommitDescription() : QString();
}

void AppController::openLocalRepositoryDialog()
{
    QString dir = QFileDialog::getExistingDirectory(
        nullptr,
        tr("Select Git Repository Directory"),
        QDir::homePath(),
        QFileDialog::ShowDirsOnly | QFileDialog::DontResolveSymlinks
    );

    if (!dir.isEmpty()) {
        addRepository(QFileInfo(dir).fileName(), dir);
    }
}

void AppController::switchRepository(const QString &repoIdOrPath)
{
    if (!m_activeService) return;
    m_activeService->openRepository(repoIdOrPath);
    updateCurrentState();
    auto files = m_activeService->getChangedFiles();
    if (!files.isEmpty()) {
        setSelectedFilePath(files.first().filePath);
    } else {
        m_diffModel->clear();
    }
    auto commits = m_activeService->getCommitHistory(1);
    if (!commits.isEmpty()) {
        setSelectedCommitSha(commits.first().sha);
    }
}

void AppController::addRepository(const QString &name, const QString &path)
{
    if (!m_activeService) return;
    m_activeService->addRepository(name, path);
    updateCurrentState();
}

void AppController::removeRepository(const QString &repoIdOrPath)
{
    if (!m_activeService) return;
    m_activeService->removeRepository(repoIdOrPath);
    updateCurrentState();
}

void AppController::switchBranch(const QString &branchName)
{
    if (!m_activeService) return;
    m_activeService->switchBranch(branchName);
    updateCurrentState();
}

void AppController::createBranch(const QString &branchName)
{
    if (!m_activeService) return;
    m_activeService->createBranch(branchName);
    updateCurrentState();
}

void AppController::deleteBranch(const QString &branchName)
{
    if (!m_activeService) return;
    m_activeService->deleteBranch(branchName);
    updateCurrentState();
}

bool AppController::commit(const QString &summary, const QString &description, const QStringList &coAuthors)
{
    if (!m_activeService) return false;
    bool res = m_activeService->createCommit(summary, description, coAuthors);
    if (res) {
        emit undoStateChanged();
        emit remoteStatusChanged();
        auto files = m_activeService->getChangedFiles();
        if (!files.isEmpty()) {
            setSelectedFilePath(files.first().filePath);
        } else {
            m_selectedFilePath.clear();
            emit selectedFilePathChanged();
            m_diffModel->clear();
        }
    }
    return res;
}

bool AppController::undoLastCommit()
{
    if (!m_activeService) return false;
    bool res = m_activeService->undoLastCommit();
    if (res) {
        emit undoStateChanged();
        emit remoteStatusChanged();
        auto files = m_activeService->getChangedFiles();
        if (!files.isEmpty()) {
            setSelectedFilePath(files.first().filePath);
        }
    }
    return res;
}

void AppController::fetchOrigin()
{
    if (m_activeService) m_activeService->fetchOrigin();
}

void AppController::pullOrigin()
{
    if (m_activeService) m_activeService->pullOrigin();
}

void AppController::pushOrigin()
{
    if (m_activeService) m_activeService->pushOrigin();
}

void AppController::discardFileChanges(const QString &filePath)
{
    if (m_activeService) m_activeService->discardFileChanges(filePath);
}

void AppController::discardAllChanges()
{
    if (m_activeService) m_activeService->discardAllChanges();
}

void AppController::stashChanges(const QString &message)
{
    if (m_activeService) m_activeService->stashChanges(message);
}

void AppController::popStash(const QString &stashId)
{
    if (!m_activeService) return;
    QString id = stashId.isEmpty() ? m_selectedStashId : stashId;
    m_activeService->popStash(id);
    clearStashSelection();
}

void AppController::dropStash(const QString &stashId)
{
    if (!m_activeService) return;
    QString id = stashId.isEmpty() ? m_selectedStashId : stashId;
    m_activeService->dropStash(id);
    clearStashSelection();
}

void AppController::selectFileForDiff(const QString &filePath)
{
    if (!m_selectedStashId.isEmpty()) {
        m_selectedStashId.clear();
        emit selectedStashIdChanged();
    }
    setSelectedFilePath(filePath);
    m_diffModel->loadDiffForFile(filePath);
}

void AppController::selectStash(const QString &stashId)
{
    if (!m_activeService) return;
    QString id = stashId;
    if (id.isEmpty()) {
        auto stashes = m_activeService->getStashes();
        if (!stashes.isEmpty()) id = stashes.first().id;
    }
    setSelectedStashId(id);
    auto s = m_activeService->getStashDetails(id);
    if (s && !s->files.isEmpty()) {
        m_diffModel->loadDiffForStash(id, s->files.first().filePath);
    }
}

void AppController::clearStashSelection()
{
    if (m_selectedStashId.isEmpty()) return;
    m_selectedStashId.clear();
    emit selectedStashIdChanged();
    if (!m_selectedFilePath.isEmpty()) {
        m_diffModel->loadDiffForFile(m_selectedFilePath);
    }
}

void AppController::selectCommit(const QString &sha)
{
    if (!m_activeService) return;
    if (!m_selectedStashId.isEmpty()) {
        m_selectedStashId.clear();
        emit selectedStashIdChanged();
    }
    setSelectedCommitSha(sha);
    auto details = m_activeService->getCommitDetails(sha);
    if (details && !details->changedFiles.isEmpty()) {
        m_diffModel->loadDiffForCommit(sha, details->changedFiles.first().filePath);
    }
}

void AppController::revertCommit(const QString &sha)
{
    if (m_activeService) m_activeService->revertCommit(sha);
}

void AppController::openInTerminal(const QString &path)
{
    QString targetDir = path.isEmpty() ? currentRepoPath() : path;
    if (targetDir.isEmpty() || !QDir(targetDir).exists()) {
        showToast("No active repository directory to open", true);
        return;
    }

    // Try konsole first, then default terminal fallback
    bool started = QProcess::startDetached("konsole", {"--workdir", targetDir});
    if (!started) {
        started = QProcess::startDetached("x-terminal-emulator", {"--working-directory", targetDir});
    }
    if (!started) {
        started = QProcess::startDetached("ptyxis", {"--working-directory", targetDir});
    }

    if (started) {
        showToast(QString("Opened terminal in %1").arg(QFileInfo(targetDir).fileName()));
    } else {
        showToast("Could not launch terminal emulator", true);
    }
}

void AppController::openInFileManager(const QString &path)
{
    QString targetDir = path.isEmpty() ? currentRepoPath() : path;
    if (targetDir.isEmpty() || !QDir(targetDir).exists()) {
        showToast("No active repository directory to open", true);
        return;
    }

    QDesktopServices::openUrl(QUrl::fromLocalFile(targetDir));
    showToast(QString("Opened file manager in %1").arg(QFileInfo(targetDir).fileName()));
}

void AppController::openOnGitHub()
{
    QString url;
    if (m_backendMode == "real" && m_gitCliService) {
        url = m_gitCliService->getRemoteUrl();
    }

    if (url.isEmpty()) {
        showToast("No remote repository URL found", true);
        return;
    }

    // Convert SSH URL to HTTPS URL: git@github.com:user/repo.git -> https://github.com/user/repo
    if (url.startsWith("git@")) {
        url.remove(0, 4);
        url.replace(':', '/');
        url = "https://" + url;
    }
    if (url.endsWith(".git")) {
        url.chop(4);
    }

    QDesktopServices::openUrl(QUrl(url));
    showToast(QString("Opening %1 in browser...").arg(url));
}

void AppController::createPullRequest()
{
    QString url;
    if (m_backendMode == "real" && m_gitCliService) {
        url = m_gitCliService->getRemoteUrl();
    }

    if (url.isEmpty()) {
        showToast("No remote repository URL found", true);
        return;
    }

    if (url.startsWith("git@")) {
        url.remove(0, 4);
        url.replace(':', '/');
        url = "https://" + url;
    }
    if (url.endsWith(".git")) {
        url.chop(4);
    }

    QString branch = currentBranchName();
    QString prUrl = QString("%1/pull/new/%2").arg(url, branch);

    QDesktopServices::openUrl(QUrl(prUrl));
    showToast(QString("Opening Pull Request in browser..."));
}

void AppController::hideToast()
{
    m_toastVisible = false;
    emit toastChanged();
}

void AppController::showToast(const QString &message, bool isError)
{
    m_toastMessage = message;
    m_toastIsError = isError;
    m_toastVisible = true;
    emit toastChanged();
}

} // namespace Cherry
