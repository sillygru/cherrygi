#include "GitRepositoryReader.h"

#include <QDir>
#include <QDirIterator>
#include <QFileInfo>
#include <QRegularExpression>
#include <QTextStream>
#include <QDateTime>
#include <QTimeZone>
#include <QSet>
#include <algorithm>
#include <cstring>
#include <zlib.h>

namespace Cherry {

namespace {
constexpr int ObjectIdBytes = 20;
constexpr int ObjectIdHexLength = 40;

QString normaliseSha(const QString &sha)
{
    return sha.trimmed().toLower();
}

QString objectTypeForPackCode(int code)
{
    switch (code) {
    case 1: return "commit";
    case 2: return "tree";
    case 3: return "blob";
    case 4: return "tag";
    default: return QString();
    }
}
}

bool GitRepositoryReader::open(const QString &path)
{
    m_workTree.clear();
    m_gitDir.clear();
    m_headGitDir.clear();
    refresh();

    const QString cleanPath = QDir::cleanPath(path.trimmed());
    if (cleanPath.isEmpty()) return false;

    const QString gitDir = resolveGitDir(cleanPath);
    if (gitDir.isEmpty()) return false;

    m_headGitDir = gitDir;
    m_gitDir = gitDir;
    if (QFileInfo(cleanPath + "/.git").exists()) {
        m_workTree = cleanPath;
    }

    // Linked worktrees keep HEAD and their index in a private gitdir while
    // refs and objects live in the shared common directory.
    QFile commonDirFile(m_headGitDir + "/commondir");
    if (commonDirFile.open(QIODevice::ReadOnly | QIODevice::Text)) {
        QString commonDir = QString::fromUtf8(commonDirFile.readAll()).trimmed();
        if (QDir::isRelativePath(commonDir)) commonDir = QDir(m_headGitDir).filePath(commonDir);
        if (QDir(commonDir).exists()) m_gitDir = QDir::cleanPath(commonDir);
    }

    QFile gitFile(m_headGitDir + "/HEAD");
    if (!gitFile.exists()) {
        m_gitDir.clear();
        m_headGitDir.clear();
        return false;
    }

    // A bare repository has no work tree, but is still a valid Git database.
    return !readHeadRef().isEmpty() || !readRef("HEAD").isEmpty();
}

void GitRepositoryReader::refresh()
{
    m_packedRefsLoaded = false;
    m_packIndexesLoaded = false;
    m_packedRefs.clear();
    m_packLocations.clear();
    m_objectCache.clear();
}

QString GitRepositoryReader::resolveGitDir(const QString &path) const
{
    QFileInfo info(path);
    QString current = info.isDir() ? info.absoluteFilePath() : info.absolutePath();

    // Git worktrees and submodules use a .git file containing "gitdir: ...".
    for (int depth = 0; depth < 128 && !current.isEmpty(); ++depth) {
        const QString dotGit = current + "/.git";
        QFileInfo dotGitInfo(dotGit);
        if (dotGitInfo.isDir()) return dotGitInfo.absoluteFilePath();
        if (dotGitInfo.isFile()) {
            QFile file(dotGit);
            if (file.open(QIODevice::ReadOnly | QIODevice::Text)) {
                const QString line = QString::fromUtf8(file.readLine()).trimmed();
                if (line.startsWith("gitdir:", Qt::CaseInsensitive)) {
                    QString target = line.mid(7).trimmed();
                    if (QDir::isRelativePath(target)) target = QDir(current).filePath(target);
                    return QDir::cleanPath(QFileInfo(target).absoluteFilePath());
                }
            }
        }

        // Also accept a direct .git directory path and bare repositories.
        if (QFileInfo(current + "/HEAD").exists() && QFileInfo(current + "/objects").isDir()) {
            return QDir::cleanPath(current);
        }

        const QString parent = QFileInfo(current).dir().absolutePath();
        if (parent == current) break;
        current = parent;
    }
    return QString();
}

QString GitRepositoryReader::readHeadRef() const
{
    if (m_gitDir.isEmpty()) return QString();
    QFile head(m_headGitDir + "/HEAD");
    if (!head.open(QIODevice::ReadOnly | QIODevice::Text)) return QString();
    const QString value = QString::fromUtf8(head.readAll()).trimmed();
    if (value.startsWith("ref: ")) return value.mid(5).trimmed();
    return QString();
}

QString GitRepositoryReader::readRef(const QString &refName) const
{
    if (m_gitDir.isEmpty() || refName.isEmpty()) return QString();

    const QString refDir = refName == "HEAD" ? m_headGitDir : m_gitDir;
    QFile loose(refDir + "/" + refName);
    if (loose.open(QIODevice::ReadOnly | QIODevice::Text)) {
        const QString value = QString::fromUtf8(loose.readAll()).trimmed();
        if (value.startsWith("ref: ")) return readRef(value.mid(5).trimmed());
        return normaliseSha(value);
    }

    loadPackedRefs();
    const QString packed = m_packedRefs.value(refName);
    if (packed.startsWith("ref: ")) return readRef(packed.mid(5).trimmed());
    return normaliseSha(packed);
}

void GitRepositoryReader::loadPackedRefs() const
{
    if (m_packedRefsLoaded || m_gitDir.isEmpty()) return;
    m_packedRefsLoaded = true;

    QFile file(m_gitDir + "/packed-refs");
    if (!file.open(QIODevice::ReadOnly | QIODevice::Text)) return;

    QString previousRef;
    const QStringList lines = QString::fromUtf8(file.readAll()).split('\n');
    for (const QString &line : lines) {
        const QString value = line.trimmed();
        if (value.isEmpty() || value.startsWith('#')) continue;
        if (value.startsWith('^')) continue; // peeled tag line
        const QStringList parts = value.split(QRegularExpression("\\s+"));
        if (parts.size() >= 2 && parts[0].size() >= ObjectIdHexLength) {
            previousRef = parts[1];
            m_packedRefs.insert(previousRef, parts[0].left(ObjectIdHexLength));
        }
    }
}

void GitRepositoryReader::loadPackIndexes() const
{
    if (m_packIndexesLoaded || m_gitDir.isEmpty()) return;
    m_packIndexesLoaded = true;

    QDir packDir(m_gitDir + "/objects/pack");
    const QStringList indexes = packDir.entryList({"*.idx"}, QDir::Files);
    for (const QString &indexName : indexes) {
        QFile indexFile(packDir.filePath(indexName));
        if (!indexFile.open(QIODevice::ReadOnly)) continue;
        const QByteArray index = indexFile.readAll();
        if (index.size() < 8 || index.left(4) != QByteArray("\xfftOc", 4)) continue;
        if (readBigEndian32(index, 4) != 2) continue;

        const quint32 objectCount = readBigEndian32(index, 8 + 255 * 4);
        const int namesOffset = 8 + 256 * 4;
        const int namesEnd = namesOffset + static_cast<int>(objectCount) * ObjectIdBytes;
        const int crcEnd = namesEnd + static_cast<int>(objectCount) * 4;
        const int offsetsEnd = crcEnd + static_cast<int>(objectCount) * 4;
        if (namesOffset < 0 || offsetsEnd > index.size()) continue;

        const QString packPath = packDir.filePath(indexName.left(indexName.size() - 4) + ".pack");
        for (quint32 i = 0; i < objectCount; ++i) {
            const QString sha = objectId(index.mid(namesOffset + static_cast<int>(i) * ObjectIdBytes, ObjectIdBytes));
            const quint32 rawOffset = readBigEndian32(index, crcEnd + static_cast<int>(i) * 4);
            quint64 offset = rawOffset;
            if (rawOffset & 0x80000000U) {
                const quint32 largeIndex = rawOffset & 0x7fffffffU;
                const int largeOffset = offsetsEnd + static_cast<int>(largeIndex) * 8;
                if (largeOffset + 8 > index.size()) continue;
                offset = readBigEndian64(index, largeOffset);
            }
            m_packLocations.insert(sha, {packPath, offset});
        }
    }
}

QString GitRepositoryReader::resolveObjectId(const QString &sha) const
{
    const QString candidate = normaliseSha(sha);
    if (candidate.size() == ObjectIdHexLength) return candidate;
    if (candidate.size() < 4) return QString();

    loadPackIndexes();
    for (auto it = m_packLocations.cbegin(); it != m_packLocations.cend(); ++it) {
        if (it.key().startsWith(candidate)) return it.key();
    }

    if (candidate.size() >= 2) {
        const QDir objectDir(m_gitDir + "/objects/" + candidate.left(2));
        const QStringList files = objectDir.entryList(QDir::Files);
        for (const QString &file : files) {
            const QString full = candidate.left(2) + file;
            if (full.startsWith(candidate)) return full;
        }
    }
    return QString();
}

std::optional<GitRepositoryReader::GitObject> GitRepositoryReader::readLooseObject(const QString &sha) const
{
    if (sha.size() != ObjectIdHexLength) return std::nullopt;
    QFile file(m_gitDir + "/objects/" + sha.left(2) + "/" + sha.mid(2));
    if (!file.open(QIODevice::ReadOnly)) return std::nullopt;
    const QByteArray inflated = inflateBytes(file.readAll());
    const int separator = inflated.indexOf('\0');
    if (separator <= 0) return std::nullopt;

    GitObject object;
    object.type = QString::fromLatin1(inflated.left(separator)).section(' ', 0, 0);
    object.data = inflated.mid(separator + 1);
    return object;
}

std::optional<GitRepositoryReader::GitObject> GitRepositoryReader::readObject(const QString &sha, int depth) const
{
    if (depth > 64) return std::nullopt;
    const QString resolved = resolveObjectId(sha);
    if (resolved.isEmpty()) return std::nullopt;
    if (m_objectCache.contains(resolved)) return m_objectCache.value(resolved);

    std::optional<GitObject> object = readLooseObject(resolved);
    if (!object) object = readPackedObject(resolved, depth);
    if (object) m_objectCache.insert(resolved, *object);
    return object;
}

std::optional<GitRepositoryReader::GitObject> GitRepositoryReader::readPackedObject(const QString &sha, int depth) const
{
    loadPackIndexes();
    const auto location = m_packLocations.constFind(sha);
    if (location == m_packLocations.cend()) return std::nullopt;
    return readPackEntry(location.value(), depth);
}

std::optional<GitRepositoryReader::GitObject> GitRepositoryReader::readPackEntry(const PackLocation &location, int depth) const
{
    if (depth > 64) return std::nullopt;
    QFile file(location.packPath);
    if (!file.open(QIODevice::ReadOnly) || !file.seek(static_cast<qint64>(location.offset))) return std::nullopt;

    const QByteArray firstByte = file.read(1);
    if (firstByte.size() != 1) return std::nullopt;

    int byte = static_cast<unsigned char>(firstByte[0]);
    const int typeCode = (byte >> 4) & 7;
    quint64 size = byte & 0x0f;
    int shift = 4;
    while (byte & 0x80) {
        const QByteArray next = file.read(1);
        if (next.size() != 1 || shift > 63) return std::nullopt;
        byte = static_cast<unsigned char>(next[0]);
        size |= static_cast<quint64>(byte & 0x7f) << shift;
        shift += 7;
    }

    QString type = objectTypeForPackCode(typeCode);
    quint64 baseOffset = 0;
    QString baseSha;
    if (typeCode == 6) {
        const QByteArray firstOffset = file.read(1);
        if (firstOffset.size() != 1) return std::nullopt;
        int c = static_cast<unsigned char>(firstOffset[0]);
        baseOffset = c & 0x7f;
        while (c & 0x80) {
            const QByteArray next = file.read(1);
            if (next.size() != 1) return std::nullopt;
            c = static_cast<unsigned char>(next[0]);
            baseOffset = ((baseOffset + 1) << 7) | (c & 0x7f);
        }
        if (baseOffset > location.offset) return std::nullopt;
        baseOffset = location.offset - baseOffset;
    } else if (typeCode == 7) {
        const QByteArray rawBase = file.read(ObjectIdBytes);
        if (rawBase.size() != ObjectIdBytes) return std::nullopt;
        baseSha = objectId(rawBase);
    } else if (type.isEmpty()) {
        return std::nullopt;
    }

    const QByteArray inflated = inflateFile(file);
    if (inflated.isEmpty() && size != 0) return std::nullopt;
    if (typeCode == 6 || typeCode == 7) {
        std::optional<GitObject> base;
        if (typeCode == 6) {
            loadPackIndexes();
            QString baseObjectSha;
            for (auto it = m_packLocations.cbegin(); it != m_packLocations.cend(); ++it) {
                if (it.value().packPath == location.packPath && it.value().offset == baseOffset) {
                    baseObjectSha = it.key();
                    break;
                }
            }
            if (!baseObjectSha.isEmpty()) base = readObject(baseObjectSha, depth + 1);
            if (!base && baseOffset != 0) base = readPackEntry({location.packPath, baseOffset}, depth + 1);
        } else {
            base = readObject(baseSha, depth + 1);
        }
        if (!base) return std::nullopt;
        GitObject object;
        object.type = base->type;
        object.data = applyDelta(base->data, inflated);
        return object;
    }

    GitObject object;
    object.type = type;
    object.data = inflated;
    return object;
}

std::optional<BranchInfo> GitRepositoryReader::currentBranch() const
{
    const QString head = readHeadRef();
    const QString headValue = readRef("HEAD");
    if (head.isEmpty() && headValue.isEmpty()) return std::nullopt;

    BranchInfo branch;
    if (!head.isEmpty() && head.startsWith("refs/heads/")) {
        branch.name = head.mid(QStringLiteral("refs/heads/").size());
    } else {
        branch.name = "(detached)";
    }
    branch.isCurrent = true;
    branch.isDefault = branch.name == "main" || branch.name == "master";
    branch.tipCommitSha = headValue.left(7);
    return branch;
}

QList<BranchInfo> GitRepositoryReader::branches() const
{
    QList<BranchInfo> result;
    const auto current = currentBranch();
    QSet<QString> seen;

    auto addRef = [&](const QString &refName, bool remote) {
        const QString sha = readRef(refName);
        if (sha.isEmpty()) return;
        QString name;
        if (remote) {
            name = refName.mid(QStringLiteral("refs/remotes/").size());
        } else {
            name = refName.mid(QStringLiteral("refs/heads/").size());
        }
        if (name.isEmpty() || seen.contains(name)) return;
        seen.insert(name);

        BranchInfo branch;
        branch.name = name;
        branch.isRemote = remote;
        branch.isCurrent = current && !remote && current->name == name;
        branch.isDefault = name == "main" || name == "master" || name == "origin/main" || name == "origin/master";
        branch.tipCommitSha = sha.left(7);
        const auto match = QRegularExpression("#?(\\d+)").match(name);
        if (match.hasMatch()) {
            branch.prNumber = "#" + match.captured(1);
            branch.prMergedOrActive = true;
        }
        result.append(branch);
    };

    for (const QString &root : {QStringLiteral("heads"), QStringLiteral("remotes")}) {
        const QDir dir(m_gitDir + "/refs/" + root);
        if (!dir.exists()) continue;
        QDirIterator it(dir.absolutePath(), QDir::Files, QDirIterator::Subdirectories);
        while (it.hasNext()) {
            const QString path = it.next();
            const QString relative = dir.relativeFilePath(path).replace(QDir::separator(), '/');
            addRef("refs/" + root + "/" + relative, root == "remotes");
        }
    }

    loadPackedRefs();
    for (auto it = m_packedRefs.cbegin(); it != m_packedRefs.cend(); ++it) {
        if (it.key().startsWith("refs/heads/") || it.key().startsWith("refs/remotes/")) {
            addRef(it.key(), it.key().startsWith("refs/remotes/"));
        }
    }
    return result;
}

QList<CommitItem> GitRepositoryReader::commitHistory(int limit) const
{
    QList<CommitItem> result;
    QString sha = readRef("HEAD");
    QSet<QString> visited;
    const int safeLimit = qBound(1, limit, 1000);

    QStringList pending{sha};
    while (!pending.isEmpty() && result.size() < safeLimit) {
        sha = pending.takeFirst();
        if (sha.isEmpty() || visited.contains(sha)) continue;
        visited.insert(sha);
        const auto commit = parseCommit(sha, false);
        if (!commit) continue;
        result.append(*commit);

        const auto object = readObject(sha);
        if (!object || object->type != "commit") continue;
        const QStringList lines = QString::fromUtf8(object->data).section("\n\n", 0, 0).split('\n');
        for (const QString &line : lines) {
            if (line.startsWith("parent ")) pending.append(line.mid(7).trimmed());
        }
    }
    return result;
}

std::optional<CommitItem> GitRepositoryReader::commitDetails(const QString &sha) const
{
    return parseCommit(sha, true);
}

std::optional<CommitItem> GitRepositoryReader::parseCommit(const QString &sha, bool includeFiles) const
{
    const QString resolved = resolveObjectId(sha);
    const auto object = readObject(resolved);
    if (!object || object->type != "commit") return std::nullopt;

    const QByteArray raw = object->data;
    const int separator = raw.indexOf("\n\n");
    if (separator < 0) return std::nullopt;
    const QStringList headers = QString::fromUtf8(raw.left(separator)).split('\n');

    CommitItem commit;
    commit.sha = resolved;
    commit.shortSha = resolved.left(7);
    QString treeSha;
    QString firstParent;
    for (const QString &line : headers) {
        if (line.startsWith("tree ")) treeSha = line.mid(5).trimmed();
        else if (line.startsWith("parent ") && firstParent.isEmpty()) firstParent = line.mid(7).trimmed();
        else if (line.startsWith("author ")) {
            const auto match = QRegularExpression("^(.*) <([^>]+)> (\\d+) ([+-]\\d{4})$").match(line.mid(7));
            if (match.hasMatch()) {
                commit.authorName = match.captured(1);
                commit.authorEmail = match.captured(2);
                commit.timestamp = QDateTime::fromSecsSinceEpoch(match.captured(3).toLongLong(), QTimeZone::UTC);
            }
        }
    }

    const QString message = QString::fromUtf8(raw.mid(separator + 2));
    const QStringList messageLines = message.split('\n');
    commit.summary = messageLines.value(0).trimmed();
    commit.description = messageLines.mid(1).join('\n').trimmed();
    commit.relativeTime = relativeTime(commit.timestamp);

    const auto coAuthorRegex = QRegularExpression("Co-authored-by:\\s*(.*?)(?:<|$)", QRegularExpression::CaseInsensitiveOption);
    auto coAuthors = coAuthorRegex.globalMatch(commit.description);
    while (coAuthors.hasNext()) {
        const QString value = coAuthors.next().captured(1).trimmed();
        if (!value.isEmpty()) commit.coAuthors.append(value);
    }

    if (includeFiles && !treeSha.isEmpty()) populateChangedFiles(commit, firstParent);
    return commit;
}

QHash<QString, QString> GitRepositoryReader::readTree(const QString &treeSha, const QString &prefix, int depth) const
{
    QHash<QString, QString> result;
    if (depth > 128) return result;
    const auto tree = readObject(treeSha);
    if (!tree || tree->type != "tree") return result;

    int cursor = 0;
    while (cursor < tree->data.size()) {
        const int modeEnd = tree->data.indexOf(' ', cursor);
        if (modeEnd < 0) break;
        const int nameEnd = tree->data.indexOf('\0', modeEnd + 1);
        if (nameEnd < 0 || nameEnd + 1 + ObjectIdBytes > tree->data.size()) break;
        const QString mode = QString::fromLatin1(tree->data.mid(cursor, modeEnd - cursor));
        const QString name = QString::fromUtf8(tree->data.mid(modeEnd + 1, nameEnd - modeEnd - 1));
        const QString path = prefix + name;
        const QString childSha = objectId(tree->data.mid(nameEnd + 1, ObjectIdBytes));
        if (mode.startsWith("04") || mode == "40000") {
            const auto children = readTree(childSha, path + "/", depth + 1);
            for (auto child = children.cbegin(); child != children.cend(); ++child) {
                result.insert(child.key(), child.value());
            }
        } else {
            result.insert(path, childSha);
        }
        cursor = nameEnd + 1 + ObjectIdBytes;
    }
    return result;
}

void GitRepositoryReader::populateChangedFiles(CommitItem &commit, const QString &parentSha) const
{
    const auto object = readObject(commit.sha);
    if (!object) return;
    QString treeSha;
    QString parentTree;
    const int separator = object->data.indexOf("\n\n");
    const QStringList headers = QString::fromUtf8(object->data.left(separator < 0 ? object->data.size() : separator)).split('\n');
    for (const QString &line : headers) {
        if (line.startsWith("tree ")) treeSha = line.mid(5).trimmed();
    }
    if (!parentSha.isEmpty()) {
        const auto parent = readObject(parentSha);
        if (parent && parent->type == "commit") {
            const QByteArray parentHeaders = parent->data.left(parent->data.indexOf("\n\n"));
            for (const QString &line : QString::fromUtf8(parentHeaders).split('\n')) {
                if (line.startsWith("tree ")) parentTree = line.mid(5).trimmed();
            }
        }
    }
    commit.changedFiles = changedFilesBetweenTrees(parentTree, treeSha);
}

QList<FileChange> GitRepositoryReader::changedFilesBetweenTrees(const QString &oldTree, const QString &newTree) const
{
    const auto oldFiles = oldTree.isEmpty() ? QHash<QString, QString>() : readTree(oldTree, QString());
    const auto newFiles = newTree.isEmpty() ? QHash<QString, QString>() : readTree(newTree, QString());
    QSet<QString> paths;
    for (auto it = oldFiles.cbegin(); it != oldFiles.cend(); ++it) paths.insert(it.key());
    for (auto it = newFiles.cbegin(); it != newFiles.cend(); ++it) paths.insert(it.key());

    QStringList sortedPaths = paths.values();
    std::sort(sortedPaths.begin(), sortedPaths.end());
    QList<FileChange> result;
    int index = 0;
    for (const QString &path : sortedPaths) {
        const bool hadOld = oldFiles.contains(path);
        const bool hasNew = newFiles.contains(path);
        if (hadOld && hasNew && oldFiles.value(path) == newFiles.value(path)) continue;

        FileChange file;
        file.id = QString("fc-%1").arg(index++);
        file.filePath = path;
        if (!hadOld) file.status = FileChangeType::Added;
        else if (!hasNew) file.status = FileChangeType::Deleted;
        else file.status = FileChangeType::Modified;
        result.append(file);
    }
    return result;
}

QList<StashItem> GitRepositoryReader::readStashReflog() const
{
    QList<StashItem> result;
    QFile reflog(m_gitDir + "/logs/refs/stash");
    QStringList lines;
    if (reflog.open(QIODevice::ReadOnly | QIODevice::Text)) {
        lines = QString::fromUtf8(reflog.readAll()).split('\n', Qt::SkipEmptyParts);
    }

    int index = 0;
    for (auto it = lines.crbegin(); it != lines.crend(); ++it) {
        const QStringList columns = it->split('\t', Qt::KeepEmptyParts);
        if (columns.isEmpty()) continue;
        const QStringList ids = columns.first().split(QRegularExpression("\\s+"), Qt::SkipEmptyParts);
        if (ids.size() < 2) continue;
        const QString sha = resolveObjectId(ids[1]);
        const auto commit = parseCommit(sha, false);
        if (!commit) continue;

        StashItem stash;
        stash.id = QString("stash@{%1}").arg(index++);
        stash.message = cleanStashMessage(columns.value(1));
        stash.branchName = "HEAD";
        const auto branchMatch = QRegularExpression("(?:On|WIP on) ([^:]+):").match(columns.value(1));
        if (branchMatch.hasMatch()) stash.branchName = branchMatch.captured(1).trimmed();
        stash.timestamp = commit->timestamp;
        result.append(stash);
    }

    // Very old/specially-created repositories may have the ref but no reflog.
    if (result.isEmpty()) {
        const QString sha = readRef("refs/stash");
        if (!sha.isEmpty()) {
            const auto commit = parseCommit(sha, false);
            if (commit) {
                StashItem stash;
                stash.id = "stash@{0}";
                stash.message = "WIP";
                stash.branchName = "HEAD";
                stash.timestamp = commit->timestamp;
                result.append(stash);
            }
        }
    }
    return result;
}

QList<StashItem> GitRepositoryReader::stashes() const
{
    return readStashReflog();
}

std::optional<StashItem> GitRepositoryReader::stashDetails(const QString &stashId) const
{
    const QString requested = stashId.isEmpty() ? "stash@{0}" : stashId;
    const auto all = readStashReflog();
    for (StashItem stash : all) {
        if (stash.id != requested) continue;
        const QStringList match = requested.mid(QStringLiteral("stash@{").size()).split('}');
        bool ok = false;
        const int index = match.value(0).toInt(&ok);
        if (!ok) return stash;

        QFile reflog(m_gitDir + "/logs/refs/stash");
        if (!reflog.open(QIODevice::ReadOnly | QIODevice::Text)) return stash;
        const QStringList lines = QString::fromUtf8(reflog.readAll()).split('\n', Qt::SkipEmptyParts);
        if (index < 0 || index >= lines.size()) return stash;
        const QStringList ids = lines.at(lines.size() - 1 - index).split(QRegularExpression("\\s+|\\t"), Qt::SkipEmptyParts);
        if (ids.size() < 2) return stash;
        const auto commit = parseCommit(ids.at(1), true);
        if (commit) stash.files = commit->changedFiles;
        return stash;
    }
    return std::nullopt;
}

quint32 GitRepositoryReader::readBigEndian32(const QByteArray &data, int offset)
{
    if (offset < 0 || offset + 4 > data.size()) return 0;
    return (static_cast<quint32>(static_cast<unsigned char>(data[offset])) << 24) |
           (static_cast<quint32>(static_cast<unsigned char>(data[offset + 1])) << 16) |
           (static_cast<quint32>(static_cast<unsigned char>(data[offset + 2])) << 8) |
           static_cast<quint32>(static_cast<unsigned char>(data[offset + 3]));
}

quint64 GitRepositoryReader::readBigEndian64(const QByteArray &data, int offset)
{
    quint64 value = 0;
    for (int i = 0; i < 8; ++i) value = (value << 8) | static_cast<unsigned char>(data.at(offset + i));
    return value;
}

QString GitRepositoryReader::objectId(const QByteArray &bytes)
{
    return QString::fromLatin1(bytes.toHex()).toLower();
}

QByteArray GitRepositoryReader::inflateBytes(const QByteArray &compressed, int *consumed)
{
    if (consumed) *consumed = 0;
    if (compressed.isEmpty()) return {};
    z_stream stream{};
    if (inflateInit(&stream) != Z_OK) return {};
    stream.next_in = reinterpret_cast<Bytef *>(const_cast<char *>(compressed.constData()));
    stream.avail_in = static_cast<uInt>(compressed.size());

    QByteArray output;
    char buffer[8192];
    int status = Z_OK;
    while (status == Z_OK) {
        stream.next_out = reinterpret_cast<Bytef *>(buffer);
        stream.avail_out = sizeof(buffer);
        status = inflate(&stream, Z_NO_FLUSH);
        const int produced = static_cast<int>(sizeof(buffer) - stream.avail_out);
        if (produced > 0) output.append(buffer, produced);
    }
    if (consumed) *consumed = static_cast<int>(stream.total_in);
    inflateEnd(&stream);
    return status == Z_STREAM_END ? output : QByteArray();
}

QByteArray GitRepositoryReader::inflateFile(QFile &file)
{
    z_stream stream{};
    if (inflateInit(&stream) != Z_OK) return {};
    QByteArray output;
    char input[8192];
    char buffer[8192];
    int status = Z_OK;
    while (status == Z_OK) {
        const qint64 read = file.read(input, sizeof(input));
        if (read <= 0) break;
        stream.next_in = reinterpret_cast<Bytef *>(input);
        stream.avail_in = static_cast<uInt>(read);
        while (stream.avail_in > 0 && status == Z_OK) {
            stream.next_out = reinterpret_cast<Bytef *>(buffer);
            stream.avail_out = sizeof(buffer);
            status = inflate(&stream, Z_NO_FLUSH);
            const int produced = static_cast<int>(sizeof(buffer) - stream.avail_out);
            if (produced > 0) output.append(buffer, produced);
        }
        if (status == Z_STREAM_END) {
            file.seek(file.pos() - stream.avail_in);
            break;
        }
    }
    inflateEnd(&stream);
    return status == Z_STREAM_END ? output : QByteArray();
}

QByteArray GitRepositoryReader::applyDelta(const QByteArray &base, const QByteArray &delta)
{
    int cursor = 0;
    auto readVarInt = [&]() -> quint64 {
        quint64 value = 0;
        int shift = 0;
        while (cursor < delta.size()) {
            const unsigned char byte = static_cast<unsigned char>(delta[cursor++]);
            value |= static_cast<quint64>(byte & 0x7f) << shift;
            if (!(byte & 0x80)) break;
            shift += 7;
            if (shift > 63) break;
        }
        return value;
    };

    const quint64 baseSize = readVarInt();
    const quint64 resultSize = readVarInt();
    if (baseSize != static_cast<quint64>(base.size()) || resultSize > 256 * 1024 * 1024ULL) return {};

    QByteArray result;
    result.reserve(static_cast<int>(resultSize));
    while (cursor < delta.size() && static_cast<quint64>(result.size()) < resultSize) {
        const unsigned char opcode = static_cast<unsigned char>(delta[cursor++]);
        if (opcode & 0x80) {
            quint64 offset = 0;
            quint64 size = 0;
            if (opcode & 0x01) offset |= static_cast<unsigned char>(delta.at(cursor++));
            if (opcode & 0x02) offset |= static_cast<quint64>(static_cast<unsigned char>(delta.at(cursor++))) << 8;
            if (opcode & 0x04) offset |= static_cast<quint64>(static_cast<unsigned char>(delta.at(cursor++))) << 16;
            if (opcode & 0x08) offset |= static_cast<quint64>(static_cast<unsigned char>(delta.at(cursor++))) << 24;
            if (opcode & 0x10) size |= static_cast<unsigned char>(delta.at(cursor++));
            if (opcode & 0x20) size |= static_cast<quint64>(static_cast<unsigned char>(delta.at(cursor++))) << 8;
            if (opcode & 0x40) size |= static_cast<quint64>(static_cast<unsigned char>(delta.at(cursor++))) << 16;
            if (size == 0) size = 0x10000;
            if (offset + size > static_cast<quint64>(base.size())) return {};
            result.append(base.mid(static_cast<int>(offset), static_cast<int>(size)));
        } else if (opcode != 0) {
            if (cursor + opcode > delta.size()) return {};
            result.append(delta.constData() + cursor, opcode);
            cursor += opcode;
        } else {
            return {};
        }
    }
    return static_cast<quint64>(result.size()) == resultSize ? result : QByteArray();
}

QString GitRepositoryReader::relativeTime(const QDateTime &timestamp)
{
    if (!timestamp.isValid()) return {};
    const qint64 seconds = timestamp.secsTo(QDateTime::currentDateTime());
    if (seconds < 60) return "just now";
    if (seconds < 3600) return QString("%1 minutes ago").arg(seconds / 60);
    if (seconds < 86400) return QString("%1 hours ago").arg(seconds / 3600);
    if (seconds < 172800) return "yesterday";
    return QString("%1 days ago").arg(seconds / 86400);
}

QString GitRepositoryReader::cleanStashMessage(const QString &message)
{
    QString result = message.trimmed();
    result.remove(QRegularExpression("^(On|WIP on) [^:]+:\\s*", QRegularExpression::CaseInsensitiveOption));
    return result.isEmpty() ? QStringLiteral("WIP") : result;
}

} // namespace Cherry
