#include "AppController.h"
#include "AvatarResolver.h"
#include <QVariantMap>
#include <QFileDialog>
#include <QDesktopServices>
#include <QUrl>
#include <QProcess>
#include <QDir>
#include <QFileInfo>
#include <QRegularExpression>
#include <QGuiApplication>
#include <QPointer>
#include <QThread>

namespace Cherry {

AppController::AppController(QObject *parent)
    : QObject(parent)
    , m_settings(std::make_unique<AppSettings>(this))
    , m_githubAvatarService(std::make_unique<GitHubAvatarService>())
    , m_aiCommitService(std::make_unique<AiCommitService>(this))
    , m_gitCliService(std::make_unique<GitCliService>())
    , m_mockService(std::make_unique<MockGitService>())
{
    // Default to real git, but isBackendDialogVisible is true on startup
    m_activeService = m_gitCliService.get();
    m_backendMode = m_settings->startupBackend() == "mock" ? "mock" : "real";
    m_isBackendDialogVisible = (m_settings->startupBackend() == "ask");
    if (m_backendMode == "mock") {
        m_activeService = m_mockService.get();
    }

    m_diffViewMode = m_settings->diffViewMode();
    m_showWhitespace = m_settings->showWhitespace();

    m_repoModel = new RepositoryListModel(m_activeService, this);
    m_branchModel = new BranchListModel(m_activeService, this);
    m_changedFilesModel = new ChangedFilesModel(m_activeService, this);
    m_commitHistoryModel = new CommitHistoryModel(m_activeService, this);
    m_diffModel = new DiffModel(m_activeService, this);
    m_stashModel = new StashModel(m_activeService, this);

    connect(m_githubAvatarService.get(), &GitHubAvatarService::avatarsChanged, this, [this]() {
        if (m_commitHistoryModel) {
            m_commitHistoryModel->setAvatarOverrides(m_githubAvatarService->avatarOverrides());
        }
        m_cachedCommitData.clear();
        m_cachedCommitDataSha.clear();
        emit currentRepoChanged();
        emit selectedCommitShaChanged();
    });

    connect(m_aiCommitService.get(), &AiCommitService::generationStarted, this, [this]() {
        m_isAiGenerating = true;
        emit aiGeneratingChanged();
        emit operatingStateChanged();
    });

    connect(m_aiCommitService.get(), &AiCommitService::generationFinished, this, [this]() {
        m_isAiGenerating = false;
        emit aiGeneratingChanged();
        emit operatingStateChanged();
    });

    connect(m_aiCommitService.get(), &AiCommitService::commitMessageGenerated, this, [this](const QString &summary, const QString &description) {
        emit aiCommitMessageReady(summary, description);
        showToast(tr("Commit message generated with AI"));
    });

    connect(m_aiCommitService.get(), &AiCommitService::generationFailed, this, [this](const QString &errorMessage) {
        showToast(tr("AI generation failed: %1").arg(errorMessage), true);
    });

    connectServiceSignals();
    updateCurrentState();

    if (m_settings) {
        connect(m_settings.get(), &AppSettings::avatarProviderChanged, this, [this]() {
            m_cachedCommitData.clear();
            m_cachedCommitDataSha.clear();
            emit currentRepoChanged();
            if (m_commitHistoryModel) {
                m_commitHistoryModel->reload();
            }
            emit selectedCommitShaChanged();
            emit avatarProviderChanged();
        });
        connect(m_settings.get(), &AppSettings::aiEnabledChanged, this, &AppController::aiSettingsChanged);
        connect(m_settings.get(), &AppSettings::aiProviderChanged, this, &AppController::aiSettingsChanged);
        connect(m_settings.get(), &AppSettings::aiApiKeyChanged, this, &AppController::aiSettingsChanged);
        connect(m_settings.get(), &AppSettings::aiEndpointChanged, this, &AppController::aiSettingsChanged);
        connect(m_settings.get(), &AppSettings::aiModelChanged, this, &AppController::aiSettingsChanged);
        connect(m_settings.get(), &AppSettings::aiCommitStyleChanged, this, &AppController::aiSettingsChanged);
        connect(m_settings.get(), &AppSettings::aiIncludeDescriptionChanged, this, &AppController::aiSettingsChanged);
        connect(m_settings.get(), &AppSettings::aiFollowRepoStyleChanged, this, &AppController::aiSettingsChanged);
        connect(m_settings.get(), &AppSettings::aiFirstRunConfiguredChanged, this, &AppController::aiSettingsChanged);
    }

    // Auto-refresh when app enters foreground / becomes active
    connect(qApp, &QGuiApplication::applicationStateChanged, this, [this](Qt::ApplicationState state) {
        if (state == Qt::ApplicationActive) {
            refresh();
        }
    });

    // Ensure all running git and helper processes are terminated when quitting
    connect(qApp, &QCoreApplication::aboutToQuit, this, [this]() {
        cancelAllOperations();
    });

    // Select initial file
    auto files = m_activeService->getChangedFiles();
    if (!files.isEmpty()) {
        setSelectedFilePath(files.first().filePath);
    } else {
        m_diffModel->clear();
    }

    // Select initial commit
    auto commits = m_activeService->getCommitHistory(1);
    if (!commits.isEmpty()) {
        m_selectedCommitSha = commits.first().sha;
    }
}

void AppController::cancelAllOperations()
{
    m_currentLoadSequence.fetch_add(1);
    if (m_diffModel) m_diffModel->cancelOperations();
    if (m_gitCliService) m_gitCliService->cancelOperations();
    if (m_githubAvatarService) m_githubAvatarService->cancelOperations();
    if (m_aiCommitService) m_aiCommitService->cancel();
}

AppController::~AppController()
{
    cancelAllOperations();

    QList<QThread *> workers;
    {
        QMutexLocker locker(&m_workerMutex);
        workers = m_workerThreads;
    }
    for (QThread *worker : workers) {
        if (worker && worker->isRunning()) worker->wait();
    }

    {
        QMutexLocker locker(&m_workerMutex);
        workers = m_workerThreads;
        m_workerThreads.clear();
    }
    for (QThread *worker : workers) {
        delete worker;
    }
}

QThread *AppController::trackWorker(QThread *thread)
{
    if (!thread) return nullptr;
    {
        QMutexLocker locker(&m_workerMutex);
        m_workerThreads.append(thread);
    }
    connect(thread, &QThread::finished, this, [this, thread]() {
        QMutexLocker locker(&m_workerMutex);
        m_workerThreads.removeAll(thread);
        thread->deleteLater();
    });
    return thread;
}

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
}

