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
    Q_PROPERTY(bool isGhAvailable READ isGhAvailable CONSTANT)
    Q_PROPERTY(QStringList availableEditors READ availableEditors CONSTANT)
    Q_PROPERTY(QStringList availableTerminals READ availableTerminals CONSTANT)

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

    QMap<QString, EditorInfo> m_knownEditors;
    QMap<QString, TerminalInfo> m_knownTerminals;
    QStringList m_detectedEditorIds;
    QStringList m_detectedTerminalIds;
};

} // namespace Cherry
