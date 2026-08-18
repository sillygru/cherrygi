#include "GitCliService.h"
#include "AppSettings.h"
#include "AvatarResolver.h"
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
#include <QElapsedTimer>
#include <QSet>

#if defined(Q_OS_LINUX)
#include <sys/prctl.h>
#endif
#if defined(Q_OS_UNIX)
#include <unistd.h>
#include <csignal>
#endif

namespace Cherry {

namespace {
thread_local quint64 s_refreshGeneration{0};

QString redactRemoteUrl(const QString &url)
{
    QString result = url;
    result.replace(QRegularExpression(R"((https?://)[^/\s@]+@)"), "\\1");
    return result;
}

void configureGitProcess(QProcess &process)
{
#if defined(Q_OS_LINUX)
    process.setChildProcessModifier([]() {
        ::setpgid(0, 0);
        ::prctl(PR_SET_PDEATHSIG, SIGKILL);
    });
#elif defined(Q_OS_UNIX)
    process.setChildProcessModifier([]() {
        ::setpgid(0, 0);
    });
#endif
}

class ScopedProcessTracker {
public:
    ScopedProcessTracker(GitCliService *service, QProcess *process)
        : m_service(service), m_process(process)
    {
        if (m_service && m_process) {
            m_service->registerActiveProcess(m_process);
        }
    }
    ~ScopedProcessTracker()
    {
        if (m_service && m_process) {
            m_service->unregisterActiveProcess(m_process);
        }
    }
private:
    GitCliService *m_service{nullptr};
    QProcess *m_process{nullptr};
};
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

void GitCliService::killProcess(QProcess *process)
{
    if (!process) return;
    const qint64 pid = process->processId();
#if defined(Q_OS_UNIX)
    if (pid > 0) {
        ::kill(-static_cast<pid_t>(pid), SIGKILL);
        ::kill(static_cast<pid_t>(pid), SIGKILL);
    }
#endif
    if (process->state() != QProcess::NotRunning) {
        process->kill();
    }
}

void GitCliService::registerActiveProcess(QProcess *process)
{
    if (!process) return;
    QMutexLocker locker(&m_activeProcessesMutex);
    if (m_shuttingDown.load(std::memory_order_acquire)) {
        killProcess(process);
        return;
    }
    m_activeProcesses.insert(process);
}

void GitCliService::unregisterActiveProcess(QProcess *process)
{
    if (!process) return;
    QMutexLocker locker(&m_activeProcessesMutex);
    m_activeProcesses.remove(process);
}

void GitCliService::killActiveProcesses()
{
    QMutexLocker locker(&m_activeProcessesMutex);
    for (QProcess *process : m_activeProcesses) {
        killProcess(process);
    }
}

void GitCliService::cancelOperations()
{
    m_shuttingDown.store(true, std::memory_order_release);
    m_repositoryGeneration.fetch_add(1);
    killActiveProcesses();
}

GitCliService::~GitCliService()
{
    cancelOperations();

    QList<QThread *> workers;
    {
        QMutexLocker locker(&m_workerMutex);
        workers = m_workers;
    }
    for (QThread *worker : workers) {
        if (worker && worker->isRunning()) worker->wait();
    }

    {
        QMutexLocker locker(&m_workerMutex);
        workers = m_workers;
        m_workers.clear();
    }
    for (QThread *worker : workers) {
        delete worker;
    }
}

QThread *GitCliService::trackWorker(QThread *thread)
{
    if (!thread) return nullptr;
    {
        QMutexLocker locker(&m_workerMutex);
        m_workers.append(thread);
    }
    connect(thread, &QThread::finished, this, [this, thread]() {
        QMutexLocker locker(&m_workerMutex);
        m_workers.removeAll(thread);
        thread->deleteLater();
    });
    return thread;
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

void GitCliService::invalidateRepositoryCaches(bool invalidateSessionCache)
{
    QMutexLocker locker(&m_cacheMutex);
    if (invalidateSessionCache && !m_repoPath.isEmpty()) {
        invalidateSession(m_repoPath);
    }
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

void GitCliService::invalidateRefreshCaches()
{
    // Keep the last complete snapshot visible while the replacement snapshot
    // is built. This is the same stale-while-revalidate shape used by Desktop's
    // per-repository GitStore and prevents refreshes from blanking the UI.
    QMutexLocker locker(&m_cacheMutex);
    if (!m_repoPath.isEmpty()) {
        invalidateSession(m_repoPath);
    }
    m_repositoryReader.refresh();
    m_changedFilesCacheValid = false;
    m_branchesCacheValid = false;
    m_currentBranchCacheValid = false;
    m_commitHistoryCacheValid = false;
    m_stashesCacheValid = false;
    m_remoteStatusCacheValid = false;
    m_fileDiffCacheValid = false;
    m_commitDetailsCache.clear();
}

void GitCliService::invalidateSession(const QString &repoPath)
{
    if (repoPath.isEmpty()) return;
    QMutexLocker locker(&m_cacheMutex);
    auto it = m_sessionCaches.find(repoPath);
    if (it != m_sessionCaches.end()) it->complete = false;
}

void GitCliService::cacheCurrentSession()
{
    QMutexLocker locker(&m_cacheMutex);
    if (m_repoPath.isEmpty() || m_isMissing ||
        !m_changedFilesCacheValid || !m_branchesCacheValid ||
        !m_currentBranchCacheValid || !m_commitHistoryCacheValid ||
        !m_stashesCacheValid || !m_remoteStatusCacheValid) {
        return;
    }

    SessionCache snapshot;
    snapshot.complete = true;
    snapshot.changedFiles = m_changedFilesCache;
    snapshot.branches = m_branchesCache;
    snapshot.currentBranch = m_currentBranchCache;
    snapshot.commitHistory = m_commitHistoryCache;
    snapshot.stashes = m_stashesCache;
    snapshot.remoteStatus = m_remoteStatus;
    m_sessionCaches.insert(m_repoPath, std::move(snapshot));

    // Keep this cache deliberately small. Qt containers are implicitly shared,
    // so retaining a few model snapshots costs little while avoiding unbounded
    // growth in long-running sessions.
    while (m_sessionCaches.size() > 8) {
        m_sessionCaches.erase(m_sessionCaches.begin());
    }
}

bool GitCliService::restoreSession(const QString &repoPath)
{
    QMutexLocker locker(&m_cacheMutex);
    const auto it = m_sessionCaches.constFind(repoPath);
    if (it == m_sessionCaches.cend() || !it->complete) return false;

    m_changedFilesCache = it->changedFiles;
    m_branchesCache = it->branches;
    m_currentBranchCache = it->currentBranch;
    m_commitHistoryCache = it->commitHistory;
    m_stashesCache = it->stashes;
    m_remoteStatus = it->remoteStatus;
    m_changedFilesCacheValid = true;
    m_branchesCacheValid = true;
    m_currentBranchCacheValid = true;
    m_commitHistoryCacheValid = true;
    m_stashesCacheValid = true;
    m_remoteStatusCacheValid = true;
    m_fileDiffCacheValid = false;
    m_fileDiffCachePath.clear();
    m_fileDiffCache.clear();
    m_commitDetailsCache.clear();
    m_fileSelection.clear();
    for (const auto &file : m_changedFilesCache) {
        m_fileSelection.insert(file.filePath, file.isSelected);
    }
    return true;
}

bool GitCliService::shouldReturnStaleCache() const
{
    return m_refreshInProgress && QThread::currentThread() == thread();
}

bool GitCliService::shouldDeferExpensiveInitialRead() const
{
    return m_initialLoadPending.load(std::memory_order_acquire) && QThread::currentThread() == thread();
}

void GitCliService::loadAuthorCache()
{
    const RepositoryMetadata previous = m_repositoryMetadata.value(m_repoPath);
    const GitResult name = runGit({"config", "--get", "user.name"}, QString(), 1000);
    const GitResult email = runGit({"config", "--get", "user.email"}, QString(), 1000);
    m_authorNameCache = name.success && !name.stdOut.trimmed().isEmpty()
        ? name.stdOut.trimmed()
        : previous.authorName;
    m_authorEmailCache = email.success && !email.stdOut.trimmed().isEmpty()
        ? email.stdOut.trimmed()
        : previous.authorEmail;
    if (m_authorNameCache.isEmpty()) m_authorNameCache = getGlobalAuthorName();
    if (m_authorEmailCache.isEmpty()) m_authorEmailCache = getGlobalAuthorEmail();
    m_authorCacheValid = true;
}

void GitCliService::loadVitalRepositoryState()
{
    // Phase one is intentionally small: HEAD, remote configuration, and the
    // author identity are needed to render the shell correctly. Status,
    // history, and file scanning remain in the background refresh.
    {
        QMutexLocker locker(&m_cacheMutex);
        m_currentBranchCache = m_repositoryReader.currentBranch();
        m_currentBranchCacheValid = true;
    }

    const RepositoryMetadata previous = m_repositoryMetadata.value(m_repoPath);
    RemoteStatus status;
    status.remoteName = previous.remoteName.isEmpty() ? QStringLiteral("origin") : previous.remoteName;
    status.remoteUrl = previous.remoteUrl;
    status.hasRemote = previous.hasRemote && !previous.remoteUrl.isEmpty();
    status.ahead = previous.ahead;
    status.behind = previous.behind;
    status.lastFetchedText = previous.lastFetchedText;

    // Remote configuration is cheap and must be authoritative before the
    // repositoryChanged signal reaches QML; otherwise a real remote can look
    // like a repository eligible for publishing for one render.
    const GitResult remotes = runGit({"remote"}, QString(), 2000);
    if (remotes.success) {
        const QStringList names = remotes.stdOut.split(QRegularExpression("\\s+"), Qt::SkipEmptyParts);
        QString selectedName;
        QString selectedUrl;
        for (const QString &name : names) {
            const QString candidate = name.trimmed();
            if (candidate.isEmpty()) continue;
            const GitResult url = runGit({"remote", "get-url", candidate}, QString(), 2000);
            if (!url.success || url.stdOut.trimmed().isEmpty()) continue;
            if (selectedName.isEmpty() || candidate == QStringLiteral("origin")) {
                selectedName = candidate;
                selectedUrl = url.stdOut.trimmed();
            }
            if (candidate == QStringLiteral("origin")) break;
        }
        status.hasRemote = !selectedUrl.isEmpty();
        status.remoteName = selectedName;
        status.remoteUrl = selectedUrl;
        if (!status.hasRemote) {
            status.ahead = 0;
            status.behind = 0;
            status.lastFetchedText = QStringLiteral("No remote repository configured");
        }
    }

    if (status.hasRemote && status.lastFetchedText.isEmpty()) {
        status.lastFetchedText = m_lastFetchTime.isValid()
            ? QStringLiteral("Last fetched %1").arg(formatRelativeTime(m_lastFetchTime))
            : QStringLiteral("Not fetched in this session");
    }
    if (!status.hasRemote && status.lastFetchedText.isEmpty()) {
        status.lastFetchedText = QStringLiteral("No remote repository configured");
    }

    m_remoteStatus = status;
    m_remoteStatusCacheValid = true;
    if (status.hasRemote) {
        m_repoRemotes[m_repoPath] = redactRemoteUrl(status.remoteUrl);
    } else {
        m_repoRemotes.remove(m_repoPath);
    }
    loadAuthorCache();
    cacheCurrentRepositoryMetadata();
    saveRepositories();
}

void GitCliService::cacheCurrentRepositoryMetadata()
{
    if (m_repoPath.isEmpty() || m_isMissing) return;
    RepositoryMetadata &metadata = m_repositoryMetadata[m_repoPath];
    metadata.currentBranch = m_currentBranchCache ? m_currentBranchCache->name : QString();
    metadata.remoteName = m_remoteStatus.remoteName;
    metadata.remoteUrl = redactRemoteUrl(m_remoteStatus.remoteUrl);
    metadata.hasRemote = m_remoteStatus.hasRemote;
    metadata.ahead = m_remoteStatus.ahead;
    metadata.behind = m_remoteStatus.behind;
    metadata.lastFetchedText = m_remoteStatus.lastFetchedText;
    metadata.authorName = m_authorNameCache;
    metadata.authorEmail = m_authorEmailCache;
}

bool GitCliService::preloadRepositoryCaches()
{
    // Populate expensive views away from the GUI thread. These readers share
    // repository state and the Git process mutex, so keeping the sequence
    // deterministic is safer than starting several competing scans at once.
    // The work is still off the GUI thread and the previous snapshot remains
    // visible until the complete replacement is ready.
    m_suppressRefreshSignals = true;
    invalidateRefreshCaches();

    const auto stillCurrent = [this]() {
        return s_refreshGeneration == 0 || m_repositoryGeneration.load() == s_refreshGeneration;
    };
    getCurrentBranch();
    if (!stillCurrent()) { m_suppressRefreshSignals = false; return false; }
    getBranches();
    if (!stillCurrent()) { m_suppressRefreshSignals = false; return false; }
    getStashes();
    if (!stillCurrent()) { m_suppressRefreshSignals = false; return false; }
    getRemoteStatus();
    if (!stillCurrent()) { m_suppressRefreshSignals = false; return false; }
    getCommitHistory();
    if (!stillCurrent()) { m_suppressRefreshSignals = false; return false; }
    getChangedFiles();
    if (!stillCurrent()) { m_suppressRefreshSignals = false; return false; }
    cacheCurrentSession();

    // Diff content is loaded lazily by DiffModel after the first selection;
    // parsing it here only delays repository visibility.
    m_suppressRefreshSignals = false;
    m_initialLoadPending.store(false, std::memory_order_release);
    return true;
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

    // Persist only the compact metadata snapshot. Large models remain in
    // memory, while the next launch can render the shell without a scan.
    cacheCurrentRepositoryMetadata();
    saveRepositories();

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
        // Hold the transition lock for the complete scan and publication
        // decision. A repository switch must wait instead of changing the
        // shared caches underneath this worker.
        QMutexLocker repositoryLocker(&m_repositoryOpenMutex);
        if (m_repositoryGeneration.load() != generation || m_repoPath != repoPath) return;

        const bool hadFilesCache = m_changedFilesCacheValid;
        const QList<FileChange> previousFiles = m_changedFilesCache;
        s_refreshGeneration = generation;
        const bool loaded = preloadRepositoryCaches();
        s_refreshGeneration = 0;
        if (!loaded) return;

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
    trackWorker(thread)->start();
}

GitResult GitCliService::runGit(const QStringList &args, const QString &workingDir, int timeoutMs, quint64 cancellationGeneration)
{
    if (m_shuttingDown.load(std::memory_order_acquire)) {
        return GitResult{-1, QString(), QStringLiteral("Shutting down"), false};
    }

    // Refresh, diff, and network workers may all issue Git commands. A single
    // process lock prevents concurrent commands from observing half-updated
    // refs/index state or competing for Git's repository lock files.
    QMutexLocker processLocker(&m_gitProcessMutex);
    if (m_shuttingDown.load(std::memory_order_acquire)) {
        return GitResult{-1, QString(), QStringLiteral("Shutting down"), false};
    }

    QProcess process;
    configureGitProcess(process);
    ScopedProcessTracker processTracker(this, &process);

    QString dir = workingDir.isEmpty() ? m_repoPath : workingDir;
    if (dir.isEmpty() || !QDir(dir).exists()) {
        dir = QDir::homePath();
    }
    process.setWorkingDirectory(dir);

    QProcessEnvironment env = QProcessEnvironment::systemEnvironment();
    env.insert("LC_ALL", "C");
    env.insert("GIT_TERMINAL_PROMPT", "0");
    if (!env.contains("GIT_SSH_COMMAND")) {
        env.insert("GIT_SSH_COMMAND", "ssh -o BatchMode=yes");
    }
    process.setProcessEnvironment(env);

    process.start("git", args);
    bool finished = false;
    bool cancelled = false;
    QElapsedTimer timer;
    timer.start();
    while (process.state() != QProcess::NotRunning && timer.elapsed() < timeoutMs) {
        const bool refreshCancelled = s_refreshGeneration != 0 &&
            m_repositoryGeneration.load() != s_refreshGeneration;
        const bool operationCancelled = cancellationGeneration != 0 &&
            m_repositoryGeneration.load() != cancellationGeneration;
        if (m_shuttingDown.load(std::memory_order_acquire) || refreshCancelled || operationCancelled) {
            cancelled = true;
            killProcess(&process);
            process.waitForFinished(100);
            break;
        }
        process.waitForFinished(100);
    }
    finished = !cancelled && process.state() == QProcess::NotRunning;

    if (!finished && process.state() != QProcess::NotRunning) {
        killProcess(&process);
        process.waitForFinished(100);
    }

    GitResult result;
    result.exitCode = process.exitCode();
    result.stdOut = QString::fromUtf8(process.readAllStandardOutput());
    result.stdErr = QString::fromUtf8(process.readAllStandardError());
    result.success = finished && (result.exitCode == 0);

    if (!result.success && !result.stdErr.trimmed().isEmpty()) {
        // Do not log the command arguments: remote URLs may contain credentials.
        qDebug().noquote() << QString("[GitCliService] Git command failed (exit %1): %2")
                                  .arg(result.exitCode)
                                  .arg(redactRemoteUrl(result.stdErr.trimmed()));
    }

    return result;
}

void GitCliService::runGitAsync(const QStringList &args, std::function<void(const GitResult &)> callback, quint64 cancellationGeneration)
{
    QString dir = m_repoPath;
    QThread *thread = QThread::create([this, args, dir, callback, cancellationGeneration]() {
        GitResult res = const_cast<GitCliService*>(this)->runGit(args, dir, 120000, cancellationGeneration);
        QMetaObject::invokeMethod(this, [callback, res]() {
            if (callback) callback(res);
        }, Qt::QueuedConnection);
    });
    trackWorker(thread)->start();
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

            RepositoryMetadata metadata;
            metadata.currentBranch = settings.value("currentBranch").toString();
            metadata.remoteName = settings.value("remoteName", "origin").toString();
            metadata.remoteUrl = redactRemoteUrl(settings.value("cachedRemoteUrl", remote).toString());
            metadata.hasRemote = settings.value("hasRemote", !metadata.remoteUrl.isEmpty()).toBool();
            metadata.ahead = settings.value("ahead", 0).toInt();
            metadata.behind = settings.value("behind", 0).toInt();
            metadata.lastFetchedText = settings.value("lastFetchedText").toString();
            metadata.authorName = settings.value("authorName").toString();
            metadata.authorEmail = settings.value("authorEmail").toString();
            m_repositoryMetadata.insert(path, metadata);
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
        const RepositoryMetadata metadata = m_repositoryMetadata.value(it.key());
        settings.setValue("currentBranch", metadata.currentBranch);
        settings.setValue("remoteName", metadata.remoteName);
        settings.setValue("cachedRemoteUrl", redactRemoteUrl(metadata.remoteUrl));
        settings.setValue("hasRemote", metadata.hasRemote);
        settings.setValue("ahead", metadata.ahead);
        settings.setValue("behind", metadata.behind);
        settings.setValue("lastFetchedText", metadata.lastFetchedText);
        settings.setValue("authorName", metadata.authorName);
        settings.setValue("authorEmail", metadata.authorEmail);
        settings.endGroup();
    }
    settings.endGroup();

    settings.setValue("General/lastRepository", m_repoPath);
    settings.sync();
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
        const RepositoryMetadata cached = m_repositoryMetadata.value(path);
        info.remoteUrl = cached.remoteUrl.isEmpty() ? m_repoRemotes.value(path) : cached.remoteUrl;
        bool existsOnDisk = QDir(path).exists() && (QFile::exists(path + "/.git") || QDir(path + "/.git").exists());
        info.isMissing = !existsOnDisk;

        if (path == m_repoPath) {
            if (m_isMissing) {
                info.isMissing = true;
                info.currentBranch = "-";
            } else {
                auto b = getCurrentBranch();
                info.currentBranch = b ? b->name : "main";
                info.changedFilesCount = m_changedFilesCacheValid ? m_changedFilesCache.size() : 0;
                info.aheadCount = m_remoteStatus.ahead;
                info.behindCount = m_remoteStatus.behind;
                info.lastFetchedTime = m_remoteStatus.lastFetchedText;
            }
        } else {
            info.currentBranch = cached.currentBranch.isEmpty() ? "main" : cached.currentBranch;
            info.aheadCount = cached.ahead;
            info.behindCount = cached.behind;
            info.lastFetchedTime = !cached.lastFetchedText.isEmpty()
                ? cached.lastFetchedText
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
    const RepositoryMetadata cached = m_repositoryMetadata.value(m_repoPath);
    info.remoteUrl = cached.remoteUrl.isEmpty() ? m_repoRemotes.value(m_repoPath) : cached.remoteUrl;
    if (m_isMissing) {
        info.currentBranch = "-";
        info.lastFetchedTime = "Repository not found on disk";
        return info;
    }
    auto b = getCurrentBranch();
    info.currentBranch = b ? b->name : "main";
    info.changedFilesCount = m_changedFilesCacheValid ? m_changedFilesCache.size() : 0;
    info.aheadCount = m_remoteStatus.ahead;
    info.behindCount = m_remoteStatus.behind;
    info.lastFetchedTime = m_remoteStatus.lastFetchedText;
    return info;
}

bool GitCliService::openRepository(const QString &pathOrId)
{
    // Opening a repository changes the reader, caches, watcher, and current
    // path as one logical transition. Never let two worker threads interleave
    // those updates.
    // Invalidate refresh workers before waiting for the transition lock. This
    // lets a long status/history scan cancel promptly instead of delaying a
    // switch to a cached repository.
    m_repositoryGeneration.fetch_add(1);
    QMutexLocker repositoryLocker(&m_repositoryOpenMutex);
    QString targetPath = pathOrId;
    if (!m_knownRepos.contains(targetPath)) {
        for (auto it = m_knownRepos.begin(); it != m_knownRepos.end(); ++it) {
            if (it.value() == pathOrId) {
                targetPath = it.key();
                break;
            }
        }
    }

    const QString normalizedTarget = QDir::cleanPath(QFileInfo(targetPath).absoluteFilePath());
    if (!m_repoPath.isEmpty() && !m_isMissing && normalizedTarget == QDir::cleanPath(m_repoPath)) {
        // Re-opening the current repository should not throw away its live
        // state or start a second expensive load. A background refresh can still
        // pick up external edits without showing the repository-loading UI.
        if (m_initialLoadPending.load(std::memory_order_acquire) || m_refreshInProgress) {
            // The generation bump above may have canceled the in-flight scan;
            // restart it after this no-op reopen instead of leaving the repo in
            // its initial-load state forever.
            m_refreshInProgress = false;
            m_initialLoadPending.store(false, std::memory_order_release);
            const quint64 generation = m_repositoryGeneration.load();
            QMetaObject::invokeMethod(this, [this, generation]() {
                if (m_repositoryGeneration.load() == generation) refreshRepository();
            }, Qt::QueuedConnection);
            return true;
        }
        if (restoreSession(m_repoPath)) {
            clearUndoState();
            emitRepositoryRefreshSignals(true);
        }
        const quint64 generation = m_repositoryGeneration.load();
        QMetaObject::invokeMethod(this, [this, generation]() {
            if (m_repositoryGeneration.load() == generation) refreshRepository();
        }, Qt::QueuedConnection);
        return true;
    }

    if (!m_repositoryReader.open(targetPath)) {
        // Directory missing or not a valid Git repo -> enter missing repository state
        m_repoPath = targetPath;
        if (m_knownRepos.contains(targetPath) && !m_knownRepos.value(targetPath).trimmed().isEmpty()) {
            m_repoName = m_knownRepos.value(targetPath).trimmed();
        } else if (m_knownRepos.contains(pathOrId) && !m_knownRepos.value(pathOrId).trimmed().isEmpty()) {
            m_repoName = m_knownRepos.value(pathOrId).trimmed();
        } else {
            m_repoName = QFileInfo(targetPath).fileName();
        }
        if (m_repoName.isEmpty()) m_repoName = "Repository";
        m_isMissing = true;
        m_refreshInProgress = false;
        m_initialLoadPending.store(false, std::memory_order_release);
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
    m_initialLoadPending.store(true, std::memory_order_release);
    m_remoteStatus = RemoteStatus{};
    const QString discoveredPath = m_repositoryReader.workTreePath().isEmpty()
        ? QDir::cleanPath(targetPath)
        : m_repositoryReader.workTreePath();
    m_repoPath = discoveredPath;

    if (m_knownRepos.contains(m_repoPath) && !m_knownRepos.value(m_repoPath).trimmed().isEmpty()) {
        m_repoName = m_knownRepos.value(m_repoPath).trimmed();
    } else if (m_knownRepos.contains(targetPath) && !m_knownRepos.value(targetPath).trimmed().isEmpty()) {
        m_repoName = m_knownRepos.value(targetPath).trimmed();
    } else if (m_knownRepos.contains(pathOrId) && !m_knownRepos.value(pathOrId).trimmed().isEmpty()) {
        m_repoName = m_knownRepos.value(pathOrId).trimmed();
    } else {
        m_repoName = QFileInfo(m_repoPath).fileName();
    }
    if (m_repoName.isEmpty()) m_repoName = "Repository";

    if (targetPath != m_repoPath && m_knownRepos.contains(targetPath)) {
        m_knownRepos.remove(targetPath);
        if (m_repoRemotes.contains(targetPath) && !m_repoRemotes.contains(m_repoPath)) {
            m_repoRemotes.insert(m_repoPath, m_repoRemotes.take(targetPath));
        }
        if (m_repoFetchTimes.contains(targetPath) && !m_repoFetchTimes.contains(m_repoPath)) {
            m_repoFetchTimes.insert(m_repoPath, m_repoFetchTimes.take(targetPath));
        }
        if (m_repositoryMetadata.contains(targetPath) && !m_repositoryMetadata.contains(m_repoPath)) {
            m_repositoryMetadata.insert(m_repoPath, m_repositoryMetadata.take(targetPath));
        }
    }

    // Preserve a complete session snapshot for this repository so switching
    // back can restore it immediately. The new repository's live caches still
    // need to be rebuilt below.
    invalidateRepositoryCaches(false);

    // Restore this repo's persisted last-fetch time so "Last fetched" survives restarts.
    m_lastFetchTime = m_repoFetchTimes.value(m_repoPath);
    m_authorCacheValid = false;

    m_knownRepos.insert(m_repoPath, m_repoName);
    loadVitalRepositoryState();

    // Reset selection & undo state. The watcher belongs to the GUI thread, so
    // install it there even when this operation is running in the loader thread.
    m_fileSelection.clear();
    clearUndoState();
    QMetaObject::invokeMethod(this, [this]() { setupFileSystemWatcher(); }, Qt::QueuedConnection);

    // Publish the vital snapshot only after it is complete. Expensive status,
    // history, and file enumeration are deliberately left to refreshRepository.
    RepositoryInfo info;
    info.id = m_repoPath;
    info.name = m_repoName;
    info.path = m_repoPath;
    info.currentBranch = m_currentBranchCache ? m_currentBranchCache->name : QStringLiteral("main");
    info.changedFilesCount = 0;
    info.aheadCount = m_remoteStatus.ahead;
    info.behindCount = m_remoteStatus.behind;
    info.lastFetchedTime = m_remoteStatus.lastFetchedText;
    info.isMissing = false;
    info.remoteUrl = m_remoteStatus.remoteUrl;
    emit repositoryChanged(info);
    const quint64 generation = m_repositoryGeneration.load();
    QMetaObject::invokeMethod(this, [this, generation]() {
        if (m_repositoryGeneration.load() == generation) refreshRepository();
    }, Qt::QueuedConnection);
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
    QString realName = name.trimmed();
    if (realName.isEmpty()) {
        realName = m_knownRepos.value(realPath, m_knownRepos.value(targetPath, QFileInfo(realPath).fileName()));
    }
    if (realName.isEmpty()) {
        realName = QFileInfo(realPath).fileName();
    }
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

bool GitCliService::renameRepository(const QString &repoIdOrPath, const QString &newName)
{
    QString targetPath = repoIdOrPath.trimmed();
    if (targetPath.isEmpty()) {
        targetPath = m_repoPath;
    }

    if (!m_knownRepos.contains(targetPath)) {
        QString clean = QDir::cleanPath(targetPath);
        if (m_knownRepos.contains(clean)) {
            targetPath = clean;
        } else {
            for (auto it = m_knownRepos.begin(); it != m_knownRepos.end(); ++it) {
                if (it.value() == repoIdOrPath || QDir::cleanPath(it.key()) == clean) {
                    targetPath = it.key();
                    break;
                }
            }
        }
    }

    if (targetPath.isEmpty() || !m_knownRepos.contains(targetPath)) {
        emit operationFailed(QString("Repository '%1' not found").arg(repoIdOrPath));
        return false;
    }

    QString cleanName = newName.trimmed();
    if (cleanName.isEmpty()) {
        cleanName = QFileInfo(targetPath).fileName();
    }

    m_knownRepos.insert(targetPath, cleanName);
    if (m_repoPath == targetPath || (!m_repoPath.isEmpty() && QDir::cleanPath(m_repoPath) == QDir::cleanPath(targetPath))) {
        m_repoName = cleanName;
    }

    saveRepositories();

    auto cur = getCurrentRepository();
    emit repositoryChanged(cur ? *cur : RepositoryInfo{});
    return true;
}

static void parseGitCloneProgress(const QString &rawLine, double &progress, QString &stage, QString &details)
{
    QString line = rawLine.trimmed();
    if (line.isEmpty()) return;

    // Strip ANSI escape codes
    line.remove(QRegularExpression(R"(\x1B\[[0-9;]*[a-zA-Z])"));
    line = line.trimmed();
    if (line.isEmpty()) return;

    if (line.endsWith(", done.")) {
        line = line.left(line.length() - 7).trimmed();
    } else if (line.endsWith("done.")) {
        line = line.left(line.length() - 5).trimmed();
    }

    // 1. Receiving objects:  45% (10830/24066), 5.21 MiB | 1.12 MiB/s
    static const QRegularExpression rxReceiving(R"((?:remote:\s*)?Receiving objects:\s*(\d+)%(?:\s*\(([^)]+)\))?(?:,\s*(.+))?)");
    auto matchReceiving = rxReceiving.match(line);
    if (matchReceiving.hasMatch()) {
        int pct = matchReceiving.captured(1).toInt();
        QString counts = matchReceiving.captured(2).trimmed();
        QString extra = matchReceiving.captured(3).trimmed();

        stage = QString("Receiving objects (%1%)").arg(pct);
        progress = 0.15 + (pct / 100.0) * 0.65; // 15% -> 80%

        QStringList parts;
        if (!counts.isEmpty()) parts << counts + " objects";
        if (!extra.isEmpty()) parts << extra;
        details = parts.join(" • ");
        return;
    }

    // 2. Resolving deltas:  80% (1234/1542)
    static const QRegularExpression rxDeltas(R"((?:remote:\s*)?Resolving deltas:\s*(\d+)%(?:\s*\(([^)]+)\))?)");
    auto matchDeltas = rxDeltas.match(line);
    if (matchDeltas.hasMatch()) {
        int pct = matchDeltas.captured(1).toInt();
        QString counts = matchDeltas.captured(2).trimmed();

        stage = QString("Resolving deltas (%1%)").arg(pct);
        progress = 0.80 + (pct / 100.0) * 0.15; // 80% -> 95%
        details = counts.isEmpty() ? QString() : (counts + " deltas");
        return;
    }

    // 3. Updating / Checking out files:  60% (120/200)
    static const QRegularExpression rxUpdating(R"((?:remote:\s*)?(?:Updating files|Checking out files):\s*(\d+)%(?:\s*\(([^)]+)\))?)");
    auto matchUpdating = rxUpdating.match(line);
    if (matchUpdating.hasMatch()) {
        int pct = matchUpdating.captured(1).toInt();
        QString counts = matchUpdating.captured(2).trimmed();

        stage = QString("Updating files (%1%)").arg(pct);
        progress = 0.95 + (pct / 100.0) * 0.05; // 95% -> 100%
        details = counts.isEmpty() ? QString() : (counts + " files");
        return;
    }

    // 4. Filtering content:  50% (5/10)
    static const QRegularExpression rxFiltering(R"((?:remote:\s*)?Filtering content:\s*(\d+)%(?:\s*\(([^)]+)\))?)");
    auto matchFiltering = rxFiltering.match(line);
    if (matchFiltering.hasMatch()) {
        int pct = matchFiltering.captured(1).toInt();
        QString counts = matchFiltering.captured(2).trimmed();

        stage = QString("Filtering content (%1%)").arg(pct);
        progress = 0.95 + (pct / 100.0) * 0.05;
        details = counts.isEmpty() ? QString() : (counts + " files");
        return;
    }

    // 5. Compressing objects:  33% (10/30)
    static const QRegularExpression rxCompress(R"((?:remote:\s*)?Compressing objects:\s*(\d+)%(?:\s*\(([^)]+)\))?)");
    auto matchCompress = rxCompress.match(line);
    if (matchCompress.hasMatch()) {
        int pct = matchCompress.captured(1).toInt();
        QString counts = matchCompress.captured(2).trimmed();

        stage = QString("Compressing objects (%1%)").arg(pct);
        progress = 0.05 + (pct / 100.0) * 0.10; // 5% -> 15%
        details = counts.isEmpty() ? QString() : (counts + " objects");
        return;
    }

    // 6. Counting objects:  50% (15/31)
    static const QRegularExpression rxCount(R"((?:remote:\s*)?Counting objects:\s*(\d+)%(?:\s*\(([^)]+)\))?)");
    auto matchCount = rxCount.match(line);
    if (matchCount.hasMatch()) {
        int pct = matchCount.captured(1).toInt();
        QString counts = matchCount.captured(2).trimmed();

        stage = QString("Counting objects (%1%)").arg(pct);
        progress = (pct / 100.0) * 0.05; // 0% -> 5%
        details = counts.isEmpty() ? QString() : (counts + " objects");
        return;
    }

    // 7. Enumerating objects: 24066
    static const QRegularExpression rxEnum(R"((?:remote:\s*)?Enumerating objects:\s*(\d+)(?:%\s*\(([^)]+)\))?)");
    auto matchEnum = rxEnum.match(line);
    if (matchEnum.hasMatch()) {
        stage = "Enumerating objects...";
        progress = 0.02;
        details = QString("%1 objects found").arg(matchEnum.captured(1));
        return;
    }

    // 8. Generic progress with percentage
    static const QRegularExpression rxGeneric(R"((.+?):\s*(\d+)%(?:\s*\(([^)]+)\))?(?:,\s*(.+))?)");
    auto matchGeneric = rxGeneric.match(line);
    if (matchGeneric.hasMatch()) {
        QString stageName = matchGeneric.captured(1).trimmed();
        if (stageName.startsWith("remote: ")) stageName = stageName.mid(8).trimmed();
        int pct = matchGeneric.captured(2).toInt();
        QString counts = matchGeneric.captured(3).trimmed();
        QString extra = matchGeneric.captured(4).trimmed();

        stage = QString("%1 (%2%)").arg(stageName).arg(pct);
        progress = (pct / 100.0);
        QStringList parts;
        if (!counts.isEmpty()) parts << counts;
        if (!extra.isEmpty()) parts << extra;
        details = parts.join(" • ");
        return;
    }

    // 9. Connecting / Initializing fallback
    if (line.startsWith("Cloning into", Qt::CaseInsensitive)) {
        stage = "Connecting to repository...";
        progress = -1.0;
        details = line;
        return;
    }
}

bool GitCliService::cloneRepository(const QString &url, const QString &targetPath)
{
    QString cleanUrl = url.trimmed();
    QString cleanPath = targetPath.trimmed();
    if (cleanPath.startsWith("file://")) {
        cleanPath = QUrl(cleanPath).toLocalFile();
    }
    if (cleanPath.startsWith("~/") || cleanPath == "~") {
        cleanPath = QDir::homePath() + cleanPath.mid(1);
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

    QString parentDir = QFileInfo(cleanPath).path();
    if (!parentDir.isEmpty()) {
        QDir().mkpath(parentDir);
    }

    if (m_shuttingDown.load(std::memory_order_acquire)) {
        return false;
    }

    QString workingDir = parentDir;
    if (workingDir.isEmpty() || !QDir(workingDir).exists()) {
        workingDir = QDir::homePath();
    }

    emit cloneProgressUpdated(-1.0, tr("Connecting to repository..."), redactRemoteUrl(cleanUrl));

    QProcess process;
    configureGitProcess(process);
    ScopedProcessTracker processTracker(this, &process);
    process.setWorkingDirectory(workingDir);

    QProcessEnvironment env = QProcessEnvironment::systemEnvironment();
    env.insert("LC_ALL", "C");
    env.insert("GIT_TERMINAL_PROMPT", "0");
    if (!env.contains("GIT_SSH_COMMAND")) {
        env.insert("GIT_SSH_COMMAND", "ssh -o BatchMode=yes");
    }
    process.setProcessEnvironment(env);

    process.start("git", {"clone", "--progress", "--", cleanUrl, cleanPath});
    if (!process.waitForStarted(10000)) {
        emit operationFailed(tr("Failed to start git clone process."));
        return false;
    }

    QString errBuffer;
    QString fullStdErr;
    QString fullStdOut;
    double currentProgress = -1.0;
    QString currentStage = tr("Connecting to repository...");
    QString currentDetails = redactRemoteUrl(cleanUrl);

    auto processStderrChunk = [&](const QByteArray &chunk) {
        if (chunk.isEmpty()) return;
        QString text = QString::fromUtf8(chunk);
        fullStdErr += text;
        errBuffer += text;

        int sepIdx = -1;
        while ((sepIdx = errBuffer.indexOf(QRegularExpression("[\r\n]"))) != -1) {
            QString token = errBuffer.left(sepIdx).trimmed();
            errBuffer = errBuffer.mid(sepIdx + 1);

            if (!token.isEmpty()) {
                double newProg = currentProgress;
                QString newStage = currentStage;
                QString newDetails = currentDetails;
                parseGitCloneProgress(token, newProg, newStage, newDetails);
                if (newProg != currentProgress || newStage != currentStage || newDetails != currentDetails) {
                    currentProgress = newProg;
                    currentStage = newStage;
                    currentDetails = newDetails;
                    emit cloneProgressUpdated(currentProgress, currentStage, currentDetails);
                }
            }
        }
    };

    while (process.state() != QProcess::NotRunning) {
        if (m_shuttingDown.load(std::memory_order_acquire)) {
            killProcess(&process);
            process.waitForFinished(100);
            break;
        }
        process.waitForReadyRead(50);
        QByteArray errChunk = process.readAllStandardError();
        if (!errChunk.isEmpty()) {
            processStderrChunk(errChunk);
        }
        QByteArray outChunk = process.readAllStandardOutput();
        if (!outChunk.isEmpty()) {
            fullStdOut += QString::fromUtf8(outChunk);
        }
    }

    processStderrChunk(process.readAllStandardError());
    fullStdOut += QString::fromUtf8(process.readAllStandardOutput());

    bool ok = (process.exitStatus() == QProcess::NormalExit && process.exitCode() == 0);
    if (!ok) {
        emit operationFailed(formatGitError(fullStdErr.isEmpty() ? fullStdOut : fullStdErr, tr("Git clone failed")));
        return false;
    }

    emit cloneProgressUpdated(1.0, tr("Clone completed"), QString());

    // Git clone on POSIX systems automatically writes core.filemode=true to .git/config.
    // If the user has global ignore file mode enabled, remove the local override so the
    // global preference takes effect immediately.
    if (ignoreFileModeChanges(true)) {
        runGit({"config", "--local", "--unset", "core.filemode"}, cleanPath, 2000);
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
    if (m_isMissing) return {};
    if (shouldDeferExpensiveInitialRead()) return {};
    if (m_branchesCacheValid) return m_branchesCache;
    if (shouldReturnStaleCache()) return m_branchesCache;
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
    if (m_isMissing) return std::nullopt;
    if (m_currentBranchCacheValid) return m_currentBranchCache;
    if (shouldReturnStaleCache()) return m_currentBranchCache;
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
    if (m_isMissing) return {};
    if (shouldDeferExpensiveInitialRead()) return {};
    if (m_changedFilesCacheValid) return m_changedFilesCache;
    if (shouldReturnStaleCache()) return m_changedFilesCache;
    if (m_repoPath.isEmpty()) return {};

    locker.unlock();

    // Query changed & untracked files with all untracked scanning to list nested files
    GitResult statusRes = runGit({"--no-optional-locks", "status", "--porcelain=v1", "-uall", "-z"}, QString(), 10000);
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
    QSet<QString> currentPaths;
    currentPaths.reserve(statusRecords.size());
    for (const StatusRecord &record : statusRecords) {
        const QChar x = record.x;
        const QChar y = record.y;
        const QString &filePath = record.filePath;
        const QString &oldPath = record.oldFilePath;
        currentPaths.insert(filePath);

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

    for (auto it = m_fileSelection.begin(); it != m_fileSelection.end();) {
        if (!currentPaths.contains(it.key())) {
            it = m_fileSelection.erase(it);
        } else {
            ++it;
        }
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
    cacheCurrentSession();
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
    cacheCurrentSession();
    emit changedFilesUpdated();
}

bool GitCliService::discardFileChanges(const QString &filePath)
{
    if (m_repoPath.isEmpty() || !isSafeRepositoryPath(filePath)) return false;

    GitResult check = runGit({"--no-optional-locks", "status", "--porcelain=v1", "-uall", "-z", "--", filePath}, QString(), 3000);
    const bool isUntracked = check.stdOut.startsWith("??");
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

    // Mark this key as the active in-flight request. If another diff is
    // requested before this one completes, only the newest request may fill
    // the cache.
    m_fileDiffCachePath = cacheKey;
    m_fileDiffCacheValid = false;
    locker.unlock();

    QString effectivePath = filePath.isEmpty() ? oldFilePath : filePath;

    // If untracked file or folder, read safely from disk
    GitResult statusCheck = runGit({"--no-optional-locks", "status", "--porcelain=v1", "-uall", "--", effectivePath}, QString(), 2000);
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
        if (!f.open(QIODevice::ReadOnly | QIODevice::Text)) {
            locker.relock();
            return {};
        }
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
    if (m_shuttingDown.load(std::memory_order_acquire)) return {};

    // Capture binary output from git show <ref>:<path>
    QProcess process;
    configureGitProcess(process);
    ScopedProcessTracker processTracker(this, &process);
    process.setWorkingDirectory(m_repoPath);
    QProcessEnvironment env = QProcessEnvironment::systemEnvironment();
    env.insert("LC_ALL", "C");
    env.insert("GIT_TERMINAL_PROMPT", "0");
    process.setProcessEnvironment(env);

    process.start("git", {"show", "--end-of-options", QString("%1:%2").arg(ref, filePath)});
    while (process.state() != QProcess::NotRunning) {
        if (m_shuttingDown.load(std::memory_order_acquire)) {
            killProcess(&process);
            process.waitForFinished(100);
            return {};
        }
        if (process.waitForFinished(100)) {
            break;
        }
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
    if (m_isMissing) return {};
    if (shouldDeferExpensiveInitialRead()) return {};
    if (m_repoPath.isEmpty()) return {};
    if (!m_commitHistoryCacheValid && shouldReturnStaleCache()) return m_commitHistoryCache;
    const int fetchCount = std::max(limit > 0 ? limit : 200, 200);
    if (m_commitHistoryCacheValid) {
        return (limit > 0) ? m_commitHistoryCache.mid(0, limit) : m_commitHistoryCache;
    }

    // Fast Git CLI query using porcelain record/unit separators
    locker.unlock();
    GitResult logRes = runGit({"log", QString("-%1").arg(fetchCount), "--format=%H%x1f%h%x1f%s%x1f%b%x1f%an%x1f%ae%x1f%at%x1f%D%x1e"}, QString(), 4000);
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
            item.authorAvatarUrl = AvatarResolver::resolve(item.authorName, item.authorEmail);
            qint64 epoch = fields[6].trimmed().toLongLong();
            item.timestamp = QDateTime::fromSecsSinceEpoch(epoch, QTimeZone::UTC);
            item.relativeTime = formatRelativeTime(item.timestamp);

            if (fields.size() >= 8) {
                const QString decorations = fields[7].trimmed();
                if (!decorations.isEmpty()) {
                    const QStringList parts = decorations.split(QLatin1Char(','), Qt::SkipEmptyParts);
                    for (const QString &part : parts) {
                        const QString tagRef = part.trimmed();
                        if (tagRef.startsWith(QLatin1String("tag: "))) {
                            item.tags.append(tagRef.mid(5).trimmed());
                        }
                    }
                }
            }

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
    GitResult showRes = runGit({"show", "-s", "--format=%H%x1f%h%x1f%s%x1f%b%x1f%an%x1f%ae%x1f%at%x1f%D", "--end-of-options", revision}, QString(), 4000);
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
            commit.authorAvatarUrl = AvatarResolver::resolve(commit.authorName, commit.authorEmail);
            const qint64 epoch = fields[6].trimmed().toLongLong();
            commit.timestamp = QDateTime::fromSecsSinceEpoch(epoch, QTimeZone::UTC);
            commit.relativeTime = formatRelativeTime(commit.timestamp);

            if (fields.size() >= 8) {
                const QString decorations = fields[7].trimmed();
                if (!decorations.isEmpty()) {
                    const QStringList parts = decorations.split(QLatin1Char(','), Qt::SkipEmptyParts);
                    for (const QString &part : parts) {
                        const QString tagRef = part.trimmed();
                        if (tagRef.startsWith(QLatin1String("tag: "))) {
                            commit.tags.append(tagRef.mid(5).trimmed());
                        }
                    }
                }
            }

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
    if (m_repoPath.isEmpty() || m_isMissing) return false;

    // Check if HEAD exists
    GitResult headRes = const_cast<GitCliService*>(this)->runGit({"rev-parse", "--verify", "HEAD"}, QString(), 2000);
    if (!headRes.success || headRes.stdOut.trimmed().isEmpty()) return false;

    // If upstream tracking branch is present, verify HEAD has not been pushed
    GitResult upstreamRes = const_cast<GitCliService*>(this)->runGit({"rev-parse", "--verify", "@{upstream}"}, QString(), 2000);
    if (upstreamRes.success && !upstreamRes.stdOut.trimmed().isEmpty()) {
        GitResult ancestorRes = const_cast<GitCliService*>(this)->runGit({"merge-base", "--is-ancestor", "HEAD", "@{upstream}"}, QString(), 2000);
        if (ancestorRes.success) {
            return false; // Already pushed to remote upstream
        }
        if (m_remoteStatus.ahead <= 0) {
            return false;
        }
        return true;
    }

    // Branch has no upstream tracking branch. Check if remotes exist.
    GitResult remoteRes = const_cast<GitCliService*>(this)->runGit({"remote"}, QString(), 2000);
    if (remoteRes.success && !remoteRes.stdOut.trimmed().isEmpty()) {
        GitResult unpushedRes = const_cast<GitCliService*>(this)->runGit({"rev-list", "--count", "HEAD", "--not", "--remotes"}, QString(), 2000);
        if (unpushedRes.success) {
            return unpushedRes.stdOut.trimmed().toInt() > 0;
        }
    }

    // No remotes configured or fresh repository: any commit on HEAD is local/unpushed
    return true;
}

QString GitCliService::getLastUndoCommitSha() const
{
    if (!m_lastUndoCommitSha.isEmpty()) {
        return m_lastUndoCommitSha;
    }
    if (canUndoCommit()) {
        if (m_commitHistoryCacheValid && !m_commitHistoryCache.isEmpty()) {
            return m_commitHistoryCache.first().sha;
        }
        GitResult res = const_cast<GitCliService*>(this)->runGit({"rev-parse", "HEAD"}, QString(), 2000);
        if (res.success) {
            return res.stdOut.trimmed();
        }
    }
    return QString();
}

QString GitCliService::getLastUndoCommitSummary() const
{
    if (!m_lastUndoCommitSummary.isEmpty()) {
        return m_lastUndoCommitSummary;
    }
    if (canUndoCommit()) {
        if (m_commitHistoryCacheValid && !m_commitHistoryCache.isEmpty()) {
            return m_commitHistoryCache.first().summary;
        }
        GitResult res = const_cast<GitCliService*>(this)->runGit({"log", "-1", "--format=%s", "HEAD"}, QString(), 2000);
        if (res.success) {
            return res.stdOut.trimmed();
        }
    }
    return QString();
}

QString GitCliService::getLastUndoCommitDescription() const
{
    if (!m_lastUndoCommitDescription.isEmpty()) {
        return m_lastUndoCommitDescription;
    }
    if (canUndoCommit()) {
        if (m_commitHistoryCacheValid && !m_commitHistoryCache.isEmpty()) {
            return m_commitHistoryCache.first().description;
        }
        GitResult res = const_cast<GitCliService*>(this)->runGit({"log", "-1", "--format=%b", "HEAD"}, QString(), 2000);
        if (res.success) {
            return res.stdOut.trimmed();
        }
    }
    return QString();
}

QStringList GitCliService::getLastUndoCommitCoAuthors() const
{
    if (!m_lastUndoCommitCoAuthors.isEmpty()) {
        return m_lastUndoCommitCoAuthors;
    }
    if (canUndoCommit()) {
        if (m_commitHistoryCacheValid && !m_commitHistoryCache.isEmpty()) {
            return m_commitHistoryCache.first().coAuthors;
        }
    }
    return QStringList();
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
    if (m_remoteStatusCacheValid || shouldReturnStaleCache()) {
        return m_remoteStatus.hasRemote;
    }
    return const_cast<GitCliService *>(this)->getRemoteStatus().hasRemote;
}

QString GitCliService::getRemoteUrl(const QString &remoteName) const
{
    if (m_repoPath.isEmpty()) return QString();
    const QString target = remoteName.isEmpty() ? QStringLiteral("origin") : remoteName.trimmed();
    static const QRegularExpression remoteNameRegex("^[A-Za-z0-9][A-Za-z0-9._-]*$");
    if (!remoteNameRegex.match(target).hasMatch()) return QString();

    if ((m_remoteStatusCacheValid || shouldReturnStaleCache()) && m_remoteStatus.hasRemote) {
        // The no-argument API historically uses "origin", but the selected
        // sync remote may legitimately be named upstream or something custom.
        if (target == m_remoteStatus.remoteName || target == QStringLiteral("origin")) {
            return m_remoteStatus.remoteUrl;
        }
    }

    GitResult res = const_cast<GitCliService *>(this)->runGit({"remote", "get-url", target}, QString(), 2000);
    return res.success ? res.stdOut.trimmed() : QString();
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
        invalidateRepositoryCaches();
        const RemoteStatus status = getRemoteStatus();
        if (status.hasRemote && !status.remoteUrl.isEmpty()) {
            m_repoRemotes[m_repoPath] = redactRemoteUrl(status.remoteUrl);
        } else {
            m_repoRemotes.remove(m_repoPath);
        }
        cacheCurrentRepositoryMetadata();
        saveRepositories();
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
        const RemoteStatus status = getRemoteStatus();
        if (status.hasRemote && !status.remoteUrl.isEmpty()) {
            m_repoRemotes[m_repoPath] = redactRemoteUrl(status.remoteUrl);
        } else {
            m_repoRemotes.remove(m_repoPath);
        }
        cacheCurrentRepositoryMetadata();
        saveRepositories();
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

    if (hasRemote()) {
        emit operationFailed("A Git remote is already configured for this repository");
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
        if (m_shuttingDown.load(std::memory_order_acquire)) return;

        QStringList args = {"repo", "create", repoName, isPrivate ? "--private" : "--public", "--source=.", "--remote=origin", "--push"};
        if (!desc.isEmpty()) {
            args << "--description" << desc;
        }

        QProcess process;
        configureGitProcess(process);
        ScopedProcessTracker processTracker(this, &process);
        process.setWorkingDirectory(dir);
        QProcessEnvironment env = QProcessEnvironment::systemEnvironment();
        env.insert("LC_ALL", "C");
        env.insert("GIT_TERMINAL_PROMPT", "0");
        if (!env.contains("GIT_SSH_COMMAND")) {
            env.insert("GIT_SSH_COMMAND", "ssh -o BatchMode=yes");
        }
        process.setProcessEnvironment(env);
        process.start("gh", args);
        bool cancelled = false;
        QElapsedTimer timer;
        timer.start();
        while (process.state() != QProcess::NotRunning && timer.elapsed() < 120000) {
            if (m_shuttingDown.load(std::memory_order_acquire) ||
                m_repositoryGeneration.load() != operationGeneration) {
                cancelled = true;
                killProcess(&process);
                process.waitForFinished(100);
                break;
            }
            process.waitForFinished(100);
        }
        const bool finished = !cancelled && process.state() == QProcess::NotRunning;
        if (!finished && process.state() != QProcess::NotRunning) {
            killProcess(&process);
            process.waitForFinished(100);
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
                emit operationSucceeded(QString("Published repository '%1'").arg(repoName));
            } else {
                getRemoteStatus();
                QString errorMsg = stdErr.trimmed().isEmpty() ? stdOut.trimmed() : stdErr.trimmed();
                emit operationFailed(QString("Failed to publish with gh CLI: %1").arg(errorMsg.isEmpty() ? "Process timed out or unknown error" : errorMsg));
            }
        }, Qt::QueuedConnection);
    });
    trackWorker(thread)->start();
    return true;
}

RemoteStatus GitCliService::getRemoteStatus()
{
    QMutexLocker locker(&m_cacheMutex);
    if (m_isMissing) return m_remoteStatus;
    if (m_remoteStatusCacheValid) return m_remoteStatus;
    if (shouldReturnStaleCache()) return m_remoteStatus;
    if (m_repoPath.isEmpty()) return m_remoteStatus;

    locker.unlock();

    GitResult res = const_cast<GitCliService*>(this)->runGit({"remote"}, QString(), 2000);
    m_remoteStatus.hasRemote = res.success && !res.stdOut.trimmed().isEmpty();

    if (!m_remoteStatus.hasRemote) {
        m_remoteStatus.remoteName = "";
        m_remoteStatus.remoteUrl = "";
        m_remoteStatus.ahead = 0;
        m_remoteStatus.behind = 0;
        m_remoteStatus.lastFetchedText = "No remote repository configured";
        m_remoteStatusCacheValid = true;
        if (!m_suppressRefreshSignals) emit remoteStatusUpdated(m_remoteStatus);
        return m_remoteStatus;
    }

    const QString remoteName = preferredRemoteName();
    m_remoteStatus.remoteName = remoteName;
    m_remoteStatus.remoteUrl = getRemoteUrl(remoteName);

    // Ahead / behind count against upstream tracking branch
    GitResult countRes = const_cast<GitCliService*>(this)->runGit(
        {"rev-list", "--left-right", "--count", "HEAD...@{upstream}"}, QString(), 3000);

    if (countRes.success) {
        QStringList parts = countRes.stdOut.trimmed().split(QRegularExpression("\\s+"), Qt::SkipEmptyParts);
        if (parts.size() >= 2) {
            m_remoteStatus.ahead = parts[0].toInt();
            m_remoteStatus.behind = parts[1].toInt();
        }
    } else {
        // Check if there are unpushed commits on a branch without upstream
        GitResult unpushedRes = runGit({"rev-list", "--count", "HEAD", "--not", "--remotes"}, QString(), 2000);
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
        } else {
            m_remoteStatus.lastFetchedText = "Fetch failed";
            emit remoteStatusUpdated(m_remoteStatus);
            emit operationFailed(formatGitError(res.stdErr, "Fetch failed"));
        }
    }, operationGeneration);
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
        } else {
            m_remoteStatus.lastFetchedText = "Pull failed";
            emit remoteStatusUpdated(m_remoteStatus);
            emit operationFailed(formatGitError(res.stdErr, "Pull failed"));
        }
    }, operationGeneration);
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
        // Fetch remote origin before pushing to avoid stale refs and pushing against unseen changes
        GitResult fetchRes = const_cast<GitCliService*>(this)->runGit({"fetch", remoteName, "--prune"}, dir, 120000, operationGeneration);
        if (!fetchRes.success) {
            QMetaObject::invokeMethod(this, [this, fetchRes, operationRepoPath, operationGeneration]() {
                if (m_repositoryGeneration.load() != operationGeneration || m_repoPath != operationRepoPath) return;
                m_remoteStatus.isPushing = false;
                emit remoteStatusUpdated(m_remoteStatus);
                emit operationFailed(formatGitError(fetchRes.stdErr, "Fetch before push failed"));
            }, Qt::QueuedConnection);
            return;
        }

        GitResult upstreamCheck = const_cast<GitCliService*>(this)->runGit({"rev-parse", "--verify", "@{upstream}"}, dir, 3000, operationGeneration);
        QStringList pushArgs = {"push"};
        if (!upstreamCheck.success) {
            // Set upstream tracking on initial push
            pushArgs = {"push", "-u", remoteName, branchName};
        }

        GitResult res = const_cast<GitCliService*>(this)->runGit(pushArgs, dir, 120000, operationGeneration);

        QMetaObject::invokeMethod(this, [this, res, remoteName, operationRepoPath, operationGeneration]() {
            if (m_repositoryGeneration.load() != operationGeneration || m_repoPath != operationRepoPath) return;
            m_remoteStatus.isPushing = false;
            if (res.success) {
                m_lastFetchTime = QDateTime::currentDateTime();
                m_repoFetchTimes[operationRepoPath] = m_lastFetchTime;
                saveFetchTimes();
                m_remoteStatus.lastFetchedText = "Last fetched just now";
                clearUndoState();
                invalidateRepositoryCaches();
                getRemoteStatus();
                emit branchListChanged();
                emit commitHistoryUpdated();
            } else {
                invalidateRepositoryCaches();
                getRemoteStatus();
                emit remoteStatusUpdated(m_remoteStatus);
                emit operationFailed(formatGitError(res.stdErr, "Push failed"));
            }
        }, Qt::QueuedConnection);
    });
    trackWorker(thread)->start();
}

QList<StashItem> GitCliService::getStashes()
{
    QMutexLocker locker(&m_cacheMutex);
    if (m_isMissing) return {};
    if (shouldDeferExpensiveInitialRead()) return {};
    if (m_repoPath.isEmpty()) return {};
    if (m_stashesCacheValid) return m_stashesCache;
    if (shouldReturnStaleCache()) return m_stashesCache;

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
    if ((m_remoteStatusCacheValid || shouldReturnStaleCache()) && m_remoteStatus.hasRemote &&
        !m_remoteStatus.remoteName.isEmpty()) {
        return m_remoteStatus.remoteName;
    }
    if (!getRemoteUrl("origin").isEmpty()) return QStringLiteral("origin");

    GitResult remotes = const_cast<GitCliService *>(this)->runGit({"remote"}, QString(), 2000);
    const QStringList names = remotes.stdOut.split(QRegularExpression("\\s+"), Qt::SkipEmptyParts);
    for (const QString &name : names) {
        const QString trimmedName = name.trimmed();
        if (!getRemoteUrl(trimmedName).isEmpty()) return trimmedName;
    }
    return QStringLiteral("origin");
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

    // Check the canonical parent as well as the final entry. This prevents
    // reads/deletes through a symlinked directory when the requested child does
    // not exist yet (the final QFileInfo then has no canonical path).
    const QFileInfo info(full);
    QString canonical = info.exists() ? info.canonicalFilePath() : QString();
    if (canonical.isEmpty()) {
        const QString canonicalParent = QFileInfo(info.path()).canonicalFilePath();
        if (!canonicalParent.isEmpty()) canonical = QDir(canonicalParent).filePath(info.fileName());
    }
    canonical = QDir::cleanPath(canonical);
    if (!canonical.isEmpty() && canonical != root && !canonical.startsWith(root + '/')) return false;
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
    QStringList args = {"config"};
    if (global) {
        args << "--global";
    }
    args << "--get" << "core.filemode";
    GitResult res = runGit(args, QString(), 1000);
    return res.success && res.stdOut.trimmed().compare("false", Qt::CaseInsensitive) == 0;
}

bool GitCliService::setIgnoreFileModeChanges(bool ignored, bool global)
{
    if (!global && m_repoPath.isEmpty()) return false;

    const QString scope = global ? "--global" : "--local";
    GitResult res = runGit({"config", scope, "core.filemode", ignored ? "false" : "true"}, QString(), 2000);

    if (!res.success) {
        emit operationFailed(formatGitError(res.stdErr, "Failed to update Git file metadata settings"));
        return false;
    }

    // Invalidate caches and re-scan so UI updates immediately
    invalidateRepositoryCaches();
    refreshRepository();
    return true;
}

QString GitCliService::getAuthorName() const
{
    if (m_authorCacheValid) return m_authorNameCache.isEmpty() ? QStringLiteral("User") : m_authorNameCache;
    if (m_repoPath.isEmpty()) return getGlobalAuthorName();
    GitResult res = const_cast<GitCliService *>(this)->runGit({"config", "--get", "user.name"}, QString(), 1000);
    if (res.success && !res.stdOut.trimmed().isEmpty()) return res.stdOut.trimmed();
    return getGlobalAuthorName();
}

QString GitCliService::getAuthorEmail() const
{
    if (m_authorCacheValid) return m_authorEmailCache.isEmpty() ? QStringLiteral("user@localhost") : m_authorEmailCache;
    if (m_repoPath.isEmpty()) return getGlobalAuthorEmail();
    GitResult res = const_cast<GitCliService *>(this)->runGit({"config", "--get", "user.email"}, QString(), 1000);
    if (res.success && !res.stdOut.trimmed().isEmpty()) return res.stdOut.trimmed();
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
    if (!ok) {
        emit operationFailed("Failed to update Git author config");
    } else {
        m_authorCacheValid = false;
        loadAuthorCache();
        cacheCurrentRepositoryMetadata();
        saveRepositories();
    }
    return ok;
}

} // namespace Cherry