void AppController::connectServiceSignals()
{
    if (!m_activeService) return;

    disconnect(m_activeService, nullptr, this, nullptr);

    connect(m_activeService, &IGitService::repositoryChanged, this, [this](const RepositoryInfo &) {
        if (m_repositoryLoadWorkerActive) return;
        updateCurrentState();
        emit currentRepoChanged();
    });

    connect(m_activeService, &IGitService::currentBranchChanged, this, [this](const BranchInfo &) {
        if (m_repositoryLoadWorkerActive) return;
        emit currentBranchChanged();
    });

    connect(m_activeService, &IGitService::remoteStatusUpdated, this, [this](const RemoteStatus &status) {
        if (m_repositoryLoadWorkerActive) return;
        m_remoteStatus = status;
        m_commitHistoryModel->setAheadCount(m_remoteStatus.ahead);
        emit remoteStatusChanged();
        emit operatingStateChanged();
        emit undoStateChanged();
    });

    connect(m_activeService, &IGitService::cloneProgressUpdated, this, [this](double progress, const QString &status, const QString &details) {
        m_cloneProgress = progress;
        m_cloneProgressMessage = status;
        m_cloneProgressDetails = details;
        emit cloneProgressChanged();
        emit cloneProgressMessageChanged();
        emit cloneProgressDetailsChanged();
        emit operatingStateChanged();
    });

    connect(m_activeService, &IGitService::operationSucceeded, this, [this](const QString &msg) {
        if (m_isPublishing) {
            m_isPublishing = false;
            emit isPublishingChanged();
            emit operatingStateChanged();
            hidePublishDialog();
        }
        qInfo().noquote() << QString("[Operation Succeeded] %1").arg(msg);
        showToast(msg, false);
    });

    connect(m_activeService, &IGitService::operationFailed, this, [this](const QString &msg) {
        if (m_isPublishing) {
            m_isPublishing = false;
            m_publishErrorMessage = msg;
            emit isPublishingChanged();
            emit publishErrorMessageChanged();
            emit operatingStateChanged();
        }
        qWarning().noquote() << QString("[Operation Failed] %1").arg(msg);
        showToast(msg, true);
    });

    connect(m_activeService, &IGitService::changedFilesUpdated, this, [this]() {
        if (m_repositoryLoadWorkerActive) return;
        auto files = m_activeService->getChangedFiles();
        if (files.isEmpty()) {
            m_selectedFilePath.clear();
            emit selectedFilePathChanged();
            m_diffModel->clear();
        } else {
            bool found = false;
            for (const auto &f : files) {
                if (f.filePath == m_selectedFilePath) {
                    found = true;
                    break;
                }
            }
            if (found) {
                m_diffModel->loadDiffForFile(m_selectedFilePath);
            } else {
                setSelectedFilePath(files.first().filePath);
            }
        }
        emit undoStateChanged();
    });

    connect(m_activeService, &IGitService::commitHistoryUpdated, this, [this]() {
        if (m_repositoryLoadWorkerActive) return;
        emit undoStateChanged();
        auto commits = m_activeService->getCommitHistory(1);
        if (!commits.isEmpty() && m_selectedCommitSha.isEmpty()) {
            setSelectedCommitSha(commits.first().sha);
        }
    });
}

void AppController::updateCurrentState()
{
    if (!m_activeService) return;

    auto repo = m_activeService->getCurrentRepository();
    if (repo && repo->isMissing) {
        m_isCurrentRepoMissing = true;
        m_missingRepoPath = repo->path;
        m_missingRepoName = repo->name;
        m_missingRepoRemoteUrl = repo->remoteUrl;
        m_selectedFilePath.clear();
        m_selectedCommitSha.clear();
        m_diffModel->clear();
        emit selectedFilePathChanged();
        emit selectedCommitShaChanged();
    } else {
        m_isCurrentRepoMissing = false;
        m_missingRepoPath.clear();
        m_missingRepoName.clear();
        m_missingRepoRemoteUrl.clear();
        m_remoteStatus = m_activeService->getRemoteStatus();
        m_commitHistoryModel->setAheadCount(m_remoteStatus.ahead);
    }

    if (m_githubAvatarService && !m_remoteStatus.remoteUrl.isEmpty()) {
        m_githubAvatarService->fetchForRemote(m_remoteStatus.remoteUrl);
    }

    emit remoteStatusChanged();
    emit currentRepoChanged();
    emit currentBranchChanged();
    emit undoStateChanged();
    emit authorInfoChanged();
    emit gitConfigChanged();
}

bool AppController::isGitHubRemote() const
{
    static const QRegularExpression githubHostRegex(R"((^|[@/:])github\.com([/:]|$))", QRegularExpression::CaseInsensitiveOption);
    return githubHostRegex.match(m_remoteStatus.remoteUrl.trimmed()).hasMatch();
}

