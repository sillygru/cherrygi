#pragma once

#include <QObject>
#include <QString>
#include <QStringList>
#include <memory>

#include "IGitService.h"
#include "GitCliService.h"
#include "MockGitService.h"
#include "AppSettings.h"
#include "../models/RepositoryListModel.h"
#include "../models/BranchListModel.h"
#include "../models/ChangedFilesModel.h"
#include "../models/CommitHistoryModel.h"
#include "../models/DiffModel.h"
#include "../models/StashModel.h"

namespace Cherry {

class AppController : public QObject {
    Q_OBJECT

    // Backend Mode & Startup Dialog
    Q_PROPERTY(QString backendMode READ backendMode WRITE setBackendMode NOTIFY backendModeChanged)
    Q_PROPERTY(bool isBackendDialogVisible READ isBackendDialogVisible WRITE setBackendDialogVisible NOTIFY backendDialogVisibleChanged)

    // Models
    Q_PROPERTY(Cherry::RepositoryListModel* repositories READ repositories CONSTANT)
    Q_PROPERTY(Cherry::BranchListModel* branches READ branches CONSTANT)
    Q_PROPERTY(Cherry::ChangedFilesModel* changedFiles READ changedFiles CONSTANT)
    Q_PROPERTY(Cherry::CommitHistoryModel* commitHistory READ commitHistory CONSTANT)
    Q_PROPERTY(Cherry::DiffModel* diffModel READ diffModel CONSTANT)
    Q_PROPERTY(Cherry::StashModel* stashes READ stashes CONSTANT)

    // Current State
    Q_PROPERTY(QString currentRepoName READ currentRepoName NOTIFY currentRepoChanged)
    Q_PROPERTY(QString currentRepoPath READ currentRepoPath NOTIFY currentRepoChanged)
    Q_PROPERTY(QString currentBranchName READ currentBranchName NOTIFY currentBranchChanged)
    Q_PROPERTY(QString currentBranchPr READ currentBranchPr NOTIFY currentBranchChanged)
    Q_PROPERTY(bool currentBranchPrActive READ currentBranchPrActive NOTIFY currentBranchChanged)
    Q_PROPERTY(QString currentAuthorName READ currentAuthorName NOTIFY currentRepoChanged)
    Q_PROPERTY(QString currentAuthorInitial READ currentAuthorInitial NOTIFY currentRepoChanged)

    // Remote Sync State & Capabilities
    Q_PROPERTY(bool hasRemote READ hasRemote NOTIFY remoteStatusChanged)
    Q_PROPERTY(QString remoteUrl READ remoteUrl NOTIFY remoteStatusChanged)
    Q_PROPERTY(int aheadCount READ aheadCount NOTIFY remoteStatusChanged)
    Q_PROPERTY(int behindCount READ behindCount NOTIFY remoteStatusChanged)
    Q_PROPERTY(QString lastFetchedText READ lastFetchedText NOTIFY remoteStatusChanged)
    Q_PROPERTY(bool isFetching READ isFetching NOTIFY remoteStatusChanged)
    Q_PROPERTY(bool isPulling READ isPulling NOTIFY remoteStatusChanged)
    Q_PROPERTY(bool isPushing READ isPushing NOTIFY remoteStatusChanged)
    Q_PROPERTY(bool isPublishing READ isPublishing NOTIFY isPublishingChanged)
    Q_PROPERTY(QString publishErrorMessage READ publishErrorMessage NOTIFY publishErrorMessageChanged)
    Q_PROPERTY(bool isCommitting READ isCommitting NOTIFY isCommittingChanged)
    Q_PROPERTY(bool isLoadingRepository READ isLoadingRepository NOTIFY isLoadingRepositoryChanged)
    Q_PROPERTY(QString loadingRepositoryMessage READ loadingRepositoryMessage NOTIFY loadingRepositoryMessageChanged)
    Q_PROPERTY(QString loadingRepositoryName READ loadingRepositoryName NOTIFY loadingRepositoryNameChanged)
    Q_PROPERTY(bool isOperating READ isOperating NOTIFY operatingStateChanged)
    Q_PROPERTY(QString operationMessage READ operationMessage NOTIFY operatingStateChanged)
    Q_PROPERTY(bool isGhAvailable READ isGhAvailable CONSTANT)

