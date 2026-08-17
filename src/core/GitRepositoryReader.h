#pragma once

#include "Types.h"
#include <QHash>
#include <QList>
#include <QFile>
#include <QString>
#include <QRecursiveMutex>
#include <optional>

namespace Cherry {

/**
 * Read-only access to Git's on-disk repository format.
 *
 * This deliberately does not replace Git CLI operations. It is used for cheap,
 * local reads (HEAD, refs, commit objects, trees, and stash metadata), while
 * GitCliService remains responsible for mutations, status, diffs, and remotes.
 */
class GitRepositoryReader {
public:
    GitRepositoryReader() = default;

    bool open(const QString &path);
    void refresh();

    bool isOpen() const { return !m_gitDir.isEmpty(); }
    bool directReadSupported() const { return m_directReadSupported; }
    QString workTreePath() const { return m_workTree; }
    QString gitDirPath() const { return m_headGitDir.isEmpty() ? m_gitDir : m_headGitDir; }
    QString commonGitDirPath() const { return m_gitDir; }

    std::optional<BranchInfo> currentBranch() const;
    QList<BranchInfo> branches() const;
    QList<CommitItem> commitHistory(int limit = 100) const;
    std::optional<CommitItem> commitDetails(const QString &sha) const;
    QList<StashItem> stashes() const;
    std::optional<StashItem> stashDetails(const QString &stashId) const;

private:
    struct GitObject {
        QString type;
        QByteArray data;
    };

    struct PackLocation {
        QString packPath;
        quint64 offset{0};
    };

    struct PackIndex {
        QString packPath;
        QByteArray data;
        quint32 objectCount{0};
        int namesOffset{0};
        int crcOffset{0};
        int offsetsOffset{0};
        int largeOffsetsOffset{0};
    };

    bool detectDirectReadSupport() const;
    QString resolveGitDir(const QString &path) const;
    QString readRef(const QString &refName) const;
    QString readHeadRef() const;
    QString resolveObjectId(const QString &sha) const;
    void loadPackedRefs() const;
    void loadPackIndexes() const;
    std::optional<PackLocation> findPackLocation(const QString &sha) const;

    std::optional<GitObject> readObject(const QString &sha, int depth = 0) const;
    std::optional<GitObject> readLooseObject(const QString &sha) const;
    std::optional<GitObject> readPackedObject(const QString &sha, int depth) const;
    std::optional<GitObject> readPackEntry(const PackLocation &location, int depth) const;

    struct TreeEntry {
        QString mode;
        QString name;
        QString sha;
        bool isTree{false};
    };

    std::optional<CommitItem> parseCommit(const QString &sha, bool includeFiles) const;
    QList<TreeEntry> readTreeEntries(const QString &treeSha) const;
    QHash<QString, QString> readTree(const QString &treeSha, const QString &prefix, int depth = 0) const;
    void populateChangedFiles(CommitItem &commit, const QString &parentSha) const;
    void diffTrees(const QString &oldTreeSha, const QString &newTreeSha, const QString &prefix, QList<FileChange> &result, int depth = 0) const;
    void collectAllTreeFiles(const QString &treeSha, const QString &prefix, FileChangeType type, QList<FileChange> &result, int depth = 0) const;
    QList<FileChange> changedFilesBetweenTrees(const QString &oldTree, const QString &newTree) const;
    QList<StashItem> readStashReflog() const;

    static quint32 readBigEndian32(const QByteArray &data, int offset);
    static quint64 readBigEndian64(const QByteArray &data, int offset);
    static QString objectId(const QByteArray &bytes);
    static QByteArray hexToBinary(const QString &hex);
    static QByteArray inflateBytes(const QByteArray &compressed, int *consumed = nullptr);
    static QByteArray inflateFile(QFile &file);
    static QByteArray applyDelta(const QByteArray &base, const QByteArray &delta);
    static QString relativeTime(const QDateTime &timestamp);
    static QString cleanStashMessage(const QString &message);

    void loadTags() const;

    QString m_workTree;
    QString m_gitDir;
    QString m_headGitDir; // per-worktree gitdir; m_gitDir may be the shared common dir
    bool m_directReadSupported{true};

    mutable bool m_packedRefsLoaded{false};
    mutable bool m_packIndexesLoaded{false};
    mutable bool m_tagsLoaded{false};
    mutable QHash<QString, QString> m_packedRefs;
    mutable QList<PackIndex> m_packIndexes;
    mutable QHash<QString, QStringList> m_commitTags;
    mutable QHash<QString, GitObject> m_objectCache;
    mutable QRecursiveMutex m_mutex;
};

} // namespace Cherry