QString AppController::remoteName() const
{
    return m_remoteStatus.hasRemote ? m_remoteStatus.remoteName : QString();
}

QString AppController::remoteHost() const
{
    const QString url = m_remoteStatus.remoteUrl.trimmed();
    if (url.isEmpty()) return QString();

    // Git's scp-style SSH syntax is not parsed as a normal QUrl.
    static const QRegularExpression scpStyleRegex(R"(^[^/@\s]+@([^:/\s]+):)");
    const auto scpMatch = scpStyleRegex.match(url);
    if (scpMatch.hasMatch()) return scpMatch.captured(1);

    const QString host = QUrl(url).host();
    return host.isEmpty() ? QStringLiteral("local path") : host;
}

QString AppController::remoteProvider() const
{
    const QString host = remoteHost().toLower();
    if (host == QStringLiteral("github.com")) return QStringLiteral("GitHub");
    if (host == QStringLiteral("gitlab.com")) return QStringLiteral("GitLab");
    if (host == QStringLiteral("invent.kde.org")) return QStringLiteral("KDE Invent");
    return m_remoteStatus.hasRemote ? QStringLiteral("Other Git remote") : QString();
}

QString AppController::remoteProviderIcon() const
{
    const QString provider = remoteProvider();
    if (provider == QStringLiteral("GitHub")) return QStringLiteral("qrc:/icons/github-mark.svg");
    if (provider == QStringLiteral("GitLab")) return QStringLiteral("vcs-merge-request");
    if (provider == QStringLiteral("KDE Invent")) return QStringLiteral("folder-git");
    return QStringLiteral("network-server");
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

QString AppController::currentAuthorEmail() const
{
    if (!m_activeService) return "";
    return m_activeService->getAuthorEmail();
}

QString AppController::currentAuthorInitial() const
{
    QString name = currentAuthorName().trimmed();
    if (name.isEmpty()) return "U";
    return name.left(1).toUpper();
}

QString AppController::avatarForAuthor(const QString &authorName, const QString &authorEmail) const
{
    if (m_commitHistoryModel) {
        return m_commitHistoryModel->avatarForAuthor(authorName, authorEmail);
    }
    if (m_githubAvatarService) {
        const QString avatar = m_githubAvatarService->avatarFor(authorName, authorEmail);
        if (!avatar.isEmpty()) return avatar;
    }
    const bool sameEmail = !currentAuthorEmail().isEmpty() &&
        authorEmail.compare(currentAuthorEmail(), Qt::CaseInsensitive) == 0;
    const bool sameName = !currentAuthorName().isEmpty() &&
        authorName.compare(currentAuthorName(), Qt::CaseSensitive) == 0;
    if ((sameEmail || sameName) && !m_remoteStatus.remoteUrl.isEmpty()) {
        return AvatarResolver::resolve(authorName, authorEmail, m_remoteStatus.remoteUrl);
    }
    return AvatarResolver::resolve(authorName, authorEmail);
}

QString AppController::currentAuthorAvatarUrl() const
{
    return avatarForAuthor(currentAuthorName(), currentAuthorEmail());
}

QString AppController::avatarProvider() const
{
    return m_settings ? m_settings->avatarProvider() : "auto";
}

bool AppController::isGhAvailable() const
{
    return m_settings ? m_settings->isGhAvailable() : false;
}

QString AppController::operationMessage() const
{
    if (m_isCloning) {
        if (!m_cloneProgressMessage.isEmpty()) {
            if (!m_cloneProgressDetails.isEmpty()) {
                return QString("%1 • %2").arg(m_cloneProgressMessage, m_cloneProgressDetails);
            }
            return m_cloneProgressMessage;
        }
        return tr("Cloning repository...");
    }
    if (m_isLoadingRepository) {
        return m_loadingRepositoryMessage.isEmpty() ? tr("Loading repository...") : m_loadingRepositoryMessage;
    }
    if (m_isPublishing) return tr("Publishing repository to GitHub...");
    if (isPushing()) {
        return aheadCount() > 0 ? (aheadCount() == 1 ? tr("Pushing 1 commit to origin...") : tr("Pushing %1 commits to origin...").arg(aheadCount())) : tr("Pushing to origin...");
    }
    if (isPulling()) {
        return behindCount() > 0 ? (behindCount() == 1 ? tr("Pulling 1 commit from origin...") : tr("Pulling %1 commits from origin...").arg(behindCount())) : tr("Pulling from origin...");
    }
    if (isFetching()) return tr("Fetching latest changes from origin...");
    if (m_isCommitting) return tr("Committing changes...");
    if (m_isDiscarding) return tr("Discarding changes...");
    if (m_isStashing) return tr("Managing stash...");
    if (m_isReverting) return tr("Reverting commit...");
    return QString();
}

void AppController::setActiveTab(const QString &tab)
{
    if (m_activeTab == tab) return;
    m_activeTab = tab;
    emit activeTabChanged();

    if (m_activeTab == "changes") {
        if (!m_selectedFilePath.isEmpty()) {
            m_diffModel->loadDiffForFile(m_selectedFilePath);
        } else {
            m_diffModel->clear();
        }
    } else if (m_activeTab == "history") {
        if (!m_selectedCommitSha.isEmpty()) {
            selectCommit(m_selectedCommitSha);
        } else {
            auto commits = m_activeService->getCommitHistory(1);
            if (!commits.isEmpty()) {
                selectCommit(commits.first().sha);
            } else {
                m_diffModel->clear();
            }
        }
    }
}

void AppController::setSelectedFilePath(const QString &path)
{
    if (m_selectedFilePath == path) return;
    m_selectedFilePath = path;
    emit selectedFilePathChanged();

    QString oldPath;
    if (m_activeService) {
        for (const auto &f : m_activeService->getChangedFiles()) {
            if (f.filePath == path) {
                oldPath = f.oldFilePath;
                break;
            }
        }
    }
    m_diffModel->loadDiffForFile(m_selectedFilePath, oldPath);
}

void AppController::setSelectedCommitSha(const QString &sha)
{
    if (m_selectedCommitSha == sha) return;
    m_selectedCommitSha = sha;
    m_cachedCommitData.clear();
    m_cachedCommitDataSha.clear();
    emit selectedCommitShaChanged();
}

QVariant AppController::selectedCommitData() const
{
    if (!m_activeService || m_selectedCommitSha.isEmpty()) return QVariantMap();

    if (m_cachedCommitDataSha == m_selectedCommitSha && !m_cachedCommitData.isEmpty()) {
        return m_cachedCommitData;
    }

    auto c = m_activeService->getCommitDetails(m_selectedCommitSha);
    if (!c) return QVariantMap();

    QVariantMap map;
    map["sha"] = c->sha;
    map["shortSha"] = c->shortSha;
    map["summary"] = c->summary;
    map["description"] = c->description;
    map["authorName"] = c->authorName;
    map["authorEmail"] = c->authorEmail;
    map["authorAvatarUrl"] = avatarForAuthor(c->authorName, c->authorEmail);
    map["relativeTime"] = c->relativeTime;
    map["timestamp"] = c->timestamp.toString("yyyy-MM-dd hh:mm");
    map["coAuthors"] = c->coAuthors;
    map["tags"] = c->tags;

    QVariantList files;
    for (const auto &f : c->changedFiles) {
        QVariantMap fm;
        fm["filePath"] = f.filePath;
        fm["oldFilePath"] = f.oldFilePath;
        fm["status"] = static_cast<int>(f.status);
        fm["additions"] = f.additions;
        fm["deletions"] = f.deletions;
        files.append(fm);
    }
    map["files"] = files;

    m_cachedCommitData = map;
    m_cachedCommitDataSha = m_selectedCommitSha;

    return map;
}

void AppController::setSelectedStashId(const QString &id)
{
    if (m_selectedStashId == id) return;
    m_selectedStashId = id;
    m_cachedStashData.clear();
    m_cachedStashDataId.clear();
    emit selectedStashIdChanged();
}

QVariant AppController::selectedStashData() const
{
    if (!m_activeService) return QVariantMap();

    if (m_cachedStashDataId == m_selectedStashId && !m_cachedStashData.isEmpty()) {
        return m_cachedStashData;
    }

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
        fm["oldFilePath"] = f.oldFilePath;
        fm["status"] = static_cast<int>(f.status);
        fm["statusText"] = (f.status == FileChangeType::Added) ? "Added" : ((f.status == FileChangeType::Deleted) ? "Deleted" : (f.status == FileChangeType::Renamed ? "Renamed" : (f.status == FileChangeType::Untracked ? "Untracked" : "Modified")));
        fm["additions"] = f.additions;
        fm["deletions"] = f.deletions;
        files.append(fm);
    }
    map["files"] = files;

    m_cachedStashData = map;
    m_cachedStashDataId = m_selectedStashId;

    return map;
}