    // Selection & Navigation
    Q_PROPERTY(QString activeTab READ activeTab WRITE setActiveTab NOTIFY activeTabChanged)
    Q_PROPERTY(QString selectedFilePath READ selectedFilePath WRITE setSelectedFilePath NOTIFY selectedFilePathChanged)
    Q_PROPERTY(QString selectedCommitSha READ selectedCommitSha WRITE setSelectedCommitSha NOTIFY selectedCommitShaChanged)
    Q_PROPERTY(QVariant selectedCommitData READ selectedCommitData NOTIFY selectedCommitShaChanged)
    Q_PROPERTY(QString selectedStashId READ selectedStashId WRITE setSelectedStashId NOTIFY selectedStashIdChanged)
    Q_PROPERTY(QVariant selectedStashData READ selectedStashData NOTIFY selectedStashIdChanged)

    // Diff Options
    Q_PROPERTY(QString diffViewMode READ diffViewMode WRITE setDiffViewMode NOTIFY diffViewModeChanged)
    Q_PROPERTY(bool showWhitespace READ showWhitespace WRITE setShowWhitespace NOTIFY showWhitespaceChanged)

    // Git Configuration
    Q_PROPERTY(bool ignoreFileModeChanges READ ignoreFileModeChanges NOTIFY gitConfigChanged)
    Q_PROPERTY(bool globalIgnoreFileModeChanges READ globalIgnoreFileModeChanges NOTIFY gitConfigChanged)

    // Undo State
    Q_PROPERTY(bool canUndoCommit READ canUndoCommit NOTIFY undoStateChanged)
    Q_PROPERTY(bool hasUndoCommit READ canUndoCommit NOTIFY undoStateChanged)
    Q_PROPERTY(QString lastUndoCommitSummary READ lastUndoCommitSummary NOTIFY undoStateChanged)
    Q_PROPERTY(QString lastUndoCommitDescription READ lastUndoCommitDescription NOTIFY undoStateChanged)
    Q_PROPERTY(QStringList lastUndoCommitCoAuthors READ lastUndoCommitCoAuthors NOTIFY undoStateChanged)

    // Settings & Dialogs
    Q_PROPERTY(bool isSettingsDialogVisible READ isSettingsDialogVisible WRITE setSettingsDialogVisible NOTIFY settingsDialogVisibleChanged)
    Q_PROPERTY(bool isPublishDialogVisible READ isPublishDialogVisible WRITE setPublishDialogVisible NOTIFY publishDialogVisibleChanged)
    Q_PROPERTY(QString settingsTab READ settingsTab WRITE setSettingsTab NOTIFY settingsTabChanged)

    // External Tools & Preferences
    Q_PROPERTY(QString defaultEditor READ defaultEditor NOTIFY editorSettingsChanged)
    Q_PROPERTY(QString customEditorCommand READ customEditorCommand NOTIFY editorSettingsChanged)
    Q_PROPERTY(QString defaultTerminal READ defaultTerminal NOTIFY terminalSettingsChanged)
    Q_PROPERTY(QString customTerminalCommand READ customTerminalCommand NOTIFY terminalSettingsChanged)
    Q_PROPERTY(QStringList availableEditors READ availableEditors CONSTANT)
    Q_PROPERTY(QStringList availableTerminals READ availableTerminals CONSTANT)

    // Author Config
    Q_PROPERTY(QString globalAuthorName READ globalAuthorName NOTIFY authorInfoChanged)
    Q_PROPERTY(QString globalAuthorEmail READ globalAuthorEmail NOTIFY authorInfoChanged)
    Q_PROPERTY(QString localAuthorName READ localAuthorName NOTIFY authorInfoChanged)
    Q_PROPERTY(QString localAuthorEmail READ localAuthorEmail NOTIFY authorInfoChanged)

    // Notification / Toast
    Q_PROPERTY(QString toastMessage READ toastMessage NOTIFY toastChanged)
    Q_PROPERTY(bool toastIsError READ toastIsError NOTIFY toastChanged)
    Q_PROPERTY(bool toastVisible READ toastVisible NOTIFY toastChanged)

public:
    explicit AppController(QObject *parent = nullptr);
    ~AppController() override;

    // Backend Mode
    QString backendMode() const { return m_backendMode; }
    void setBackendMode(const QString &mode);

    bool isBackendDialogVisible() const { return m_isBackendDialogVisible; }
    void setBackendDialogVisible(bool visible);

    // Getters
    RepositoryListModel* repositories() const { return m_repoModel; }
    BranchListModel* branches() const { return m_branchModel; }
    ChangedFilesModel* changedFiles() const { return m_changedFilesModel; }
    CommitHistoryModel* commitHistory() const { return m_commitHistoryModel; }
    DiffModel* diffModel() const { return m_diffModel; }
    StashModel* stashes() const { return m_stashModel; }

