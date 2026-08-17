#include "GitRepositoryReader.h"
#include "AvatarResolver.h"

#include <QDir>
#include <QDirIterator>
#include <QFileInfo>
#include <QRegularExpression>
#include <QTextStream>
#include <QDateTime>
#include <QTimeZone>
#include <QCryptographicHash>
#include <QSet>
#include <algorithm>
#include <cstring>
#include <limits>
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
    QMutexLocker locker(&m_mutex);
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
    m_directReadSupported = true;

    // Opening a subdirectory of a worktree is valid Git usage. Resolve the
    // actual worktree root instead of mistaking that case for a bare repo.
    QString candidate = cleanPath;
    for (int depth = 0; depth < 128 && !candidate.isEmpty(); ++depth) {
        if (QFileInfo(candidate + "/.git").exists()) {
            m_workTree = candidate;
            break;
        }
        const QString parent = QFileInfo(candidate).dir().absolutePath();
        if (parent == candidate) break;
        candidate = parent;
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
    const bool valid = !readHeadRef().isEmpty() || !readRef("HEAD").isEmpty();
    if (valid) m_directReadSupported = detectDirectReadSupport();
    return valid;
}

bool GitRepositoryReader::detectDirectReadSupport() const
{
    if (m_gitDir.isEmpty()) return false;

    QFile config(m_gitDir + "/config");
    if (config.open(QIODevice::ReadOnly | QIODevice::Text)) {
        QString section;
        const QStringList lines = QString::fromUtf8(config.readAll()).split('\n');
        for (const QString &line : lines) {
            const QString trimmed = line.trimmed().toLower();
            if (trimmed.startsWith('[') && trimmed.endsWith(']')) {
                section = trimmed;
                continue;
            }
            QString value = trimmed;
            value.remove(' ').remove('\t');
            if (section == "[extensions]" &&
                (value == "objectformat=sha256" || value == "refstorage=reftable" ||
                 value.startsWith("partialclone="))) {
                return false;
            }
            if (value == "promisor=true") return false;
        }
    }

    // These repositories can legitimately refer to objects outside the main
    // object database or require Git-specific history semantics.
    if (QFile::exists(m_gitDir + "/objects/info/alternates") ||
        QFile::exists(m_gitDir + "/shallow") ||
        QFile::exists(m_gitDir + "/info/grafts") ||
        QDir(m_gitDir + "/refs/replace").exists()) {
        return false;
    }

    // The direct reader currently understands only pack index v2.
    const QDir packDir(m_gitDir + "/objects/pack");
    for (const QString &indexName : packDir.entryList({"*.idx"}, QDir::Files)) {
        QFile indexFile(packDir.filePath(indexName));
        if (!indexFile.open(QIODevice::ReadOnly)) return false;
        const QByteArray prefix = indexFile.read(8);
        if (prefix.size() < 8 || prefix.left(4) != QByteArray("\xfftOc", 4) ||
            readBigEndian32(prefix, 4) != 2) {
            return false;
        }
    }
    return true;
}

void GitRepositoryReader::refresh()
{
    QMutexLocker locker(&m_mutex);
    m_packedRefsLoaded = false;
    m_packIndexesLoaded = false;
    m_tagsLoaded = false;
    m_packedRefs.clear();
    m_packIndexes.clear();
    m_commitTags.clear();
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
    const QString content = QString::fromUtf8(file.readAll());
    const QStringList lines = content.split('\n');
    for (const QString &line : lines) {
        const QString value = line.trimmed();
        if (value.isEmpty() || value.startsWith('#')) continue;
        if (value.startsWith('^')) continue; // peeled tag line
        int spaceIdx = value.indexOf(' ');
        if (spaceIdx < 0) spaceIdx = value.indexOf('\t');
        if (spaceIdx >= ObjectIdHexLength) {
            QString sha = value.left(ObjectIdHexLength);
            QString ref = value.mid(spaceIdx + 1).trimmed();
            if (!ref.isEmpty()) {
                m_packedRefs.insert(ref, sha);
            }
        }
    }
}

