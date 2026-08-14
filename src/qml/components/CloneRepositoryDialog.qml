import QtQuick
import QtQuick.Controls as QQC2
import QtQuick.Layouts
import QtQuick.Dialogs
import org.kde.kirigami as Kirigami
import org.kde.cherrygi
import "../style"

QQC2.Popup {
    id: root
    width: 520
    implicitHeight: mainLayout.implicitHeight + Kirigami.Units.largeSpacing * 2 + 30
    modal: true
    dim: true
    focus: true
    closePolicy: appController.isCloning ? QQC2.Popup.NoAutoClose : (QQC2.Popup.CloseOnEscape | QQC2.Popup.CloseOnPressOutside)
    anchors.centerIn: parent

    background: Rectangle {
        color: CherryStyle.surfacePopup
        border.color: CherryStyle.popupBorderColor
        border.width: 1
        radius: CherryStyle.radiusLarge

        Rectangle {
            anchors.fill: parent
            anchors.margins: -1
            z: -1
            color: "transparent"
            border.color: Qt.rgba(0, 0, 0, 0.4)
            radius: CherryStyle.radiusLarge + 1
        }
    }

    FolderDialog {
        id: destinationFolderDialog
        title: qsTr("Select Local Directory")
        onAccepted: {
            var path = selectedFolder.toString();
            if (path.startsWith("file://")) {
                path = path.substring(7);
            }
            localPathField.text = path;
        }
    }

    onAboutToShow: {
        if (appController.clonePrefillUrl !== "") {
            urlField.text = appController.clonePrefillUrl;
        } else {
            urlField.text = "";
        }
        if (appController.clonePrefillPath !== "") {
            localPathField.text = appController.clonePrefillPath;
        } else {
            localPathField.text = "";
        }
        urlField.forceActiveFocus();
    }

    ColumnLayout {
        id: mainLayout
        anchors.fill: parent
        anchors.margins: Kirigami.Units.largeSpacing
        spacing: Kirigami.Units.mediumSpacing

        // Title Row
        RowLayout {
            Layout.fillWidth: true
            spacing: Kirigami.Units.smallSpacing

            Kirigami.Icon {
                source: "download"
                width: 22
                height: 22
                color: CherryStyle.accentColor
            }

            QQC2.Label {
                text: qsTr("Clone a Repository")
                font.bold: true
                font.pixelSize: CherryStyle.defaultFont.pixelSize + 3
                color: Kirigami.Theme.textColor
                Layout.fillWidth: true
            }

            QQC2.ToolButton {
                icon.name: "window-close"
                enabled: !appController.isCloning
                onClicked: root.close()
            }
        }

        Kirigami.Separator {
            Layout.fillWidth: true
        }

        // Repository URL field
        ColumnLayout {
            Layout.fillWidth: true
            spacing: 4

            QQC2.Label {
                text: qsTr("Repository URL or GitHub identifier")
                font.bold: true
                font.pixelSize: CherryStyle.smallFont.pixelSize + 1
                color: Kirigami.Theme.textColor
            }

            QQC2.TextField {
                id: urlField
                Layout.fillWidth: true
                placeholderText: qsTr("https://github.com/owner/repo.git or git@github.com:owner/repo.git")
                enabled: !appController.isCloning
                background: Rectangle {
                    color: CherryStyle.inputBackground
                    border.color: urlField.activeFocus ? CherryStyle.accentColor : CherryStyle.borderColor
                    border.width: urlField.activeFocus ? 2 : 1
                    radius: CherryStyle.radiusSmall
                }
                onTextChanged: {
                    if (localPathField.text === "" && text.trim().length > 0) {
                        var clean = text.trim();
                        var repoName = "";
                        if (clean.endsWith(".git")) clean = clean.substring(0, clean.length - 4);
                        var slashIdx = Math.max(clean.lastIndexOf("/"), clean.lastIndexOf(":"));
                        if (slashIdx >= 0 && slashIdx < clean.length - 1) {
                            repoName = clean.substring(slashIdx + 1);
                        }
                        if (repoName.length > 0) {
                            localPathField.placeholderText = "~/Projects/" + repoName;
                        }
                    }
                }
            }
        }

        // Local Path field
        ColumnLayout {
            Layout.fillWidth: true
            spacing: 4

            QQC2.Label {
                text: qsTr("Local Path")
                font.bold: true
                font.pixelSize: CherryStyle.smallFont.pixelSize + 1
                color: Kirigami.Theme.textColor
            }

            RowLayout {
                Layout.fillWidth: true
                spacing: Kirigami.Units.smallSpacing

                QQC2.TextField {
                    id: localPathField
                    Layout.fillWidth: true
                    placeholderText: qsTr("Select or enter destination folder path...")
                    enabled: !appController.isCloning
                    background: Rectangle {
                        color: CherryStyle.inputBackground
                        border.color: localPathField.activeFocus ? CherryStyle.accentColor : CherryStyle.borderColor
                        border.width: localPathField.activeFocus ? 2 : 1
                        radius: CherryStyle.radiusSmall
                    }
                }

                QQC2.Button {
                    text: qsTr("Browse...")
                    icon.name: "folder-open"
                    enabled: !appController.isCloning
                    onClicked: destinationFolderDialog.open()
                }
            }
        }

        // Progress bar during cloning
        ColumnLayout {
            Layout.fillWidth: true
            visible: appController.isCloning
            spacing: 4

            QQC2.ProgressBar {
                Layout.fillWidth: true
                indeterminate: true
            }

            QQC2.Label {
                text: appController.cloneProgressMessage !== "" ? appController.cloneProgressMessage : qsTr("Cloning repository...")
                font.pixelSize: CherryStyle.smallFont.pixelSize
                color: CherryStyle.secondaryTextColor
                elide: Text.ElideRight
                Layout.fillWidth: true
            }
        }

        Item {
            Layout.fillWidth: true
            height: Kirigami.Units.smallSpacing
        }

        Kirigami.Separator {
            Layout.fillWidth: true
        }

        // Action Buttons
        RowLayout {
            Layout.fillWidth: true
            spacing: Kirigami.Units.smallSpacing

            Item { Layout.fillWidth: true }

            QQC2.Button {
                text: qsTr("Cancel")
                enabled: !appController.isCloning
                onClicked: root.close()
            }

            QQC2.Button {
                text: qsTr("Clone")
                icon.name: "download"
                highlighted: true
                enabled: !appController.isCloning && urlField.text.trim().length > 0 && localPathField.text.trim().length > 0
                onClicked: {
                    var url = urlField.text.trim();
                    var path = localPathField.text.trim();
                    appController.cloneRepository(url, path);
                }
            }
        }
    }
}