    QString currentRepoName() const;
    QString currentRepoPath() const;
    QString currentBranchName() const;
    QString currentBranchPr() const;
    bool currentBranchPrActive() const;
    QString currentAuthorName() const;
    QString currentAuthorInitial() const;

    bool hasRemote() const { return m_remoteStatus.hasRemote; }
    QString remoteUrl() const { return m_remoteStatus.remoteUrl; }
    int aheadCount() const { return m_remoteStatus.ahead; }
    int behindCount() const { return m_remoteStatus.behind; }
    QString lastFetchedText() const { return m_remoteStatus.lastFetchedText; }
    bool isFetching() const { return m_remoteStatus.isFetching; }
    bool isPulling() const { return m_remoteStatus.isPulling; }
    bool isPushing() const { return m_remoteStatus.isPushing; }
    bool isPublishing() const { return m_isPublishing; }
    QString publishErrorMessage() const { return m_publishErrorMessage; }
    bool isCommitting() const { return m_isCommitting; }
    bool isLoadingRepository() const { return m_isLoadingRepository; }
    QString loadingRepositoryMessage() const { return m_loadingRepositoryMessage; }
    QString loadingRepositoryName() const { return m_loadingRepositoryName; }
    bool isOperating() const { return m_isLoadingRepository || m_isPublishing || m_isCommitting || m_isDiscarding || m_isStashing || m_isReverting || isFetching() || isPulling() || isPushing(); }
    QString operationMessage() const;
    bool isGhAvailable() const;

    QString activeTab() const { return m_activeTab; }
    void setActiveTab(const QString &tab);

    QString selectedFilePath() const { return m_selectedFilePath; }
    void setSelectedFilePath(const QString &path);

    QString selectedCommitSha() const { return m_selectedCommitSha; }
    void setSelectedCommitSha(const QString &sha);
    QVariant selectedCommitData() const;

    QString selectedStashId() const { return m_selectedStashId; }
    void setSelectedStashId(const QString &id);
    QVariant selectedStashData() const;

    QString diffViewMode() const { return m_diffViewMode; }
    void setDiffViewMode(const QString &mode);

    bool showWhitespace() const { return m_showWhitespace; }
    void setShowWhitespace(bool show);

    bool ignoreFileModeChanges() const { return m_activeService ? m_activeService->ignoreFileModeChanges(false) : false; }
    bool globalIgnoreFileModeChanges() const { return m_activeService ? m_activeService->ignoreFileModeChanges(true) : false; }

    bool canUndoCommit() const;
    QString lastUndoCommitSummary() const;
    QString lastUndoCommitDescription() const;
    QStringList lastUndoCommitCoAuthors() const;

    // Settings Dialog
    bool isSettingsDialogVisible() const { return m_isSettingsDialogVisible; }
    void setSettingsDialogVisible(bool visible);

    bool isPublishDialogVisible() const { return m_isPublishDialogVisible; }
    void setPublishDialogVisible(bool visible);

    QString settingsTab() const { return m_settingsTab; }
    void setSettingsTab(const QString &tab);

    // Tools & Author info
    QString defaultEditor() const;
    QString customEditorCommand() const;
    QString defaultTerminal() const;
    QString customTerminalCommand() const;
    QStringList availableEditors() const;
    QStringList availableTerminals() const;

    QString globalAuthorName() const;
    QString globalAuthorEmail() const;
    QString localAuthorName() const;
    QString localAuthorEmail() const;

    QString toastMessage() const { return m_toastMessage; }
    bool toastIsError() const { return m_toastIsError; }
    bool toastVisible() const { return m_toastVisible; }

    // Invocable Actions for QML
    Q_INVOKABLE void showBackendSelectionDialog();
    Q_INVOKABLE void hideBackendSelectionDialog();
    Q_INVOKABLE void selectBackend(const QString &mode); // "real" | "mock"

    Q_INVOKABLE void showSettingsDialog(const QString &tab = "repository");
    Q_INVOKABLE void hideSettingsDialog();

    Q_INVOKABLE void showPublishDialog();
    Q_INVOKABLE void hidePublishDialog();
    Q_INVOKABLE bool publishRepository(const QString &name, const QString &description, bool isPrivate);

    Q_INVOKABLE bool saveRemoteUrl(const QString &url, const QString &remoteName = "origin");
    Q_INVOKABLE bool removeRemoteUrl(const QString &remoteName = "origin");
    Q_INVOKABLE bool setIgnoreFileModeChanges(bool ignored, bool global = false);
    Q_INVOKABLE bool saveAuthorInfo(const QString &name, const QString &email, bool global = false);
    Q_INVOKABLE void saveEditorSettings(const QString &editor, const QString &customCmd);
    Q_INVOKABLE void saveTerminalSettings(const QString &terminal, const QString &customCmd);

