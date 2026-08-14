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
    property bool remoteFieldsUpdating: false
    property string pendingDiffViewMode: "unified"
    property string pendingBackendMode: "real"

    function updateRemoteUrlFromParts() {
        if (remoteFieldsUpdating || remotePresetCombo.currentIndex < 0) return;
        var preset = remotePresetCombo.model[remotePresetCombo.currentIndex].split("|");
        if (preset[0] === "custom") return;

        var owner = remoteOwnerField.text.trim();
        var repository = remoteRepositoryField.text.trim();
        while (owner.startsWith("/")) owner = owner.substring(1);
        while (owner.endsWith("/")) owner = owner.substring(0, owner.length - 1);
        while (repository.startsWith("/")) repository = repository.substring(1);
        while (repository.endsWith("/")) repository = repository.substring(0, repository.length - 1);
        if (owner.length === 0 || repository.length === 0) return;
        repoUrlField.text = preset[2] + owner + "/" + repository;
    }

    function populateRemoteParts(url) {
        var cleanUrl = url.trim();
        if (cleanUrl.length === 0) {
            remoteOwnerField.text = "";
            remoteRepositoryField.text = "";
            return;
        }

        if (cleanUrl.toLowerCase().endsWith(".git")) {
            cleanUrl = cleanUrl.substring(0, cleanUrl.length - 4);
        }

        var path = "";
        var schemeIndex = cleanUrl.indexOf("://");
        if (schemeIndex >= 0) {
            var pathStart = cleanUrl.indexOf("/", schemeIndex + 3);
            if (pathStart >= 0) path = cleanUrl.substring(pathStart + 1);
        } else {
            var separator = cleanUrl.indexOf(":");
            if (separator >= 0) path = cleanUrl.substring(separator + 1);
        }

        var pieces = path.split("/").filter(function(piece) { return piece.length > 0; });
        if (pieces.length >= 2) {
            remoteOwnerField.text = pieces.slice(0, pieces.length - 1).join("/");
            remoteRepositoryField.text = pieces[pieces.length - 1];
        } else {
            remoteOwnerField.text = "";
            remoteRepositoryField.text = "";
        }
    }

    function selectRemotePreset() {
        var host = appController.remoteHost.toLowerCase();
        remotePresetCombo.currentIndex = 3;
        for (var i = 0; i < remotePresetCombo.count; ++i) {
            var preset = remotePresetCombo.model[i].split("|");
            var presetHost = preset[2].replace("https://", "").replace("http://", "");
            while (presetHost.endsWith("/")) presetHost = presetHost.substring(0, presetHost.length - 1);
            if (host !== "" && host === presetHost) {
                remotePresetCombo.currentIndex = i;
                break;
            }
        }
        if (!appController.hasRemote) remotePresetCombo.currentIndex = 0;
    }

    function saveSettingsAndClose() {
        var url = repoUrlField.text.trim();
        if (url.length > 0 && url !== appController.remoteUrl) {
            if (!appController.saveRemoteUrl(url)) return;
        } else if (url.length === 0 && appController.hasRemote) {
            if (!appController.removeRemoteUrl()) return;
        }

        var localName = localNameField.text.trim();
        var localEmail = localEmailField.text.trim();
        if (localName !== appController.localAuthorName || localEmail !== appController.localAuthorEmail) {
            appController.saveAuthorInfo(localName, localEmail, false);
        }

        if (localFileModeCheckBox.checked !== appController.ignoreFileModeChanges) {
            appController.setIgnoreFileModeChanges(localFileModeCheckBox.checked, false);
        }
        if (globalFileModeCheckBox.checked !== appController.globalIgnoreFileModeChanges) {
            appController.setIgnoreFileModeChanges(globalFileModeCheckBox.checked, true);
        }

        var globalName = globalNameField.text.trim();
        var globalEmail = globalEmailField.text.trim();
        if (globalName !== appController.globalAuthorName || globalEmail !== appController.globalAuthorEmail) {
            appController.saveAuthorInfo(globalName, globalEmail, true);
        }

        if (editorCombo.currentIndex >= 0 && editorCombo.currentIndex < editorCombo.count) {
            var editorId = editorCombo.model[editorCombo.currentIndex].split("|")[0];
            var customEd = customEditorField.text.trim();
            if (editorId !== appController.defaultEditor || customEd !== appController.customEditorCommand) {
                appController.saveEditorSettings(editorId, customEd);
            }
        }
        if (terminalCombo.currentIndex >= 0 && terminalCombo.currentIndex < terminalCombo.count) {
            var terminalId = terminalCombo.model[terminalCombo.currentIndex].split("|")[0];
            var customTerm = customTermField.text.trim();
            if (terminalId !== appController.defaultTerminal || customTerm !== appController.customTerminalCommand) {
                appController.saveTerminalSettings(terminalId, customTerm);
            }
        }

        if (avatarProviderCombo.currentIndex >= 0) {
            var selectedProv = avatarProviderCombo.model[avatarProviderCombo.currentIndex].value;
            if (selectedProv !== appController.avatarProvider) {
                appController.saveAvatarSettings(selectedProv);
            }
        }

        if (pendingDiffViewMode !== appController.diffViewMode) {
            appController.diffViewMode = pendingDiffViewMode;
        }
        if (showWhitespaceCheckBox.checked !== appController.showWhitespace) {
            appController.showWhitespace = showWhitespaceCheckBox.checked;
        }

        if (pendingBackendMode !== appController.backendMode) {
            appController.setBackendMode(pendingBackendMode);
        }

        root.close();
    }

    onAboutToShow: {
        remoteFieldsUpdating = true;
        currentTab = appController.settingsTab;
        repoUrlField.text = appController.remoteUrl;
        selectRemotePreset();
        populateRemoteParts(appController.remoteUrl);
        remoteFieldsUpdating = false;
        localNameField.text = appController.localAuthorName;
        localEmailField.text = appController.localAuthorEmail;
        globalNameField.text = appController.globalAuthorName;
        globalEmailField.text = appController.globalAuthorEmail;
        customEditorField.text = appController.customEditorCommand;
        customTermField.text = appController.customTerminalCommand;

        localFileModeCheckBox.checked = appController.ignoreFileModeChanges;
        globalFileModeCheckBox.checked = appController.globalIgnoreFileModeChanges;

        pendingDiffViewMode = appController.diffViewMode;
        showWhitespaceCheckBox.checked = appController.showWhitespace;
        pendingBackendMode = appController.backendMode;

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

        // Select active avatar provider in combo
        avatarProviderCombo.currentIndex = 0;
        for (var k = 0; k < avatarProviderCombo.model.length; ++k) {
            if (avatarProviderCombo.model[k].value === appController.avatarProvider) {
                avatarProviderCombo.currentIndex = k;
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
                        color: CherryStyle.accentColor
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
                            color: root.currentTab === "repository" ? CherryStyle.accentColor : Kirigami.Theme.textColor
                        }

                        QQC2.Label {
                            text: qsTr("Repository & Remote")
                            font.bold: root.currentTab === "repository"
                            color: root.currentTab === "repository" ? CherryStyle.accentColor : Kirigami.Theme.textColor
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
                            color: root.currentTab === "git" ? CherryStyle.accentColor : Kirigami.Theme.textColor
                        }

                        QQC2.Label {
                            text: qsTr("Git Settings")
                            font.bold: root.currentTab === "git"
                            color: root.currentTab === "git" ? CherryStyle.accentColor : Kirigami.Theme.textColor
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
                            color: root.currentTab === "identity" ? CherryStyle.accentColor : Kirigami.Theme.textColor
                        }

                        QQC2.Label {
                            text: qsTr("Git Identity")
                            font.bold: root.currentTab === "identity"
                            color: root.currentTab === "identity" ? CherryStyle.accentColor : Kirigami.Theme.textColor
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
                            color: root.currentTab === "editor" ? CherryStyle.accentColor : Kirigami.Theme.textColor
                        }

                        QQC2.Label {
                            text: qsTr("Editor & Terminal")
                            font.bold: root.currentTab === "editor"
                            color: root.currentTab === "editor" ? CherryStyle.accentColor : Kirigami.Theme.textColor
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
                            color: root.currentTab === "appearance" ? CherryStyle.accentColor : Kirigami.Theme.textColor
                        }

                        QQC2.Label {
                            text: qsTr("Appearance & Diff")
                            font.bold: root.currentTab === "appearance"
                            color: root.currentTab === "appearance" ? CherryStyle.accentColor : Kirigami.Theme.textColor
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
                                    color: CherryStyle.accentColor
                                    Layout.fillWidth: true
                                }
                            }

                            RowLayout {
                                Layout.fillWidth: true
                                spacing: Kirigami.Units.smallSpacing

                                QQC2.Label {
                                    text: qsTr("Path:")
                                    font.pixelSize: CherryStyle.smallFont.pixelSize
                                    color: CherryStyle.secondaryTextColor
                                }

                                QQC2.Label {
                                    text: appController.currentRepoPath
                                    font.pixelSize: CherryStyle.smallFont.pixelSize
                                    color: CherryStyle.secondaryTextColor
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
                                text: qsTr("Configured Git Remote")
                                font.bold: true
                                color: Kirigami.Theme.textColor
                            }

                            Rectangle {
                                implicitWidth: remoteBadge.implicitWidth + 8
                                implicitHeight: 18
                                radius: 9
                                color: appController.hasRemote ? CherryStyle.additionBg : CherryStyle.warningBg
                                border.color: appController.hasRemote ? CherryStyle.additionColor : CherryStyle.warningColor
                                border.width: 1

                                QQC2.Label {
                                    id: remoteBadge
                                    anchors.centerIn: parent
                                    text: appController.hasRemote ? qsTr("Configured") : qsTr("No Remote")
                                    font.pixelSize: CherryStyle.smallFont.pixelSize - 1
                                    font.bold: true
                                    color: appController.hasRemote ? CherryStyle.additionColor : CherryStyle.warningColor
                                }
                            }
                        }

                        RowLayout {
                            Layout.fillWidth: true
                            spacing: Kirigami.Units.smallSpacing

                            QQC2.Label {
                                text: qsTr("Quick setup:")
                                font.pixelSize: CherryStyle.smallFont.pixelSize
                                color: CherryStyle.secondaryTextColor
                            }

                            QQC2.ComboBox {
                                id: remotePresetCombo
                                Layout.preferredWidth: 150
                                model: [
                                    "github|GitHub|https://github.com/",
                                    "gitlab|GitLab|https://gitlab.com/",
                                    "invent|KDE Invent|https://invent.kde.org/",
                                    "custom|Custom host|"
                                ]
                                displayText: currentIndex >= 0 ? model[currentIndex].split("|")[1] : qsTr("Select host")
                                delegate: QQC2.ItemDelegate {
                                    width: remotePresetCombo.width
                                    text: modelData.split("|")[1]
                                    highlighted: remotePresetCombo.currentIndex === index
                                }
                                onActivated: {
                                    if (currentIndex < 3) {
                                        remoteOwnerField.forceActiveFocus();
                                        root.updateRemoteUrlFromParts();
                                    }
                                }
                            }

                            QQC2.TextField {
                                id: remoteOwnerField
                                Layout.fillWidth: true
                                placeholderText: qsTr("Owner or group")
                                enabled: remotePresetCombo.currentIndex < 3
                                background: Rectangle {
                                    color: CherryStyle.inputBackground
                                    border.color: remoteOwnerField.activeFocus ? CherryStyle.accentColor : CherryStyle.borderColor
                                    border.width: remoteOwnerField.activeFocus ? 2 : 1
                                    radius: CherryStyle.radiusSmall
                                }
                                onTextChanged: root.updateRemoteUrlFromParts()
                            }

                            QQC2.TextField {
                                id: remoteRepositoryField
                                Layout.fillWidth: true
                                placeholderText: qsTr("Repository (.git optional)")
                                enabled: remotePresetCombo.currentIndex < 3
                                background: Rectangle {
                                    color: CherryStyle.inputBackground
                                    border.color: remoteRepositoryField.activeFocus ? CherryStyle.accentColor : CherryStyle.borderColor
                                    border.width: remoteRepositoryField.activeFocus ? 2 : 1
                                    radius: CherryStyle.radiusSmall
                                }
                                onTextChanged: root.updateRemoteUrlFromParts()
                            }
                        }

                        QQC2.Label {
                            text: qsTr("Choose a host, then enter the owner/group and repository name. The URL below is updated automatically; .git is optional.")
                            font.pixelSize: CherryStyle.smallFont.pixelSize - 1
                            color: CherryStyle.secondaryTextColor
                            wrapMode: Text.Wrap
                            Layout.fillWidth: true
                        }

                        RowLayout {
                            Layout.fillWidth: true
                            spacing: Kirigami.Units.smallSpacing

                            QQC2.Label {
                                text: qsTr("Provider:")
                                font.pixelSize: CherryStyle.smallFont.pixelSize
                                color: CherryStyle.secondaryTextColor
                            }

                            Kirigami.Icon {
                                source: appController.hasRemote ? appController.remoteProviderIcon : "network-server"
                                width: 16
                                height: 16
                                isMask: appController.hasRemote && appController.remoteProvider === "GitHub"
                                color: appController.hasRemote ? CherryStyle.accentColor : CherryStyle.secondaryTextColor
                            }

                            QQC2.Label {
                                text: appController.hasRemote ? appController.remoteProvider : qsTr("None")
                                font.bold: appController.hasRemote
                                color: appController.hasRemote ? CherryStyle.accentColor : CherryStyle.secondaryTextColor
                                Layout.fillWidth: true
                            }
                        }

                        RowLayout {
                            Layout.fillWidth: true
                            spacing: Kirigami.Units.mediumSpacing

                            QQC2.Label {
                                text: qsTr("Name:")
                                font.pixelSize: CherryStyle.smallFont.pixelSize
                                color: CherryStyle.secondaryTextColor
                            }

                            QQC2.Label {
                                text: appController.hasRemote ? appController.remoteName : qsTr("None")
                                font.bold: appController.hasRemote
                                color: appController.hasRemote ? CherryStyle.accentColor : CherryStyle.secondaryTextColor
                            }

                            QQC2.Label {
                                text: qsTr("Host:")
                                font.pixelSize: CherryStyle.smallFont.pixelSize
                                color: CherryStyle.secondaryTextColor
                            }

                            QQC2.Label {
                                text: appController.hasRemote ? appController.remoteHost : qsTr("None")
                                font.bold: appController.hasRemote
                                color: appController.hasRemote ? CherryStyle.accentColor : CherryStyle.secondaryTextColor
                                elide: Text.ElideRight
                                Layout.fillWidth: true
                            }
                        }

                        QQC2.TextField {
                            id: repoUrlField
                            Layout.fillWidth: true
                            placeholderText: qsTr("e.g. https://github.com/owner/repository.git or git@github.com:owner/repository.git")
                            background: Rectangle {
                                color: CherryStyle.inputBackground
                                border.color: repoUrlField.activeFocus ? CherryStyle.accentColor : CherryStyle.borderColor
                                border.width: repoUrlField.activeFocus ? 2 : 1
                                radius: CherryStyle.radiusSmall
                            }
                        }

                        RowLayout {
                            Layout.fillWidth: true
                            spacing: Kirigami.Units.smallSpacing

                            QQC2.Button {
                                text: qsTr("Publish with gh...")
                                icon.name: "cloud-upload"
                                visible: !appController.hasRemote
                                onClicked: {
                                    root.close();
                                    appController.showPublishDialog();
                                }
                            }

                            QQC2.Button {
                                text: qsTr("Remove Remote")
                                icon.name: "edit-delete"
                                visible: repoUrlField.text.trim().length > 0 || appController.hasRemote
                                onClicked: {
                                    repoUrlField.text = "";
                                    remotePresetCombo.currentIndex = 0;
                                    remoteOwnerField.text = "";
                                    remoteRepositoryField.text = "";
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
                            color: CherryStyle.secondaryTextColor
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
                                    border.color: localNameField.activeFocus ? CherryStyle.accentColor : CherryStyle.borderColor
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
                                    border.color: localEmailField.activeFocus ? CherryStyle.accentColor : CherryStyle.borderColor
                                    border.width: localEmailField.activeFocus ? 2 : 1
                                    radius: CherryStyle.radiusSmall
                                }
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
                        color: CherryStyle.secondaryTextColor
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
                                id: localFileModeCheckBox
                                text: qsTr("Ignore file metadata changes in this repository")
                            }

                            QQC2.Label {
                                text: qsTr("Writes core.filemode=false to this repository's .git/config.")
                                font.pixelSize: CherryStyle.smallFont.pixelSize - 1
                                color: CherryStyle.secondaryTextColor
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
                                id: globalFileModeCheckBox
                                text: qsTr("Ignore file metadata changes globally")
                            }

                            QQC2.Label {
                                text: qsTr("Writes core.filemode=false to your global Git configuration and applies to repositories without a local override.")
                                font.pixelSize: CherryStyle.smallFont.pixelSize - 1
                                color: CherryStyle.secondaryTextColor
                                wrapMode: Text.Wrap
                                Layout.fillWidth: true
                            }
                        }
                    }

                    QQC2.Label {
                        text: qsTr("The repository setting takes precedence over the global setting.")
                        font.pixelSize: CherryStyle.smallFont.pixelSize
                        color: CherryStyle.secondaryTextColor
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
                        color: CherryStyle.secondaryTextColor
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
                                border.color: globalNameField.activeFocus ? CherryStyle.accentColor : CherryStyle.borderColor
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
                                border.color: globalEmailField.activeFocus ? CherryStyle.accentColor : CherryStyle.borderColor
                                border.width: globalEmailField.activeFocus ? 2 : 1
                                radius: CherryStyle.radiusSmall
                            }
                        }
                    }

                    // Avatar Service / Profile Pictures Section
                    Rectangle {
                        Layout.fillWidth: true
                        implicitHeight: 1
                        color: CherryStyle.separatorColor
                        Layout.topMargin: Kirigami.Units.smallSpacing
                        Layout.bottomMargin: Kirigami.Units.smallSpacing
                    }

                    RowLayout {
                        Layout.fillWidth: true
                        spacing: Kirigami.Units.mediumSpacing

                        Kirigami.Avatar {
                            implicitWidth: 44
                            implicitHeight: 44
                            name: globalNameField.text.length > 0 ? globalNameField.text : appController.currentAuthorName
                            source: appController.resolveAvatarUrl(globalNameField.text, globalEmailField.text.length > 0 ? globalEmailField.text : appController.currentAuthorEmail)
                            color: CherryStyle.accentColor
                            Layout.alignment: Qt.AlignVCenter
                        }

                        ColumnLayout {
                            Layout.fillWidth: true
                            spacing: 2

                            QQC2.Label {
                                text: qsTr("Author Profile Picture & Avatars")
                                font.bold: true
                                font.pixelSize: CherryStyle.basePixelSize
                                color: Kirigami.Theme.textColor
                            }

                            QQC2.Label {
                                text: qsTr("Resolves author profile pictures from GitHub, GitLab, Codeberg, and Gravatar/Libravatar based on commit emails.")
                                font.pixelSize: CherryStyle.smallFont.pixelSize
                                color: CherryStyle.secondaryTextColor
                                wrapMode: Text.Wrap
                                Layout.fillWidth: true
                            }
                        }
                    }

                    ColumnLayout {
                        Layout.fillWidth: true
                        spacing: 4

                        QQC2.Label {
                            text: qsTr("Avatar Provider Service")
                            font.bold: true
                            font.pixelSize: CherryStyle.smallFont.pixelSize
                            color: Kirigami.Theme.textColor
                        }

                        QQC2.ComboBox {
                            id: avatarProviderCombo
                            Layout.fillWidth: true
                            textRole: "text"
                            valueRole: "value"
                            model: [
                                { text: qsTr("Automatic (GitHub, GitLab, Codeberg, Gravatar)"), value: "auto" },
                                { text: qsTr("Gravatar (https://gravatar.com)"), value: "gravatar" },
                                { text: qsTr("Libravatar (https://libravatar.org)"), value: "libravatar" },
                                { text: qsTr("Disabled (Colored initial badges only)"), value: "none" }
                            ]
                        }
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
                                color: CherryStyle.secondaryTextColor
                            }

                            QQC2.TextField {
                                id: customEditorField
                                Layout.fillWidth: true
                                placeholderText: qsTr("e.g. gedit %f or /opt/editor/bin %f:%l")
                                background: Rectangle {
                                    color: CherryStyle.inputBackground
                                    border.color: customEditorField.activeFocus ? CherryStyle.accentColor : CherryStyle.borderColor
                                    border.width: customEditorField.activeFocus ? 2 : 1
                                    radius: CherryStyle.radiusSmall
                                }
                            }
                        }

                        RowLayout {
                            Layout.fillWidth: true
                            spacing: Kirigami.Units.smallSpacing

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
                                color: CherryStyle.secondaryTextColor
                            }

                            QQC2.TextField {
                                id: customTermField
                                Layout.fillWidth: true
                                placeholderText: qsTr("e.g. alacritty --working-directory %d")
                                background: Rectangle {
                                    color: CherryStyle.inputBackground
                                    border.color: customTermField.activeFocus ? CherryStyle.accentColor : CherryStyle.borderColor
                                    border.width: customTermField.activeFocus ? 2 : 1
                                    radius: CherryStyle.radiusSmall
                                }
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
                            checked: root.pendingDiffViewMode === "unified"
                            onClicked: root.pendingDiffViewMode = "unified"
                        }

                        QQC2.RadioButton {
                            text: qsTr("Split (Side-by-Side)")
                            checked: root.pendingDiffViewMode === "split"
                            onClicked: root.pendingDiffViewMode = "split"
                        }
                    }

                    QQC2.CheckBox {
                        id: showWhitespaceCheckBox
                        text: qsTr("Show whitespace changes by default")
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
                        color: CherryStyle.secondaryTextColor
                    }

                    RowLayout {
                        Layout.fillWidth: true
                        spacing: Kirigami.Units.smallSpacing

                        QQC2.Button {
                            text: qsTr("Use Real Git Backend")
                            icon.name: "folder-git"
                            highlighted: root.pendingBackendMode === "real"
                            onClicked: root.pendingBackendMode = "real"
                        }

                        QQC2.Button {
                            text: qsTr("Use Mock Sandbox")
                            icon.name: "system-run"
                            highlighted: root.pendingBackendMode === "mock"
                            onClicked: root.pendingBackendMode = "mock"
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

            // Save settings and close
            RowLayout {
                Layout.fillWidth: true
                spacing: Kirigami.Units.smallSpacing

                Item { Layout.fillWidth: true }

                QQC2.Button {
                    text: qsTr("Cancel")
                    icon.name: "dialog-cancel"
                    onClicked: root.close()
                }

                QQC2.Button {
                    text: qsTr("Save")
                    icon.name: "document-save"
                    highlighted: true
                    onClicked: root.saveSettingsAndClose()
                }
            }
        }
    }
}
