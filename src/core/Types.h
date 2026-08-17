#pragma once

#include <QString>
#include <QStringList>
#include <QDateTime>
#include <QMetaType>
#include <QList>

namespace Cherry {

enum class FileChangeType {
    Modified,
    Added,
    Deleted,
    Renamed,
    Untracked
};

enum class DiffLineType {
    Context,
    Addition,
    Deletion,
    HunkHeader
};

struct DiffLine {
    int oldLineNumber{-1};
    int newLineNumber{-1};
    DiffLineType type{DiffLineType::Context};
    QString content;
};

struct FileChange {
    QString id;
    QString filePath;
    QString oldFilePath; // for renames
    FileChangeType status{FileChangeType::Modified};
    bool isSelected{true};
    int additions{0};
    int deletions{0};
    QList<DiffLine> diffLines;
};

struct CommitItem {
    QString sha;
    QString shortSha;
    QString summary;
    QString description;
    QString authorName;
    QString authorEmail;
    QString authorAvatarUrl;
    QDateTime timestamp;
    QString relativeTime;
    QStringList coAuthors;
    QStringList tags;
    QList<FileChange> changedFiles;
};

struct BranchInfo {
    QString name;
    bool isCurrent{false};
    bool isDefault{false};
    bool isRemote{false};
    QString prNumber; // e.g. "#17192"
    bool prMergedOrActive{false};
    QString tipCommitSha;
};

struct RepositoryInfo {
    QString id;
    QString name;
    QString path;
    QString currentBranch;
    int changedFilesCount{0};
    int aheadCount{0};
    int behindCount{0};
    QString lastFetchedTime;
    bool isMissing{false};
    QString remoteUrl;
};

struct StashItem {
    QString id;
    QString message;
    QString branchName;
    QDateTime timestamp;
    QList<FileChange> files;
};

struct RemoteStatus {
    bool hasRemote{false};
    QString remoteName{"origin"};
    QString remoteUrl;
    int ahead{0};
    int behind{0};
    QString lastFetchedText{"No remote repository configured"};
    bool isFetching{false};
    bool isPulling{false};
    bool isPushing{false};
};

} // namespace Cherry

Q_DECLARE_METATYPE(Cherry::DiffLine)
Q_DECLARE_METATYPE(Cherry::FileChange)
Q_DECLARE_METATYPE(Cherry::CommitItem)
Q_DECLARE_METATYPE(Cherry::BranchInfo)
Q_DECLARE_METATYPE(Cherry::RepositoryInfo)