void AppController::setDiffViewMode(const QString &mode)
{
    if (m_diffViewMode == mode) return;
    m_diffViewMode = mode;
    if (m_settings) m_settings->setDiffViewMode(mode);
    emit diffViewModeChanged();
}

void AppController::setShowWhitespace(bool show)
{
    if (m_showWhitespace == show) return;
    m_showWhitespace = show;
    if (m_settings) m_settings->setShowWhitespace(show);
    emit showWhitespaceChanged();
}

bool AppController::canUndoCommit() const
{
    return m_activeService ? m_activeService->canUndoCommit() : false;
}

QString AppController::lastUndoCommitSummary() const
{
    return m_activeService ? m_activeService->getLastUndoCommitSummary() : QString();
}

QString AppController::lastUndoCommitDescription() const
{
    return m_activeService ? m_activeService->getLastUndoCommitDescription() : QString();
}

QStringList AppController::lastUndoCommitCoAuthors() const
{
    return m_activeService ? m_activeService->getLastUndoCommitCoAuthors() : QStringList();
}

void AppController::setSettingsDialogVisible(bool visible)
{
    if (m_isSettingsDialogVisible == visible) return;
    m_isSettingsDialogVisible = visible;
    emit settingsDialogVisibleChanged();
}

void AppController::setPublishDialogVisible(bool visible)
{
    if (m_isPublishDialogVisible == visible) return;
    m_isPublishDialogVisible = visible;
    emit publishDialogVisibleChanged();
}

void AppController::setSettingsTab(const QString &tab)
{
    if (m_settingsTab == tab) return;
    m_settingsTab = tab;
    emit settingsTabChanged();
}

QString AppController::defaultEditor() const
{
    return m_settings ? m_settings->defaultEditor() : "kate";
}

QString AppController::customEditorCommand() const
{
    return m_settings ? m_settings->customEditorCommand() : "";
}

QString AppController::defaultTerminal() const
{
    return m_settings ? m_settings->defaultTerminal() : "konsole";
}

QString AppController::customTerminalCommand() const
{
    return m_settings ? m_settings->customTerminalCommand() : "";
}

QStringList AppController::availableEditors() const
{
    return m_settings ? m_settings->availableEditors() : QStringList();
}

QStringList AppController::availableTerminals() const
{
    return m_settings ? m_settings->availableTerminals() : QStringList();
}

QString AppController::globalAuthorName() const
{
    return m_activeService ? m_activeService->getGlobalAuthorName() : "";
}

