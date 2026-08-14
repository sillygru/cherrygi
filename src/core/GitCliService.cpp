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

namespace Cherry {

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
}

void GitCliService::autoStageChanges()
{
    if (m_repoPath.isEmpty()) return;

    // Fast check: look for any untracked or unstaged modifications
    GitResult statusRes = runGit({"status", "--porcelain=v1", "-unormal"}, QString(), 5000);
    if (!statusRes.success || statusRes.stdOut.trimmed().isEmpty()) {
        return;
    }

    bool needsAdd = false;
    const QStringList lines = statusRes.stdOut.split('\n', Qt::SkipEmptyParts);
    for (const QString &line : lines) {
        if (line.length() < 2) continue;
        QChar x = line[0];
        QChar y = line[1];
        // If untracked (??) or worktree has unstaged modifications/deletions/additions (y != ' ')
        if (x == '?' || y != ' ') {
            needsAdd = true;
            break;
        }
    }

    if (needsAdd) {
        runGit({"add", "."}, QString(), 10000);
    }
}

void GitCliService::preloadRepositoryCaches()
{
    // All expensive Git queries happen before the update signals are posted. Model
    // reloads can therefore return these snapshots without running Git on the UI thread.
    m_suppressRefreshSignals = true;
    invalidateRepositoryCaches();
    autoStageChanges();
    getRemoteStatus();
    getCurrentBranch();
    getBranches();
    getChangedFiles();
    getCommitHistory();
    getStashes();

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

    m_refreshInProgress = true;
    const QString repoPath = m_repoPath;
    QThread *thread = QThread::create([this, repoPath]() {
        Q_UNUSED(repoPath);
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

        QMetaObject::invokeMethod(this, [this, filesChanged]() {
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
    env.insert("GIT_SSH_COMMAND", "ssh -o BatchMode=yes -o StrictHostKeyChecking=accept-new");
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
        qWarning().noquote() << QString("[GitCliService] 'git %1' failed (exit %2): %3")
                                    .arg(args.join(' '))
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
        if (!path.isEmpty() && QDir(path).exists()) {
            if (name.isEmpty()) name = QFileInfo(path).fileName();
            m_knownRepos.insert(path, name);
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
        settings.endGroup();
    }
    settings.endGroup();

    settings.setValue("General/lastRepository", m_repoPath);
}

void GitCliService::loadFetchTimes()
{
    m_repoFetchTimes.clear();
    QSettings settings(AppSettings::dataDir() + "/fetchTimes.ini", QSettings::IniFormat);
    settings.beginGroup("FetchTimes");
    const QStringList groups = settings.childGroups();
    for (const QString &group : groups) {
        settings.beginGroup(group);
        QString path = settings.value("path").toString();
        QDateTime dt = QDateTime::fromString(settings.value("time").toString(), Qt::ISODate);
        settings.endGroup();
        if (!path.isEmpty() && dt.isValid()) {
            m_repoFetchTimes.insert(path, dt);
        }
    }
    settings.endGroup();
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

    if (!lastRepo.isEmpty() && QDir(lastRepo).exists()) {
        if (openRepository(lastRepo)) return;
    }

    // Auto-discover the current working directory directly from .git/HEAD.
    GitRepositoryReader reader;
    const QString current = QDir::currentPath();
    if (reader.open(current)) {
        openRepository(reader.workTreePath().isEmpty() ? current : reader.workTreePath());
        return;
    }

    // Check if any saved repo is valid
    for (auto it = m_knownRepos.begin(); it != m_knownRepos.end(); ++it) {
        if (openRepository(it.key())) return;
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

        if (path == m_repoPath) {
            auto b = getCurrentBranch();
            info.currentBranch = b ? b->name : "main";
            info.changedFilesCount = m_fileSelection.size();
            info.aheadCount = m_remoteStatus.ahead;
            info.behindCount = m_remoteStatus.behind;
            info.lastFetchedTime = m_remoteStatus.lastFetchedText;
        } else {
            info.currentBranch = "main";
            QDateTime repoFetch = m_repoFetchTimes.value(path);
            info.lastFetchedTime = repoFetch.isValid()
                ? QString("Last fetched %1").arg(formatRelativeTime(repoFetch))
                : "Not fetched in this session";
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
        emit operationFailed(QString("Not a valid Git repository: %1").arg(pathOrId));
        return false;
    }

    const QString discoveredPath = m_repositoryReader.workTreePath().isEmpty()
        ? QDir::cleanPath(targetPath)
        : m_repositoryReader.workTreePath();
    m_repoPath = discoveredPath;
    m_repoName = QFileInfo(m_repoPath).fileName();
    if (m_repoName.isEmpty()) m_repoName = "Repository";

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
    if (!reader.open(targetPath)) {
        emit operationFailed(QString("Directory '%1' is not a Git repository").arg(targetPath));
        return false;
    }

    QString realPath = reader.workTreePath().isEmpty() ? QDir::cleanPath(targetPath) : reader.workTreePath();
    QString realName = name.trimmed().isEmpty() ? QFileInfo(realPath).fileName() : name.trimmed();
    m_knownRepos.insert(realPath, realName);
    saveRepositories();

    return openRepository(realPath);
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

    m_branchesCache = m_repositoryReader.branches();
    m_branchesCacheValid = true;
    return m_branchesCache;
}

std::optional<BranchInfo> GitCliService::getCurrentBranch()
{
    QMutexLocker locker(&m_cacheMutex);
    if (m_currentBranchCacheValid) return m_currentBranchCache;
    if (m_repoPath.isEmpty()) return std::nullopt;

    m_currentBranchCache = m_repositoryReader.currentBranch();
    m_currentBranchCacheValid = true;
    return m_currentBranchCache;
}

bool GitCliService::switchBranch(const QString &branchName)
{
    if (m_repoPath.isEmpty()) return false;

    GitResult res = runGit({"checkout", branchName}, QString(), 15000);
    if (!res.success) {
        res = runGit({"switch", branchName}, QString(), 15000);
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
    if (m_repoPath.isEmpty() || branchName.trimmed().isEmpty()) return false;

    QString name = branchName.trimmed();
    QStringList args = {"checkout", "-b", name};
    if (!sourceBranch.trimmed().isEmpty()) {
        args.append(sourceBranch.trimmed());
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
    if (m_repoPath.isEmpty() || branchName.trimmed().isEmpty()) return false;

    GitResult res = runGit({"branch", "-D", branchName.trimmed()}, QString(), 10000);
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
    autoStageChanges();

    // Avoid recursively enumerating every file in large untracked directories.
    GitResult statusRes = runGit({"status", "--porcelain=v1", "-unormal"}, QString(), 5000);
    if (!statusRes.success || statusRes.stdOut.trimmed().isEmpty()) {
        locker.relock();
        m_fileSelection.clear();
        m_changedFilesCache.clear();
        m_changedFilesCacheValid = true;
        return m_changedFilesCache;
    }

    // One combined query is enough for tracked additions/deletions and avoids
    // spawning two extra Git processes for every refresh.
    QMap<QString, QPair<int, int>> numStats;
    GitResult numRes = runGit({"diff", "HEAD", "--numstat"}, QString(), 4000);
    if (!numRes.success) {
        numRes = runGit({"diff", "--cached", "--numstat"}, QString(), 4000);
    }
    if (numRes.success) {
        for (const QString &line : numRes.stdOut.split('\n', Qt::SkipEmptyParts)) {
            QStringList p = line.split('\t');
            if (p.size() >= 3) {
                numStats[p[2]].first += p[0].toInt();
                numStats[p[2]].second += p[1].toInt();
            }
        }
    }

    QList<FileChange> files;
    const QStringList lines = statusRes.stdOut.split('\n', Qt::SkipEmptyParts);

    int idx = 0;
    locker.relock();
    for (const QString &line : lines) {
        if (line.length() < 4) continue;
        QChar x = line[0];
        QChar y = line[1];
        QString filePath = line.mid(3).trimmed();

        // Strip quotes if git escaped path
        if (filePath.startsWith('"') && filePath.endsWith('"')) {
            filePath = filePath.mid(1, filePath.length() - 2);
        }

        QString oldPath;
        if (filePath.contains(" -> ")) {
            QStringList parts = filePath.split(" -> ");
            oldPath = parts[0];
            filePath = parts[1];
        }

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
    if (m_repoPath.isEmpty() || filePath.isEmpty()) return false;

    GitResult check = runGit({"status", "--porcelain=v1", "--", filePath}, QString(), 3000);
    if (check.stdOut.startsWith("??") || check.stdOut.startsWith("A ") || check.stdOut.startsWith("A")) {
        runGit({"rm", "-f", "--", filePath}, QString(), 3000);
        QString fullPath = m_repoPath + "/" + filePath;
        QFileInfo fi(fullPath);
        if (fi.exists()) {
            if (fi.isDir()) {
                QDir(fullPath).removeRecursively();
            } else {
                QFile::remove(fullPath);
            }
        }
    } else {
        runGit({"restore", "--staged", "--worktree", "--", filePath}, QString(), 5000);
    }

    invalidateRepositoryCaches();
    emit changedFilesUpdated();
    emit operationSucceeded(QString("Discarded changes in '%1'").arg(filePath));
    return true;
}

bool GitCliService::discardAllChanges()
{
    if (m_repoPath.isEmpty()) return false;

    runGit({"restore", "--staged", "--worktree", "."}, QString(), 8000);
    runGit({"clean", "-fd"}, QString(), 8000);

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
    QRegularExpression hunkRegex(R"(^@@\s+-(\d+)(?:,(\d+))?\s+\+(\d+)(?:,(\d+))?\s+@@(.*)$)");

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

QList<DiffLine> GitCliService::getDiffForFile(const QString &filePath)
{
    QMutexLocker locker(&m_cacheMutex);
    if (m_fileDiffCacheValid && m_fileDiffCachePath == filePath) return m_fileDiffCache;
    if (m_repoPath.isEmpty() || filePath.isEmpty()) return {};

    locker.unlock();

    // If untracked file or folder, read safely from disk
    GitResult statusCheck = runGit({"status", "--porcelain=v1", "--", filePath}, QString(), 2000);
    if (statusCheck.stdOut.startsWith("??")) {
        QString fullPath = m_repoPath + "/" + filePath;
        QFileInfo fi(fullPath);
        if (fi.isDir()) {
            DiffLine hunk;
            hunk.oldLineNumber = -1;
            hunk.newLineNumber = -1;
            hunk.type = DiffLineType::HunkHeader;
            hunk.content = QString("@@ Directory: %1 @@").arg(filePath);
            DiffLine dl;
            dl.oldLineNumber = -1;
            dl.newLineNumber = 1;
            dl.type = DiffLineType::Context;
            dl.content = "Untracked Directory";
            QList<DiffLine> directoryDiff{hunk, dl};
            locker.relock();
            if (filePath == m_fileDiffCachePath) {
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
        if (filePath == m_fileDiffCachePath) {
            m_fileDiffCache = lines;
            m_fileDiffCacheValid = true;
        }
        return lines;
    }

    // Tracked diff against HEAD
    GitResult res = runGit({"diff", "HEAD", "--", filePath}, QString(), 4000);
    if (res.stdOut.isEmpty()) {
        res = runGit({"diff", "--cached", "--", filePath}, QString(), 4000);
        if (res.stdOut.isEmpty()) {
            res = runGit({"diff", "--", filePath}, QString(), 4000);
        }
    }
    QList<DiffLine> lines = parseDiffOutput(res.stdOut);
    locker.relock();
    if (filePath == m_fileDiffCachePath) {
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

QList<DiffLine> GitCliService::getDiffForCommitFile(const QString &commitSha, const QString &filePath)
{
    if (m_repoPath.isEmpty() || commitSha.isEmpty() || filePath.isEmpty()) return {};

    GitResult res = runGit({"show", commitSha, "--", filePath}, QString(), 4000);
    return parseDiffOutput(res.stdOut);
}

QList<DiffLine> GitCliService::getDiffForStashFile(const QString &stashId, const QString &filePath)
{
    if (m_repoPath.isEmpty()) return {};
    QString id = stashId.isEmpty() ? "stash@{0}" : stashId;

    QStringList args = {"stash", "show", "-p", id};
    if (!filePath.isEmpty()) {
        args.append("--");
        args.append(filePath);
    }
    GitResult res = runGit(args, QString(), 4000);
    return parseDiffOutput(res.stdOut);
}

QByteArray GitCliService::getFileBlob(const QString &filePath, const QString &ref)
{
    if (m_repoPath.isEmpty() || filePath.isEmpty()) return {};

    if (ref.isEmpty()) {
        // Read directly from disk (working copy)
        QString fullPath = m_repoPath + "/" + filePath;
        QFile file(fullPath);
        if (file.open(QIODevice::ReadOnly)) {
            return file.readAll();
        }
        return {};
    }

    // Capture binary output from git show <ref>:<path>
    QProcess process;
    process.setWorkingDirectory(m_repoPath);
    QProcessEnvironment env = QProcessEnvironment::systemEnvironment();
    env.insert("LC_ALL", "C");
    env.insert("GIT_TERMINAL_PROMPT", "0");
    process.setProcessEnvironment(env);

    process.start("git", {"show", QString("%1:%2").arg(ref, filePath)});
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
    const int safeLimit = qBound(1, limit, 100);
    if (m_commitHistoryCacheValid) return m_commitHistoryCache.mid(0, safeLimit);

    // Fast Git CLI query using porcelain record/unit separators
    locker.unlock();
    GitResult logRes = runGit({"log", QString("-%1").arg(safeLimit), "--format=%H%x1f%h%x1f%s%x1f%b%x1f%an%x1f%ae%x1f%at%x1e"}, QString(), 4000);
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

            QRegularExpression coRegex(R"(Co-authored-by:\s*(.*?)(?:<|$))", QRegularExpression::CaseInsensitiveOption);
            auto it = coRegex.globalMatch(item.description);
            while (it.hasNext()) {
                QString ca = it.next().captured(1).trimmed();
                if (!ca.isEmpty()) item.coAuthors.append(ca);
            }
            list.append(item);
        }
        m_commitHistoryCache = list;
        m_commitHistoryCacheValid = true;
        return m_commitHistoryCache;
    }

    m_commitHistoryCache = m_repositoryReader.commitHistory(safeLimit);
    m_commitHistoryCacheValid = true;
    return m_commitHistoryCache;
}

std::optional<CommitItem> GitCliService::getCommitDetails(const QString &sha)
{
    QMutexLocker locker(&m_cacheMutex);
    if (m_repoPath.isEmpty() || sha.isEmpty()) return std::nullopt;
    return m_repositoryReader.commitDetails(sha);
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
        } else {
            filesToUnstage.append(f.filePath);
        }
    }

    if (filesToAdd.isEmpty() && filesToRm.isEmpty()) {
        emit operationFailed("No files selected to commit");
        return false;
    }

    if (!filesToAdd.isEmpty()) {
        QStringList addArgs = {"add", "--"};
        addArgs.append(filesToAdd);
        runGit(addArgs, QString(), 10000);
    }
    if (!filesToRm.isEmpty()) {
        QStringList rmArgs = {"rm", "--ignore-unmatch", "--"};
        rmArgs.append(filesToRm);
        runGit(rmArgs, QString(), 10000);
    }
    if (!filesToUnstage.isEmpty()) {
        QStringList resetArgs = {"reset", "HEAD", "--"};
        resetArgs.append(filesToUnstage);
        runGit(resetArgs, QString(), 10000);
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
    if (m_repoPath.isEmpty() || sha.isEmpty()) return false;

    GitResult res = runGit({"revert", "--no-edit", sha}, QString(), 15000);
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
    QString target = remoteName.isEmpty() ? "origin" : remoteName;
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
        getRemoteStatus();
        emit branchListChanged();
        emit operationSucceeded(QString("Remote '%1' set to %2").arg(target, cleanUrl));
        return true;
    }

    emit operationFailed(res.stdErr.trimmed().isEmpty() ? "Failed to set remote URL" : res.stdErr.trimmed());
    return false;
}

bool GitCliService::removeRemote(const QString &remoteName)
{
    if (m_repoPath.isEmpty()) return false;
    QString target = remoteName.isEmpty() ? "origin" : remoteName.trimmed();

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

    QThread *thread = QThread::create([this, dir, repoName, desc, isPrivate]() {
        QStringList args = {"repo", "create", repoName, isPrivate ? "--private" : "--public", "--source=.", "--remote=origin", "--push"};
        if (!desc.isEmpty()) {
            args << "--description" << desc;
        }

        QProcess process;
        process.setWorkingDirectory(dir);
        QProcessEnvironment env = QProcessEnvironment::systemEnvironment();
        env.insert("LC_ALL", "C");
        env.insert("GIT_TERMINAL_PROMPT", "0");
        env.insert("GIT_SSH_COMMAND", "ssh -o BatchMode=yes -o StrictHostKeyChecking=accept-new");
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

        QMetaObject::invokeMethod(this, [this, finished, exitCode, stdOut, stdErr, repoName]() {
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
    QString remoteUrl = getRemoteUrl("origin");
    GitResult revRes = runGit({"rev-list", "--left-right", "--count", "HEAD...@{upstream}"}, QString(), 2000);
    locker.relock();
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
        GitResult unpushedRes = runGit({"rev-list", "--count", "HEAD"}, QString(), 2000);
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
    if (m_repoPath.isEmpty()) return;

    if (!hasRemote()) {
        emit operationFailed("No remote repository configured. Set a remote URL in Repository Settings to fetch.");
        return;
    }

    m_remoteStatus.isFetching = true;
    emit remoteStatusUpdated(m_remoteStatus);

    runGitAsync({"fetch", "--all", "--prune"}, [this](const GitResult &res) {
        m_remoteStatus.isFetching = false;
        if (res.success) {
            m_lastFetchTime = QDateTime::currentDateTime();
            m_repoFetchTimes[m_repoPath] = m_lastFetchTime;
            saveFetchTimes();
            m_remoteStatus.lastFetchedText = "Last fetched just now";
            invalidateRepositoryCaches();
            getRemoteStatus();
            emit branchListChanged();
            emit operationSucceeded("Fetched latest changes from origin");
        } else {
            m_remoteStatus.lastFetchedText = "Fetch failed";
            emit remoteStatusUpdated(m_remoteStatus);
            emit operationFailed(formatGitError(res.stdErr, "Fetch failed"));
        }
    });
}

void GitCliService::pullOrigin()
{
    if (m_repoPath.isEmpty()) return;

    if (!hasRemote()) {
        emit operationFailed("No remote repository configured. Cannot pull.");
        return;
    }

    m_remoteStatus.isPulling = true;
    emit remoteStatusUpdated(m_remoteStatus);

    runGitAsync({"pull"}, [this](const GitResult &res) {
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
            emit operationSucceeded("Successfully pulled from origin");
        } else {
            m_remoteStatus.lastFetchedText = "Pull failed";
            emit remoteStatusUpdated(m_remoteStatus);
            emit operationFailed(formatGitError(res.stdErr, "Pull failed"));
        }
    });
}

void GitCliService::pushOrigin()
{
    if (m_repoPath.isEmpty()) return;

    if (!hasRemote()) {
        emit operationFailed("No remote repository configured. Publish repository or add a remote origin before pushing.");
        return;
    }

    m_remoteStatus.isPushing = true;
    emit remoteStatusUpdated(m_remoteStatus);

    auto curBranch = getCurrentBranch();
    QString branchName = curBranch ? curBranch->name : "main";
    QString dir = m_repoPath;

    QThread *thread = QThread::create([this, dir, branchName]() {
        GitResult upstreamCheck = const_cast<GitCliService*>(this)->runGit({"rev-parse", "--verify", "@{upstream}"}, dir, 3000);
        QStringList pushArgs = {"push"};
        if (!upstreamCheck.success) {
            // Set upstream tracking on initial push
            pushArgs = {"push", "-u", "origin", branchName};
        }

        GitResult res = const_cast<GitCliService*>(this)->runGit(pushArgs, dir, 120000);

        QMetaObject::invokeMethod(this, [this, res]() {
            m_remoteStatus.isPushing = false;
            if (res.success) {
                clearUndoState();
                invalidateRepositoryCaches();
                getRemoteStatus();
                emit commitHistoryUpdated();
                emit operationSucceeded("Successfully pushed commits to origin");
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

    m_stashesCache = m_repositoryReader.stashes();
    m_stashesCacheValid = true;
    return m_stashesCache;
}

std::optional<StashItem> GitCliService::getStashDetails(const QString &stashId)
{
    QMutexLocker locker(&m_cacheMutex);
    if (m_repoPath.isEmpty()) return std::nullopt;
    return m_repositoryReader.stashDetails(stashId);
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
    if (m_repoPath.isEmpty()) return false;

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
    if (m_repoPath.isEmpty()) return false;

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

QString GitCliService::formatGitError(const QString &rawError, const QString &fallbackContext) const
{
    QString trimmed = rawError.trimmed();
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
