#include "AppSettings.h"
#include <QStandardPaths>
#include <QProcess>
#include <QDir>
#include <QFileInfo>
#include <QUrl>
#include <QDesktopServices>

namespace Cherry {

AppSettings *AppSettings::s_instance = nullptr;

AppSettings::AppSettings(QObject *parent)
    : QObject(parent)
{
    s_instance = this;
    detectTools();
    loadSettings();
}

AppSettings* AppSettings::instance()
{
    return s_instance;
}

QString AppSettings::dataDir()
{
    QString dir = QStandardPaths::writableLocation(QStandardPaths::AppDataLocation);
    if (dir.isEmpty()) {
        dir = QDir::homePath() + "/.local/share/cherrygi";
    }
    QDir().mkpath(dir);
    return dir;
}

void AppSettings::detectTools()
{
    // Define supported external editors
    m_knownEditors["kate"] = {"kate", "Kate", "kate", "accessories-text-editor", "%f", "%d"};
    m_knownEditors["code"] = {"code", "Visual Studio Code", "code", "code", "-g %f:%l", "%d"};
    m_knownEditors["cursor"] = {"cursor", "Cursor", "cursor", "accessories-text-editor", "-g %f:%l", "%d"};
    m_knownEditors["codium"] = {"codium", "VSCodium", "codium", "accessories-text-editor", "-g %f:%l", "%d"};
    m_knownEditors["kwrite"] = {"kwrite", "KWrite", "kwrite", "accessories-text-editor", "%f", "%d"};
    m_knownEditors["subl"] = {"subl", "Sublime Text", "subl", "accessories-text-editor", "%f:%l", "%d"};
    m_knownEditors["zed"] = {"zed", "Zed", "zed", "accessories-text-editor", "%f:%l", "%d"};
    m_knownEditors["gvim"] = {"gvim", "GVim", "gvim", "accessories-text-editor", "+%l %f", "%d"};
    m_knownEditors["custom"] = {"custom", "Custom Command", "", "system-run", "%f", "%d"};

    // Detect installed editors
    m_detectedEditorIds.clear();
    for (auto it = m_knownEditors.begin(); it != m_knownEditors.end(); ++it) {
        if (it.key() == "custom") continue;
        if (!QStandardPaths::findExecutable(it.value().executable).isEmpty()) {
            m_detectedEditorIds.append(it.key());
        }
    }
    m_detectedEditorIds.append("custom");

    // Define supported terminal emulators
    m_knownTerminals["konsole"] = {"konsole", "Konsole (KDE)", "konsole", "utilities-terminal", "--workdir %d"};
    m_knownTerminals["ptyxis"] = {"ptyxis", "Ptyxis", "ptyxis", "utilities-terminal", "--working-directory=%d"};
    m_knownTerminals["alacritty"] = {"alacritty", "Alacritty", "alacritty", "utilities-terminal", "--working-directory %d"};
    m_knownTerminals["kitty"] = {"kitty", "Kitty", "kitty", "utilities-terminal", "--directory %d"};
    m_knownTerminals["foot"] = {"foot", "Foot", "foot", "utilities-terminal", "-D %d"};
    m_knownTerminals["wezterm"] = {"wezterm", "WezTerm", "wezterm", "utilities-terminal", "start --cwd %d"};
    m_knownTerminals["gnome-terminal"] = {"gnome-terminal", "GNOME Terminal", "gnome-terminal", "utilities-terminal", "--working-directory=%d"};
    m_knownTerminals["x-terminal-emulator"] = {"x-terminal-emulator", "Default X Terminal", "x-terminal-emulator", "utilities-terminal", "--working-directory %d"};
    m_knownTerminals["custom"] = {"custom", "Custom Command", "", "system-run", "%d"};

    // Detect installed terminals
    m_detectedTerminalIds.clear();
    for (auto it = m_knownTerminals.begin(); it != m_knownTerminals.end(); ++it) {
        if (it.key() == "custom") continue;
        if (!QStandardPaths::findExecutable(it.value().executable).isEmpty()) {
            m_detectedTerminalIds.append(it.key());
        }
    }
    m_detectedTerminalIds.append("custom");
}

void AppSettings::loadSettings()
{
    QSettings settings("KDE", "cherrygi");

    // Default Editor
    QString savedEditor = settings.value("General/defaultEditor").toString();
    if (!savedEditor.isEmpty() && (m_detectedEditorIds.contains(savedEditor) || savedEditor == "custom")) {
        m_defaultEditor = savedEditor;
    } else if (!m_detectedEditorIds.isEmpty() && m_detectedEditorIds.first() != "custom") {
        m_defaultEditor = m_detectedEditorIds.first();
    } else {
        m_defaultEditor = "kate";
    }
    m_customEditorCommand = settings.value("General/customEditorCommand").toString();

    // Default Terminal
    QString savedTerminal = settings.value("General/defaultTerminal").toString();
    if (!savedTerminal.isEmpty() && (m_detectedTerminalIds.contains(savedTerminal) || savedTerminal == "custom")) {
        m_defaultTerminal = savedTerminal;
    } else if (!m_detectedTerminalIds.isEmpty() && m_detectedTerminalIds.first() != "custom") {
        m_defaultTerminal = m_detectedTerminalIds.first();
    } else {
        m_defaultTerminal = "konsole";
    }
    m_customTerminalCommand = settings.value("General/customTerminalCommand").toString();

    // Appearance & Diff
    m_diffViewMode = settings.value("General/diffViewMode", "unified").toString();
    m_showWhitespace = settings.value("General/showWhitespace", true).toBool();
    m_tabSize = settings.value("General/tabSize", 4).toInt();
    m_startupBackend = settings.value("General/startupBackend", "real").toString();
    m_avatarProvider = settings.value("General/avatarProvider", "auto").toString();

    // AI Commit Assistant
    m_aiEnabled = settings.value("AI/enabled", false).toBool();
    m_aiProvider = settings.value("AI/provider", "openai").toString();
    m_aiApiKey = settings.value("AI/apiKey", "").toString();
    m_aiEndpoint = settings.value("AI/endpoint", "").toString();
    m_aiModel = settings.value("AI/model", "").toString();
    m_aiCommitStyle = settings.value("AI/commitStyle", "conventional").toString();
    m_aiIncludeDescription = settings.value("AI/includeDescription", true).toBool();
    m_aiFollowRepoStyle = settings.value("AI/followRepoStyle", true).toBool();
    m_aiFirstRunConfigured = settings.value("AI/firstRunConfigured", false).toBool();
}

void AppSettings::saveSettings()
{
    QSettings settings("KDE", "cherrygi");
    settings.setValue("General/defaultEditor", m_defaultEditor);
    settings.setValue("General/customEditorCommand", m_customEditorCommand);
    settings.setValue("General/defaultTerminal", m_defaultTerminal);
    settings.setValue("General/customTerminalCommand", m_customTerminalCommand);
    settings.setValue("General/diffViewMode", m_diffViewMode);
    settings.setValue("General/showWhitespace", m_showWhitespace);
    settings.setValue("General/tabSize", m_tabSize);
    settings.setValue("General/startupBackend", m_startupBackend);
    settings.setValue("General/avatarProvider", m_avatarProvider);

    // AI Commit Assistant
    settings.setValue("AI/enabled", m_aiEnabled);
    settings.setValue("AI/provider", m_aiProvider);
    settings.setValue("AI/apiKey", m_aiApiKey);
    settings.setValue("AI/endpoint", m_aiEndpoint);
    settings.setValue("AI/model", m_aiModel);
    settings.setValue("AI/commitStyle", m_aiCommitStyle);
    settings.setValue("AI/includeDescription", m_aiIncludeDescription);
    settings.setValue("AI/followRepoStyle", m_aiFollowRepoStyle);
    settings.setValue("AI/firstRunConfigured", m_aiFirstRunConfigured);
}

void AppSettings::setAiEnabled(bool enabled)
{
    if (m_aiEnabled == enabled) return;
    m_aiEnabled = enabled;
    saveSettings();
    emit aiEnabledChanged();
}

void AppSettings::setAiProvider(const QString &provider)
{
    if (m_aiProvider == provider) return;
    m_aiProvider = provider;
    saveSettings();
    emit aiProviderChanged();
}

void AppSettings::setAiApiKey(const QString &apiKey)
{
    if (m_aiApiKey == apiKey) return;
    m_aiApiKey = apiKey;
    saveSettings();
    emit aiApiKeyChanged();
}

void AppSettings::setAiEndpoint(const QString &endpoint)
{
    if (m_aiEndpoint == endpoint) return;
    m_aiEndpoint = endpoint;
    saveSettings();
    emit aiEndpointChanged();
}

void AppSettings::setAiModel(const QString &model)
{
    if (m_aiModel == model) return;
    m_aiModel = model;
    saveSettings();
    emit aiModelChanged();
}

void AppSettings::setAiCommitStyle(const QString &style)
{
    if (m_aiCommitStyle == style) return;
    m_aiCommitStyle = style;
    saveSettings();
    emit aiCommitStyleChanged();
}

void AppSettings::setAiIncludeDescription(bool includeDesc)
{
    if (m_aiIncludeDescription == includeDesc) return;
    m_aiIncludeDescription = includeDesc;
    saveSettings();
    emit aiIncludeDescriptionChanged();
}

void AppSettings::setAiFollowRepoStyle(bool follow)
{
    if (m_aiFollowRepoStyle == follow) return;
    m_aiFollowRepoStyle = follow;
    saveSettings();
    emit aiFollowRepoStyleChanged();
}

void AppSettings::setAiFirstRunConfigured(bool configured)
{
    if (m_aiFirstRunConfigured == configured) return;
    m_aiFirstRunConfigured = configured;
    saveSettings();
    emit aiFirstRunConfiguredChanged();
}

void AppSettings::setDefaultEditor(const QString &editorId)
{
    if (m_defaultEditor == editorId) return;
    m_defaultEditor = editorId;
    saveSettings();
    emit defaultEditorChanged();
}

void AppSettings::setCustomEditorCommand(const QString &cmd)
{
    if (m_customEditorCommand == cmd) return;
    m_customEditorCommand = cmd;
    saveSettings();
    emit customEditorCommandChanged();
}

void AppSettings::setDefaultTerminal(const QString &termId)
{
    if (m_defaultTerminal == termId) return;
    m_defaultTerminal = termId;
    saveSettings();
    emit defaultTerminalChanged();
}

void AppSettings::setCustomTerminalCommand(const QString &cmd)
{
    if (m_customTerminalCommand == cmd) return;
    m_customTerminalCommand = cmd;
    saveSettings();
    emit customTerminalCommandChanged();
}

void AppSettings::setDiffViewMode(const QString &mode)
{
    if (m_diffViewMode == mode) return;
    m_diffViewMode = mode;
    saveSettings();
    emit diffViewModeChanged();
}

void AppSettings::setShowWhitespace(bool show)
{
    if (m_showWhitespace == show) return;
    m_showWhitespace = show;
    saveSettings();
    emit showWhitespaceChanged();
}

void AppSettings::setTabSize(int size)
{
    if (m_tabSize == size) return;
    m_tabSize = size;
    saveSettings();
    emit tabSizeChanged();
}

void AppSettings::setStartupBackend(const QString &backend)
{
    if (m_startupBackend == backend) return;
    m_startupBackend = backend;
    saveSettings();
    emit startupBackendChanged();
}

void AppSettings::setAvatarProvider(const QString &provider)
{
    if (m_avatarProvider == provider) return;
    m_avatarProvider = provider;
    saveSettings();
    emit avatarProviderChanged();
}

bool AppSettings::isGhAvailable() const
{
    return !QStandardPaths::findExecutable("gh").isEmpty();
}

QStringList AppSettings::availableEditors() const
{
    QStringList result;
    for (const QString &id : m_detectedEditorIds) {
        if (m_knownEditors.contains(id)) {
            result.append(QString("%1|%2|%3").arg(id, m_knownEditors[id].displayName, m_knownEditors[id].iconName));
        }
    }
    return result;
}

QStringList AppSettings::availableTerminals() const
{
    QStringList result;
    for (const QString &id : m_detectedTerminalIds) {
        if (m_knownTerminals.contains(id)) {
            result.append(QString("%1|%2|%3").arg(id, m_knownTerminals[id].displayName, m_knownTerminals[id].iconName));
        }
    }
    return result;
}

bool AppSettings::openInEditor(const QString &filePath, const QString &repoPath, int line)
{
    QString targetFile = filePath;
    QString targetRepo = repoPath;

    if (!targetFile.isEmpty() && !QDir::isAbsolutePath(targetFile) && !targetRepo.isEmpty()) {
        targetFile = QDir(targetRepo).filePath(targetFile);
    }

    if (m_defaultEditor == "custom" && !m_customEditorCommand.trimmed().isEmpty()) {
        QString cmd = m_customEditorCommand;
        cmd.replace("%f", targetFile.isEmpty() ? targetRepo : targetFile);
        cmd.replace("%d", targetRepo);
        cmd.replace("%l", line > 0 ? QString::number(line) : "1");
        return QProcess::startDetached("/bin/sh", {"-c", cmd});
    }

    QString editorExe = m_defaultEditor;
    if (m_knownEditors.contains(m_defaultEditor)) {
        editorExe = m_knownEditors[m_defaultEditor].executable;
    }

    if (QStandardPaths::findExecutable(editorExe).isEmpty()) {
        // Fallback: try opening file via system default
        if (!targetFile.isEmpty()) {
            return QDesktopServices::openUrl(QUrl::fromLocalFile(targetFile));
        } else if (!targetRepo.isEmpty()) {
            return QDesktopServices::openUrl(QUrl::fromLocalFile(targetRepo));
        }
        return false;
    }

    QStringList args;
    if (targetFile.isEmpty()) {
        // Open repo directory
        if (!targetRepo.isEmpty()) {
            args << targetRepo;
        }
    } else {
        // Open specific file
        if (m_defaultEditor == "code" || m_defaultEditor == "cursor" || m_defaultEditor == "codium") {
            if (line > 0) {
                args << "-g" << QString("%1:%2").arg(targetFile).arg(line);
            } else {
                args << targetFile;
            }
        } else if (m_defaultEditor == "subl" || m_defaultEditor == "zed") {
            if (line > 0) {
                args << QString("%1:%2").arg(targetFile).arg(line);
            } else {
                args << targetFile;
            }
        } else if (m_defaultEditor == "kate" || m_defaultEditor == "kwrite") {
            if (line > 0) {
                args << "-l" << QString::number(line) << targetFile;
            } else {
                args << targetFile;
            }
        } else {
            args << targetFile;
        }
    }

    return QProcess::startDetached(editorExe, args);
}

bool AppSettings::openInTerminal(const QString &repoPath)
{
    QString targetDir = repoPath.isEmpty() ? QDir::currentPath() : repoPath;

    if (m_defaultTerminal == "custom" && !m_customTerminalCommand.trimmed().isEmpty()) {
        QString cmd = m_customTerminalCommand;
        cmd.replace("%d", targetDir);
        return QProcess::startDetached("/bin/sh", {"-c", cmd});
    }

    QString termExe = m_defaultTerminal;
    if (m_knownTerminals.contains(m_defaultTerminal)) {
        termExe = m_knownTerminals[m_defaultTerminal].executable;
    }

    if (QStandardPaths::findExecutable(termExe).isEmpty()) {
        // Fallback chain
        if (QProcess::startDetached("konsole", {"--workdir", targetDir})) return true;
        if (QProcess::startDetached("ptyxis", {"--working-directory", targetDir})) return true;
        if (QProcess::startDetached("x-terminal-emulator", {"--working-directory", targetDir})) return true;
        return false;
    }

    QStringList args;
    if (m_defaultTerminal == "konsole") {
        args << "--workdir" << targetDir;
    } else if (m_defaultTerminal == "ptyxis" || m_defaultTerminal == "gnome-terminal") {
        args << QString("--working-directory=%1").arg(targetDir);
    } else if (m_defaultTerminal == "alacritty") {
        args << "--working-directory" << targetDir;
    } else if (m_defaultTerminal == "kitty") {
        args << "--directory" << targetDir;
    } else if (m_defaultTerminal == "foot") {
        args << "-D" << targetDir;
    } else if (m_defaultTerminal == "wezterm") {
        args << "start" << "--cwd" << targetDir;
    } else {
        args << "--working-directory" << targetDir;
    }

    return QProcess::startDetached(termExe, args);
}

} // namespace Cherry