QString AppController::globalAuthorEmail() const
{
    return m_activeService ? m_activeService->getGlobalAuthorEmail() : "";
}

QString AppController::localAuthorName() const
{
    return m_activeService ? m_activeService->getAuthorName() : "";
}

QString AppController::localAuthorEmail() const
{
    return m_activeService ? m_activeService->getAuthorEmail() : "";
}

void AppController::showSettingsDialog(const QString &tab)
{
    setSettingsTab(tab);
    setSettingsDialogVisible(true);
}

void AppController::hideSettingsDialog()
{
    setSettingsDialogVisible(false);
}

void AppController::showPublishDialog()
{
    if (m_activeService && m_activeService->hasRemote()) {
        showToast(tr("This repository already has a Git remote configured"), true);
        return;
    }

    m_publishErrorMessage.clear();
    emit publishErrorMessageChanged();
    setPublishDialogVisible(true);
}

void AppController::hidePublishDialog()
{
    setPublishDialogVisible(false);
}

bool AppController::publishRepository(const QString &name, const QString &description, bool isPrivate)
{
    if (!m_activeService) return false;
    m_isPublishing = true;
    m_publishErrorMessage.clear();
    emit isPublishingChanged();
    emit publishErrorMessageChanged();
    emit operatingStateChanged();

    bool res = m_activeService->publishRepository(name, description, isPrivate);
    if (!res) {
        m_isPublishing = false;
        emit isPublishingChanged();
        emit operatingStateChanged();
    }
    return res;
}

bool AppController::saveRemoteUrl(const QString &url, const QString &remoteName)
{
    if (!m_activeService) return false;
    bool res = m_activeService->setRemoteUrl(url, remoteName);
    if (res) {
        updateCurrentState();
        fetchOrigin();
    }
    return res;
}

bool AppController::removeRemoteUrl(const QString &remoteName)
{
    if (!m_activeService) return false;
    bool res = m_activeService->removeRemote(remoteName);
    if (res) {
        updateCurrentState();
    }
    return res;
}

bool AppController::setIgnoreFileModeChanges(bool ignored, bool global)
{
    if (!m_activeService) return false;
    const bool result = m_activeService->setIgnoreFileModeChanges(ignored, global);
    if (result) {
        emit gitConfigChanged();
        if (!m_selectedFilePath.isEmpty()) {
            m_diffModel->loadDiffForFile(m_selectedFilePath);
        }
    }
    return result;
}

bool AppController::saveAuthorInfo(const QString &name, const QString &email, bool global)
{
    if (!m_activeService) return false;
    bool res = m_activeService->setAuthorInfo(name, email, global);
    if (res) {
        emit authorInfoChanged();
        emit currentRepoChanged();
    }
    return res;
}

void AppController::saveEditorSettings(const QString &editor, const QString &customCmd)
{
    if (!m_settings) return;
    m_settings->setDefaultEditor(editor);
    m_settings->setCustomEditorCommand(customCmd);
    emit editorSettingsChanged();
}

void AppController::saveTerminalSettings(const QString &terminal, const QString &customCmd)
{
    if (!m_settings) return;
    m_settings->setDefaultTerminal(terminal);
    m_settings->setCustomTerminalCommand(customCmd);
    emit terminalSettingsChanged();
}

void AppController::saveAvatarSettings(const QString &provider)
{
    if (!m_settings) return;
    m_settings->setAvatarProvider(provider);
}

bool AppController::isAiEnabled() const { return m_settings ? m_settings->aiEnabled() : false; }
QString AppController::aiProvider() const { return m_settings ? m_settings->aiProvider() : "openai"; }
QString AppController::aiApiKey() const { return m_settings ? m_settings->aiApiKey() : ""; }
QString AppController::aiEndpoint() const { return m_settings ? m_settings->aiEndpoint() : ""; }
QString AppController::aiModel() const { return m_settings ? m_settings->aiModel() : ""; }
QString AppController::aiCommitStyle() const { return m_settings ? m_settings->aiCommitStyle() : "conventional"; }
bool AppController::aiIncludeDescription() const { return m_settings ? m_settings->aiIncludeDescription() : true; }
bool AppController::aiFollowRepoStyle() const { return m_settings ? m_settings->aiFollowRepoStyle() : true; }
bool AppController::aiFirstRunConfigured() const { return m_settings ? m_settings->aiFirstRunConfigured() : false; }

void AppController::setAiFirstRunConfigured(bool configured)
{
    if (!m_settings) return;
    m_settings->setAiFirstRunConfigured(configured);
    emit aiSettingsChanged();
}

void AppController::saveAiSettings(bool enabled, const QString &provider, const QString &apiKey, const QString &endpoint, const QString &model, const QString &commitStyle, bool includeDescription, bool followRepoStyle, bool firstRunConfigured)
{
    if (!m_settings) return;
    m_settings->setAiEnabled(enabled);
    m_settings->setAiProvider(provider);
    m_settings->setAiApiKey(apiKey);
    m_settings->setAiEndpoint(endpoint);
    m_settings->setAiModel(model);
    m_settings->setAiCommitStyle(commitStyle);
    m_settings->setAiIncludeDescription(includeDescription);
    m_settings->setAiFollowRepoStyle(followRepoStyle);
    m_settings->setAiFirstRunConfigured(firstRunConfigured);
    emit aiSettingsChanged();
}

void AppController::cancelAiCommit()
{
    if (m_aiCommitService) {
        m_aiCommitService->cancel();
    }
}