void GitRepositoryReader::loadTags() const
{
    if (m_tagsLoaded || m_gitDir.isEmpty()) return;
    m_tagsLoaded = true;
    m_commitTags.clear();

    auto peelTag = [this](const QString &sha) -> QString {
        auto obj = readObject(sha);
        if (obj && obj->type == "tag") {
            const QString content = QString::fromUtf8(obj->data);
            const QStringList lines = content.split('\n');
            for (const QString &line : lines) {
                if (line.startsWith("object ")) {
                    return line.mid(7).trimmed();
                }
            }
        }
        return sha;
    };

    // 1. Loose tags
    const QDir tagDir(m_gitDir + "/refs/tags");
    if (tagDir.exists()) {
        QDirIterator it(tagDir.absolutePath(), QDir::Files, QDirIterator::Subdirectories);
        while (it.hasNext()) {
            const QString path = it.next();
            const QString tagName = tagDir.relativeFilePath(path).replace(QDir::separator(), '/');
            const QString refName = "refs/tags/" + tagName;
            const QString sha = readRef(refName);
            if (!sha.isEmpty()) {
                const QString commitSha = peelTag(sha);
                if (!commitSha.isEmpty() && !m_commitTags[commitSha].contains(tagName)) {
                    m_commitTags[commitSha].append(tagName);
                }
            }
        }
    }

    // 2. Packed tags
    QFile file(m_gitDir + "/packed-refs");
    if (file.open(QIODevice::ReadOnly | QIODevice::Text)) {
        const QString content = QString::fromUtf8(file.readAll());
        const QStringList lines = content.split('\n');
        QString lastTagName;
        QString lastSha;
        for (const QString &line : lines) {
            const QString value = line.trimmed();
            if (value.isEmpty() || value.startsWith('#')) continue;
            if (value.startsWith('^')) {
                // Peeled tag
                if (!lastTagName.isEmpty()) {
                    QString peeledSha = normaliseSha(value.mid(1).trimmed());
                    if (!peeledSha.isEmpty() && !m_commitTags[peeledSha].contains(lastTagName)) {
                        m_commitTags[peeledSha].append(lastTagName);
                    }
                    lastTagName.clear();
                    lastSha.clear();
                }
                continue;
            }

            if (!lastTagName.isEmpty() && !lastSha.isEmpty()) {
                QString commitSha = peelTag(lastSha);
                if (!commitSha.isEmpty() && !m_commitTags[commitSha].contains(lastTagName)) {
                    m_commitTags[commitSha].append(lastTagName);
                }
                lastTagName.clear();
                lastSha.clear();
            }

            int spaceIdx = value.indexOf(' ');
            if (spaceIdx < 0) spaceIdx = value.indexOf('\t');
            if (spaceIdx >= ObjectIdHexLength) {
                QString sha = normaliseSha(value.left(ObjectIdHexLength));
                QString ref = value.mid(spaceIdx + 1).trimmed();
                if (ref.startsWith("refs/tags/")) {
                    lastTagName = ref.mid(10);
                    lastSha = sha;
                }
            }
        }
        if (!lastTagName.isEmpty() && !lastSha.isEmpty()) {
            QString commitSha = peelTag(lastSha);
            if (!commitSha.isEmpty() && !m_commitTags[commitSha].contains(lastTagName)) {
                m_commitTags[commitSha].append(lastTagName);
            }
        }
    }
}

