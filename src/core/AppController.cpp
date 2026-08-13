#include "AppController.h"
#include <QVariantMap>

namespace Cherry {

AppController::AppController(QObject *parent)
    : QObject(parent)
    , m_service(std::make_unique<MockGitService>())
{
    m_repoModel = new RepositoryListModel(m_service.get(), this);
    m_branchModel = new BranchListModel(m_service.get(), this);
    m_changedFilesModel = new ChangedFilesModel(m_service.get(), this);
    m_commitHistoryModel = new CommitHistoryModel(m_service.get(), this);
    m_diffModel = new DiffModel(m_service.get(), this);
    m_stashModel = new StashModel(m_service.get(), this);

    connectServiceSignals();
    updateCurrentState();

    // Select initial file
    auto files = m_service->getChangedFiles();
    if (!files.isEmpty()) {
        setSelectedFilePath(files.first().filePath);
    }

    // Select initial commit
    auto commits = m_service->getCommitHistory(1);
    if (!commits.isEmpty()) {
        m_selectedCommitSha = commits.first().sha;
    }
}

AppController::~AppController() = default;

void AppController::connectServiceSignals()
{
    connect(m_service.get(), &IGitService::repositoryChanged, this, [this]() {
        updateCurrentState();
        emit currentRepoChanged();
    });

    connect(m_service.get(), &IGitService::currentBranchChanged, this, [this]() {
        emit currentBranchChanged();
    });

    connect(m_service.get(), &IGitService::remoteStatusUpdated, this, [this](const RemoteStatus &status) {
        m_remoteStatus = status;
        emit remoteStatusChanged();
    });

    connect(m_service.get(), &IGitService::operationSucceeded, this, [this](const QString &msg) {
        showToast(msg, false);
    });

    connect(m_service.get(), &IGitService::operationFailed, this, [this](const QString &msg) {
        showToast(msg, true);
    });

    connect(m_service.get(), &IGitService::changedFilesUpdated, this, [this]() {
        if (!m_selectedFilePath.isEmpty()) {
            m_diffModel->loadDiffForFile(m_selectedFilePath);
        } else {
            auto files = m_service->getChangedFiles();
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
    m_remoteStatus = m_service->getRemoteStatus();
    emit remoteStatusChanged();
    emit currentRepoChanged();
    emit currentBranchChanged();
    emit undoStateChanged();
}

QString AppController::currentRepoName() const
{
    auto repo = m_service->getCurrentRepository();
    return repo ? repo->name : "No repository";
}

QString AppController::currentRepoPath() const
{
    auto repo = m_service->getCurrentRepository();
    return repo ? repo->path : "";
}

QString AppController::currentBranchName() const
{
    auto b = m_service->getCurrentBranch();
    return b ? b->name : "main";
}

QString AppController::currentBranchPr() const
{
    auto b = m_service->getCurrentBranch();
    return b ? b->prNumber : "";
}

bool AppController::currentBranchPrActive() const
{
    auto b = m_service->getCurrentBranch();
    return b ? b->prMergedOrActive : false;
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
    auto c = m_service->getCommitDetails(m_selectedCommitSha);
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
    auto s = m_service->getStashDetails(m_selectedStashId);
    if (!s) {
        auto stashes = m_service->getStashes();
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
    return m_service->hasUndoCommit();
}

QString AppController::lastUndoCommitSummary() const
{
    if (!m_service->hasUndoCommit()) return QString();
    return m_service->getLastUndoSnapshot().commit.summary;
}

QString AppController::lastUndoCommitDescription() const
{
    if (!m_service->hasUndoCommit()) return QString();
    return m_service->getLastUndoSnapshot().commit.description;
}

void AppController::switchRepository(const QString &repoIdOrPath)
{
    m_service->openRepository(repoIdOrPath);
    updateCurrentState();
    auto files = m_service->getChangedFiles();
    if (!files.isEmpty()) {
        setSelectedFilePath(files.first().filePath);
    } else {
        m_diffModel->clear();
    }
    auto commits = m_service->getCommitHistory(1);
    if (!commits.isEmpty()) {
        setSelectedCommitSha(commits.first().sha);
    }
}

void AppController::addRepository(const QString &name, const QString &path)
{
    m_service->addRepository(name, path);
    updateCurrentState();
}

void AppController::switchBranch(const QString &branchName)
{
    m_service->switchBranch(branchName);
    updateCurrentState();
}

void AppController::createBranch(const QString &branchName)
{
    m_service->createBranch(branchName);
    updateCurrentState();
}

void AppController::deleteBranch(const QString &branchName)
{
    m_service->deleteBranch(branchName);
    updateCurrentState();
}

bool AppController::commit(const QString &summary, const QString &description, const QStringList &coAuthors)
{
    bool res = m_service->createCommit(summary, description, coAuthors);
    if (res) {
        emit undoStateChanged();
        emit remoteStatusChanged();
        auto files = m_service->getChangedFiles();
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
    bool res = m_service->undoLastCommit();
    if (res) {
        emit undoStateChanged();
        emit remoteStatusChanged();
        auto files = m_service->getChangedFiles();
        if (!files.isEmpty()) {
            setSelectedFilePath(files.first().filePath);
        }
    }
    return res;
}

void AppController::fetchOrigin()
{
    m_service->fetchOrigin();
}

void AppController::pullOrigin()
{
    m_service->pullOrigin();
}

void AppController::pushOrigin()
{
    m_service->pushOrigin();
}

void AppController::discardFileChanges(const QString &filePath)
{
    m_service->discardFileChanges(filePath);
}

void AppController::discardAllChanges()
{
    m_service->discardAllChanges();
}

void AppController::stashChanges(const QString &message)
{
    m_service->stashChanges(message);
}

void AppController::popStash(const QString &stashId)
{
    QString id = stashId.isEmpty() ? m_selectedStashId : stashId;
    m_service->popStash(id);
    clearStashSelection();
}

void AppController::dropStash(const QString &stashId)
{
    QString id = stashId.isEmpty() ? m_selectedStashId : stashId;
    m_service->dropStash(id);
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
    QString id = stashId;
    if (id.isEmpty()) {
        auto stashes = m_service->getStashes();
        if (!stashes.isEmpty()) id = stashes.first().id;
    }
    setSelectedStashId(id);
    auto s = m_service->getStashDetails(id);
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
    if (!m_selectedStashId.isEmpty()) {
        m_selectedStashId.clear();
        emit selectedStashIdChanged();
    }
    setSelectedCommitSha(sha);
    auto details = m_service->getCommitDetails(sha);
    if (details && !details->changedFiles.isEmpty()) {
        m_diffModel->loadDiffForCommit(sha, details->changedFiles.first().filePath);
    }
}

void AppController::revertCommit(const QString &sha)
{
    m_service->revertCommit(sha);
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
