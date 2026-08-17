#pragma once

#include <QObject>
#include <QString>
#include <QStringList>
#include <QMap>
#include <QSettings>

namespace Cherry {

struct EditorInfo {
    QString id;
    QString displayName;
    QString executable;
    QString iconName;
    QString openFileArg;
    QString openDirArg;
};

struct TerminalInfo {
    QString id;
    QString displayName;
    QString executable;
    QString iconName;
    QString workDirArg;
};

class AppSettings : public QObject {
    Q_OBJECT

    Q_PROPERTY(QString defaultEditor READ defaultEditor WRITE setDefaultEditor NOTIFY defaultEditorChanged)
    Q_PROPERTY(QString customEditorCommand READ customEditorCommand WRITE setCustomEditorCommand NOTIFY customEditorCommandChanged)
    Q_PROPERTY(QString defaultTerminal READ defaultTerminal WRITE setDefaultTerminal NOTIFY defaultTerminalChanged)
    Q_PROPERTY(QString customTerminalCommand READ customTerminalCommand WRITE setCustomTerminalCommand NOTIFY customTerminalCommandChanged)
    Q_PROPERTY(QString diffViewMode READ diffViewMode WRITE setDiffViewMode NOTIFY diffViewModeChanged)
    Q_PROPERTY(bool showWhitespace READ showWhitespace WRITE setShowWhitespace NOTIFY showWhitespaceChanged)
    Q_PROPERTY(int tabSize READ tabSize WRITE setTabSize NOTIFY tabSizeChanged)
    Q_PROPERTY(QString startupBackend READ startupBackend WRITE setStartupBackend NOTIFY startupBackendChanged)
    Q_PROPERTY(QString avatarProvider READ avatarProvider WRITE setAvatarProvider NOTIFY avatarProviderChanged)
    Q_PROPERTY(bool isGhAvailable READ isGhAvailable CONSTANT)
    Q_PROPERTY(QStringList availableEditors READ availableEditors CONSTANT)
    Q_PROPERTY(QStringList availableTerminals READ availableTerminals CONSTANT)

    // AI Commit Assistant Settings
    Q_PROPERTY(bool aiEnabled READ aiEnabled WRITE setAiEnabled NOTIFY aiEnabledChanged)
    Q_PROPERTY(QString aiProvider READ aiProvider WRITE setAiProvider NOTIFY aiProviderChanged)
    Q_PROPERTY(QString aiApiKey READ aiApiKey WRITE setAiApiKey NOTIFY aiApiKeyChanged)
    Q_PROPERTY(QString aiEndpoint READ aiEndpoint WRITE setAiEndpoint NOTIFY aiEndpointChanged)
    Q_PROPERTY(QString aiModel READ aiModel WRITE setAiModel NOTIFY aiModelChanged)
    Q_PROPERTY(QString aiCommitStyle READ aiCommitStyle WRITE setAiCommitStyle NOTIFY aiCommitStyleChanged)
    Q_PROPERTY(bool aiIncludeDescription READ aiIncludeDescription WRITE setAiIncludeDescription NOTIFY aiIncludeDescriptionChanged)
    Q_PROPERTY(bool aiFollowRepoStyle READ aiFollowRepoStyle WRITE setAiFollowRepoStyle NOTIFY aiFollowRepoStyleChanged)
    Q_PROPERTY(bool aiFirstRunConfigured READ aiFirstRunConfigured WRITE setAiFirstRunConfigured NOTIFY aiFirstRunConfiguredChanged)

public:
    explicit AppSettings(QObject *parent = nullptr);
    ~AppSettings() override = default;

    static AppSettings* instance();

    // Returns the canonical app-data directory (e.g. ~/.local/share/cherrygi),
    // creating it if needed. Use this for program-tracked data, not user prefs.
    static QString dataDir();

    // Editor & Terminal Settings
    QString defaultEditor() const { return m_defaultEditor; }
    void setDefaultEditor(const QString &editorId);