    Q_INVOKABLE void openLocalRepositoryDialog();
    Q_INVOKABLE void switchRepository(const QString &repoIdOrPath);
    Q_INVOKABLE void addRepository(const QString &name, const QString &path);
    Q_INVOKABLE void removeRepository(const QString &repoIdOrPath);
    Q_INVOKABLE void refresh();
    Q_INVOKABLE void refreshRepository();

    Q_INVOKABLE void switchBranch(const QString &branchName);
    Q_INVOKABLE void createBranch(const QString &branchName);
    Q_INVOKABLE void deleteBranch(const QString &branchName);

    Q_INVOKABLE bool commit(const QString &summary, const QString &description, const QStringList &coAuthors);
    Q_INVOKABLE bool undoLastCommit();

    Q_INVOKABLE void fetchOrigin();
    Q_INVOKABLE void pullOrigin();
    Q_INVOKABLE void pushOrigin();

    Q_INVOKABLE void discardFileChanges(const QString &filePath);
    Q_INVOKABLE void discardAllChanges();

    Q_INVOKABLE void stashChanges(const QString &message = QString());
    Q_INVOKABLE void popStash(const QString &stashId = QString());
    Q_INVOKABLE void dropStash(const QString &stashId);

    Q_INVOKABLE void selectFileForDiff(const QString &filePath);
    Q_INVOKABLE void selectCommit(const QString &sha);
    Q_INVOKABLE void selectStash(const QString &stashId);
    Q_INVOKABLE void clearStashSelection();
    Q_INVOKABLE void revertCommit(const QString &sha);

    Q_INVOKABLE void openInEditor(const QString &filePath = QString());
    Q_INVOKABLE void openInTerminal(const QString &path = QString());
    Q_INVOKABLE void openInFileManager(const QString &path = QString());
    Q_INVOKABLE void openOnGitHub();
    Q_INVOKABLE void createPullRequest();

    Q_INVOKABLE void hideToast();
    Q_INVOKABLE void showToast(const QString &message, bool isError = false);

signals:
    void backendModeChanged();
    void backendDialogVisibleChanged();
    void currentRepoChanged();
    void currentBranchChanged();
    void remoteStatusChanged();
    void isPublishingChanged();
    void publishErrorMessageChanged();
    void isCommittingChanged();
    void isLoadingRepositoryChanged();
    void loadingRepositoryMessageChanged();
    void loadingRepositoryNameChanged();
    void operatingStateChanged();
    void activeTabChanged();
    void selectedFilePathChanged();
    void selectedCommitShaChanged();
    void selectedStashIdChanged();
    void diffViewModeChanged();
    void showWhitespaceChanged();
    void gitConfigChanged();
    void undoStateChanged();
    void settingsDialogVisibleChanged();
    void publishDialogVisibleChanged();
    void settingsTabChanged();
    void editorSettingsChanged();
    void terminalSettingsChanged();
    void authorInfoChanged();
    void toastChanged();

private:
    void updateCurrentState();
    void connectServiceSignals();

    std::unique_ptr<AppSettings> m_settings;
    std::unique_ptr<GitCliService> m_gitCliService;
    std::unique_ptr<MockGitService> m_mockService;
    IGitService *m_activeService{nullptr};

    QString m_backendMode{"real"};
    bool m_isBackendDialogVisible{true};
    bool m_isSettingsDialogVisible{false};
    bool m_isPublishDialogVisible{false};
    bool m_isPublishing{false};
    QString m_publishErrorMessage;
    bool m_isCommitting{false};
    bool m_isLoadingRepository{false};
    QString m_loadingRepositoryMessage;
    QString m_loadingRepositoryName;
    bool m_isDiscarding{false};
    bool m_isStashing{false};
    bool m_isReverting{false};
    QString m_settingsTab{"repository"};

    RepositoryListModel *m_repoModel{nullptr};
    BranchListModel *m_branchModel{nullptr};
    ChangedFilesModel *m_changedFilesModel{nullptr};
    CommitHistoryModel *m_commitHistoryModel{nullptr};
    DiffModel *m_diffModel{nullptr};
    StashModel *m_stashModel{nullptr};

    QString m_activeTab{"changes"};
    QString m_selectedFilePath;
    QString m_selectedCommitSha;
    QString m_selectedStashId;
    QString m_diffViewMode{"unified"};
    bool m_showWhitespace{true};

    RemoteStatus m_remoteStatus;

    QString m_toastMessage;
    bool m_toastIsError{false};
    bool m_toastVisible{false};
};

} // namespace Cherry