void GitRepositoryReader::loadPackIndexes() const
{
    if (m_packIndexesLoaded || m_gitDir.isEmpty()) return;
    m_packIndexesLoaded = true;

    QDir packDir(m_gitDir + "/objects/pack");
    const QStringList indexes = packDir.entryList({"*.idx"}, QDir::Files);
    m_packIndexes.clear();
    m_packIndexes.reserve(indexes.size());

    for (const QString &indexName : indexes) {
        QFile indexFile(packDir.filePath(indexName));
        if (!indexFile.open(QIODevice::ReadOnly)) continue;
        const QByteArray index = indexFile.readAll();
        if (index.size() < 8 || index.left(4) != QByteArray("\xfftOc", 4)) continue;
        if (readBigEndian32(index, 4) != 2) continue;

        const quint32 objectCount = readBigEndian32(index, 8 + 255 * 4);
        const int namesOffset = 8 + 256 * 4;
        const qint64 namesEnd64 = static_cast<qint64>(namesOffset) + static_cast<qint64>(objectCount) * ObjectIdBytes;
        const qint64 crcEnd64 = namesEnd64 + static_cast<qint64>(objectCount) * 4;
        const qint64 offsetsEnd64 = crcEnd64 + static_cast<qint64>(objectCount) * 4;
        if (namesOffset < 0 || namesEnd64 > std::numeric_limits<int>::max() ||
            offsetsEnd64 > index.size()) continue;
        const int namesEnd = static_cast<int>(namesEnd64);
        const int crcEnd = static_cast<int>(crcEnd64);
        const int offsetsEnd = static_cast<int>(offsetsEnd64);

        const QString packPath = packDir.filePath(indexName.left(indexName.size() - 4) + ".pack");
        PackIndex pIdx;
        pIdx.packPath = packPath;
        pIdx.data = index;
        pIdx.objectCount = objectCount;
        pIdx.namesOffset = namesOffset;
        pIdx.crcOffset = namesEnd;
        pIdx.offsetsOffset = crcEnd;
        pIdx.largeOffsetsOffset = offsetsEnd;
        m_packIndexes.append(std::move(pIdx));
    }
}

QByteArray GitRepositoryReader::hexToBinary(const QString &hex)
{
    return QByteArray::fromHex(hex.toLatin1());
}

std::optional<GitRepositoryReader::PackLocation> GitRepositoryReader::findPackLocation(const QString &sha) const
{
    const QString norm = normaliseSha(sha);
    if (norm.size() != ObjectIdHexLength) return std::nullopt;
    const QByteArray bin = hexToBinary(norm);
    if (bin.size() != ObjectIdBytes) return std::nullopt;

    const quint8 firstByte = static_cast<quint8>(bin[0]);
    const unsigned char *target = reinterpret_cast<const unsigned char*>(bin.constData());

    loadPackIndexes();
    for (const auto &idx : m_packIndexes) {
        if (idx.objectCount == 0) continue;
        const quint32 start = (firstByte == 0) ? 0 : readBigEndian32(idx.data, 8 + (firstByte - 1) * 4);
        const quint32 end = readBigEndian32(idx.data, 8 + firstByte * 4);
        if (start >= end || end > idx.objectCount) continue;

        quint32 low = start;
        quint32 high = end;
        while (low < high) {
            const quint32 mid = low + (high - low) / 2;
            const unsigned char *candidate = reinterpret_cast<const unsigned char*>(idx.data.constData() + idx.namesOffset + mid * ObjectIdBytes);
            const int cmp = std::memcmp(target, candidate, ObjectIdBytes);
            if (cmp == 0) {
                const quint32 rawOffset = readBigEndian32(idx.data, idx.offsetsOffset + mid * 4);
                quint64 offset = rawOffset;
                if (rawOffset & 0x80000000U) {
                    const quint32 largeIndex = rawOffset & 0x7fffffffU;
                    const int largeOffsetPos = idx.largeOffsetsOffset + largeIndex * 8;
                    if (largeOffsetPos + 8 <= idx.data.size()) {
                        offset = readBigEndian64(idx.data, largeOffsetPos);
                    }
                }
                return PackLocation{idx.packPath, offset};
            } else if (cmp < 0) {
                high = mid;
            } else {
                low = mid + 1;
            }
        }
    }
    return std::nullopt;
}