void AppController::generateAiCommit()
{
    if (!m_activeService || !m_settings) return;

    if (!m_settings->aiEnabled()) {
        showToast(tr("AI Commit Assistant is disabled. Enable it in Settings."), true);
        return;
    }

    auto changedFiles = m_activeService->getChangedFiles();
    QList<FileChange> selectedFiles;
    for (const auto &file : changedFiles) {
        if (file.isSelected) {
            selectedFiles.append(file);
        }
    }

    if (selectedFiles.isEmpty()) {
        showToast(tr("No files selected to generate commit message for"), true);
        return;
    }

    QList<QPair<QString, QString>> selectedFileDiffs;
    for (const auto &file : selectedFiles) {
        QString diffContent;
        if (file.status == FileChangeType::Untracked) {
            QByteArray blob = m_activeService->getFileBlob(file.filePath);
            diffContent = QString::fromUtf8(blob);
            if (diffContent.size() > 6000) {
                diffContent = diffContent.left(6000) + "\n... [content truncated]";
            }
        } else {
            auto lines = m_activeService->getDiffForFile(file.filePath, file.oldFilePath);
            QStringList formattedLines;
            for (const auto &line : lines) {
                if (line.type == DiffLineType::Addition) {
                    formattedLines.append("+" + line.content);
                } else if (line.type == DiffLineType::Deletion) {
                    formattedLines.append("-" + line.content);
                } else if (line.type == DiffLineType::HunkHeader) {
                    formattedLines.append(line.content);
                } else {
                    formattedLines.append(" " + line.content);
                }
            }
            diffContent = formattedLines.join("\n");
            if (diffContent.size() > 6000) {
                diffContent = diffContent.left(6000) + "\n... [diff truncated]";
            }
        }
        selectedFileDiffs.append({file.filePath, diffContent});
    }

    QList<CommitItem> recentCommits;
    bool shouldFollowRepo = m_settings->aiFollowRepoStyle() || m_settings->aiCommitStyle() == "repo_history";
    if (shouldFollowRepo) {
        recentCommits = m_activeService->getCommitHistory(5);
    }

    m_aiCommitService->generateCommitMessage(
        m_settings->aiProvider(),
        m_settings->aiApiKey(),
        m_settings->aiEndpoint(),
        m_settings->aiModel(),
        m_settings->aiCommitStyle(),
        m_settings->aiIncludeDescription(),
        shouldFollowRepo,
        recentCommits,
        selectedFileDiffs
    );
}