    QString customEditorCommand() const { return m_customEditorCommand; }
    void setCustomEditorCommand(const QString &cmd);

    QString defaultTerminal() const { return m_defaultTerminal; }
    void setDefaultTerminal(const QString &termId);

    QString customTerminalCommand() const { return m_customTerminalCommand; }
    void setCustomTerminalCommand(const QString &cmd);

    // Diff & Display Settings
    QString diffViewMode() const { return m_diffViewMode; }
    void setDiffViewMode(const QString &mode);

    bool showWhitespace() const { return m_showWhitespace; }
    void setShowWhitespace(bool show);

    int tabSize() const { return m_tabSize; }
    void setTabSize(int size);

    QString startupBackend() const { return m_startupBackend; }
    void setStartupBackend(const QString &backend);

    QString avatarProvider() const { return m_avatarProvider; }
    void setAvatarProvider(const QString &provider);

    // AI Commit Assistant Settings
    bool aiEnabled() const { return m_aiEnabled; }
    void setAiEnabled(bool enabled);

    QString aiProvider() const { return m_aiProvider; }
    void setAiProvider(const QString &provider);

    QString aiApiKey() const { return m_aiApiKey; }
    void setAiApiKey(const QString &apiKey);

    QString aiEndpoint() const { return m_aiEndpoint; }
    void setAiEndpoint(const QString &endpoint);

    QString aiModel() const { return m_aiModel; }
    void setAiModel(const QString &model);

    QString aiCommitStyle() const { return m_aiCommitStyle; }
    void setAiCommitStyle(const QString &style);

    bool aiIncludeDescription() const { return m_aiIncludeDescription; }
    void setAiIncludeDescription(bool includeDesc);

    bool aiFollowRepoStyle() const { return m_aiFollowRepoStyle; }
    void setAiFollowRepoStyle(bool follow);

    bool aiFirstRunConfigured() const { return m_aiFirstRunConfigured; }
    void setAiFirstRunConfigured(bool configured);

    // Discovery & Capabilities
    bool isGhAvailable() const;
    QStringList availableEditors() const;
    QStringList availableTerminals() const;

    // Execution Helpers
    bool openInEditor(const QString &filePath, const QString &repoPath, int line = -1);
    bool openInTerminal(const QString &repoPath);

signals:
    void defaultEditorChanged();
    void customEditorCommandChanged();
    void defaultTerminalChanged();
    void customTerminalCommandChanged();
    void diffViewModeChanged();
    void showWhitespaceChanged();
    void tabSizeChanged();
    void startupBackendChanged();
    void avatarProviderChanged();
    void aiEnabledChanged();
    void aiProviderChanged();
    void aiApiKeyChanged();
    void aiEndpointChanged();
    void aiModelChanged();
    void aiCommitStyleChanged();
    void aiIncludeDescriptionChanged();
    void aiFollowRepoStyleChanged();
    void aiFirstRunConfiguredChanged();

private:
    void detectTools();
    void loadSettings();
    void saveSettings();

    static AppSettings *s_instance;

    QString m_defaultEditor;
    QString m_customEditorCommand;
    QString m_defaultTerminal;
    QString m_customTerminalCommand;

    QString m_diffViewMode{"unified"};
    bool m_showWhitespace{true};
    int m_tabSize{4};
    QString m_startupBackend{"real"};
    QString m_avatarProvider{"auto"};

    // AI Commit Assistant
    bool m_aiEnabled{false};
    QString m_aiProvider{"openai"};
    QString m_aiApiKey;
    QString m_aiEndpoint;
    QString m_aiModel;
    QString m_aiCommitStyle{"conventional"};
    bool m_aiIncludeDescription{true};
    bool m_aiFollowRepoStyle{true};
    bool m_aiFirstRunConfigured{false};

    QMap<QString, EditorInfo> m_knownEditors;
    QMap<QString, TerminalInfo> m_knownTerminals;
    QStringList m_detectedEditorIds;
    QStringList m_detectedTerminalIds;
};

} // namespace Cherry