QString GitRepositoryReader::resolveObjectId(const QString &sha) const
{
    const QString candidate = normaliseSha(sha);
    if (candidate.size() == ObjectIdHexLength) return candidate;
    if (candidate.size() < 4) return QString();

    loadPackIndexes();
    const QByteArray binPrefix = QByteArray::fromHex(candidate.toLatin1());
    if (!binPrefix.isEmpty()) {
        const quint8 firstByte = static_cast<quint8>(binPrefix[0]);
        for (const auto &idx : m_packIndexes) {
            if (idx.objectCount == 0) continue;
            const quint32 start = (firstByte == 0) ? 0 : readBigEndian32(idx.data, 8 + (firstByte - 1) * 4);
            const quint32 end = readBigEndian32(idx.data, 8 + firstByte * 4);
            if (start >= end || end > idx.objectCount) continue;

            for (quint32 i = start; i < end; ++i) {
                const QString full = objectId(idx.data.mid(idx.namesOffset + static_cast<int>(i) * ObjectIdBytes, ObjectIdBytes));
                if (full.startsWith(candidate)) return full;
            }
        }
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
    if (inflated.isEmpty()) return std::nullopt;
    const int separator = inflated.indexOf('\0');
    if (separator <= 0) return std::nullopt;

    GitObject object;
    object.type = QString::fromLatin1(inflated.left(separator)).section(' ', 0, 0);
    object.data = inflated.mid(separator + 1);
    return object;
}

std::optional<GitRepositoryReader::GitObject> GitRepositoryReader::readObject(const QString &sha, int depth) const
{
    if (depth > 64 || !m_directReadSupported) return std::nullopt;
    const QString resolved = resolveObjectId(sha);
    if (resolved.isEmpty()) return std::nullopt;
    if (m_objectCache.contains(resolved)) return m_objectCache.value(resolved);

    std::optional<GitObject> object = readLooseObject(resolved);
    if (!object) object = readPackedObject(resolved, depth);
    if (object) {
        const QByteArray header = object->type.toLatin1() + ' ' + QByteArray::number(object->data.size()) + '\0';
        const QString calculated = objectId(QCryptographicHash::hash(header + object->data, QCryptographicHash::Sha1));
        if (calculated != resolved) object.reset();
    }
    if (object) {
        if (m_objectCache.size() > 128) {
            m_objectCache.remove(m_objectCache.begin().key());
        }
        m_objectCache.insert(resolved, *object);
    }
    return object;
}

std::optional<GitRepositoryReader::GitObject> GitRepositoryReader::readPackedObject(const QString &sha, int depth) const
{
    auto location = findPackLocation(sha);
    if (!location) return std::nullopt;
    return readPackEntry(*location, depth);
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
    if (size > 256 * 1024 * 1024ULL) return std::nullopt;

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
    if (static_cast<quint64>(inflated.size()) != size) return std::nullopt;
    if (typeCode == 6 || typeCode == 7) {
        std::optional<GitObject> base;
        if (typeCode == 6) {
            if (baseOffset != 0) base = readPackEntry({location.packPath, baseOffset}, depth + 1);
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
    QMutexLocker locker(&m_mutex);
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
    QMutexLocker locker(&m_mutex);
    QList<BranchInfo> result;
    const auto current = currentBranch();
    QSet<QString> seen;
    static const QRegularExpression prRegex(R"(#?(\d+))");

    auto addRefWithSha = [&](const QString &refName, const QString &sha, bool remote) {
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
        const auto match = prRegex.match(name);
        if (match.hasMatch()) {
            branch.prNumber = "#" + match.captured(1);
            branch.prMergedOrActive = true;
        }
        result.append(branch);
    };

    // 1. Loose refs
    for (const QString &root : {QStringLiteral("heads"), QStringLiteral("remotes")}) {
        const QDir dir(m_gitDir + "/refs/" + root);
        if (!dir.exists()) continue;
        QDirIterator it(dir.absolutePath(), QDir::Files, QDirIterator::Subdirectories);
        while (it.hasNext()) {
            const QString path = it.next();
            const QString relative = dir.relativeFilePath(path).replace(QDir::separator(), '/');
            const QString refName = "refs/" + root + "/" + relative;
            addRefWithSha(refName, readRef(refName), root == "remotes");
        }
    }

    // 2. Packed refs (SHA is already in memory, no disk syscalls needed)
    loadPackedRefs();
    for (auto it = m_packedRefs.cbegin(); it != m_packedRefs.cend(); ++it) {
        if (it.key().startsWith("refs/heads/")) {
            addRefWithSha(it.key(), it.value(), false);
        } else if (it.key().startsWith("refs/remotes/")) {
            addRefWithSha(it.key(), it.value(), true);
        }
    }
    return result;
}

QList<CommitItem> GitRepositoryReader::commitHistory(int limit) const
{
    QMutexLocker locker(&m_mutex);
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
    QMutexLocker locker(&m_mutex);
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
    static const QRegularExpression authorRegex("^(.*) <([^>]+)> (\\d+) ([+-]\\d{4})$");
    for (const QString &line : headers) {
        if (line.startsWith("tree ")) treeSha = line.mid(5).trimmed();
        else if (line.startsWith("parent ") && firstParent.isEmpty()) firstParent = line.mid(7).trimmed();
        else if (line.startsWith("author ")) {
            const auto match = authorRegex.match(line.mid(7));
            if (match.hasMatch()) {
                commit.authorName = match.captured(1);
                commit.authorEmail = match.captured(2);
                commit.authorAvatarUrl = AvatarResolver::resolve(commit.authorName, commit.authorEmail);
                commit.timestamp = QDateTime::fromSecsSinceEpoch(match.captured(3).toLongLong(), QTimeZone::UTC);
            }
        }
    }

    const QString message = QString::fromUtf8(raw.mid(separator + 2));
    const QStringList messageLines = message.split('\n');
    commit.summary = messageLines.value(0).trimmed();
    commit.description = messageLines.mid(1).join('\n').trimmed();
    commit.relativeTime = relativeTime(commit.timestamp);

    static const QRegularExpression coAuthorRegex("Co-authored-by:\\s*(.*?)(?:<|$)", QRegularExpression::CaseInsensitiveOption);
    auto coAuthors = coAuthorRegex.globalMatch(commit.description);
    while (coAuthors.hasNext()) {
        const QString value = coAuthors.next().captured(1).trimmed();
        if (!value.isEmpty()) commit.coAuthors.append(value);
    }

    if (includeFiles && !treeSha.isEmpty()) populateChangedFiles(commit, firstParent);
    loadTags();
    commit.tags = m_commitTags.value(commit.sha);
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

QList<GitRepositoryReader::TreeEntry> GitRepositoryReader::readTreeEntries(const QString &treeSha) const
{
    QList<TreeEntry> entries;
    const auto tree = readObject(treeSha);
    if (!tree || tree->type != "tree") return entries;

    int cursor = 0;
    while (cursor < tree->data.size()) {
        const int modeEnd = tree->data.indexOf(' ', cursor);
        if (modeEnd < 0) break;
        const int nameEnd = tree->data.indexOf('\0', modeEnd + 1);
        if (nameEnd < 0 || nameEnd + 1 + ObjectIdBytes > tree->data.size()) break;
        const QString mode = QString::fromLatin1(tree->data.mid(cursor, modeEnd - cursor));
        const QString name = QString::fromUtf8(tree->data.mid(modeEnd + 1, nameEnd - modeEnd - 1));
        const QString childSha = objectId(tree->data.mid(nameEnd + 1, ObjectIdBytes));
        const bool isTree = mode.startsWith("04") || mode == "40000";
        entries.append({mode, name, childSha, isTree});
        cursor = nameEnd + 1 + ObjectIdBytes;
    }
    return entries;
}

void GitRepositoryReader::collectAllTreeFiles(const QString &treeSha, const QString &prefix, FileChangeType type, QList<FileChange> &result, int depth) const
{
    if (depth > 64 || treeSha.isEmpty()) return;
    const auto entries = readTreeEntries(treeSha);
    for (const auto &e : entries) {
        const QString path = prefix + e.name;
        if (e.isTree) {
            collectAllTreeFiles(e.sha, path + "/", type, result, depth + 1);
        } else {
            FileChange file;
            file.id = QString("fc-%1").arg(result.size());
            file.filePath = path;
            file.status = type;
            result.append(file);
        }
    }
}

void GitRepositoryReader::diffTrees(const QString &oldTreeSha, const QString &newTreeSha, const QString &prefix, QList<FileChange> &result, int depth) const
{
    if (depth > 64) return;
    if (oldTreeSha == newTreeSha) return; // Unchanged subtree pruned immediately

    if (oldTreeSha.isEmpty() && !newTreeSha.isEmpty()) {
        collectAllTreeFiles(newTreeSha, prefix, FileChangeType::Added, result, depth);
        return;
    }
    if (!oldTreeSha.isEmpty() && newTreeSha.isEmpty()) {
        collectAllTreeFiles(oldTreeSha, prefix, FileChangeType::Deleted, result, depth);
        return;
    }

    const auto oldEntries = readTreeEntries(oldTreeSha);
    const auto newEntries = readTreeEntries(newTreeSha);

    QHash<QString, TreeEntry> oldMap;
    for (const auto &e : oldEntries) oldMap.insert(e.name, e);

    QHash<QString, TreeEntry> newMap;
    for (const auto &e : newEntries) newMap.insert(e.name, e);

    QSet<QString> allNames;
    for (auto it = oldMap.cbegin(); it != oldMap.cend(); ++it) allNames.insert(it.key());
    for (auto it = newMap.cbegin(); it != newMap.cend(); ++it) allNames.insert(it.key());

    QStringList sortedNames = allNames.values();
    std::sort(sortedNames.begin(), sortedNames.end());

    for (const QString &name : sortedNames) {
        const bool hadOld = oldMap.contains(name);
        const bool hasNew = newMap.contains(name);
        const QString path = prefix + name;

        if (hadOld && !hasNew) {
            const auto &oldE = oldMap[name];
            if (oldE.isTree) {
                collectAllTreeFiles(oldE.sha, path + "/", FileChangeType::Deleted, result, depth + 1);
            } else {
                FileChange file;
                file.id = QString("fc-%1").arg(result.size());
                file.filePath = path;
                file.oldFilePath = path;
                file.status = FileChangeType::Deleted;
                result.append(file);
            }
        } else if (!hadOld && hasNew) {
            const auto &newE = newMap[name];
            if (newE.isTree) {
                collectAllTreeFiles(newE.sha, path + "/", FileChangeType::Added, result, depth + 1);
            } else {
                FileChange file;
                file.id = QString("fc-%1").arg(result.size());
                file.filePath = path;
                file.status = FileChangeType::Added;
                result.append(file);
            }
        } else {
            const auto &oldE = oldMap[name];
            const auto &newE = newMap[name];
            if (oldE.sha == newE.sha) continue; // Same object SHA

            if (oldE.isTree && newE.isTree) {
                diffTrees(oldE.sha, newE.sha, path + "/", result, depth + 1);
            } else if (!oldE.isTree && !newE.isTree) {
                FileChange file;
                file.id = QString("fc-%1").arg(result.size());
                file.filePath = path;
                file.status = FileChangeType::Modified;
                result.append(file);
            } else {
                // One is tree, one is blob
                if (oldE.isTree) collectAllTreeFiles(oldE.sha, path + "/", FileChangeType::Deleted, result, depth + 1);
                else {
                    FileChange f;
                    f.id = QString("fc-%1").arg(result.size());
                    f.filePath = path;
                    f.status = FileChangeType::Deleted;
                    result.append(f);
                }
                if (newE.isTree) collectAllTreeFiles(newE.sha, path + "/", FileChangeType::Added, result, depth + 1);
                else {
                    FileChange f;
                    f.id = QString("fc-%1").arg(result.size());
                    f.filePath = path;
                    f.status = FileChangeType::Added;
                    result.append(f);
                }
            }
        }
    }
}

QList<FileChange> GitRepositoryReader::changedFilesBetweenTrees(const QString &oldTree, const QString &newTree) const
{
    QList<FileChange> result;
    diffTrees(oldTree, newTree, QString(), result);

    // The tree format has no rename primitive. Recover exact renames by
    // matching deleted and added paths with the same blob object ID so the
    // direct-reader result remains consistent with Git's rename-aware CLI.
    const auto oldFiles = oldTree.isEmpty() ? QHash<QString, QString>() : readTree(oldTree, QString());
    const auto newFiles = newTree.isEmpty() ? QHash<QString, QString>() : readTree(newTree, QString());
    QHash<QString, QString> renameSourceByTarget;
    QSet<QString> deletedPaths;
    for (const auto &file : result) {
        if (file.status != FileChangeType::Deleted || !oldFiles.contains(file.filePath)) continue;
        const QString objectId = oldFiles.value(file.filePath);
        for (auto it = newFiles.cbegin(); it != newFiles.cend(); ++it) {
            if (it.value() == objectId && !oldFiles.contains(it.key())) {
                renameSourceByTarget.insert(it.key(), file.filePath);
                deletedPaths.insert(file.filePath);
                break;
            }
        }
    }

    QList<FileChange> normalized;
    for (auto file : result) {
        if (file.status == FileChangeType::Deleted && deletedPaths.contains(file.filePath)) continue;
        if (file.status == FileChangeType::Added && renameSourceByTarget.contains(file.filePath)) {
            file.status = FileChangeType::Renamed;
            file.oldFilePath = renameSourceByTarget.value(file.filePath);
        }
        file.id = QString("fc-%1").arg(normalized.size());
        normalized.append(file);
    }
    return normalized;
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
    QMutexLocker locker(&m_mutex);
    return readStashReflog();
}

std::optional<StashItem> GitRepositoryReader::stashDetails(const QString &stashId) const
{
    QMutexLocker locker(&m_mutex);
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
    if (offset < 0 || offset + 8 > data.size()) return 0;
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
        if (produced > 0) {
            if (output.size() > 256 * 1024 * 1024 - produced) {
                inflateEnd(&stream);
                return {};
            }
            output.append(buffer, produced);
        }
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
            if (produced > 0) {
                if (output.size() > 256 * 1024 * 1024 - produced) {
                    inflateEnd(&stream);
                    return {};
                }
                output.append(buffer, produced);
            }
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
    auto readVarInt = [&]() -> std::optional<quint64> {
        quint64 value = 0;
        int shift = 0;
        while (cursor < delta.size()) {
            const unsigned char byte = static_cast<unsigned char>(delta[cursor++]);
            if (shift >= 64 || (shift == 63 && (byte & 0x7f) > 1)) return std::nullopt;
            value |= static_cast<quint64>(byte & 0x7f) << shift;
            if (!(byte & 0x80)) return value;
            shift += 7;
        }
        return std::nullopt;
    };

    const auto baseSizeValue = readVarInt();
    const auto resultSizeValue = readVarInt();
    if (!baseSizeValue || !resultSizeValue) return {};
    const quint64 baseSize = *baseSizeValue;
    const quint64 resultSize = *resultSizeValue;
    if (baseSize != static_cast<quint64>(base.size()) || resultSize > 256 * 1024 * 1024ULL) return {};

    QByteArray result;
    result.reserve(static_cast<int>(resultSize));
    while (cursor < delta.size() && static_cast<quint64>(result.size()) < resultSize) {
        const unsigned char opcode = static_cast<unsigned char>(delta[cursor++]);
        if (opcode & 0x80) {
            quint64 offset = 0;
            quint64 size = 0;
            auto readDeltaByte = [&](quint64 shift) -> bool {
                if (cursor >= delta.size()) return false;
                offset |= static_cast<quint64>(static_cast<unsigned char>(delta[cursor++])) << shift;
                return true;
            };
            if ((opcode & 0x01) && !readDeltaByte(0)) return {};
            if ((opcode & 0x02) && !readDeltaByte(8)) return {};
            if ((opcode & 0x04) && !readDeltaByte(16)) return {};
            if ((opcode & 0x08) && !readDeltaByte(24)) return {};
            auto readDeltaSizeByte = [&](quint64 shift) -> bool {
                if (cursor >= delta.size()) return false;
                size |= static_cast<quint64>(static_cast<unsigned char>(delta[cursor++])) << shift;
                return true;
            };
            if ((opcode & 0x10) && !readDeltaSizeByte(0)) return {};
            if ((opcode & 0x20) && !readDeltaSizeByte(8)) return {};
            if ((opcode & 0x40) && !readDeltaSizeByte(16)) return {};
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