QString AppController::resolveAvatarUrl(const QString &name, const QString &email) const
{
    return avatarForAuthor(name, email);
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

void AppController::setRepositoryModelsSuspended(bool suspended)
{
    m_branchModel->setUpdatesSuspended(suspended);
    m_changedFilesModel->setUpdatesSuspended(suspended);
    m_commitHistoryModel->setUpdatesSuspended(suspended);
    m_stashModel->setUpdatesSuspended(suspended);

    if (suspended) {
        m_branchModel->clear();
        m_changedFilesModel->clear();
        m_commitHistoryModel->clear();
        m_stashModel->clear();
    }
}

void AppController::switchRepository(const QString &repoIdOrPath)
{
    if (!m_activeService) return;

    quint64 seq = ++m_currentLoadSequence;

    QString cleanName = QFileInfo(repoIdOrPath).fileName();
    if (cleanName.isEmpty()) cleanName = repoIdOrPath;

    // Never leave repository-specific rows visible while the service is
    // changing its active repository. Service signals are suspended until the
    // final load wins, preventing Repo A's rows from being shown for Repo B.
    setRepositoryModelsSuspended(true);
    m_selectedFilePath.clear();
    m_selectedCommitSha.clear();
    if (!m_selectedStashId.isEmpty()) setSelectedStashId(QString());
    emit selectedFilePathChanged();
    emit selectedCommitShaChanged();
    m_diffModel->clear();

    m_isLoadingRepository = true;
    m_loadingRepositoryName = cleanName;
    m_loadingRepositoryMessage = tr("Opening repository...");
    emit loadingRepositoryNameChanged();
    emit loadingRepositoryMessageChanged();

    emit isLoadingRepositoryChanged();
    emit operatingStateChanged();

    // Do not start a second open against the same service while the first one
    // is still mutating its repository state. Keep only the latest target; the
    // completion callback below starts it after the current load is finished.
    if (m_repositoryLoadWorkerActive) {
        m_pendingRepositoryTarget = repoIdOrPath;
        return;
    }
    m_repositoryLoadWorkerActive = true;

    QPointer<AppController> self(this);
    QString target = repoIdOrPath;
    IGitService *service = m_activeService;

    QThread *thread = QThread::create([self, service, target, seq]() {
        bool ok = service->openRepository(target);
        QMetaObject::invokeMethod(self, [self, ok, seq]() {
            if (!self) return;
            self->m_repositoryLoadWorkerActive = false;

            if (self->m_currentLoadSequence != seq) {
                const QString pendingTarget = self->m_pendingRepositoryTarget;
                self->m_pendingRepositoryTarget.clear();
                if (!pendingTarget.isEmpty()) {
                    self->switchRepository(pendingTarget);
                }
                return;
            }

            self->setRepositoryModelsSuspended(false);
            if (self->m_isLoadingRepository) {
                self->m_isLoadingRepository = false;
                emit self->isLoadingRepositoryChanged();
                emit self->operatingStateChanged();
            }

            if (ok) {
                // The service publishes lightweight metadata first and fills
                // the models from its background refresh. Avoid querying status
                // or history synchronously on this GUI callback.
                self->updateCurrentState();
            }
        }, Qt::QueuedConnection);
    });
    trackWorker(thread)->start();
}

void AppController::addRepository(const QString &name, const QString &path)
{
    if (!m_activeService) return;
    if (m_repositoryLoadWorkerActive) {
        showToast(tr("Please wait for the current repository operation to finish"), true);
        return;
    }

    quint64 seq = ++m_currentLoadSequence;

    QString cleanName = name.trimmed().isEmpty() ? QFileInfo(path).fileName() : name.trimmed();

    setRepositoryModelsSuspended(true);
    m_selectedFilePath.clear();
    m_selectedCommitSha.clear();
    if (!m_selectedStashId.isEmpty()) setSelectedStashId(QString());
    emit selectedFilePathChanged();
    emit selectedCommitShaChanged();
    m_diffModel->clear();

    m_isLoadingRepository = true;
    m_loadingRepositoryName = cleanName;
    m_loadingRepositoryMessage = tr("Scanning repository files...");
    emit loadingRepositoryNameChanged();
    emit loadingRepositoryMessageChanged();

    m_repositoryLoadWorkerActive = true;
    emit isLoadingRepositoryChanged();
    emit operatingStateChanged();
    QPointer<AppController> self(this);
    QString targetName = name;
    QString targetPath = path;
    IGitService *service = m_activeService;

    QThread *thread = QThread::create([self, service, targetName, targetPath, seq]() {
        bool ok = service->addRepository(targetName, targetPath);
        QMetaObject::invokeMethod(self, [self, ok, seq]() {
            if (!self) return;
            self->m_repositoryLoadWorkerActive = false;

            if (self->m_currentLoadSequence != seq) {
                const QString pendingTarget = self->m_pendingRepositoryTarget;
                self->m_pendingRepositoryTarget.clear();
                if (!pendingTarget.isEmpty()) {
                    self->switchRepository(pendingTarget);
                }
                return;
            }

            self->setRepositoryModelsSuspended(false);
            if (self->m_isLoadingRepository) {
                self->m_isLoadingRepository = false;
                emit self->isLoadingRepositoryChanged();
                emit self->operatingStateChanged();
            }

            if (ok) {
                // The service publishes lightweight metadata first and fills
                // the models from its background refresh. Avoid querying status
                // or history synchronously on this GUI callback.
                self->updateCurrentState();
            }
        }, Qt::QueuedConnection);
    });
    trackWorker(thread)->start();
}

void AppController::removeRepository(const QString &repoIdOrPath)
{
    if (!m_activeService) return;
    m_activeService->removeRepository(repoIdOrPath);
    updateCurrentState();
}

bool AppController::renameRepository(const QString &repoIdOrPath, const QString &newName)
{
    if (!m_activeService) return false;
    bool ok = m_activeService->renameRepository(repoIdOrPath, newName);
    if (ok) {
        if (m_isCurrentRepoMissing && (m_missingRepoPath == repoIdOrPath || m_missingRepoName == repoIdOrPath)) {
            m_missingRepoName = newName.trimmed().isEmpty() ? QFileInfo(m_missingRepoPath).fileName() : newName.trimmed();
        }
        updateCurrentState();
    }
    return ok;
}

bool AppController::renameCurrentRepository(const QString &newName)
{
    if (!m_activeService) return false;
    QString target = m_isCurrentRepoMissing ? m_missingRepoPath : currentRepoPath();
    return renameRepository(target, newName);
}

void AppController::setCloneDialogVisible(bool visible)
{
    if (m_isCloneDialogVisible == visible) return;
    m_isCloneDialogVisible = visible;
    emit cloneDialogVisibleChanged();
}

void AppController::showCloneDialog(const QString &prefillUrl, const QString &prefillPath)
{
    m_clonePrefillUrl = !prefillUrl.isEmpty() ? prefillUrl : (m_isCurrentRepoMissing ? m_missingRepoRemoteUrl : QString());
    m_clonePrefillPath = !prefillPath.isEmpty() ? prefillPath : (m_isCurrentRepoMissing ? m_missingRepoPath : QString());
    emit cloneDialogVisibleChanged();
    setCloneDialogVisible(true);
}

void AppController::hideCloneDialog()
{
    setCloneDialogVisible(false);
}

void AppController::cloneRepository(const QString &url, const QString &targetDir)
{
    if (!m_activeService || m_isCloning) return;

    m_isCloning = true;
    m_cloneProgress = -1.0;
    m_cloneProgressMessage = tr("Connecting to %1...").arg(url);
    m_cloneProgressDetails.clear();
    emit isCloningChanged();
    emit cloneProgressChanged();
    emit cloneProgressMessageChanged();
    emit cloneProgressDetailsChanged();
    emit operatingStateChanged();

    QPointer<AppController> self(this);
    QString cleanUrl = url;
    QString cleanDir = targetDir;
    IGitService *service = m_activeService;

    QThread *thread = QThread::create([self, service, cleanUrl, cleanDir]() {
        bool ok = service->cloneRepository(cleanUrl, cleanDir);
        QMetaObject::invokeMethod(self, [self, ok]() {
            if (!self) return;
            self->m_isCloning = false;
            self->m_cloneProgress = -1.0;
            self->m_cloneProgressMessage.clear();
            self->m_cloneProgressDetails.clear();
            emit self->isCloningChanged();
            emit self->cloneProgressChanged();
            emit self->cloneProgressMessageChanged();
            emit self->cloneProgressDetailsChanged();
            emit self->operatingStateChanged();
            if (ok) {
                self->hideCloneDialog();
                self->updateCurrentState();
            }
        }, Qt::QueuedConnection);
    });
    trackWorker(thread)->start();
}

void AppController::cloneMissingRepository(const QString &targetDir)
{
    QString url = m_missingRepoRemoteUrl;
    if (url.isEmpty()) {
        showCloneDialog();
        return;
    }

    QString chosenDir = targetDir;
    if (chosenDir.isEmpty()) {
        chosenDir = QFileDialog::getExistingDirectory(
            nullptr,
            tr("Select Destination Directory for Cloned Repository"),
            QDir::homePath(),
            QFileDialog::ShowDirsOnly | QFileDialog::DontResolveSymlinks
        );
        if (chosenDir.isEmpty()) return;
        QString name = m_missingRepoName.isEmpty() ? "repository" : m_missingRepoName;
        chosenDir = chosenDir + "/" + name;
    }

    cloneRepository(url, chosenDir);
}

void AppController::locateMissingRepository()
{
    QString dir = QFileDialog::getExistingDirectory(
        nullptr,
        tr("Locate Repository Folder for '%1'").arg(m_missingRepoName),
        QDir::homePath(),
        QFileDialog::ShowDirsOnly | QFileDialog::DontResolveSymlinks
    );

    if (!dir.isEmpty()) {
        relocateCurrentRepository(dir);
    }
}

bool AppController::relocateCurrentRepository(const QString &newPath)
{
    if (!m_activeService) return false;
    QString oldPath = m_missingRepoPath.isEmpty() ? currentRepoPath() : m_missingRepoPath;
    bool ok = m_activeService->relocateRepository(oldPath, newPath);
    if (ok) {
        updateCurrentState();
        showToast(tr("Repository location updated successfully"), false);
    }
    return ok;
}

void AppController::recheckMissingRepository()
{
    if (!m_activeService) return;
    QString target = m_missingRepoPath.isEmpty() ? currentRepoPath() : m_missingRepoPath;
    bool ok = m_activeService->recheckRepository(target);
    if (ok) {
        updateCurrentState();
        showToast(tr("Repository found and opened"), false);
    }
}

void AppController::removeCurrentMissingRepository()
{
    if (!m_activeService) return;
    QString target = m_missingRepoPath.isEmpty() ? currentRepoPath() : m_missingRepoPath;
    removeRepository(target);
}

void AppController::refresh()
{
    // GitCliService performs the repository scan in its worker thread and only
    // emits model updates after the complete snapshot is ready.
    if (m_activeService) {
        m_activeService->refreshRepository();
    }
}

void AppController::refreshRepository()
{
    refresh();
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
    m_isCommitting = true;
    emit isCommittingChanged();
    emit operatingStateChanged();

    bool res = m_activeService->createCommit(summary, description, coAuthors);

    m_isCommitting = false;
    emit isCommittingChanged();
    emit operatingStateChanged();

    if (res) {
        emit undoStateChanged();
        emit remoteStatusChanged();

        // Synchronously update commit history and selection
        auto commits = m_activeService->getCommitHistory(1);
        if (!commits.isEmpty()) {
            setSelectedCommitSha(commits.first().sha);
        }

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

        auto commits = m_activeService->getCommitHistory(1);
        if (!commits.isEmpty()) {
            setSelectedCommitSha(commits.first().sha);
        }

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
    if (!m_activeService) return;
    m_isDiscarding = true;
    emit operatingStateChanged();

    m_activeService->discardFileChanges(filePath);

    m_isDiscarding = false;
    emit operatingStateChanged();
}

void AppController::discardAllChanges()
{
    if (!m_activeService) return;
    m_isDiscarding = true;
    emit operatingStateChanged();

    m_activeService->discardAllChanges();

    m_isDiscarding = false;
    emit operatingStateChanged();
}

void AppController::stashChanges(const QString &message)
{
    if (!m_activeService) return;
    m_isStashing = true;
    emit operatingStateChanged();

    m_activeService->stashChanges(message);

    m_isStashing = false;
    emit operatingStateChanged();
}

void AppController::popStash(const QString &stashId)
{
    if (!m_activeService) return;
    m_isStashing = true;
    emit operatingStateChanged();

    QString id = stashId.isEmpty() ? m_selectedStashId : stashId;
    m_activeService->popStash(id);
    clearStashSelection();

    m_isStashing = false;
    emit operatingStateChanged();
}

void AppController::dropStash(const QString &stashId)
{
    if (!m_activeService) return;
    m_isStashing = true;
    emit operatingStateChanged();

    QString id = stashId.isEmpty() ? m_selectedStashId : stashId;
    m_activeService->dropStash(id);
    clearStashSelection();

    m_isStashing = false;
    emit operatingStateChanged();
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
    } else {
        m_diffModel->clear();
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
    if (!m_activeService) return;
    m_isReverting = true;
    emit operatingStateChanged();

    m_activeService->revertCommit(sha);

    m_isReverting = false;
    emit operatingStateChanged();
}

void AppController::openInEditor(const QString &filePath)
{
    if (!m_settings) return;
    bool ok = m_settings->openInEditor(filePath, currentRepoPath());
    if (!ok) {
        showToast("Could not launch configured text editor", true);
    }
}

void AppController::openInTerminal(const QString &path)
{
    QString targetDir = path.isEmpty() ? currentRepoPath() : path;
    if (targetDir.isEmpty() || !QDir(targetDir).exists()) {
        showToast("No active repository directory to open", true);
        return;
    }

    if (m_settings) {
        bool started = m_settings->openInTerminal(targetDir);
        if (started) {
            return;
        }
    }

    // Fallback konsole
    if (!QProcess::startDetached("konsole", {"--workdir", targetDir})) {
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
}

void AppController::openOnGitHub()
{
    if (!isGitHubRemote()) {
        showToast("The configured remote is not hosted on GitHub", true);
        return;
    }

    QString url;
    if (m_activeService) {
        url = m_activeService->getRemoteUrl();
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
}

void AppController::createPullRequest()
{
    if (!isGitHubRemote()) {
        showToast("Pull requests are only available for GitHub remotes", true);
        return;
    }

    QString url;
    if (m_activeService) {
        url = m_activeService->getRemoteUrl();
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
