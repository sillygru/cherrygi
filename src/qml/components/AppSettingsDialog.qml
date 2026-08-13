import QtQuick
import QtQuick.Controls as QQC2
import QtQuick.Layouts
import org.kde.kirigami as Kirigami
import org.kde.cherrygi
import "../style"

QQC2.Popup {
    id: root
    width: 760
    height: 600
    anchors.centerIn: parent
    padding: 0
    modal: true
    dim: true
    focus: true
    closePolicy: QQC2.Popup.CloseOnEscape | QQC2.Popup.CloseOnPressOutside

    property string currentTab: appController.settingsTab

    onAboutToShow: {
        currentTab = appController.settingsTab;
        repoUrlField.text = appController.remoteUrl;
        localNameField.text = appController.localAuthorName;
        localEmailField.text = appController.localAuthorEmail;
        globalNameField.text = appController.globalAuthorName;
        globalEmailField.text = appController.globalAuthorEmail;
        customEditorField.text = appController.customEditorCommand;
        customTermField.text = appController.customTerminalCommand;

        // Select active editor in combo
        for (var i = 0; i < editorCombo.count; ++i) {
            var item = editorCombo.model[i];
            if (item.split("|")[0] === appController.defaultEditor) {
                editorCombo.currentIndex = i;
                break;
            }
        }

        // Select active terminal in combo
        for (var j = 0; j < terminalCombo.count; ++j) {
            var titem = terminalCombo.model[j];
            if (titem.split("|")[0] === appController.defaultTerminal) {
                terminalCombo.currentIndex = j;
                break;
            }
        }
    }

    background: Rectangle {
        color: CherryStyle.surfacePopup
        border.color: CherryStyle.popupBorderColor
        border.width: 1
        radius: CherryStyle.radiusLarge

        // Multi-layer drop shadow
        Rectangle {
            anchors.fill: parent
            anchors.margins: -1
            z: -1
            color: "transparent"
            border.color: Qt.rgba(0, 0, 0, 0.4)
            radius: CherryStyle.radiusLarge + 1
        }
        Rectangle {
            anchors.fill: parent
            anchors.margins: -4
            z: -2
            color: "transparent"
            border.color: Qt.rgba(0, 0, 0, 0.2)
            radius: CherryStyle.radiusLarge + 4
        }
    }

    contentItem: RowLayout {
        anchors.fill: parent
        spacing: 0

        // ==========================================
        // LEFT SIDEBAR TABS (Kirigami Style)
        // ==========================================
        Rectangle {
            Layout.preferredWidth: 200
            Layout.fillHeight: true
            color: CherryStyle.surfaceSidebar
            radius: CherryStyle.radiusLarge

            // Only round left corners
            Rectangle {
                anchors.right: parent.right
                anchors.top: parent.top
                anchors.bottom: parent.bottom
                width: CherryStyle.radiusLarge
                color: CherryStyle.surfaceSidebar
            }

            Rectangle {
                anchors.right: parent.right
                anchors.top: parent.top
                anchors.bottom: parent.bottom
                width: 1
                color: CherryStyle.borderColor
            }

            ColumnLayout {
                anchors.fill: parent
                anchors.margins: Kirigami.Units.mediumSpacing
                spacing: 4

                RowLayout {
                    Layout.fillWidth: true
                    Layout.bottomMargin: Kirigami.Units.smallSpacing
                    spacing: Kirigami.Units.smallSpacing

                    Kirigami.Icon {
                        source: "settings-configure"
                        width: 20
                        height: 20
                        color: Kirigami.Theme.highlightColor
                    }

                    QQC2.Label {
                        text: qsTr("Settings")
                        font.bold: true
                        font.pixelSize: CherryStyle.basePixelSize + 2
                        color: Kirigami.Theme.textColor
                    }
                }

                // Tab 1: Repository & Remote
                Rectangle {
                    Layout.fillWidth: true
                    height: 38
                    radius: CherryStyle.radiusMedium
                    color: root.currentTab === "repository" ? CherryStyle.activeBackground : (repoTabMouse.containsMouse ? CherryStyle.hoverBackground : "transparent")

                    RowLayout {
                        anchors.fill: parent
                        anchors.leftMargin: Kirigami.Units.smallSpacing + 2
                        anchors.rightMargin: Kirigami.Units.smallSpacing
                        spacing: Kirigami.Units.smallSpacing

                        Kirigami.Icon {
                            source: "folder-git"
                            width: 16
                            height: 16
                            color: root.currentTab === "repository" ? Kirigami.Theme.highlightColor : Kirigami.Theme.textColor
                        }

                        QQC2.Label {
                            text: qsTr("Repository & Remote")
                            font.bold: root.currentTab === "repository"
                            color: root.currentTab === "repository" ? Kirigami.Theme.highlightColor : Kirigami.Theme.textColor
                            Layout.fillWidth: true
                            elide: Text.ElideRight
                        }
                    }

                    MouseArea {
                        id: repoTabMouse
                        anchors.fill: parent
                        hoverEnabled: true
                        cursorShape: Qt.PointingHandCursor
                        onClicked: root.currentTab = "repository"
                    }
                }

                // Tab 2: Git Settings
                Rectangle {
                    Layout.fillWidth: true
                    height: 38
                    radius: CherryStyle.radiusMedium
                    color: root.currentTab === "git" ? CherryStyle.activeBackground : (gitTabMouse.containsMouse ? CherryStyle.hoverBackground : "transparent")

                    RowLayout {
                        anchors.fill: parent
                        anchors.leftMargin: Kirigami.Units.smallSpacing + 2
                        anchors.rightMargin: Kirigami.Units.smallSpacing
                        spacing: Kirigami.Units.smallSpacing

                        Kirigami.Icon {
                            source: "vcs-branch"
                            width: 16
                            height: 16
                            color: root.currentTab === "git" ? Kirigami.Theme.highlightColor : Kirigami.Theme.textColor
                        }

                        QQC2.Label {
                            text: qsTr("Git Settings")
                            font.bold: root.currentTab === "git"
                            color: root.currentTab === "git" ? Kirigami.Theme.highlightColor : Kirigami.Theme.textColor
                            Layout.fillWidth: true
                            elide: Text.ElideRight
                        }
                    }

                    MouseArea {
                        id: gitTabMouse
                        anchors.fill: parent
                        hoverEnabled: true
                        cursorShape: Qt.PointingHandCursor
                        onClicked: root.currentTab = "git"
                    }
                }

                // Tab 3: Git Identity
                Rectangle {
                    Layout.fillWidth: true
                    height: 38
                    radius: CherryStyle.radiusMedium
                    color: root.currentTab === "identity" ? CherryStyle.activeBackground : (identTabMouse.containsMouse ? CherryStyle.hoverBackground : "transparent")

                    RowLayout {
                        anchors.fill: parent
                        anchors.leftMargin: Kirigami.Units.smallSpacing + 2
                        anchors.rightMargin: Kirigami.Units.smallSpacing
                        spacing: Kirigami.Units.smallSpacing

                        Kirigami.Icon {
                            source: "user-identity"
                            width: 16
                            height: 16
                            color: root.currentTab === "identity" ? Kirigami.Theme.highlightColor : Kirigami.Theme.textColor
                        }

                        QQC2.Label {
                            text: qsTr("Git Identity")
                            font.bold: root.currentTab === "identity"
                            color: root.currentTab === "identity" ? Kirigami.Theme.highlightColor : Kirigami.Theme.textColor
                            Layout.fillWidth: true
                            elide: Text.ElideRight
                        }
                    }

                    MouseArea {
                        id: identTabMouse
                        anchors.fill: parent
                        hoverEnabled: true
                        cursorShape: Qt.PointingHandCursor
                        onClicked: root.currentTab = "identity"
                    }
                }

                // Tab 3: Text Editor & Tools
                Rectangle {
                    Layout.fillWidth: true
                    height: 38
                    radius: CherryStyle.radiusMedium
                    color: root.currentTab === "editor" ? CherryStyle.activeBackground : (editorTabMouse.containsMouse ? CherryStyle.hoverBackground : "transparent")

                    RowLayout {
                        anchors.fill: parent
                        anchors.leftMargin: Kirigami.Units.smallSpacing + 2
                        anchors.rightMargin: Kirigami.Units.smallSpacing
                        spacing: Kirigami.Units.smallSpacing

                        Kirigami.Icon {
                            source: "accessories-text-editor"
                            width: 16
                            height: 16
                            color: root.currentTab === "editor" ? Kirigami.Theme.highlightColor : Kirigami.Theme.textColor
                        }

                        QQC2.Label {
                            text: qsTr("Editor & Terminal")
                            font.bold: root.currentTab === "editor"
                            color: root.currentTab === "editor" ? Kirigami.Theme.highlightColor : Kirigami.Theme.textColor
                            Layout.fillWidth: true
                            elide: Text.ElideRight
                        }
                    }

                    MouseArea {
                        id: editorTabMouse
                        anchors.fill: parent
                        hoverEnabled: true
                        cursorShape: Qt.PointingHandCursor
                        onClicked: root.currentTab = "editor"
                    }
                }

                // Tab 5: Appearance & Diff
                Rectangle {
                    Layout.fillWidth: true
                    height: 38
                    radius: CherryStyle.radiusMedium
                    color: root.currentTab === "appearance" ? CherryStyle.activeBackground : (appTabMouse.containsMouse ? CherryStyle.hoverBackground : "transparent")

                    RowLayout {
                        anchors.fill: parent
                        anchors.leftMargin: Kirigami.Units.smallSpacing + 2
                        anchors.rightMargin: Kirigami.Units.smallSpacing
                        spacing: Kirigami.Units.smallSpacing

                        Kirigami.Icon {
                            source: "preferences-desktop-theme"
                            width: 16
                            height: 16
                            color: root.currentTab === "appearance" ? Kirigami.Theme.highlightColor : Kirigami.Theme.textColor
                        }

                        QQC2.Label {
                            text: qsTr("Appearance & Diff")
                            font.bold: root.currentTab === "appearance"
                            color: root.currentTab === "appearance" ? Kirigami.Theme.highlightColor : Kirigami.Theme.textColor
                            Layout.fillWidth: true
                            elide: Text.ElideRight
                        }
                    }

                    MouseArea {
                        id: appTabMouse
                        anchors.fill: parent
                        hoverEnabled: true
                        cursorShape: Qt.PointingHandCursor
                        onClicked: root.currentTab = "appearance"
                    }
                }

                Item { Layout.fillHeight: true }
            }
        }

        // ==========================================
        // RIGHT CONTENT PANEL
        // ==========================================
        ColumnLayout {
            Layout.fillWidth: true
            Layout.fillHeight: true
            Layout.margins: Kirigami.Units.largeSpacing
            spacing: Kirigami.Units.mediumSpacing

            // Header Row of Right Panel
            RowLayout {
                Layout.fillWidth: true
                spacing: Kirigami.Units.smallSpacing

                QQC2.Label {
                    text: {
                        if (root.currentTab === "repository") return qsTr("Repository & Remote Origin");
                        if (root.currentTab === "git") return qsTr("Git Settings");
                        if (root.currentTab === "identity") return qsTr("Git Identity");
                        if (root.currentTab === "editor") return qsTr("External Editor & Terminal");
                        return qsTr("Appearance & Diff Settings");
                    }
                    font.bold: true
                    font.pixelSize: CherryStyle.basePixelSize + 2
                    color: Kirigami.Theme.textColor
                    Layout.fillWidth: true
                }

                QQC2.ToolButton {
                    icon.name: "window-close"
                    icon.width: 14
                    icon.height: 14
                    onClicked: root.close()
                }
            }

            // Separator
            Rectangle {
                Layout.fillWidth: true
                height: 1
                color: CherryStyle.subtleBorderColor
            }

            // Tab View Body (ScrollView)
            QQC2.ScrollView {
                Layout.fillWidth: true
                Layout.fillHeight: true
                clip: true

                // ==========================================
                // 1. REPOSITORY & REMOTE TAB
                // ==========================================
                ColumnLayout {
                    width: parent.width
                    visible: root.currentTab === "repository"
                    spacing: Kirigami.Units.mediumSpacing

                    // Active Repository info banner
                    Rectangle {
                        Layout.fillWidth: true
                        implicitHeight: repoInfoCol.implicitHeight + 16
                        radius: CherryStyle.radiusMedium
                        color: CherryStyle.surfaceCard
                        border.color: CherryStyle.borderColor
                        border.width: 1

                        ColumnLayout {
                            id: repoInfoCol
                            anchors.fill: parent
                            anchors.margins: Kirigami.Units.mediumSpacing
                            spacing: 4

                            RowLayout {
                                Layout.fillWidth: true
                                spacing: Kirigami.Units.smallSpacing

                                QQC2.Label {
                                    text: qsTr("Repository:")
                                    font.bold: true
                                    color: Kirigami.Theme.textColor
                                }

                                QQC2.Label {
                                    text: appController.currentRepoName
                                    font.bold: true
                                    color: Kirigami.Theme.highlightColor
                                    Layout.fillWidth: true
                                }
                            }

                            RowLayout {
                                Layout.fillWidth: true
                                spacing: Kirigami.Units.smallSpacing

                                QQC2.Label {
                                    text: qsTr("Path:")
                                    font.pixelSize: CherryStyle.smallFont.pixelSize
                                    color: Kirigami.Theme.disabledTextColor
                                }

                                QQC2.Label {
                                    text: appController.currentRepoPath
                                    font.pixelSize: CherryStyle.smallFont.pixelSize
                                    color: Kirigami.Theme.disabledTextColor
                                    elide: Text.ElideMiddle
                                    Layout.fillWidth: true
                                }

                                QQC2.ToolButton {
                                    icon.name: "folder"
                                    icon.width: 14
                                    icon.height: 14
                                    QQC2.ToolTip.text: qsTr("Open folder in File Manager")
                                    QQC2.ToolTip.visible: hovered
                                    onClicked: appController.openInFileManager(appController.currentRepoPath)
                                }
                            }
                        }
                    }

                    // Remote Origin URL Section
                    ColumnLayout {
                        Layout.fillWidth: true
                        spacing: Kirigami.Units.smallSpacing

                        RowLayout {
                            Layout.fillWidth: true
                            spacing: Kirigami.Units.smallSpacing

                            QQC2.Label {
                                text: qsTr("Primary Git Remote (origin)")
                                font.bold: true
                                color: Kirigami.Theme.textColor
                            }

                            Rectangle {
                                implicitWidth: remoteBadge.implicitWidth + 8
                                implicitHeight: 18
                                radius: 9
                                color: appController.hasRemote ? Qt.rgba(0.2, 0.7, 0.3, 0.15) : Qt.rgba(0.9, 0.6, 0.1, 0.15)
                                border.color: appController.hasRemote ? "#2ec27e" : "#e5a50a"
                                border.width: 1

                                QQC2.Label {
                                    id: remoteBadge
                                    anchors.centerIn: parent
                                    text: appController.hasRemote ? qsTr("Configured") : qsTr("No Remote")
                                    font.pixelSize: CherryStyle.smallFont.pixelSize - 1
                                    font.bold: true
                                    color: appController.hasRemote ? "#2ec27e" : "#e5a50a"
                                }
                            }
                        }

                        QQC2.TextField {
                            id: repoUrlField
                            Layout.fillWidth: true
                            placeholderText: qsTr("e.g. https://github.com/owner/repository.git or git@github.com:owner/repository.git")
                            background: Rectangle {
                                color: CherryStyle.inputBackground
                                border.color: repoUrlField.activeFocus ? Kirigami.Theme.highlightColor : CherryStyle.borderColor
                                border.width: repoUrlField.activeFocus ? 2 : 1
                                radius: CherryStyle.radiusSmall
                            }
                        }

                        RowLayout {
                            Layout.fillWidth: true
                            spacing: Kirigami.Units.smallSpacing

                            QQC2.Button {
                                text: qsTr("Save Remote URL")
                                icon.name: "document-save"
                                highlighted: true
                                enabled: repoUrlField.text.trim().length > 0
                                onClicked: appController.saveRemoteUrl(repoUrlField.text.trim())
                            }

                            QQC2.Button {
                                text: qsTr("Publish with gh...")
                                icon.name: "cloud-upload"
                                onClicked: {
                                    root.close();
                                    appController.showPublishDialog();
                                }
                            }

                            QQC2.Button {
                                text: qsTr("Remove Remote")
                                icon.name: "edit-delete"
                                visible: appController.hasRemote
                                onClicked: {
                                    if (appController.removeRemoteUrl()) {
                                        repoUrlField.text = "";
                                    }
                                }
                            }
                        }
                    }

                    // Local Author Override Section
                    ColumnLayout {
                        Layout.fillWidth: true
                        Layout.topMargin: Kirigami.Units.smallSpacing
                        spacing: Kirigami.Units.smallSpacing

                        QQC2.Label {
                            text: qsTr("Repository Author Override (Local)")
                            font.bold: true
                            color: Kirigami.Theme.textColor
                        }

                        QQC2.Label {
                            text: qsTr("Override your global Git name and email specifically for this repository.")
                            font.pixelSize: CherryStyle.smallFont.pixelSize - 1
                            color: Kirigami.Theme.disabledTextColor
                        }

                        RowLayout {
                            Layout.fillWidth: true
                            spacing: Kirigami.Units.smallSpacing

                            QQC2.TextField {
                                id: localNameField
                                Layout.fillWidth: true
                                placeholderText: qsTr("Local Author Name")
                                background: Rectangle {
                                    color: CherryStyle.inputBackground
                                    border.color: localNameField.activeFocus ? Kirigami.Theme.highlightColor : CherryStyle.borderColor
                                    border.width: localNameField.activeFocus ? 2 : 1
                                    radius: CherryStyle.radiusSmall
                                }
                            }

                            QQC2.TextField {
                                id: localEmailField
                                Layout.fillWidth: true
                                placeholderText: qsTr("local@example.com")
                                background: Rectangle {
                                    color: CherryStyle.inputBackground
                                    border.color: localEmailField.activeFocus ? Kirigami.Theme.highlightColor : CherryStyle.borderColor
                                    border.width: localEmailField.activeFocus ? 2 : 1
                                    radius: CherryStyle.radiusSmall
                                }
                            }

                            QQC2.Button {
                                text: qsTr("Save")
                                icon.name: "document-save"
                                onClicked: appController.saveAuthorInfo(localNameField.text.trim(), localEmailField.text.trim(), false)
                            }
                        }
                    }
                }

                // ==========================================
                // 2. GIT SETTINGS TAB
                // ==========================================
                ColumnLayout {
                    width: parent.width
                    visible: root.currentTab === "git"
                    spacing: Kirigami.Units.mediumSpacing

                    QQC2.Label {
                        text: qsTr("File Metadata Changes")
                        font.bold: true
                        color: Kirigami.Theme.textColor
                    }

                    QQC2.Label {
                        text: qsTr("Control whether Git reports executable-bit and file permission changes as repository changes.")
                        font.pixelSize: CherryStyle.smallFont.pixelSize
                        color: Kirigami.Theme.disabledTextColor
                        wrapMode: Text.Wrap
                        Layout.fillWidth: true
                    }

                    Rectangle {
                        Layout.fillWidth: true
                        implicitHeight: localFileModeCol.implicitHeight + 24
                        radius: CherryStyle.radiusMedium
                        color: CherryStyle.surfaceCard
                        border.color: CherryStyle.borderColor
                        border.width: 1

                        ColumnLayout {
                            id: localFileModeCol
                            anchors.fill: parent
                            anchors.margins: Kirigami.Units.mediumSpacing
                            spacing: Kirigami.Units.smallSpacing

                            QQC2.CheckBox {
                                text: qsTr("Ignore file metadata changes in this repository")
                                checked: appController.ignoreFileModeChanges
                                onClicked: appController.setIgnoreFileModeChanges(checked, false)
                            }

                            QQC2.Label {
                                text: qsTr("Writes core.filemode=false to this repository's .git/config.")
                                font.pixelSize: CherryStyle.smallFont.pixelSize - 1
                                color: Kirigami.Theme.disabledTextColor
                                wrapMode: Text.Wrap
                                Layout.fillWidth: true
                            }
                        }
                    }

                    Rectangle {
                        Layout.fillWidth: true
                        implicitHeight: globalFileModeCol.implicitHeight + 24
                        radius: CherryStyle.radiusMedium
                        color: CherryStyle.surfaceCard
                        border.color: CherryStyle.borderColor
                        border.width: 1

                        ColumnLayout {
                            id: globalFileModeCol
                            anchors.fill: parent
                            anchors.margins: Kirigami.Units.mediumSpacing
                            spacing: Kirigami.Units.smallSpacing

                            QQC2.CheckBox {
                                text: qsTr("Ignore file metadata changes globally")
                                checked: appController.globalIgnoreFileModeChanges
                                onClicked: appController.setIgnoreFileModeChanges(checked, true)
                            }

                            QQC2.Label {
                                text: qsTr("Writes core.filemode=false to your global Git configuration and applies to repositories without a local override.")
                                font.pixelSize: CherryStyle.smallFont.pixelSize - 1
                                color: Kirigami.Theme.disabledTextColor
                                wrapMode: Text.Wrap
                                Layout.fillWidth: true
                            }
                        }
                    }

                    QQC2.Label {
                        text: qsTr("The repository setting takes precedence over the global setting.")
                        font.pixelSize: CherryStyle.smallFont.pixelSize
                        color: Kirigami.Theme.disabledTextColor
                        wrapMode: Text.Wrap
                        Layout.fillWidth: true
                    }
                }

                // ==========================================
                // 3. GIT IDENTITY TAB
                // ==========================================
                ColumnLayout {
                    width: parent.width
                    visible: root.currentTab === "identity"
                    spacing: Kirigami.Units.mediumSpacing

                    QQC2.Label {
                        text: qsTr("Global Git User Profile")
                        font.bold: true
                        font.pixelSize: CherryStyle.basePixelSize
                        color: Kirigami.Theme.textColor
                    }

                    QQC2.Label {
                        text: qsTr("These credentials are used by default for all Git commits across all repositories on your system (git config --global).")
                        font.pixelSize: CherryStyle.smallFont.pixelSize
                        color: Kirigami.Theme.disabledTextColor
                        wrapMode: Text.Wrap
                        Layout.fillWidth: true
                    }

                    ColumnLayout {
                        Layout.fillWidth: true
                        spacing: 4

                        QQC2.Label {
                            text: qsTr("Global Name (user.name)")
                            font.bold: true
                            font.pixelSize: CherryStyle.smallFont.pixelSize
                            color: Kirigami.Theme.textColor
                        }

                        QQC2.TextField {
                            id: globalNameField
                            Layout.fillWidth: true
                            placeholderText: qsTr("Your Full Name")
                            background: Rectangle {
                                color: CherryStyle.inputBackground
                                border.color: globalNameField.activeFocus ? Kirigami.Theme.highlightColor : CherryStyle.borderColor
                                border.width: globalNameField.activeFocus ? 2 : 1
                                radius: CherryStyle.radiusSmall
                            }
                        }
                    }

                    ColumnLayout {
                        Layout.fillWidth: true
                        spacing: 4

                        QQC2.Label {
                            text: qsTr("Global Email (user.email)")
                            font.bold: true
                            font.pixelSize: CherryStyle.smallFont.pixelSize
                            color: Kirigami.Theme.textColor
                        }

                        QQC2.TextField {
                            id: globalEmailField
                            Layout.fillWidth: true
                            placeholderText: qsTr("user@example.com")
                            background: Rectangle {
                                color: CherryStyle.inputBackground
                                border.color: globalEmailField.activeFocus ? Kirigami.Theme.highlightColor : CherryStyle.borderColor
                                border.width: globalEmailField.activeFocus ? 2 : 1
                                radius: CherryStyle.radiusSmall
                            }
                        }
                    }

                    QQC2.Button {
                        text: qsTr("Save Global Identity")
                        icon.name: "document-save"
                        highlighted: true
                        enabled: globalNameField.text.trim().length > 0 && globalEmailField.text.trim().length > 0
                        onClicked: appController.saveAuthorInfo(globalNameField.text.trim(), globalEmailField.text.trim(), true)
                    }
                }

                // ==========================================
                // 3. EDITOR & TERMINAL TAB
                // ==========================================
                ColumnLayout {
                    width: parent.width
                    visible: root.currentTab === "editor"
                    spacing: Kirigami.Units.mediumSpacing

                    // Editor section
                    ColumnLayout {
                        Layout.fillWidth: true
                        spacing: Kirigami.Units.smallSpacing

                        QQC2.Label {
                            text: qsTr("Default Text Editor / IDE")
                            font.bold: true
                            color: Kirigami.Theme.textColor
                        }

                        QQC2.ComboBox {
                            id: editorCombo
                            Layout.fillWidth: true
                            model: appController.availableEditors
                            textRole: ""
                            displayText: {
                                if (currentIndex >= 0 && currentIndex < count) {
                                    var parts = model[currentIndex].split("|");
                                    return parts.length > 1 ? parts[1] : parts[0];
                                }
                                return qsTr("Select Editor");
                            }
                            delegate: QQC2.ItemDelegate {
                                width: editorCombo.width
                                text: {
                                    var p = modelData.split("|");
                                    return p.length > 1 ? p[1] : p[0];
                                }
                                icon.name: {
                                    var p = modelData.split("|");
                                    return p.length > 2 ? p[2] : "accessories-text-editor";
                                }
                                highlighted: editorCombo.currentIndex === index
                            }
                        }

                        // Custom command field if custom is selected
                        ColumnLayout {
                            Layout.fillWidth: true
                            visible: {
                                if (editorCombo.currentIndex >= 0 && editorCombo.currentIndex < editorCombo.count) {
                                    return editorCombo.model[editorCombo.currentIndex].split("|")[0] === "custom";
                                }
                                return false;
                            }
                            spacing: 2

                            QQC2.Label {
                                text: qsTr("Custom Editor Command (%f = file, %l = line, %d = repo dir):")
                                font.pixelSize: CherryStyle.smallFont.pixelSize - 1
                                color: Kirigami.Theme.disabledTextColor
                            }

                            QQC2.TextField {
                                id: customEditorField
                                Layout.fillWidth: true
                                placeholderText: qsTr("e.g. gedit %f or /opt/editor/bin %f:%l")
                                background: Rectangle {
                                    color: CherryStyle.inputBackground
                                    border.color: customEditorField.activeFocus ? Kirigami.Theme.highlightColor : CherryStyle.borderColor
                                    border.width: customEditorField.activeFocus ? 2 : 1
                                    radius: CherryStyle.radiusSmall
                                }
                            }
                        }

                        RowLayout {
                            Layout.fillWidth: true
                            spacing: Kirigami.Units.smallSpacing

                            QQC2.Button {
                                text: qsTr("Save Editor")
                                icon.name: "document-save"
                                onClicked: {
                                    var edId = editorCombo.model[editorCombo.currentIndex].split("|")[0];
                                    appController.saveEditorSettings(edId, customEditorField.text.trim());
                                }
                            }

                            QQC2.Button {
                                text: qsTr("Open Repo in Editor")
                                icon.name: "accessories-text-editor"
                                onClicked: appController.openInEditor()
                            }
                        }
                    }

                    // Terminal section
                    ColumnLayout {
                        Layout.fillWidth: true
                        Layout.topMargin: Kirigami.Units.smallSpacing
                        spacing: Kirigami.Units.smallSpacing

                        QQC2.Label {
                            text: qsTr("Default Terminal Emulator")
                            font.bold: true
                            color: Kirigami.Theme.textColor
                        }

                        QQC2.ComboBox {
                            id: terminalCombo
                            Layout.fillWidth: true
                            model: appController.availableTerminals
                            displayText: {
                                if (currentIndex >= 0 && currentIndex < count) {
                                    var parts = model[currentIndex].split("|");
                                    return parts.length > 1 ? parts[1] : parts[0];
                                }
                                return qsTr("Select Terminal");
                            }
                            delegate: QQC2.ItemDelegate {
                                width: terminalCombo.width
                                text: {
                                    var p = modelData.split("|");
                                    return p.length > 1 ? p[1] : p[0];
                                }
                                icon.name: "utilities-terminal"
                                highlighted: terminalCombo.currentIndex === index
                            }
                        }

                        // Custom command field if custom is selected
                        ColumnLayout {
                            Layout.fillWidth: true
                            visible: {
                                if (terminalCombo.currentIndex >= 0 && terminalCombo.currentIndex < terminalCombo.count) {
                                    return terminalCombo.model[terminalCombo.currentIndex].split("|")[0] === "custom";
                                }
                                return false;
                            }
                            spacing: 2

                            QQC2.Label {
                                text: qsTr("Custom Terminal Command (%d = directory):")
                                font.pixelSize: CherryStyle.smallFont.pixelSize - 1
                                color: Kirigami.Theme.disabledTextColor
                            }

                            QQC2.TextField {
                                id: customTermField
                                Layout.fillWidth: true
                                placeholderText: qsTr("e.g. alacritty --working-directory %d")
                                background: Rectangle {
                                    color: CherryStyle.inputBackground
                                    border.color: customTermField.activeFocus ? Kirigami.Theme.highlightColor : CherryStyle.borderColor
                                    border.width: customTermField.activeFocus ? 2 : 1
                                    radius: CherryStyle.radiusSmall
                                }
                            }
                        }

                        QQC2.Button {
                            text: qsTr("Save Terminal")
                            icon.name: "document-save"
                            onClicked: {
                                var termId = terminalCombo.model[terminalCombo.currentIndex].split("|")[0];
                                appController.saveTerminalSettings(termId, customTermField.text.trim());
                            }
                        }
                    }
                }

                // ==========================================
                // 4. APPEARANCE & DIFF TAB
                // ==========================================
                ColumnLayout {
                    width: parent.width
                    visible: root.currentTab === "appearance"
                    spacing: Kirigami.Units.mediumSpacing

                    QQC2.Label {
                        text: qsTr("Diff Display Preferences")
                        font.bold: true
                        color: Kirigami.Theme.textColor
                    }

                    RowLayout {
                        Layout.fillWidth: true
                        spacing: Kirigami.Units.mediumSpacing

                        QQC2.Label {
                            text: qsTr("Default Diff View:")
                            color: Kirigami.Theme.textColor
                        }

                        QQC2.RadioButton {
                            text: qsTr("Unified Diff")
                            checked: appController.diffViewMode === "unified"
                            onClicked: appController.diffViewMode = "unified"
                        }

                        QQC2.RadioButton {
                            text: qsTr("Split (Side-by-Side)")
                            checked: appController.diffViewMode === "split"
                            onClicked: appController.diffViewMode = "split"
                        }
                    }

                    QQC2.CheckBox {
                        text: qsTr("Show whitespace changes by default")
                        checked: appController.showWhitespace
                        onClicked: appController.showWhitespace = checked
                    }

                    Rectangle {
                        Layout.fillWidth: true
                        height: 1
                        color: CherryStyle.subtleBorderColor
                    }

                    QQC2.Label {
                        text: qsTr("Startup Backend")
                        font.bold: true
                        color: Kirigami.Theme.textColor
                    }

                    QQC2.Label {
                        text: qsTr("Configure how cherrygi starts up by default:")
                        font.pixelSize: CherryStyle.smallFont.pixelSize
                        color: Kirigami.Theme.disabledTextColor
                    }

                    RowLayout {
                        Layout.fillWidth: true
                        spacing: Kirigami.Units.smallSpacing

                        QQC2.Button {
                            text: qsTr("Use Real Git Backend")
                            icon.name: "folder-git"
                            highlighted: appController.backendMode === "real"
                            onClicked: {
                                appController.setBackendMode("real");
                                appController.showToast("Default backend: Real Git");
                            }
                        }

                        QQC2.Button {
                            text: qsTr("Use Mock Sandbox")
                            icon.name: "system-run"
                            highlighted: appController.backendMode === "mock"
                            onClicked: {
                                appController.setBackendMode("mock");
                                appController.showToast("Default backend: Mock Demo");
                            }
                        }

                        QQC2.Button {
                            text: qsTr("Show Startup Choice...")
                            icon.name: "view-refresh"
                            onClicked: {
                                root.close();
                                appController.showBackendSelectionDialog();
                            }
                        }
                    }
                }
            }

            // Bottom Done button
            RowLayout {
                Layout.fillWidth: true
                spacing: Kirigami.Units.smallSpacing

                Item { Layout.fillWidth: true }

                QQC2.Button {
                    text: qsTr("Done")
                    highlighted: true
                    onClicked: root.close()
                }
            }
        }
    }
}
