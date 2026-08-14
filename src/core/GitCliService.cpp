#include "GitCliService.h"
#include "AppSettings.h"
#include <QProcess>
#include <QDir>
#include <QFileInfo>
#include <QUrl>
#include <QRegularExpression>
#include <QThread>
#include <QStandardPaths>
#include <QFile>
#include <QTextStream>
#include <QFileSystemWatcher>
#include <QTimer>
#include <future>

namespace Cherry {

namespace {
QString redactRemoteUrl(const QString &url)
{
    QString result = url;
    result.replace(QRegularExpression(R"((https?://)[^/\s@]+@)"), "\\1");
    return result;
}
}

GitCliService::GitCliService(QObject *parent)
    : IGitService(parent)
{
    m_fsWatcher = new QFileSystemWatcher(this);
    m_fsDebounceTimer = new QTimer(this);
    m_fsDebounceTimer->setSingleShot(true);
    m_fsDebounceTimer->setInterval(300);

    connect(m_fsDebounceTimer, &QTimer::timeout, this, &GitCliService::refreshRepository);
    connect(m_fsWatcher, &QFileSystemWatcher::fileChanged, this, [this](const QString &) {
        if (m_fsDebounceTimer) m_fsDebounceTimer->start();
    });
    connect(m_fsWatcher, &QFileSystemWatcher::directoryChanged, this, [this](const QString &) {
        if (m_fsDebounceTimer) m_fsDebounceTimer->start();
    });

    loadSavedRepositories();
    loadFetchTimes();
    discoverInitialRepository();
}

void GitCliService::setupFileSystemWatcher()
{
    if (!m_fsWatcher) return;

    const QStringList files = m_fsWatcher->files();
    if (!files.isEmpty()) m_fsWatcher->removePaths(files);
    const QStringList dirs = m_fsWatcher->directories();
    if (!dirs.isEmpty()) m_fsWatcher->removePaths(dirs);

    if (m_repoPath.isEmpty()) return;

    QString gitDir = m_repositoryReader.gitDirPath();
    if (gitDir.isEmpty()) gitDir = m_repoPath + "/.git";
    if (QDir(gitDir).exists()) {
        m_fsWatcher->addPath(gitDir);
        if (QFile::exists(gitDir + "/HEAD")) m_fsWatcher->addPath(gitDir + "/HEAD");
        if (QFile::exists(gitDir + "/index")) m_fsWatcher->addPath(gitDir + "/index");
        if (QFile::exists(gitDir + "/packed-refs")) m_fsWatcher->addPath(gitDir + "/packed-refs");
        if (QDir(gitDir + "/refs/heads").exists()) m_fsWatcher->addPath(gitDir + "/refs/heads");
        if (QDir(gitDir + "/refs/remotes").exists()) m_fsWatcher->addPath(gitDir + "/refs/remotes");
        if (QFile::exists(gitDir + "/logs/refs/stash")) m_fsWatcher->addPath(gitDir + "/logs/refs/stash");
    }
    const QString commonGitDir = m_repositoryReader.commonGitDirPath();
    if (!commonGitDir.isEmpty() && commonGitDir != gitDir && QDir(commonGitDir).exists()) {
        m_fsWatcher->addPath(commonGitDir);
        if (QFile::exists(commonGitDir + "/packed-refs")) m_fsWatcher->addPath(commonGitDir + "/packed-refs");
        if (QDir(commonGitDir + "/refs/heads").exists()) m_fsWatcher->addPath(commonGitDir + "/refs/heads");
        if (QDir(commonGitDir + "/refs/remotes").exists()) m_fsWatcher->addPath(commonGitDir + "/refs/remotes");
        if (QFile::exists(commonGitDir + "/logs/refs/stash")) m_fsWatcher->addPath(commonGitDir + "/logs/refs/stash");
    }
    if (QDir(m_repoPath).exists()) {
        m_fsWatcher->addPath(m_repoPath);
    }
}

void GitCliService::clearUndoState()
{
    m_lastUndoCommitSha.clear();
    m_lastUndoCommitSummary.clear();
    m_lastUndoCommitDescription.clear();
    m_lastUndoCommitCoAuthors.clear();
}

void GitCliService::invalidateRepositoryCaches()
{
    QMutexLocker locker(&m_cacheMutex);
    m_repositoryReader.refresh();
    m_changedFilesCacheValid = false;
    m_branchesCacheValid = false;
    m_currentBranchCacheValid = false;
    m_commitHistoryCacheValid = false;
    m_stashesCacheValid = false;
    m_remoteStatusCacheValid = false;
    m_fileDiffCacheValid = false;
    m_fileDiffCachePath.clear();
    m_fileDiffCache.clear();
    m_commitDetailsCache.clear();
}

void GitCliService::preloadRepositoryCaches()
{
    // All expensive Git queries happen before the update signals are posted.
    // We run independent queries in parallel so big repositories open almost instantly.
    m_suppressRefreshSignals = true;
    invalidateRepositoryCaches();

    auto fReader = std::async(std::launch::async, [this]() {
        getCurrentBranch();
        getBranches();
        getStashes();
    });

    auto fRemote = std::async(std::launch::async, [this]() {
        getRemoteStatus();
    });

    auto fHistory = std::async(std::launch::async, [this]() {
        getCommitHistory();
    });

    auto fFiles = std::async(std::launch::async, [this]() {
        getChangedFiles();
    });

    fReader.get();
    fRemote.get();
    fHistory.get();
    fFiles.get();

    {
        QMutexLocker locker(&m_cacheMutex);
        if (!m_changedFilesCache.isEmpty()) {
            m_fileDiffCachePath = m_changedFilesCache.first().filePath;
            locker.unlock();
            auto diff = getDiffForFile(m_fileDiffCachePath);
            locker.relock();
            m_fileDiffCache = diff;
            m_fileDiffCacheValid = true;
        }
    }
    m_suppressRefreshSignals = false;
}

void GitCliService::emitRepositoryRefreshSignals(bool changedFilesChanged)
{
    if (auto b = getCurrentBranch()) {
        emit currentBranchChanged(*b);
    }
    emit branchListChanged();
    if (changedFilesChanged) {
        emit changedFilesUpdated();
    }
    emit remoteStatusUpdated(m_remoteStatus);
    emit commitHistoryUpdated();
    emit stashesUpdated();

    if (auto repo = getCurrentRepository()) {
        emit repositoryChanged(*repo);
    }
}

void GitCliService::refreshRepository()
{
    if (m_repoPath.isEmpty() || m_refreshInProgress) return;

    if (!QDir(m_repoPath).exists() || (!QFile::exists(m_repoPath + "/.git") && !QDir(m_repoPath + "/.git").exists())) {
        if (!m_isMissing) {
            m_isMissing = true;
            invalidateRepositoryCaches();
            RepositoryInfo missingInfo;
            missingInfo.id = m_repoPath;
            missingInfo.name = m_repoName;
            missingInfo.path = m_repoPath;
            missingInfo.isMissing = true;
            missingInfo.remoteUrl = m_repoRemotes.value(m_repoPath);
            emit repositoryChanged(missingInfo);
            emitRepositoryRefreshSignals();
        }
        return;
    }

    m_refreshInProgress = true;
    const QString repoPath = m_repoPath;
    const quint64 generation = m_repositoryGeneration.load();
    QThread *thread = QThread::create([this, repoPath, generation]() {
        const bool hadFilesCache = m_changedFilesCacheValid;
        const QList<FileChange> previousFiles = m_changedFilesCache;
        preloadRepositoryCaches();

        bool filesChanged = !hadFilesCache || previousFiles.size() != m_changedFilesCache.size();
        if (!filesChanged) {
            for (int i = 0; i < previousFiles.size(); ++i) {
                const auto &oldFile = previousFiles.at(i);
                const auto &newFile = m_changedFilesCache.at(i);
                if (oldFile.filePath != newFile.filePath ||
                    oldFile.status != newFile.status ||
                    oldFile.additions != newFile.additions ||
                    oldFile.deletions != newFile.deletions) {
                    filesChanged = true;
                    break;
                }
            }
        }

        if (m_repositoryGeneration.load() != generation || m_repoPath != repoPath) return;
        QMetaObject::invokeMethod(this, [this, filesChanged, repoPath, generation]() {
            if (m_repositoryGeneration.load() != generation || m_repoPath != repoPath) return;
            m_refreshInProgress = false;
            emitRepositoryRefreshSignals(filesChanged);
        }, Qt::QueuedConnection);
    });
    connect(thread, &QThread::finished, thread, &QObject::deleteLater);
    thread->start();
}

GitResult GitCliService::runGit(const QStringList &args, const QString &workingDir, int timeoutMs)
{
    QProcess process;
    QString dir = workingDir.isEmpty() ? m_repoPath : workingDir;
    if (!dir.isEmpty()) {
        process.setWorkingDirectory(dir);
    }

    QProcessEnvironment env = QProcessEnvironment::systemEnvironment();
    env.insert("LC_ALL", "C");
    env.insert("GIT_TERMINAL_PROMPT", "0");
    if (!env.contains("GIT_SSH_COMMAND")) {
        env.insert("GIT_SSH_COMMAND", "ssh -o BatchMode=yes");
    }
    process.setProcessEnvironment(env);

    process.start("git", args);
    bool finished = process.waitForFinished(timeoutMs);

    if (!finished) {
        process.kill();
        process.waitForFinished(500);
    }

    GitResult result;
    result.exitCode = process.exitCode();
    result.stdOut = QString::fromUtf8(process.readAllStandardOutput());
    result.stdErr = QString::fromUtf8(process.readAllStandardError());
    result.success = finished && (result.exitCode == 0);

    if (!result.success && !result.stdErr.trimmed().isEmpty()) {
        // Do not log the command arguments: remote URLs may contain credentials.
        qWarning().noquote() << QString("[GitCliService] Git command failed (exit %1): %2")
                                    .arg(result.exitCode)
                                    .arg(result.stdErr.trimmed());
    }

    return result;
}

void GitCliService::runGitAsync(const QStringList &args, std::function<void(const GitResult &)> callback)
{
    QString dir = m_repoPath;
    QThread *thread = QThread::create([this, args, dir, callback]() {
        GitResult res = const_cast<GitCliService*>(this)->runGit(args, dir, 120000);
        QMetaObject::invokeMethod(this, [callback, res]() {
            if (callback) callback(res);
        }, Qt::QueuedConnection);
    });
    connect(thread, &QThread::finished, thread, &QObject::deleteLater);
    thread->start();
}

void GitCliService::loadSavedRepositories()
{
    QSettings settings("KDE", "cherrygi");
    settings.beginGroup("Repositories");
    const QStringList childGroups = settings.childGroups();
    for (const QString &group : childGroups) {
        settings.beginGroup(group);
        QString path = settings.value("path").toString();
        QString name = settings.value("name").toString();
        QString remote = settings.value("remoteUrl").toString();
        if (!path.isEmpty()) {
            if (name.isEmpty()) name = QFileInfo(path).fileName();
            m_knownRepos.insert(path, name);
            if (!remote.isEmpty()) {
                m_repoRemotes.insert(path, redactRemoteUrl(remote));
            }
        }
        settings.endGroup();
    }
    settings.endGroup();
}

void GitCliService::saveRepositories()
{
    QSettings settings("KDE", "cherrygi");
    settings.remove("Repositories");
    settings.beginGroup("Repositories");
    int idx = 0;
    for (auto it = m_knownRepos.begin(); it != m_knownRepos.end(); ++it) {
        settings.beginGroup(QString::number(idx++));
        settings.setValue("path", it.key());
        settings.setValue("name", it.value());
        if (m_repoRemotes.contains(it.key())) {
            settings.setValue("remoteUrl", redactRemoteUrl(m_repoRemotes.value(it.key())));
        }
        settings.endGroup();
    }
    settings.endGroup();

    settings.setValue("General/lastRepository", m_repoPath);
}

void GitCliService::loadFetchTimes()
{
    m_repoFetchTimes.clear();
    bool needsPruning = false;
    QSettings settings(AppSettings::dataDir() + "/fetchTimes.ini", QSettings::IniFormat);
    settings.beginGroup("FetchTimes");
    const QStringList groups = settings.childGroups();
    for (const QString &group : groups) {
        settings.beginGroup(group);
        QString path = settings.value("path").toString();
        QDateTime dt = QDateTime::fromString(settings.value("time").toString(), Qt::ISODate);
        settings.endGroup();
        if (!path.isEmpty() && dt.isValid() && QDir(path).exists()) {
            m_repoFetchTimes.insert(path, dt);
        } else if (!path.isEmpty() && !QDir(path).exists()) {
            needsPruning = true;
        }
    }
    settings.endGroup();
    if (needsPruning) {
        saveFetchTimes();
    }
}

void GitCliService::saveFetchTimes()
{
    QSettings settings(AppSettings::dataDir() + "/fetchTimes.ini", QSettings::IniFormat);
    settings.remove("FetchTimes");
    settings.beginGroup("FetchTimes");
    int idx = 0;
    for (auto it = m_repoFetchTimes.begin(); it != m_repoFetchTimes.end(); ++it) {
        settings.beginGroup(QString::number(idx++));
        settings.setValue("path", it.key());
        settings.setValue("time", it.value().toString(Qt::ISODate));
        settings.endGroup();
    }
    settings.endGroup();
    settings.sync();
}

void GitCliService::discoverInitialRepository()
{
    QSettings settings("KDE", "cherrygi");
    QString lastRepo = settings.value("General/lastRepository").toString();

    if (!lastRepo.isEmpty() && m_knownRepos.contains(lastRepo)) {
        openRepository(lastRepo);
        return;
    }

    // Auto-discover the current working directory directly from .git/HEAD.
    GitRepositoryReader reader;
    const QString current = QDir::currentPath();
    if (reader.open(current)) {
        openRepository(reader.workTreePath().isEmpty() ? current : reader.workTreePath());
        return;
    }

    // Check if any saved repo exists
    for (auto it = m_knownRepos.begin(); it != m_knownRepos.end(); ++it) {
        if (QDir(it.key()).exists()) {
            if (openRepository(it.key())) return;
        }
    }

    if (!m_knownRepos.isEmpty()) {
        openRepository(m_knownRepos.begin().key());
    }
}

QList<RepositoryInfo> GitCliService::getRepositories()
{
    QList<RepositoryInfo> list;
    for (auto it = m_knownRepos.begin(); it != m_knownRepos.end(); ++it) {
        const QString &path = it.key();
        const QString &name = it.value();

        RepositoryInfo info;
        info.id = path;
        info.name = name;
        info.path = path;
        info.remoteUrl = m_repoRemotes.value(path);
        bool existsOnDisk = QDir(path).exists() && (QFile::exists(path + "/.git") || QDir(path + "/.git").exists());
        info.isMissing = !existsOnDisk;

        if (path == m_repoPath) {
            if (m_isMissing) {
                info.isMissing = true;
                info.currentBranch = "-";
            } else {
                auto b = getCurrentBranch();
                info.currentBranch = b ? b->name : "main";
                info.changedFilesCount = m_fileSelection.size();
                info.aheadCount = m_remoteStatus.ahead;
                info.behindCount = m_remoteStatus.behind;
                info.lastFetchedTime = m_remoteStatus.lastFetchedText;
            }
        } else {
            info.currentBranch = "main";
            QDateTime repoFetch = m_repoFetchTimes.value(path);
            info.lastFetchedTime = repoFetch.isValid()
                ? QString("Last fetched %1").arg(formatRelativeTime(repoFetch))
                : (info.isMissing ? "Repository not found on disk" : "Not fetched in this session");
        }
        list.append(info);
    }
    return list;
}

std::optional<RepositoryInfo> GitCliService::getCurrentRepository()
{
    if (m_repoPath.isEmpty()) return std::nullopt;
    RepositoryInfo info;
    info.id = m_repoPath;
    info.name = m_repoName;
    info.path = m_repoPath;
    info.isMissing = m_isMissing;
    info.remoteUrl = m_repoRemotes.value(m_repoPath);
    if (m_isMissing) {
        info.currentBranch = "-";
        info.lastFetchedTime = "Repository not found on disk";
        return info;
    }
    auto b = getCurrentBranch();
    info.currentBranch = b ? b->name : "main";
    info.changedFilesCount = m_fileSelection.size();
    info.aheadCount = m_remoteStatus.ahead;
    info.behindCount = m_remoteStatus.behind;
    info.lastFetchedTime = m_remoteStatus.lastFetchedText;
    return info;
}

bool GitCliService::openRepository(const QString &pathOrId)
{
    m_repositoryGeneration.fetch_add(1);
    QString targetPath = pathOrId;
    if (!m_knownRepos.contains(targetPath)) {
        for (auto it = m_knownRepos.begin(); it != m_knownRepos.end(); ++it) {
            if (it.value() == pathOrId) {
                targetPath = it.key();
                break;
            }
        }
    }

    if (!m_repositoryReader.open(targetPath)) {
        // Directory missing or not a valid Git repo -> enter missing repository state
        m_repoPath = targetPath;
        m_repoName = m_knownRepos.value(targetPath, QFileInfo(targetPath).fileName());
        if (m_repoName.isEmpty()) m_repoName = "Repository";
        m_isMissing = true;
        m_refreshInProgress = false;
        m_remoteStatus.isFetching = false;
        m_remoteStatus.isPulling = false;
        m_remoteStatus.isPushing = false;
        m_knownRepos.insert(m_repoPath, m_repoName);
        saveRepositories();

        invalidateRepositoryCaches();
        m_fileSelection.clear();
        clearUndoState();

        RepositoryInfo missingInfo;
        missingInfo.id = m_repoPath;
        missingInfo.name = m_repoName;
        missingInfo.path = m_repoPath;
        missingInfo.isMissing = true;
        missingInfo.remoteUrl = m_repoRemotes.value(m_repoPath);

        emit repositoryChanged(missingInfo);
        emitRepositoryRefreshSignals();
        return true;
    }

    if (m_repositoryReader.workTreePath().isEmpty()) {
        m_refreshInProgress = false;
        emit operationFailed("Bare repositories are not supported; select a repository with a working tree");
        return false;
    }

    m_isMissing = false;
    m_refreshInProgress = false;
    m_remoteStatus.isFetching = false;
    m_remoteStatus.isPulling = false;
    m_remoteStatus.isPushing = false;
    const QString discoveredPath = m_repositoryReader.workTreePath().isEmpty()
        ? QDir::cleanPath(targetPath)
        : m_repositoryReader.workTreePath();
    m_repoPath = discoveredPath;
    m_repoName = QFileInfo(m_repoPath).fileName();
    if (m_repoName.isEmpty()) m_repoName = "Repository";

    // Cache remote URL if available
    QString remote = getRemoteUrl("origin");
    if (!remote.isEmpty()) {
        m_repoRemotes.insert(m_repoPath, redactRemoteUrl(remote));
    }

    // Restore this repo's persisted last-fetch time so "Last fetched" survives restarts.
    m_lastFetchTime = m_repoFetchTimes.value(m_repoPath);

    m_knownRepos.insert(m_repoPath, m_repoName);
    saveRepositories();

    // Reset selection & undo state. The watcher belongs to the GUI thread, so
    // install it there even when this operation is running in the loader thread.
    m_fileSelection.clear();
    clearUndoState();
    QMetaObject::invokeMethod(this, [this]() { setupFileSystemWatcher(); }, Qt::QueuedConnection);

    preloadRepositoryCaches();
    emitRepositoryRefreshSignals();
    emit operationSucceeded(QString("Opened repository '%1'").arg(m_repoName));
    return true;
}

bool GitCliService::addRepository(const QString &name, const QString &path)
{
    QString targetPath = path.trimmed();
    if (targetPath.startsWith("file://")) {
        targetPath = QUrl(targetPath).toLocalFile();
    }

    GitRepositoryReader reader;
    if (!reader.open(targetPath) || reader.workTreePath().isEmpty()) {
        emit operationFailed(QString("Directory '%1' is not a non-bare Git repository").arg(targetPath));
        return false;
    }

    QString realPath = reader.workTreePath();
    QString realName = name.trimmed().isEmpty() ? QFileInfo(realPath).fileName() : name.trimmed();
    m_knownRepos.insert(realPath, realName);
    saveRepositories();

    return openRepository(realPath);
}

bool GitCliService::relocateRepository(const QString &oldPath, const QString &newPath)
{
    QString cleanNew = newPath.trimmed();
    if (cleanNew.startsWith("file://")) {
        cleanNew = QUrl(cleanNew).toLocalFile();
    }
    cleanNew = QDir::cleanPath(cleanNew);

    GitRepositoryReader reader;
    if (!reader.open(cleanNew) || reader.workTreePath().isEmpty()) {
        emit operationFailed(QString("Directory '%1' is not a non-bare Git repository").arg(cleanNew));
        return false;
    }

    QString realPath = reader.workTreePath();
    QString repoName = m_knownRepos.value(oldPath, QFileInfo(realPath).fileName());
    if (repoName.isEmpty()) repoName = QFileInfo(realPath).fileName();

    QString remote = m_repoRemotes.value(oldPath);

    m_knownRepos.remove(oldPath);
    m_repoFetchTimes.remove(oldPath);
    m_repoRemotes.remove(oldPath);

    m_knownRepos.insert(realPath, repoName);
    if (!remote.isEmpty()) {
        m_repoRemotes.insert(realPath, remote);
    }

    saveRepositories();
    saveFetchTimes();

    return openRepository(realPath);
}

bool GitCliService::cloneRepository(const QString &url, const QString &targetPath)
{
    QString cleanUrl = url.trimmed();
    QString cleanPath = targetPath.trimmed();
    if (cleanPath.startsWith("file://")) {
        cleanPath = QUrl(cleanPath).toLocalFile();
    }
    cleanPath = QDir::cleanPath(cleanPath);

    if (cleanUrl.isEmpty() || cleanPath.isEmpty()) {
        emit operationFailed(tr("Please provide a valid repository URL and destination directory."));
        return false;
    }

    QDir targetDir(cleanPath);
    if (targetDir.exists() && !targetDir.isEmpty()) {
        emit operationFailed(tr("Destination directory '%1' exists and is not empty.").arg(cleanPath));
        return false;
    }

    GitResult res = runGit({"clone", "--progress", "--", cleanUrl, cleanPath}, QString(), 180000);
    if (!res.success) {
        emit operationFailed(formatGitError(res.stdErr.isEmpty() ? res.stdOut : res.stdErr, tr("Git clone failed")));
        return false;
    }

    QString repoName = QFileInfo(cleanPath).fileName();
    m_knownRepos.insert(cleanPath, repoName);
    m_repoRemotes.insert(cleanPath, redactRemoteUrl(cleanUrl));
    saveRepositories();

    return openRepository(cleanPath);
}

bool GitCliService::recheckRepository(const QString &pathOrId)
{
    QString targetPath = pathOrId.isEmpty() ? m_repoPath : pathOrId;
    if (!m_knownRepos.contains(targetPath)) {
        for (auto it = m_knownRepos.begin(); it != m_knownRepos.end(); ++it) {
            if (it.value() == targetPath) {
                targetPath = it.key();
                break;
            }
        }
    }

    GitRepositoryReader reader;
    if (reader.open(targetPath) && !reader.workTreePath().isEmpty()) {
        return openRepository(targetPath);
    }

    emit operationFailed(QString("Repository is still not found at '%1'").arg(targetPath));
    return false;
}

bool GitCliService::removeRepository(const QString &repoIdOrPath)
{
    QString targetPath = repoIdOrPath;
    if (!m_knownRepos.contains(targetPath)) {
        for (auto it = m_knownRepos.begin(); it != m_knownRepos.end(); ++it) {
            if (it.value() == repoIdOrPath) {
                targetPath = it.key();
                break;
            }
        }
    }

    if (m_knownRepos.remove(targetPath) > 0) {
        m_repoRemotes.remove(targetPath);
        saveRepositories();
        if (m_repoFetchTimes.remove(targetPath) > 0) {
            saveFetchTimes();
        }
        if (m_repoPath == targetPath) {
            if (!m_knownRepos.isEmpty()) {
                openRepository(m_knownRepos.firstKey());
            } else {
                m_repoPath.clear();
                m_repoName.clear();
                m_isMissing = false;
                emit repositoryChanged({});
            }
        } else {
            auto cur = getCurrentRepository();
            emit repositoryChanged(cur ? *cur : RepositoryInfo{});
        }
        emit operationSucceeded("Repository removed from list");
        return true;
    }
    return false;
}

QList<BranchInfo> GitCliService::getBranches()
{
    QMutexLocker locker(&m_cacheMutex);
    if (m_branchesCacheValid) return m_branchesCache;
    if (m_repoPath.isEmpty()) return {};

    if (m_repositoryReader.directReadSupported()) {
        m_branchesCache = m_repositoryReader.branches();
    }

    if (!m_repositoryReader.directReadSupported() || m_branchesCache.isEmpty()) {
        locker.unlock();
        const GitResult refs = runGit({"for-each-ref", "--format=%(refname)%x1f%(objectname)%x1e", "refs/heads", "refs/remotes"}, QString(), 3000);
        locker.relock();
        if (refs.success) {
            QList<BranchInfo> parsed;
            const QStringList records = refs.stdOut.split(QChar('\x1e'), Qt::SkipEmptyParts);
            for (const QString &record : records) {
                const QStringList fields = record.split(QChar('\x1f'));
                if (fields.size() < 2 || fields[0].isEmpty()) continue;
                const QString refName = fields[0].trimmed();
                BranchInfo branch;
                branch.isRemote = refName.startsWith("refs/remotes/");
                branch.name = branch.isRemote ? refName.mid(QStringLiteral("refs/remotes/").size())
                                              : refName.mid(QStringLiteral("refs/heads/").size());
                if (branch.name.isEmpty()) continue;
                branch.isDefault = branch.name == "main" || branch.name == "master" ||
                                   branch.name == "origin/main" || branch.name == "origin/master";
                branch.tipCommitSha = fields[1].trimmed().left(7);
                parsed.append(branch);
            }
            m_branchesCache = parsed;
        }
    }

    m_branchesCacheValid = true;
    return m_branchesCache;
}

std::optional<BranchInfo> GitCliService::getCurrentBranch()
{
    QMutexLocker locker(&m_cacheMutex);
    if (m_currentBranchCacheValid) return m_currentBranchCache;
    if (m_repoPath.isEmpty()) return std::nullopt;

    if (m_repositoryReader.directReadSupported()) {
        m_currentBranchCache = m_repositoryReader.currentBranch();
    }

    if (!m_repositoryReader.directReadSupported() || !m_currentBranchCache) {
        locker.unlock();
        const GitResult nameRes = runGit({"symbolic-ref", "--quiet", "--short", "HEAD"}, QString(), 2000);
        const GitResult shaRes = runGit({"rev-parse", "--verify", "HEAD"}, QString(), 2000);
        locker.relock();
        BranchInfo branch;
        if (nameRes.success && !nameRes.stdOut.trimmed().isEmpty()) {
            branch.name = nameRes.stdOut.trimmed();
        } else if (shaRes.success) {
            branch.name = "(detached)";
        }
        if (shaRes.success) branch.tipCommitSha = shaRes.stdOut.trimmed().left(7);
        if (!branch.name.isEmpty()) {
            branch.isCurrent = true;
            branch.isDefault = branch.name == "main" || branch.name == "master";
            m_currentBranchCache = branch;
        }
    }

    m_currentBranchCacheValid = true;
    return m_currentBranchCache;
}

bool GitCliService::switchBranch(const QString &branchName)
{
    if (m_repoPath.isEmpty() || !isValidBranchName(branchName)) {
        emit operationFailed("Invalid branch name");
        return false;
    }

    const QString name = branchName.trimmed();
    GitResult res = runGit({"checkout", name}, QString(), 15000);
    if (!res.success) {
        res = runGit({"switch", "--", name}, QString(), 15000);
    }

    if (res.success) {
        clearUndoState();
        invalidateRepositoryCaches();
        emit branchListChanged();
        if (auto b = getCurrentBranch()) {
            emit currentBranchChanged(*b);
        }
        emit changedFilesUpdated();
        emit commitHistoryUpdated();
        getRemoteStatus();
        emit operationSucceeded(QString("Switched to branch '%1'").arg(branchName));
        return true;
    }

    emit operationFailed(formatGitError(res.stdErr, "Failed to switch branch"));
    return false;
}

bool GitCliService::createBranch(const QString &branchName, const QString &sourceBranch)
{
    if (m_repoPath.isEmpty() || !isValidBranchName(branchName)) {
        emit operationFailed("Invalid branch name");
        return false;
    }

    QString name = branchName.trimmed();
    QStringList args = {"checkout", "-b", name};
    if (!sourceBranch.trimmed().isEmpty()) {
        const QString source = sourceBranch.trimmed();
        if (source.startsWith('-')) {
            emit operationFailed("Invalid source branch");
            return false;
        }
        args.append(source);
    }

    GitResult res = runGit(args, QString(), 10000);
    if (res.success) {
        clearUndoState();
        invalidateRepositoryCaches();
        emit branchListChanged();
        if (auto b = getCurrentBranch()) {
            emit currentBranchChanged(*b);
        }
        emit operationSucceeded(QString("Created and switched to branch '%1'").arg(name));
        return true;
    }

    emit operationFailed(formatGitError(res.stdErr, "Failed to create branch"));
    return false;
}

bool GitCliService::deleteBranch(const QString &branchName)
{
    if (m_repoPath.isEmpty() || !isValidBranchName(branchName)) {
        emit operationFailed("Invalid branch name");
        return false;
    }

    GitResult res = runGit({"branch", "-d", "--", branchName.trimmed()}, QString(), 10000);
    if (res.success) {
        clearUndoState();
        invalidateRepositoryCaches();
        emit branchListChanged();
        emit operationSucceeded(QString("Deleted branch '%1'").arg(branchName));
        return true;
    }

    emit operationFailed(formatGitError(res.stdErr, "Failed to delete branch"));
    return false;
}

QList<FileChange> GitCliService::getChangedFiles()
{
    QMutexLocker locker(&m_cacheMutex);
    if (m_changedFilesCacheValid) return m_changedFilesCache;
    if (m_repoPath.isEmpty()) return {};

    locker.unlock();

    // Query changed & untracked files with normal untracked scanning for max speed in huge repos
    GitResult statusRes = runGit({"--no-optional-locks", "status", "--porcelain=v1", "-unormal", "-z"}, QString(), 10000);
    if (!statusRes.success) {
        locker.relock();
        m_changedFilesCacheValid = false;
        return m_changedFilesCache;
    }
    if (statusRes.stdOut.isEmpty()) {
        locker.relock();
        m_fileSelection.clear();
        m_changedFilesCache.clear();
        m_changedFilesCacheValid = true;
        return m_changedFilesCache;
    }

    const QStringList records = statusRes.stdOut.split(QChar('\0'), Qt::KeepEmptyParts);
    if (records.isEmpty() || (records.size() == 1 && records.first().isEmpty())) {
        locker.relock();
        m_fileSelection.clear();
        m_changedFilesCache.clear();
        m_changedFilesCacheValid = true;
        return m_changedFilesCache;
    }

    struct StatusRecord {
        QChar x{' '};
        QChar y{' '};
        QString filePath;
        QString oldFilePath;
    };
    QList<StatusRecord> statusRecords;
    QStringList trackedModifiedPaths;
    for (int i = 0; i < records.size(); ++i) {
        const QString &record = records.at(i);
        if (record.size() < 4) continue;
        StatusRecord parsed;
        parsed.x = record.at(0);
        parsed.y = record.at(1);
        parsed.filePath = record.mid(3);
        if ((parsed.x == 'R' || parsed.y == 'R' || parsed.x == 'C' || parsed.y == 'C') && i + 1 < records.size()) {
            parsed.oldFilePath = records.at(++i);
        }
        statusRecords.append(parsed);
        if (parsed.x != '?' && parsed.y != '?') {
            trackedModifiedPaths.append(parsed.filePath);
            if (!parsed.oldFilePath.isEmpty()) trackedModifiedPaths.append(parsed.oldFilePath);
        }
    }

    // Only run diff numstat if there are actually modified tracked files.
    QMap<QString, QPair<int, int>> numStats;
    if (!trackedModifiedPaths.isEmpty()) {
        QStringList numArgs = {"diff", "HEAD", "--numstat", "-z", "--"};
        if (trackedModifiedPaths.size() <= 100) numArgs.append(trackedModifiedPaths);
        GitResult numRes = runGit(numArgs, QString(), 6000);
        if (!numRes.success) {
            QStringList cachedArgs = {"diff", "--cached", "--numstat", "-z", "--"};
            if (trackedModifiedPaths.size() <= 100) cachedArgs.append(trackedModifiedPaths);
            numRes = runGit(cachedArgs, QString(), 6000);
        }
        if (numRes.success) {
            const QStringList statRecords = numRes.stdOut.split(QChar('\0'), Qt::KeepEmptyParts);
            for (int i = 0; i < statRecords.size(); ++i) {
                const QStringList parts = statRecords.at(i).split('\t');
                if (parts.size() >= 3 && !parts[2].isEmpty()) {
                    numStats[parts[2]] = qMakePair(parts[0] == "-" ? 0 : parts[0].toInt(),
                                                    parts[1] == "-" ? 0 : parts[1].toInt());
                } else if (parts.size() >= 3 && parts[2].isEmpty() && i + 2 < statRecords.size()) {
                    const auto stats = qMakePair(parts[0] == "-" ? 0 : parts[0].toInt(),
                                                 parts[1] == "-" ? 0 : parts[1].toInt());
                    numStats[statRecords.at(i + 1)] = stats;
                    numStats[statRecords.at(i + 2)] = stats;
                    i += 2;
                }
            }
        }
    }

    QList<FileChange> files;
    files.reserve(statusRecords.size());

    int idx = 0;
    locker.relock();
    for (const StatusRecord &record : statusRecords) {
        const QChar x = record.x;
        const QChar y = record.y;
        const QString &filePath = record.filePath;
        const QString &oldPath = record.oldFilePath;

        FileChange fc;
        fc.id = QString("fc-%1").arg(idx++);
        fc.filePath = filePath;
        fc.oldFilePath = oldPath;

        if (x == '?' && y == '?') {
            fc.status = FileChangeType::Untracked;
            fc.additions = 1;
            fc.deletions = 0;
        } else if (x == 'A' || y == 'A') {
            fc.status = FileChangeType::Added;
        } else if (x == 'D' || y == 'D') {
            fc.status = FileChangeType::Deleted;
        } else if (x == 'R' || y == 'R') {
            fc.status = FileChangeType::Renamed;
        } else {
            fc.status = FileChangeType::Modified;
        }

        if (!m_fileSelection.contains(filePath)) {
            m_fileSelection.insert(filePath, true);
        }
        fc.isSelected = m_fileSelection.value(filePath, true);

        if (numStats.contains(filePath)) {
            fc.additions = numStats[filePath].first;
            fc.deletions = numStats[filePath].second;
        }

        files.append(fc);
    }

    m_changedFilesCache = files;
    m_changedFilesCacheValid = true;
    return m_changedFilesCache;
}

void GitCliService::setFileSelected(const QString &filePath, bool selected)
{
    QMutexLocker locker(&m_cacheMutex);
    m_fileSelection.insert(filePath, selected);
    if (m_changedFilesCacheValid) {
        for (auto &file : m_changedFilesCache) {
            if (file.filePath == filePath) file.isSelected = selected;
        }
    }
    emit changedFilesUpdated();
}

void GitCliService::setAllFilesSelected(bool selected)
{
    QMutexLocker locker(&m_cacheMutex);
    for (auto it = m_fileSelection.begin(); it != m_fileSelection.end(); ++it) {
        it.value() = selected;
    }
    if (m_changedFilesCacheValid) {
        for (auto &file : m_changedFilesCache) file.isSelected = selected;
    }
    emit changedFilesUpdated();
}

bool GitCliService::discardFileChanges(const QString &filePath)
{
    if (m_repoPath.isEmpty() || !isSafeRepositoryPath(filePath)) return false;

    GitResult check = runGit({"--no-optional-locks", "status", "--porcelain=v1", "-z", "--", filePath}, QString(), 3000);
    const bool isUntracked = check.stdOut.startsWith("?? ");
    const bool isAdded = check.stdOut.startsWith("A ") || check.stdOut.startsWith(" A");
    if (isUntracked || isAdded) {
        const GitResult remove = runGit({"rm", "-f", "--ignore-unmatch", "--", filePath}, QString(), 3000);
        if (!remove.success && !isUntracked) {
            emit operationFailed(formatGitError(remove.stdErr, "Failed to discard file changes"));
            return false;
        }
        QString fullPath = m_repoPath + "/" + filePath;
        QFileInfo fi(fullPath);
        if (fi.exists()) {
            if (fi.isSymLink() || !fi.isDir()) {
                if (!QFile::remove(fullPath)) {
                    emit operationFailed("Failed to remove the selected file");
                    return false;
                }
            } else if (!QDir(fullPath).removeRecursively()) {
                emit operationFailed("Failed to remove the selected directory");
                return false;
            }
        }
    } else {
        const GitResult restore = runGit({"restore", "--staged", "--worktree", "--", filePath}, QString(), 5000);
        if (!restore.success) {
            emit operationFailed(formatGitError(restore.stdErr, "Failed to discard file changes"));
            return false;
        }
    }

    invalidateRepositoryCaches();
    emit changedFilesUpdated();
    emit operationSucceeded(QString("Discarded changes in '%1'").arg(filePath));
    return true;
}

bool GitCliService::discardAllChanges()
{
    if (m_repoPath.isEmpty()) return false;

    const GitResult restore = runGit({"restore", "--staged", "--worktree", "--", "."}, QString(), 8000);
    const GitResult clean = runGit({"clean", "-fd", "--", "."}, QString(), 8000);
    if (!restore.success || !clean.success) {
        emit operationFailed(formatGitError(!restore.success ? restore.stdErr : clean.stdErr,
                                            "Failed to discard all changes"));
        return false;
    }

    invalidateRepositoryCaches();
    emit changedFilesUpdated();
    emit operationSucceeded("Discarded all uncommitted changes");
    return true;
}

QList<DiffLine> GitCliService::parseDiffOutput(const QString &diffText)
{
    QList<DiffLine> lines;
    if (diffText.isEmpty()) return lines;

    const QStringList rawLines = diffText.split('\n');
    lines.reserve(rawLines.size());
    static const QRegularExpression hunkRegex(R"(^@@\s+-(\d+)(?:,(\d+))?\s+\+(\d+)(?:,(\d+))?\s+@@(.*)$)");

    int curOld = 0;
    int curNew = 0;
    bool inHunk = false;

    for (const QString &line : rawLines) {
        auto match = hunkRegex.match(line);
        if (match.hasMatch()) {
            inHunk = true;
            curOld = match.captured(1).toInt();
            curNew = match.captured(3).toInt();

            DiffLine dl;
            dl.oldLineNumber = -1;
            dl.newLineNumber = -1;
            dl.type = DiffLineType::HunkHeader;
            dl.content = line;
            lines.append(dl);
            continue;
        }

        if (!inHunk) continue; // Skip git headers before first @@ hunk

        DiffLine dl;
        if (line.startsWith('+')) {
            dl.oldLineNumber = -1;
            dl.newLineNumber = curNew++;
            dl.type = DiffLineType::Addition;
            dl.content = line.mid(1);
            lines.append(dl);
        } else if (line.startsWith('-')) {
            dl.oldLineNumber = curOld++;
            dl.newLineNumber = -1;
            dl.type = DiffLineType::Deletion;
            dl.content = line.mid(1);
            lines.append(dl);
        } else if (line.startsWith(' ')) {
            dl.oldLineNumber = curOld++;
            dl.newLineNumber = curNew++;
            dl.type = DiffLineType::Context;
            dl.content = line.mid(1);
            lines.append(dl);
        } else if (line.startsWith('\\')) {
            dl.oldLineNumber = -1;
            dl.newLineNumber = -1;
            dl.type = DiffLineType::Context;
            dl.content = line;
            lines.append(dl);
        }
    }

    return lines;
}

QList<DiffLine> GitCliService::getDiffForFile(const QString &filePath, const QString &oldFilePath)
{
    QMutexLocker locker(&m_cacheMutex);
    QString cacheKey = (oldFilePath.isEmpty() || oldFilePath == filePath) ? filePath : QString("%1|%2").arg(oldFilePath, filePath);
    if (m_fileDiffCacheValid && m_fileDiffCachePath == cacheKey) return m_fileDiffCache;
    if (m_repoPath.isEmpty() || (filePath.isEmpty() && oldFilePath.isEmpty()) ||
        (!filePath.isEmpty() && !isSafeRepositoryPath(filePath)) ||
        (!oldFilePath.isEmpty() && !isSafeRepositoryPath(oldFilePath))) return {};

    locker.unlock();

    QString effectivePath = filePath.isEmpty() ? oldFilePath : filePath;

    // If untracked file or folder, read safely from disk
    GitResult statusCheck = runGit({"--no-optional-locks", "status", "--porcelain=v1", "--", effectivePath}, QString(), 2000);
    if (statusCheck.stdOut.startsWith("??")) {
        QString fullPath = m_repoPath + "/" + effectivePath;
        QFileInfo fi(fullPath);
        if (fi.isDir()) {
            DiffLine hunk;
            hunk.oldLineNumber = -1;
            hunk.newLineNumber = -1;
            hunk.type = DiffLineType::HunkHeader;
            hunk.content = QString("@@ Directory: %1 @@").arg(effectivePath);
            DiffLine dl;
            dl.oldLineNumber = -1;
            dl.newLineNumber = 1;
            dl.type = DiffLineType::Context;
            dl.content = "Untracked Directory";
            QList<DiffLine> directoryDiff{hunk, dl};
            locker.relock();
            if (cacheKey == m_fileDiffCachePath) {
                m_fileDiffCache = directoryDiff;
                m_fileDiffCacheValid = true;
            }
            return directoryDiff;
        }

        QFile f(fullPath);
        if (!f.open(QIODevice::ReadOnly | QIODevice::Text)) return {};
        QTextStream in(&f);
        QList<DiffLine> lines;
        int lineNum = 1;

        DiffLine hunk;
        hunk.oldLineNumber = -1;
        hunk.newLineNumber = -1;
        hunk.type = DiffLineType::HunkHeader;
        hunk.content = "@@ -0,0 +1,1 @@";
        lines.append(hunk);

        // Cap preview to 1000 lines to prevent UI freezing on giant files
        while (!in.atEnd() && lineNum <= 1000) {
            DiffLine dl;
            dl.oldLineNumber = -1;
            dl.newLineNumber = lineNum++;
            dl.type = DiffLineType::Addition;
            dl.content = in.readLine();
            lines.append(dl);
        }
        locker.relock();
        if (cacheKey == m_fileDiffCachePath) {
            m_fileDiffCache = lines;
            m_fileDiffCacheValid = true;
        }
        return lines;
    }

    // Tracked diff against HEAD with rename detection
    QStringList argsHead = {"diff", "-M", "HEAD", "--"};
    if (!oldFilePath.isEmpty() && oldFilePath != filePath) argsHead.append(oldFilePath);
    if (!filePath.isEmpty()) argsHead.append(filePath);

    GitResult res = runGit(argsHead, QString(), 4000);
    if (res.stdOut.isEmpty()) {
        QStringList argsCached = {"diff", "-M", "--cached", "--"};
        if (!oldFilePath.isEmpty() && oldFilePath != filePath) argsCached.append(oldFilePath);
        if (!filePath.isEmpty()) argsCached.append(filePath);
        res = runGit(argsCached, QString(), 4000);
        if (res.stdOut.isEmpty()) {
            QStringList argsWorking = {"diff", "-M", "--"};
            if (!oldFilePath.isEmpty() && oldFilePath != filePath) argsWorking.append(oldFilePath);
            if (!filePath.isEmpty()) argsWorking.append(filePath);
            res = runGit(argsWorking, QString(), 4000);
        }
    }
    QList<DiffLine> lines = parseDiffOutput(res.stdOut);
    locker.relock();
    if (cacheKey == m_fileDiffCachePath) {
        m_fileDiffCache = lines;
        m_fileDiffCacheValid = true;
    }
    return lines;
}

bool GitCliService::isFileMetadataOnly(const QString &filePath)
{
    if (m_repoPath.isEmpty() || filePath.isEmpty()) return false;

    GitResult summary = runGit({"diff", "HEAD", "--summary", "--", filePath}, QString(), 3000);
    if (!summary.success || summary.stdOut.trimmed().isEmpty()) return false;

    const QString summaryText = summary.stdOut.toLower();
    if (!summaryText.contains("mode change") &&
        !summaryText.contains("new file mode") &&
        !summaryText.contains("deleted file mode")) {
        return false;
    }

    // A file can have both a mode and content change. Only classify it as
    // metadata-only when Git reports no line-level changes at all.
    GitResult numstat = runGit({"diff", "HEAD", "--numstat", "--", filePath}, QString(), 3000);
    return numstat.success && numstat.stdOut.trimmed().isEmpty();
}

QList<DiffLine> GitCliService::getDiffForCommitFile(const QString &commitSha, const QString &filePath, const QString &oldFilePath)
{
    static const QRegularExpression revisionRegex("^[0-9a-fA-F]{4,64}$");
    if (m_repoPath.isEmpty() || !revisionRegex.match(commitSha.trimmed()).hasMatch() ||
        (filePath.isEmpty() && oldFilePath.isEmpty())) return {};

    QStringList args = {"show", "-M", "--end-of-options", commitSha.trimmed(), "--"};
    if (!oldFilePath.isEmpty() && oldFilePath != filePath) {
        args.append(oldFilePath);
    }
    if (!filePath.isEmpty()) {
        args.append(filePath);
    }
    GitResult res = runGit(args, QString(), 4000);
    return parseDiffOutput(res.stdOut);
}

QList<DiffLine> GitCliService::getDiffForStashFile(const QString &stashId, const QString &filePath)
{
    if (m_repoPath.isEmpty() || (!filePath.isEmpty() && !isSafeRepositoryPath(filePath))) return {};
    QString id = stashId.isEmpty() ? "stash@{0}" : stashId;
    if (!isValidStashId(id)) return {};
    if (!id.startsWith("stash@{")) id = QString("stash@{%1}").arg(id);

    QStringList args = {"diff", "-M", QString("%1^").arg(id), id, "--"};
    if (!filePath.isEmpty()) {
        args.append(filePath);
    }
    GitResult res = runGit(args, QString(), 4000);
    if (!res.stdOut.isEmpty()) return parseDiffOutput(res.stdOut);

    // `git stash push -u` stores untracked files in a third parent. The
    // normal stash^..stash diff cannot see those paths, so inspect that
    // parent as a root commit when the tracked diff is empty.
    const GitResult untrackedParent = runGit({"rev-parse", "--verify", QString("%1^3").arg(id)}, QString(), 2000);
    if (untrackedParent.success) {
        const QString parent = untrackedParent.stdOut.trimmed();
        QStringList untrackedArgs = {"show", "-M", "--end-of-options", parent, "--"};
        if (!filePath.isEmpty()) untrackedArgs.append(filePath);
        const GitResult untracked = runGit(untrackedArgs, QString(), 4000);
        return parseDiffOutput(untracked.stdOut);
    }
    return {};
}

QByteArray GitCliService::getFileBlob(const QString &filePath, const QString &ref)
{
    if (m_repoPath.isEmpty() || filePath.isEmpty() || !isSafeRepositoryPath(filePath)) return {};

    if (ref.isEmpty()) {
        // Read directly from disk (working copy)
        QString fullPath = m_repoPath + "/" + filePath;
        QFile file(fullPath);
        if (file.open(QIODevice::ReadOnly)) {
            return file.readAll();
        }
        return {};
    }

    if (ref.startsWith('-')) return {};

    // Capture binary output from git show <ref>:<path>
    QProcess process;
    process.setWorkingDirectory(m_repoPath);
    QProcessEnvironment env = QProcessEnvironment::systemEnvironment();
    env.insert("LC_ALL", "C");
    env.insert("GIT_TERMINAL_PROMPT", "0");
    process.setProcessEnvironment(env);

    process.start("git", {"show", "--end-of-options", QString("%1:%2").arg(ref, filePath)});
    bool finished = process.waitForFinished(5000);
    if (!finished) {
        process.kill();
        process.waitForFinished(500);
        return {};
    }

    if (process.exitCode() == 0) {
        return process.readAllStandardOutput();
    }
    return {};
}

bool GitCliService::isImageFile(const QString &filePath) const
{
    if (filePath.isEmpty()) return false;
    static const QStringList imageExtensions = {
        "png", "jpg", "jpeg", "gif", "webp", "svg", "bmp", "ico", "avif", "tif", "tiff"
    };
    const QString ext = QFileInfo(filePath).suffix().toLower();
    return imageExtensions.contains(ext);
}

QList<CommitItem> GitCliService::getCommitHistory(int limit)
{
    QMutexLocker locker(&m_cacheMutex);
    if (m_repoPath.isEmpty()) return {};
    const int fetchCount = std::max(limit > 0 ? limit : 200, 200);
    if (m_commitHistoryCacheValid) {
        return (limit > 0) ? m_commitHistoryCache.mid(0, limit) : m_commitHistoryCache;
    }

    // Fast Git CLI query using porcelain record/unit separators
    locker.unlock();
    GitResult logRes = runGit({"log", QString("-%1").arg(fetchCount), "--format=%H%x1f%h%x1f%s%x1f%b%x1f%an%x1f%ae%x1f%at%x1e"}, QString(), 4000);
    locker.relock();
    if (logRes.success && !logRes.stdOut.isEmpty()) {
        QList<CommitItem> list;
        const QStringList records = logRes.stdOut.split(QChar('\x1e'), Qt::SkipEmptyParts);
        for (const QString &rec : records) {
            QStringList fields = rec.split(QChar('\x1f'));
            if (fields.size() < 7) continue;
            CommitItem item;
            item.sha = fields[0].trimmed();
            item.shortSha = fields[1].trimmed();
            item.summary = fields[2].trimmed();
            item.description = fields[3].trimmed();
            item.authorName = fields[4].trimmed();
            item.authorEmail = fields[5].trimmed();
            qint64 epoch = fields[6].trimmed().toLongLong();
            item.timestamp = QDateTime::fromSecsSinceEpoch(epoch, QTimeZone::UTC);
            item.relativeTime = formatRelativeTime(item.timestamp);

            static const QRegularExpression coRegex(R"(Co-authored-by:\s*(.*?)(?:<|$))", QRegularExpression::CaseInsensitiveOption);
            auto it = coRegex.globalMatch(item.description);
            while (it.hasNext()) {
                QString ca = it.next().captured(1).trimmed();
                if (!ca.isEmpty()) item.coAuthors.append(ca);
            }
            list.append(item);
        }
        m_commitHistoryCache = list;
        m_commitHistoryCacheValid = true;
        return (limit > 0) ? m_commitHistoryCache.mid(0, limit) : m_commitHistoryCache;
    }

    m_commitHistoryCache = m_repositoryReader.commitHistory(fetchCount);
    m_commitHistoryCacheValid = true;
    return (limit > 0) ? m_commitHistoryCache.mid(0, limit) : m_commitHistoryCache;
}

std::optional<CommitItem> GitCliService::getCommitDetails(const QString &sha)
{
    QMutexLocker locker(&m_cacheMutex);
    static const QRegularExpression revisionRegex("^[0-9a-fA-F]{4,64}$");
    if (m_repoPath.isEmpty() || !revisionRegex.match(sha.trimmed()).hasMatch()) return std::nullopt;

    if (m_commitDetailsCache.contains(sha)) {
        return m_commitDetailsCache.value(sha);
    }

    locker.unlock();
    // Fast CLI queries for commit metadata, numstat, and name-status
    const QString revision = sha.trimmed();
    GitResult showRes = runGit({"show", "-s", "--format=%H%x1f%h%x1f%s%x1f%b%x1f%an%x1f%ae%x1f%at", "--end-of-options", revision}, QString(), 4000);
    GitResult statusRes = runGit({"diff-tree", "--no-commit-id", "--name-status", "-r", "-M", "-z", "--end-of-options", revision}, QString(), 4000);
    GitResult numstatRes = runGit({"diff-tree", "--no-commit-id", "--numstat", "-r", "-M", "-z", "--end-of-options", revision}, QString(), 4000);
    locker.relock();

    if (showRes.success && !showRes.stdOut.isEmpty()) {
        const QStringList fields = showRes.stdOut.split(QChar('\x1f'));

        if (fields.size() >= 7) {
            CommitItem commit;
            commit.sha = fields[0].trimmed();
            commit.shortSha = fields[1].trimmed();
            commit.summary = fields[2].trimmed();
            commit.description = fields[3].trimmed();
            commit.authorName = fields[4].trimmed();
            commit.authorEmail = fields[5].trimmed();
            const qint64 epoch = fields[6].trimmed().toLongLong();
            commit.timestamp = QDateTime::fromSecsSinceEpoch(epoch, QTimeZone::UTC);
            commit.relativeTime = formatRelativeTime(commit.timestamp);

            static const QRegularExpression coRegex(R"(Co-authored-by:\s*(.*?)(?:<|$))", QRegularExpression::CaseInsensitiveOption);
            auto it = coRegex.globalMatch(commit.description);
            while (it.hasNext()) {
                QString ca = it.next().captured(1).trimmed();
                if (!ca.isEmpty()) commit.coAuthors.append(ca);
            }

            // Parse numstat additions/deletions from -z output
            QHash<QString, QPair<int, int>> statMap;
            if (numstatRes.success && !numstatRes.stdOut.isEmpty()) {
                const QStringList tokens = numstatRes.stdOut.split(QChar('\0'));
                for (int i = 0; i < tokens.size(); ++i) {
                    const QString &token = tokens[i];
                    if (token.isEmpty()) continue;
                    const QStringList parts = token.split('\t');
                    if (parts.size() >= 3) {
                        int adds = (parts[0] == "-") ? 0 : parts[0].toInt();
                        int dels = (parts[1] == "-") ? 0 : parts[1].toInt();
                        QString path = parts.mid(2).join('\t').trimmed();
                        statMap[path] = qMakePair(adds, dels);
                    } else if (parts.size() >= 2 && i + 2 < tokens.size()) {
                        int adds = (parts[0] == "-") ? 0 : parts[0].toInt();
                        int dels = (parts[1] == "-") ? 0 : parts[1].toInt();
                        QString oldPath = tokens[++i].trimmed();
                        QString newPath = tokens[++i].trimmed();
                        statMap[newPath] = qMakePair(adds, dels);
                        statMap[oldPath] = qMakePair(adds, dels);
                    }
                }
            }

            // Parse NUL-delimited name-status for accurate paths and rename types.
            if (statusRes.success && !statusRes.stdOut.isEmpty()) {
                const QStringList tokens = statusRes.stdOut.split(QChar('\0'), Qt::KeepEmptyParts);
                int fcIndex = 0;
                for (int i = 0; i < tokens.size(); ++i) {
                    const QString code = tokens.at(i).trimmed();
                    if (code.isEmpty() || i + 1 >= tokens.size()) continue;

                    FileChange fc;
                    fc.id = QString("fc-%1").arg(fcIndex++);
                    if (code.startsWith('R') || code.startsWith('C')) {
                        fc.status = FileChangeType::Renamed;
                        fc.filePath = tokens.at(++i);
                        if (i + 1 < tokens.size()) fc.oldFilePath = tokens.at(++i);
                    } else {
                        fc.filePath = tokens.at(++i);
                        if (code.startsWith('A')) fc.status = FileChangeType::Added;
                        else if (code.startsWith('D')) {
                            fc.status = FileChangeType::Deleted;
                            fc.oldFilePath = fc.filePath;
                        } else fc.status = FileChangeType::Modified;
                    }

                    if (statMap.contains(fc.filePath)) {
                        fc.additions = statMap.value(fc.filePath).first;
                        fc.deletions = statMap.value(fc.filePath).second;
                    } else if (!fc.oldFilePath.isEmpty() && statMap.contains(fc.oldFilePath)) {
                        fc.additions = statMap.value(fc.oldFilePath).first;
                        fc.deletions = statMap.value(fc.oldFilePath).second;
                    }
                    commit.changedFiles.append(fc);
                }
            }

            m_commitDetailsCache[sha] = commit;
            return commit;
        }
    }

    auto fallback = m_repositoryReader.commitDetails(sha);
    if (fallback) {
        m_commitDetailsCache[sha] = *fallback;
    }
    return fallback;
}

bool GitCliService::createCommit(const QString &summary, const QString &description, const QStringList &coAuthors)
{
    if (m_repoPath.isEmpty() || summary.trimmed().isEmpty()) {
        emit operationFailed("Commit summary cannot be empty");
        return false;
    }

    auto changes = getChangedFiles();
    QStringList filesToAdd;
    QStringList filesToRm;
    QStringList filesToUnstage;

    for (const auto &f : changes) {
        if (f.isSelected) {
            if (f.status == FileChangeType::Deleted) {
                filesToRm.append(f.filePath);
            } else {
                filesToAdd.append(f.filePath);
            }
        } else if (f.status != FileChangeType::Untracked) {
            filesToUnstage.append(f.filePath);
            if (!f.oldFilePath.isEmpty() && f.oldFilePath != f.filePath) {
                filesToUnstage.append(f.oldFilePath);
            }
        }
    }

    if (filesToAdd.isEmpty() && filesToRm.isEmpty()) {
        emit operationFailed("No files selected to commit");
        return false;
    }

    auto runStageCommand = [this](const QStringList &args) {
        const GitResult result = runGit(args, QString(), 10000);
        if (!result.success) {
            emit operationFailed(formatGitError(result.stdErr, "Failed to prepare selected files for commit"));
            return false;
        }
        return true;
    };

    if (!filesToAdd.isEmpty()) {
        QStringList addArgs = {"add", "--"};
        addArgs.append(filesToAdd);
        if (!runStageCommand(addArgs)) return false;
    }
    if (!filesToRm.isEmpty()) {
        QStringList rmArgs = {"rm", "--ignore-unmatch", "--"};
        rmArgs.append(filesToRm);
        if (!runStageCommand(rmArgs)) return false;
    }
    if (!filesToUnstage.isEmpty()) {
        QStringList resetArgs = {"reset", "HEAD", "--"};
        resetArgs.append(filesToUnstage);
        if (!runStageCommand(resetArgs)) return false;
    }

    QString fullBody = description.trimmed();
    if (!coAuthors.isEmpty()) {
        if (!fullBody.isEmpty()) fullBody += "\n\n";
        for (const QString &ca : coAuthors) {
            fullBody += QString("Co-authored-by: %1\n").arg(ca);
        }
    }

    QStringList commitArgs = {"commit", "-m", summary.trimmed()};
    if (!fullBody.isEmpty()) {
        commitArgs.append("-m");
        commitArgs.append(fullBody);
    }

    GitResult res = runGit(commitArgs, QString(), 15000);
    if (res.success) {
        GitResult shaRes = runGit({"rev-parse", "HEAD"}, QString(), 2000);
        if (shaRes.success) {
            m_lastUndoCommitSha = shaRes.stdOut.trimmed();
            m_lastUndoCommitSummary = summary.trimmed();
            m_lastUndoCommitDescription = description;
            m_lastUndoCommitCoAuthors = coAuthors;
        }

        invalidateRepositoryCaches();
        getCommitHistory();
        getChangedFiles();
        getRemoteStatus();
        emit commitHistoryUpdated();
        emit changedFilesUpdated();
        emit operationSucceeded(QString("Committed: %1").arg(summary.trimmed()));
        return true;
    }

    emit operationFailed(formatGitError(res.stdErr, "Commit failed"));
    return false;
}

bool GitCliService::canUndoCommit() const
{
    if (m_repoPath.isEmpty() || m_lastUndoCommitSha.isEmpty()) return false;

    // Check if HEAD exists and matches the recorded undo commit SHA
    GitResult headRes = const_cast<GitCliService*>(this)->runGit({"rev-parse", "--verify", "HEAD"}, QString(), 2000);
    if (!headRes.success) return false;
    if (headRes.stdOut.trimmed() != m_lastUndoCommitSha) return false;

    // If upstream tracking branch is present, verify it has not been pushed
    GitResult upstreamRes = const_cast<GitCliService*>(this)->runGit({"rev-parse", "--verify", "@{upstream}"}, QString(), 2000);
    if (upstreamRes.success) {
        GitResult ancestorRes = const_cast<GitCliService*>(this)->runGit({"merge-base", "--is-ancestor", "HEAD", "@{upstream}"}, QString(), 2000);
        if (ancestorRes.success) {
            return false; // Already pushed to remote upstream
        }
        if (m_remoteStatus.ahead <= 0) {
            return false;
        }
    }

    return true;
}

bool GitCliService::undoLastCommit()
{
    if (m_repoPath.isEmpty()) {
        emit operationFailed("No repository active");
        return false;
    }

    if (!canUndoCommit()) {
        emit operationFailed("The latest commit is not eligible for undo");
        return false;
    }

    // Inspect current HEAD to extract its message and co-authors before resetting
    GitResult headLog = runGit({"log", "-1", "--format=%H\x1f%s\x1f%b\x1f%B"}, QString(), 3000);
    if (!headLog.success || headLog.stdOut.trimmed().isEmpty()) {
        emit operationFailed("No commit available to undo");
        return false;
    }

    QStringList p = headLog.stdOut.split('\x1f');
    QString sha = p.value(0).trimmed();
    QString summary = p.value(1).trimmed();
    QString description = p.value(2).trimmed();
    QString fullMessage = p.value(3);

    // Extract co-authors from description/fullMessage
    QStringList coAuthors;
    QRegularExpression coRegex(R"(Co-authored-by:\s*(.*?)(?:<|$))", QRegularExpression::CaseInsensitiveOption);
    auto it = coRegex.globalMatch(fullMessage);
    while (it.hasNext()) {
        auto m = it.next();
        QString ca = m.captured(1).trimmed();
        if (!ca.isEmpty()) coAuthors.append(ca);
    }

    // Clean description of co-authored trailers
    QString cleanedDesc = description;
    cleanedDesc.remove(QRegularExpression(R"(\n*Co-authored-by:.*$)", QRegularExpression::MultilineOption));
    cleanedDesc = cleanedDesc.trimmed();

    // Check if HEAD~1 exists (i.e. whether this is a root commit or normal commit)
    GitResult parentCheck = runGit({"rev-parse", "--verify", "HEAD~1"}, QString(), 2000);
    bool resetSuccess = false;

    if (parentCheck.success) {
        // Normal commit with parent: soft reset to HEAD~1
        GitResult res = runGit({"reset", "--soft", "HEAD~1"}, QString(), 10000);
        resetSuccess = res.success;
    } else {
        // Root commit with no parent: delete HEAD ref safely
        GitResult res = runGit({"update-ref", "-d", "HEAD"}, QString(), 10000);
        resetSuccess = res.success;
    }

    if (resetSuccess) {
        invalidateRepositoryCaches();
        m_lastUndoCommitSha.clear();
        m_lastUndoCommitSummary = summary;
        m_lastUndoCommitDescription = cleanedDesc;
        m_lastUndoCommitCoAuthors = coAuthors;

        // Select all restored files in working directory
        auto changed = getChangedFiles();
        for (const auto &f : changed) {
            m_fileSelection[f.filePath] = true;
        }

        getCommitHistory();
        getRemoteStatus();
        emit commitHistoryUpdated();
        emit changedFilesUpdated();
        emit operationSucceeded(QString("Undid commit: %1").arg(summary.isEmpty() ? sha.left(7) : summary));
        return true;
    }

    emit operationFailed("Failed to undo commit");
    return false;
}

bool GitCliService::revertCommit(const QString &sha)
{
    static const QRegularExpression revisionRegex("^[0-9a-fA-F]{4,64}$");
    if (m_repoPath.isEmpty() || !revisionRegex.match(sha.trimmed()).hasMatch()) return false;

    GitResult res = runGit({"revert", "--no-edit", sha.trimmed()}, QString(), 15000);
    if (res.success) {
        invalidateRepositoryCaches();
        emit commitHistoryUpdated();
        emit changedFilesUpdated();
        getRemoteStatus();
        emit operationSucceeded(QString("Reverted commit %1").arg(sha.left(7)));
        return true;
    }

    emit operationFailed(formatGitError(res.stdErr, "Revert failed"));
    return false;
}

bool GitCliService::hasRemote() const
{
    if (m_repoPath.isEmpty()) return false;
    GitResult res = const_cast<GitCliService*>(this)->runGit({"remote"}, QString(), 2000);
    return res.success && !res.stdOut.trimmed().isEmpty();
}

QString GitCliService::getRemoteUrl(const QString &remoteName) const
{
    if (m_repoPath.isEmpty()) return QString();
    QString target = remoteName.isEmpty() ? "origin" : remoteName.trimmed();
    static const QRegularExpression remoteNameRegex("^[A-Za-z0-9][A-Za-z0-9._-]*$");
    if (!remoteNameRegex.match(target).hasMatch()) return QString();
    GitResult res = const_cast<GitCliService*>(this)->runGit({"remote", "get-url", target}, QString(), 2000);
    if (res.success) {
        return res.stdOut.trimmed();
    }
    return QString();
}

bool GitCliService::setRemoteUrl(const QString &url, const QString &remoteName)
{
    if (m_repoPath.isEmpty() || url.trimmed().isEmpty()) {
        emit operationFailed("Remote URL cannot be empty");
        return false;
    }

    QString target = remoteName.isEmpty() ? "origin" : remoteName.trimmed();
    QString cleanUrl = url.trimmed();
    static const QRegularExpression remoteNameRegex("^[A-Za-z0-9][A-Za-z0-9._-]*$");
    if (!remoteNameRegex.match(target).hasMatch() || cleanUrl.startsWith('-')) {
        emit operationFailed("Invalid remote name or URL");
        return false;
    }

    GitResult checkRemote = runGit({"remote"}, QString(), 2000);
    QStringList existingRemotes = checkRemote.stdOut.split(QRegularExpression("\\s+"), Qt::SkipEmptyParts);

    GitResult res;
    if (existingRemotes.contains(target)) {
        res = runGit({"remote", "set-url", target, cleanUrl}, QString(), 3000);
    } else {
        res = runGit({"remote", "add", target, cleanUrl}, QString(), 3000);
    }

    if (res.success) {
        m_repoRemotes[m_repoPath] = redactRemoteUrl(cleanUrl);
        saveRepositories();
        invalidateRepositoryCaches();
        getRemoteStatus();
        emit branchListChanged();
        emit operationSucceeded(QString("Remote '%1' updated").arg(target));
        return true;
    }

    emit operationFailed(res.stdErr.trimmed().isEmpty() ? "Failed to set remote URL" : res.stdErr.trimmed());
    return false;
}

bool GitCliService::removeRemote(const QString &remoteName)
{
    if (m_repoPath.isEmpty()) return false;
    QString target = remoteName.isEmpty() ? "origin" : remoteName.trimmed();
    static const QRegularExpression remoteNameRegex("^[A-Za-z0-9][A-Za-z0-9._-]*$");
    if (!remoteNameRegex.match(target).hasMatch()) {
        emit operationFailed("Invalid remote name");
        return false;
    }

    GitResult res = runGit({"remote", "remove", target}, QString(), 3000);
    if (res.success) {
        invalidateRepositoryCaches();
        getRemoteStatus();
        emit branchListChanged();
        emit operationSucceeded(QString("Removed remote '%1'").arg(target));
        return true;
    }

    emit operationFailed(res.stdErr.trimmed().isEmpty() ? "Failed to remove remote" : res.stdErr.trimmed());
    return false;
}

bool GitCliService::publishRepository(const QString &name, const QString &description, bool isPrivate)
{
    if (m_repoPath.isEmpty()) {
        emit operationFailed("No active repository to publish");
        return false;
    }

    QString repoName = name.trimmed().isEmpty() ? m_repoName : name.trimmed();
    if (repoName.startsWith('-')) {
        emit operationFailed("Invalid repository name");
        return false;
    }

    // Check if gh CLI is installed
    QString ghExe = QStandardPaths::findExecutable("gh");
    if (ghExe.isEmpty()) {
        emit operationFailed("GitHub CLI ('gh') is not installed. Please configure a remote origin URL manually in Repository Settings.");
        return false;
    }

    m_remoteStatus.isPushing = true;
    emit remoteStatusUpdated(m_remoteStatus);

    QString dir = m_repoPath;
    QString desc = description.trimmed();
    const QString operationRepoPath = m_repoPath;
    const quint64 operationGeneration = m_repositoryGeneration.load();

    QThread *thread = QThread::create([this, dir, repoName, desc, isPrivate, operationRepoPath, operationGeneration]() {
        QStringList args = {"repo", "create", repoName, isPrivate ? "--private" : "--public", "--source=.", "--remote=origin", "--push"};
        if (!desc.isEmpty()) {
            args << "--description" << desc;
        }

        QProcess process;
        process.setWorkingDirectory(dir);
        QProcessEnvironment env = QProcessEnvironment::systemEnvironment();
        env.insert("LC_ALL", "C");
        env.insert("GIT_TERMINAL_PROMPT", "0");
        if (!env.contains("GIT_SSH_COMMAND")) {
            env.insert("GIT_SSH_COMMAND", "ssh -o BatchMode=yes");
        }
        process.setProcessEnvironment(env);
        process.start("gh", args);
        bool finished = process.waitForFinished(120000);
        if (!finished) {
            process.kill();
            process.waitForFinished(500);
        }

        int exitCode = process.exitCode();
        QString stdOut = QString::fromUtf8(process.readAllStandardOutput());
        QString stdErr = QString::fromUtf8(process.readAllStandardError());

        QMetaObject::invokeMethod(this, [this, finished, exitCode, stdOut, stdErr, repoName, operationRepoPath, operationGeneration]() {
            if (m_repositoryGeneration.load() != operationGeneration || m_repoPath != operationRepoPath) return;
            m_remoteStatus.isPushing = false;
            if (finished && exitCode == 0) {
                clearUndoState();
                invalidateRepositoryCaches();
                getRemoteStatus();
                emit branchListChanged();
                emit commitHistoryUpdated();
                emit operationSucceeded(QString("Successfully published '%1' to GitHub!").arg(repoName));
            } else {
                getRemoteStatus();
                QString errorMsg = stdErr.trimmed().isEmpty() ? stdOut.trimmed() : stdErr.trimmed();
                emit operationFailed(QString("Failed to publish with gh CLI: %1").arg(errorMsg.isEmpty() ? "Process timed out or unknown error" : errorMsg));
            }
        }, Qt::QueuedConnection);
    });
    connect(thread, &QThread::finished, thread, &QObject::deleteLater);
    thread->start();
    return true;
}

RemoteStatus GitCliService::getRemoteStatus()
{
    QMutexLocker locker(&m_cacheMutex);
    if (m_remoteStatusCacheValid) return m_remoteStatus;
    if (m_repoPath.isEmpty()) return m_remoteStatus;

    locker.unlock();
    // First check if any remote is configured
    bool remoteExists = hasRemote();
    locker.relock();
    m_remoteStatus.hasRemote = remoteExists;

    if (!remoteExists) {
        m_remoteStatus.remoteUrl.clear();
        m_remoteStatus.ahead = 0;
        m_remoteStatus.behind = 0;
        m_remoteStatus.lastFetchedText = "No remote repository configured";
        m_remoteStatusCacheValid = true;
        if (!m_suppressRefreshSignals) emit remoteStatusUpdated(m_remoteStatus);
        return m_remoteStatus;
    }

    locker.unlock();
    const QString remoteName = preferredRemoteName();
    QString remoteUrl = getRemoteUrl(remoteName);
    GitResult revRes = runGit({"rev-list", "--left-right", "--count", "HEAD...@{upstream}"}, QString(), 2000);
    locker.relock();
    m_remoteStatus.remoteName = remoteName;
    m_remoteStatus.remoteUrl = remoteUrl;

    if (revRes.success) {
        QStringList parts = revRes.stdOut.trimmed().split('\t');
        if (parts.size() >= 2) {
            m_remoteStatus.ahead = parts[0].toInt();
            m_remoteStatus.behind = parts[1].toInt();
        }
    } else {
        // Check if there are unpushed commits on a branch without upstream
        locker.unlock();
        GitResult unpushedRes = runGit({"rev-list", "--count", "HEAD", "--not", "--remotes"}, QString(), 2000);
        locker.relock();
        if (unpushedRes.success) {
            m_remoteStatus.ahead = unpushedRes.stdOut.trimmed().toInt();
            m_remoteStatus.behind = 0;
        } else {
            m_remoteStatus.ahead = 0;
            m_remoteStatus.behind = 0;
        }
    }

    if (m_lastFetchTime.isValid()) {
        m_remoteStatus.lastFetchedText = QString("Last fetched %1").arg(formatRelativeTime(m_lastFetchTime));
    } else {
        m_remoteStatus.lastFetchedText = "Not fetched in this session";
    }

    m_remoteStatusCacheValid = true;
    if (!m_suppressRefreshSignals) emit remoteStatusUpdated(m_remoteStatus);
    return m_remoteStatus;
}

void GitCliService::fetchOrigin()
{
    if (m_repoPath.isEmpty() || m_remoteStatus.isFetching || m_remoteStatus.isPulling || m_remoteStatus.isPushing) return;

    if (!hasRemote()) {
        emit operationFailed("No remote repository configured. Set a remote URL in Repository Settings to fetch.");
        return;
    }

    const QString operationRepoPath = m_repoPath;
    const quint64 operationGeneration = m_repositoryGeneration.load();
    const QString remoteName = preferredRemoteName();
    m_remoteStatus.isFetching = true;
    emit remoteStatusUpdated(m_remoteStatus);

    runGitAsync({"fetch", remoteName, "--prune"}, [this, operationRepoPath, operationGeneration, remoteName](const GitResult &res) {
        if (m_repositoryGeneration.load() != operationGeneration || m_repoPath != operationRepoPath) return;
        m_remoteStatus.isFetching = false;
        if (res.success) {
            m_lastFetchTime = QDateTime::currentDateTime();
            m_repoFetchTimes[operationRepoPath] = m_lastFetchTime;
            saveFetchTimes();
            m_remoteStatus.lastFetchedText = "Last fetched just now";
            invalidateRepositoryCaches();
            getRemoteStatus();
            emit branchListChanged();
            emit operationSucceeded(QString("Fetched latest changes from '%1'").arg(remoteName));

        } else {
            m_remoteStatus.lastFetchedText = "Fetch failed";
            emit remoteStatusUpdated(m_remoteStatus);
            emit operationFailed(formatGitError(res.stdErr, "Fetch failed"));
        }
    });
}

void GitCliService::pullOrigin()
{
    if (m_repoPath.isEmpty() || m_remoteStatus.isFetching || m_remoteStatus.isPulling || m_remoteStatus.isPushing) return;

    if (!hasRemote()) {
        emit operationFailed("No remote repository configured. Cannot pull.");
        return;
    }

    const QString operationRepoPath = m_repoPath;
    const quint64 operationGeneration = m_repositoryGeneration.load();
    m_remoteStatus.isPulling = true;
    emit remoteStatusUpdated(m_remoteStatus);

    runGitAsync({"pull", "--ff-only"}, [this, operationRepoPath, operationGeneration](const GitResult &res) {
        if (m_repositoryGeneration.load() != operationGeneration || m_repoPath != operationRepoPath) return;
        m_remoteStatus.isPulling = false;
        if (res.success) {
            clearUndoState();
            m_lastFetchTime = QDateTime::currentDateTime();
            m_remoteStatus.lastFetchedText = "Last fetched just now";
            invalidateRepositoryCaches();
            getRemoteStatus();
            emit commitHistoryUpdated();
            emit changedFilesUpdated();
            emit branchListChanged();
            emit operationSucceeded("Successfully pulled latest changes");
        } else {
            m_remoteStatus.lastFetchedText = "Pull failed";
            emit remoteStatusUpdated(m_remoteStatus);
            emit operationFailed(formatGitError(res.stdErr, "Pull failed"));
        }
    });
}

void GitCliService::pushOrigin()
{
    if (m_repoPath.isEmpty() || m_remoteStatus.isFetching || m_remoteStatus.isPulling || m_remoteStatus.isPushing) return;

    if (!hasRemote()) {
        emit operationFailed("No remote repository configured. Publish repository or add a remote origin before pushing.");
        return;
    }

    m_remoteStatus.isPushing = true;
    emit remoteStatusUpdated(m_remoteStatus);

    auto curBranch = getCurrentBranch();
    if (!curBranch || curBranch->name == "(detached)") {
        m_remoteStatus.isPushing = false;
        emit remoteStatusUpdated(m_remoteStatus);
        emit operationFailed("Cannot push while HEAD is detached");
        return;
    }
    QString branchName = curBranch->name;
    const QString remoteName = preferredRemoteName();
    QString dir = m_repoPath;

    const QString operationRepoPath = m_repoPath;
    const quint64 operationGeneration = m_repositoryGeneration.load();
    QThread *thread = QThread::create([this, dir, branchName, remoteName, operationRepoPath, operationGeneration]() {
        GitResult upstreamCheck = const_cast<GitCliService*>(this)->runGit({"rev-parse", "--verify", "@{upstream}"}, dir, 3000);
        QStringList pushArgs = {"push"};
        if (!upstreamCheck.success) {
            // Set upstream tracking on initial push
            pushArgs = {"push", "-u", remoteName, branchName};
        }

        GitResult res = const_cast<GitCliService*>(this)->runGit(pushArgs, dir, 120000);

        QMetaObject::invokeMethod(this, [this, res, remoteName, operationRepoPath, operationGeneration]() {
            if (m_repositoryGeneration.load() != operationGeneration || m_repoPath != operationRepoPath) return;
            m_remoteStatus.isPushing = false;
            if (res.success) {
                clearUndoState();
                invalidateRepositoryCaches();
                getRemoteStatus();
                emit commitHistoryUpdated();
                emit operationSucceeded(QString("Successfully pushed commits to '%1'").arg(remoteName));
            } else {
                emit remoteStatusUpdated(m_remoteStatus);
                emit operationFailed(formatGitError(res.stdErr, "Push failed"));
            }
        }, Qt::QueuedConnection);
    });
    connect(thread, &QThread::finished, thread, &QObject::deleteLater);
    thread->start();
}

QList<StashItem> GitCliService::getStashes()
{
    QMutexLocker locker(&m_cacheMutex);
    if (m_repoPath.isEmpty()) return {};
    if (m_stashesCacheValid) return m_stashesCache;

    if (m_repositoryReader.directReadSupported()) {
        m_stashesCache = m_repositoryReader.stashes();
    }
    if (!m_repositoryReader.directReadSupported() || m_stashesCache.isEmpty()) {
        locker.unlock();
        const GitResult listRes = runGit({"stash", "list", "--format=%gd%x1f%gs%x1f%ct%x1e"}, QString(), 3000);
        locker.relock();
        if (listRes.success) {
            QList<StashItem> parsed;
            const QStringList records = listRes.stdOut.split(QChar('\x1e'), Qt::SkipEmptyParts);
            static const QRegularExpression branchRegex("(?:On|WIP on) ([^:]+):");
            static const QRegularExpression prefixRegex("^(?:On|WIP on) [^:]+:\\s*");
            for (const QString &record : records) {
                const QStringList fields = record.split(QChar('\x1f'));
                if (fields.size() < 3) continue;
                StashItem stash;
                stash.id = fields[0].trimmed();
                stash.message = fields[1].trimmed();
                stash.message.remove(prefixRegex);
                if (stash.message.isEmpty()) stash.message = "WIP";
                const auto branchMatch = branchRegex.match(fields[1]);
                stash.branchName = branchMatch.hasMatch() ? branchMatch.captured(1).trimmed() : "HEAD";
                stash.timestamp = QDateTime::fromSecsSinceEpoch(fields[2].trimmed().toLongLong(), QTimeZone::UTC);
                parsed.append(stash);
            }
            m_stashesCache = parsed;
        }
    }

    m_stashesCacheValid = true;
    return m_stashesCache;
}

std::optional<StashItem> GitCliService::getStashDetails(const QString &stashId)
{
    QMutexLocker locker(&m_cacheMutex);
    if (m_repoPath.isEmpty()) return std::nullopt;
    locker.unlock();

    const QString requested = stashId.isEmpty() ? "stash@{0}" : stashId;
    static const QRegularExpression stashIdRegex("^stash@\\{[0-9]+\\}$");
    if (!stashIdRegex.match(requested).hasMatch()) return std::nullopt;

    auto stashes = getStashes();
    std::optional<StashItem> result;
    for (const auto &stash : stashes) {
        if (stash.id == requested) {
            result = stash;
            break;
        }
    }
    if (!result) return std::nullopt;

    const GitResult statusRes = runGit({"stash", "show", "--include-untracked", "--format=", "--name-status", "-z", requested}, QString(), 4000);
    const GitResult statRes = runGit({"stash", "show", "--include-untracked", "--format=", "--numstat", "-z", requested}, QString(), 4000);

    QHash<QString, QPair<int, int>> stats;
    if (statRes.success) {
        const QStringList records = statRes.stdOut.split(QChar('\0'), Qt::KeepEmptyParts);
        for (int i = 0; i < records.size(); ++i) {
            const QStringList fields = records.at(i).split('\t');
            if (fields.size() >= 3 && !fields[2].isEmpty()) {
                stats[fields[2]] = qMakePair(fields[0] == "-" ? 0 : fields[0].toInt(),
                                             fields[1] == "-" ? 0 : fields[1].toInt());
            } else if (fields.size() >= 3 && fields[2].isEmpty() && i + 2 < records.size()) {
                const auto value = qMakePair(fields[0] == "-" ? 0 : fields[0].toInt(),
                                             fields[1] == "-" ? 0 : fields[1].toInt());
                stats[records.at(i + 1)] = value;
                stats[records.at(i + 2)] = value;
                i += 2;
            }
        }
    }

    if (statusRes.success) {
        const QStringList records = statusRes.stdOut.split(QChar('\0'), Qt::KeepEmptyParts);
        int index = 0;
        for (int i = 0; i < records.size(); ++i) {
            const QString code = records.at(i).trimmed();
            if (code.isEmpty()) continue;
            FileChange file;
            file.id = QString("fc-%1").arg(index++);
            if ((code.startsWith('R') || code.startsWith('C')) && i + 2 < records.size()) {
                file.status = FileChangeType::Renamed;
                file.filePath = records.at(++i);
                file.oldFilePath = records.at(++i);
            } else {
                if (i + 1 >= records.size()) break;
                file.filePath = records.at(++i);
                if (code.startsWith('A')) file.status = FileChangeType::Added;
                else if (code.startsWith('D')) file.status = FileChangeType::Deleted;
                else file.status = FileChangeType::Modified;
            }
            if (stats.contains(file.filePath)) {
                file.additions = stats.value(file.filePath).first;
                file.deletions = stats.value(file.filePath).second;
            } else if (stats.contains(file.oldFilePath)) {
                file.additions = stats.value(file.oldFilePath).first;
                file.deletions = stats.value(file.oldFilePath).second;
            }
            result->files.append(file);
        }
    }
    return result;
}

bool GitCliService::stashChanges(const QString &message)
{
    if (m_repoPath.isEmpty()) return false;

    QStringList args = {"stash", "push", "-u"};
    if (!message.trimmed().isEmpty()) {
        args.append("-m");
        args.append(message.trimmed());
    }

    GitResult res = runGit(args, QString(), 10000);
    if (res.success) {
        invalidateRepositoryCaches();
        getStashes();
        getChangedFiles();
        emit stashesUpdated();
        emit changedFilesUpdated();
        emit operationSucceeded("Stashed changes");
        return true;
    }

    emit operationFailed(formatGitError(res.stdErr, "Stash failed"));
    return false;
}

bool GitCliService::popStash(const QString &stashId)
{
    if (m_repoPath.isEmpty() || (!stashId.isEmpty() && !isValidStashId(stashId))) return false;

    QStringList args = {"stash", "pop"};
    if (!stashId.isEmpty()) {
        args.append(stashId);
    }

    GitResult res = runGit(args, QString(), 10000);
    if (res.success) {
        invalidateRepositoryCaches();
        getStashes();
        getChangedFiles();
        emit stashesUpdated();
        emit changedFilesUpdated();
        emit operationSucceeded("Restored stashed changes");
        return true;
    }

    emit operationFailed(formatGitError(res.stdErr, "Failed to pop stash"));
    return false;
}

bool GitCliService::dropStash(const QString &stashId)
{
    if (m_repoPath.isEmpty() || (!stashId.isEmpty() && !isValidStashId(stashId))) return false;

    QStringList args = {"stash", "drop"};
    if (!stashId.isEmpty()) {
        args.append(stashId);
    }

    GitResult res = runGit(args, QString(), 10000);
    if (res.success) {
        invalidateRepositoryCaches();
        getStashes();
        emit stashesUpdated();
        emit operationSucceeded("Dropped stash");
        return true;
    }

    emit operationFailed(formatGitError(res.stdErr, "Failed to drop stash"));
    return false;
}

QString GitCliService::formatRelativeTime(const QDateTime &dt) const
{
    if (!dt.isValid()) return "";
    qint64 secs = dt.secsTo(QDateTime::currentDateTime());
    if (secs < 60) return "just now";
    if (secs < 3600) return QString("%1 minutes ago").arg(secs / 60);
    if (secs < 86400) return QString("%1 hours ago").arg(secs / 3600);
    if (secs < 172800) return "yesterday";
    return QString("%1 days ago").arg(secs / 86400);
}

bool GitCliService::isValidBranchName(const QString &name)
{
    const QString trimmed = name.trimmed();
    if (trimmed.isEmpty() || trimmed != name || trimmed.startsWith('-')) return false;
    return runGit({"check-ref-format", "--branch", trimmed}, QString(), 2000).success;
}

QString GitCliService::preferredRemoteName() const
{
    if (!getRemoteUrl("origin").isEmpty()) return "origin";
    GitResult remotes = const_cast<GitCliService *>(this)->runGit({"remote"}, QString(), 2000);
    const QStringList names = remotes.stdOut.split(QRegularExpression("\\s+"), Qt::SkipEmptyParts);
    return names.isEmpty() ? QStringLiteral("origin") : names.first().trimmed();
}

bool GitCliService::isValidStashId(const QString &stashId) const
{
    static const QRegularExpression stashIdRegex("^(?:stash@\\{[0-9]+\\}|[0-9]+)$");
    return stashIdRegex.match(stashId.trimmed()).hasMatch();
}

bool GitCliService::isSafeRepositoryPath(const QString &path) const
{
    if (m_repoPath.isEmpty() || path.isEmpty() || path.startsWith('/')) return false;
    const QString root = QDir::cleanPath(QDir(m_repoPath).absolutePath());
    const QString full = QDir::cleanPath(QDir(root).filePath(path));
    if (full != root && !full.startsWith(root + '/')) return false;

    const QFileInfo info(full);
    if (info.exists() && info.isSymLink()) {
        const QString canonical = QDir::cleanPath(info.canonicalFilePath());
        if (!canonical.isEmpty() && canonical != root && !canonical.startsWith(root + '/')) return false;
    }
    return true;
}

QString GitCliService::formatGitError(const QString &rawError, const QString &fallbackContext) const
{
    QString trimmed = rawError.trimmed();
    trimmed.replace(QRegularExpression(R"((https?://)[^/\s@]+@)"), "\\1***@");
    if (trimmed.isEmpty()) {
        return fallbackContext;
    }

    // SSH & Authentication diagnostics
    if (trimmed.contains("Permission denied (publickey)", Qt::CaseInsensitive)) {
        return "Authentication failed: SSH key permission denied. Please verify your SSH key or repository access permissions.";
    }
    if (trimmed.contains("Authentication failed", Qt::CaseInsensitive)) {
        return "Authentication failed: Invalid credentials or Personal Access Token.";
    }
    if (trimmed.contains("Host key verification failed", Qt::CaseInsensitive)) {
        return "SSH host key verification failed.";
    }
    if (trimmed.contains("Could not read from remote repository", Qt::CaseInsensitive) ||
        trimmed.contains("Repository not found", Qt::CaseInsensitive)) {
        return "Remote repository not found or access denied. Please check your remote URL.";
    }
    if (trimmed.contains("Could not resolve host", Qt::CaseInsensitive)) {
        return "Network error: Unable to resolve remote host. Check your internet connection.";
    }
    if (trimmed.contains("Connection refused", Qt::CaseInsensitive) || trimmed.contains("Connection timed out", Qt::CaseInsensitive)) {
        return "Network error: Connection to remote repository timed out or refused.";
    }
    if (trimmed.contains("Updates were rejected", Qt::CaseInsensitive)) {
        return "Push rejected: Remote contains newer changes. Please pull before pushing.";
    }
    if (trimmed.contains("Automatic merge failed", Qt::CaseInsensitive)) {
        return "Merge conflicts detected. Please resolve conflicts.";
    }
    if (trimmed.contains("Your local changes to the following files would be overwritten", Qt::CaseInsensitive)) {
        return "Operation blocked: Local changes would be overwritten. Commit or stash them first.";
    }

    // Parse first readable line
    const QStringList lines = trimmed.split('\n', Qt::SkipEmptyParts);
    for (const QString &line : lines) {
        QString l = line.trimmed();
        if (l.startsWith("fatal: ", Qt::CaseInsensitive)) {
            l = l.mid(7).trimmed();
        } else if (l.startsWith("error: ", Qt::CaseInsensitive)) {
            l = l.mid(7).trimmed();
        }
        if (!l.isEmpty() && !l.startsWith("git@") && !l.startsWith("ssh:")) {
            return l.left(120);
        }
    }

    if (!lines.isEmpty()) {
        return lines.first().left(120);
    }

    return fallbackContext;
}

bool GitCliService::ignoreFileModeChanges(bool global)
{
    if (!global && m_repoPath.isEmpty()) return false;
    const QString scope = global ? "--global" : "--local";
    GitResult res = runGit({"config", scope, "--get", "core.filemode"}, QString(), 1000);
    return res.success && res.stdOut.trimmed().compare("false", Qt::CaseInsensitive) == 0;
}

bool GitCliService::setIgnoreFileModeChanges(bool ignored, bool global)
{
    if (!global && m_repoPath.isEmpty()) return false;

    const QString scope = global ? "--global" : "--local";
    GitResult res;
    if (ignored) {
        res = runGit({"config", scope, "core.filemode", "false"}, QString(), 2000);
    } else {
        res = runGit({"config", scope, "--unset", "core.filemode"}, QString(), 2000);
        // --unset exits with code 5 when the key does not exist; that is already
        // the desired state.
        if (!res.success && res.exitCode == 5) res.success = true;
    }

    if (!res.success) {
        emit operationFailed(formatGitError(res.stdErr, "Failed to update Git file metadata settings"));
        return false;
    }

    // Re-scan asynchronously so changing Git config never blocks the UI on a
    // large worktree. The refresh emits model updates once its snapshot is ready.
    refreshRepository();
    emit operationSucceeded(QString("File metadata changes are now %1 (%2)")
                                .arg(ignored ? "ignored" : "tracked", global ? "global" : "this repository"));
    return true;
}

QString GitCliService::getAuthorName() const
{
    if (m_repoPath.isEmpty()) return getGlobalAuthorName();
    GitResult res = const_cast<GitCliService*>(this)->runGit({"config", "--local", "user.name"}, QString(), 1000);
    if (res.success && !res.stdOut.trimmed().isEmpty()) {
        return res.stdOut.trimmed();
    }
    return getGlobalAuthorName();
}

QString GitCliService::getAuthorEmail() const
{
    if (m_repoPath.isEmpty()) return getGlobalAuthorEmail();
    GitResult res = const_cast<GitCliService*>(this)->runGit({"config", "--local", "user.email"}, QString(), 1000);
    if (res.success && !res.stdOut.trimmed().isEmpty()) {
        return res.stdOut.trimmed();
    }
    return getGlobalAuthorEmail();
}

QString GitCliService::getGlobalAuthorName() const
{
    GitResult res = const_cast<GitCliService*>(this)->runGit({"config", "--global", "user.name"}, QString(), 1000);
    if (res.success && !res.stdOut.trimmed().isEmpty()) {
        return res.stdOut.trimmed();
    }
    QString user = qgetenv("USER");
    return user.isEmpty() ? "User" : user;
}

QString GitCliService::getGlobalAuthorEmail() const
{
    GitResult res = const_cast<GitCliService*>(this)->runGit({"config", "--global", "user.email"}, QString(), 1000);
    if (res.success && !res.stdOut.trimmed().isEmpty()) {
        return res.stdOut.trimmed();
    }
    return "user@localhost";
}

bool GitCliService::setAuthorInfo(const QString &name, const QString &email, bool global)
{
    QString scope = global ? "--global" : "--local";
    bool ok = true;
    if (!name.trimmed().isEmpty()) {
        GitResult r1 = runGit({"config", scope, "user.name", name.trimmed()}, QString(), 2000);
        ok = ok && r1.success;
    }
    if (!email.trimmed().isEmpty()) {
        GitResult r2 = runGit({"config", scope, "user.email", email.trimmed()}, QString(), 2000);
        ok = ok && r2.success;
    }
    if (ok) {
        emit operationSucceeded(QString("Updated Git author info (%1)").arg(global ? "global" : "repository"));
    } else {
        emit operationFailed("Failed to update Git author config");
    }
    return ok;
}

} // namespace Cherry
