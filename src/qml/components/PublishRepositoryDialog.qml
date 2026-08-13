import QtQuick
import QtQuick.Controls as QQC2
import QtQuick.Layouts
import org.kde.kirigami as Kirigami
import org.kde.cherrygi
import "../style"

QQC2.Popup {
    id: root
    width: 560
    height: 480
    anchors.centerIn: parent
    padding: Kirigami.Units.largeSpacing
    modal: true
    dim: true
    focus: true
    closePolicy: appController.isPublishing ? QQC2.Popup.NoAutoClose : (QQC2.Popup.CloseOnEscape | QQC2.Popup.CloseOnPressOutside)

    property int activeTab: 0 // 0: GitHub Publish, 1: Custom Git Remote

    onAboutToShow: {
        repoNameField.text = (appController.currentRepoName !== "No repository" && appController.currentRepoName !== "") ? appController.currentRepoName : "";
        descField.text = "";
        privateCheckBox.checked = true;
        remoteUrlField.text = "";
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

    ColumnLayout {
        anchors.fill: parent
        spacing: Kirigami.Units.mediumSpacing

        // Header Title + Close button
        RowLayout {
            Layout.fillWidth: true
            spacing: Kirigami.Units.smallSpacing

            Kirigami.Icon {
                source: "cloud-upload"
                width: 24
                height: 24
                color: Kirigami.Theme.highlightColor
            }

            ColumnLayout {
                Layout.fillWidth: true
                spacing: 1

                QQC2.Label {
                    text: qsTr("Publish Repository")
                    font.bold: true
                    font.pixelSize: CherryStyle.basePixelSize + 3
                    color: Kirigami.Theme.textColor
                }

                QQC2.Label {
                    text: qsTr("Publish this local repository to a remote server or GitHub")
                    font.pixelSize: CherryStyle.smallFont.pixelSize
                    color: Kirigami.Theme.disabledTextColor
                }
            }

            QQC2.ToolButton {
                icon.name: "window-close"
                icon.width: 14
                icon.height: 14
                enabled: !appController.isPublishing
                onClicked: root.close()
            }
        }

        // Active Publishing Loading Banner
        Rectangle {
            Layout.fillWidth: true
            implicitHeight: publishingRow.implicitHeight + 16
            radius: CherryStyle.radiusMedium
            color: Qt.rgba(Kirigami.Theme.highlightColor.r, Kirigami.Theme.highlightColor.g, Kirigami.Theme.highlightColor.b, 0.12)
            border.color: Qt.rgba(Kirigami.Theme.highlightColor.r, Kirigami.Theme.highlightColor.g, Kirigami.Theme.highlightColor.b, 0.35)
            border.width: 1
            visible: appController.isPublishing

            ColumnLayout {
                id: publishingRow
                anchors.fill: parent
                anchors.margins: 10
                spacing: 6

                RowLayout {
                    Layout.fillWidth: true
                    spacing: 8

                    QQC2.BusyIndicator {
                        width: 18
                        height: 18
                        running: appController.isPublishing
                    }

                    QQC2.Label {
                        text: qsTr("Publishing repository to GitHub...")
                        font.bold: true
                        color: Kirigami.Theme.textColor
                        Layout.fillWidth: true
                    }
                }

                // Indeterminate moving progress bar
                Rectangle {
                    Layout.fillWidth: true
                    height: 4
                    radius: 2
                    clip: true
                    color: Qt.rgba(Kirigami.Theme.highlightColor.r, Kirigami.Theme.highlightColor.g, Kirigami.Theme.highlightColor.b, 0.25)

                    Rectangle {
                        id: dialogProgressBarThumb
                        height: parent.height
                        width: parent.width * 0.35
                        radius: 2
                        color: Kirigami.Theme.highlightColor

                        SequentialAnimation on x {
                            running: appController.isPublishing
                            loops: Animation.Infinite
                            NumberAnimation {
                                from: -dialogProgressBarThumb.width
                                to: parent.width
                                duration: 1100
                                easing.type: Easing.InOutCubic
                            }
                        }
                    }
                }

                QQC2.Label {
                    text: qsTr("Creating GitHub repository and pushing branch commits. This runs in background without freezing.")
                    font.pixelSize: CherryStyle.smallFont.pixelSize - 1
                    color: Kirigami.Theme.disabledTextColor
                    elide: Text.ElideRight
                    Layout.fillWidth: true
                }
            }
        }

        // Inline Error Banner (if publish failed)
        Rectangle {
            Layout.fillWidth: true
            implicitHeight: errorRow.implicitHeight + 16
            radius: CherryStyle.radiusMedium
            color: Qt.rgba(0.9, 0.2, 0.2, 0.15)
            border.color: Qt.rgba(0.9, 0.2, 0.2, 0.4)
            border.width: 1
            visible: appController.publishErrorMessage.length > 0 && !appController.isPublishing

            RowLayout {
                id: errorRow
                anchors.fill: parent
                anchors.margins: 10
                spacing: 8

                Kirigami.Icon {
                    source: "dialog-error"
                    width: 18
                    height: 18
                    color: "#e01b24"
                }

                ColumnLayout {
                    Layout.fillWidth: true
                    spacing: 1

                    QQC2.Label {
                        text: qsTr("Publish Failed")
                        font.bold: true
                        color: "#e01b24"
                    }

                    QQC2.Label {
                        text: appController.publishErrorMessage
                        font.pixelSize: CherryStyle.smallFont.pixelSize
                        color: Kirigami.Theme.textColor
                        wrapMode: Text.Wrap
                        Layout.fillWidth: true
                    }
                }
            }
        }

        // Segmented Tab Switcher (GitHub vs Custom Remote)
        Rectangle {
            Layout.fillWidth: true
            height: 34
            radius: CherryStyle.radiusMedium
            color: CherryStyle.surfaceCard
            border.color: CherryStyle.borderColor
            border.width: 1
            opacity: appController.isPublishing ? 0.5 : 1.0

            RowLayout {
                anchors.fill: parent
                anchors.margins: 2
                spacing: 2

                Rectangle {
                    Layout.fillWidth: true
                    Layout.fillHeight: true
                    radius: CherryStyle.radiusSmall
                    color: root.activeTab === 0 ? CherryStyle.activeBackground : "transparent"

                    RowLayout {
                        anchors.centerIn: parent
                        spacing: 4

                        Kirigami.Icon {
                            source: "globe"
                            width: 14
                            height: 14
                            color: root.activeTab === 0 ? Kirigami.Theme.highlightColor : Kirigami.Theme.disabledTextColor
                        }

                        QQC2.Label {
                            text: qsTr("Publish to GitHub")
                            font.bold: root.activeTab === 0
                            font.pixelSize: CherryStyle.smallFont.pixelSize
                            color: root.activeTab === 0 ? Kirigami.Theme.highlightColor : Kirigami.Theme.textColor
                        }
                    }

                    MouseArea {
                        anchors.fill: parent
                        enabled: !appController.isPublishing
                        cursorShape: appController.isPublishing ? Qt.ArrowCursor : Qt.PointingHandCursor
                        onClicked: root.activeTab = 0
                    }
                }

                Rectangle {
                    Layout.fillWidth: true
                    Layout.fillHeight: true
                    radius: CherryStyle.radiusSmall
                    color: root.activeTab === 1 ? CherryStyle.activeBackground : "transparent"

                    RowLayout {
                        anchors.centerIn: parent
                        spacing: 4

                        Kirigami.Icon {
                            source: "folder-git"
                            width: 14
                            height: 14
                            color: root.activeTab === 1 ? Kirigami.Theme.highlightColor : Kirigami.Theme.disabledTextColor
                        }

                        QQC2.Label {
                            text: qsTr("Custom Git Remote URL")
                            font.bold: root.activeTab === 1
                            font.pixelSize: CherryStyle.smallFont.pixelSize
                            color: root.activeTab === 1 ? Kirigami.Theme.highlightColor : Kirigami.Theme.textColor
                        }
                    }

                    MouseArea {
                        anchors.fill: parent
                        enabled: !appController.isPublishing
                        cursorShape: appController.isPublishing ? Qt.ArrowCursor : Qt.PointingHandCursor
                        onClicked: root.activeTab = 1
                    }
                }
            }
        }

        // Stacked Content
        Item {
            Layout.fillWidth: true
            Layout.fillHeight: true

            // ==========================================
            // TAB 0: GITHUB PUBLISH
            // ==========================================
            ColumnLayout {
                anchors.fill: parent
                visible: root.activeTab === 0
                spacing: Kirigami.Units.smallSpacing + 2

                // Repository Name
                ColumnLayout {
                    Layout.fillWidth: true
                    spacing: 2

                    QQC2.Label {
                        text: qsTr("Repository Name")
                        font.bold: true
                        font.pixelSize: CherryStyle.smallFont.pixelSize
                        color: Kirigami.Theme.textColor
                    }

                    QQC2.TextField {
                        id: repoNameField
                        Layout.fillWidth: true
                        enabled: !appController.isPublishing
                        placeholderText: qsTr("e.g. my-awesome-project")
                        background: Rectangle {
                            color: CherryStyle.inputBackground
                            border.color: repoNameField.activeFocus ? Kirigami.Theme.highlightColor : CherryStyle.borderColor
                            border.width: repoNameField.activeFocus ? 2 : 1
                            radius: CherryStyle.radiusSmall
                        }
                    }
                }

                // Description
                ColumnLayout {
                    Layout.fillWidth: true
                    spacing: 2

                    QQC2.Label {
                        text: qsTr("Description (optional)")
                        font.bold: true
                        font.pixelSize: CherryStyle.smallFont.pixelSize
                        color: Kirigami.Theme.textColor
                    }

                    QQC2.TextField {
                        id: descField
                        Layout.fillWidth: true
                        enabled: !appController.isPublishing
                        placeholderText: qsTr("Short description of this repository...")
                        background: Rectangle {
                            color: CherryStyle.inputBackground
                            border.color: descField.activeFocus ? Kirigami.Theme.highlightColor : CherryStyle.borderColor
                            border.width: descField.activeFocus ? 2 : 1
                            radius: CherryStyle.radiusSmall
                        }
                    }
                }

                // Keep this code private Checkbox (GitHub Desktop standard)
                RowLayout {
                    Layout.fillWidth: true
                    spacing: Kirigami.Units.smallSpacing

                    QQC2.CheckBox {
                        id: privateCheckBox
                        checked: true
                        enabled: !appController.isPublishing
                    }

                    ColumnLayout {
                        Layout.fillWidth: true
                        spacing: 0

                        QQC2.Label {
                            text: qsTr("Keep this code private")
                            font.bold: true
                            font.pixelSize: CherryStyle.smallFont.pixelSize
                            color: Kirigami.Theme.textColor
                        }

                        QQC2.Label {
                            text: qsTr("Only you and selected collaborators can access this repository.")
                            font.pixelSize: CherryStyle.smallFont.pixelSize - 1
                            color: Kirigami.Theme.disabledTextColor
                        }
                    }
                }

                Item { Layout.fillHeight: true }

                // CLI Status Note
                Rectangle {
                    Layout.fillWidth: true
                    implicitHeight: 32
                    radius: CherryStyle.radiusSmall
                    color: appController.isGhAvailable ? Qt.rgba(0.2, 0.7, 0.3, 0.12) : Qt.rgba(0.9, 0.6, 0.1, 0.12)
                    border.color: appController.isGhAvailable ? "#2ec27e" : "#e5a50a"
                    border.width: 1

                    RowLayout {
                        anchors.fill: parent
                        anchors.margins: Kirigami.Units.smallSpacing
                        spacing: Kirigami.Units.smallSpacing

                        Kirigami.Icon {
                            source: appController.isGhAvailable ? "dialog-ok" : "dialog-information"
                            width: 14
                            height: 14
                            color: appController.isGhAvailable ? "#2ec27e" : "#e5a50a"
                        }

                        QQC2.Label {
                            text: appController.isGhAvailable ?
                                  qsTr("GitHub CLI (gh) is available and ready to publish.") :
                                  qsTr("gh CLI not detected. You can also paste an existing remote URL.")
                            font.pixelSize: CherryStyle.smallFont.pixelSize - 1
                            color: Kirigami.Theme.textColor
                            Layout.fillWidth: true
                            elide: Text.ElideRight
                        }
                    }
                }
            }

            // ==========================================
            // TAB 1: CUSTOM REMOTE URL
            // ==========================================
            ColumnLayout {
                anchors.fill: parent
                visible: root.activeTab === 1
                spacing: Kirigami.Units.smallSpacing + 2

                ColumnLayout {
                    Layout.fillWidth: true
                    spacing: 2

                    QQC2.Label {
                        text: qsTr("Remote Name")
                        font.bold: true
                        font.pixelSize: CherryStyle.smallFont.pixelSize
                        color: Kirigami.Theme.textColor
                    }

                    QQC2.TextField {
                        id: remoteNameField
                        text: "origin"
                        Layout.fillWidth: true
                        enabled: !appController.isPublishing
                        background: Rectangle {
                            color: CherryStyle.inputBackground
                            border.color: remoteNameField.activeFocus ? Kirigami.Theme.highlightColor : CherryStyle.borderColor
                            border.width: remoteNameField.activeFocus ? 2 : 1
                            radius: CherryStyle.radiusSmall
                        }
                    }
                }

                ColumnLayout {
                    Layout.fillWidth: true
                    spacing: 2

                    QQC2.Label {
                        text: qsTr("Remote Repository URL")
                        font.bold: true
                        font.pixelSize: CherryStyle.smallFont.pixelSize
                        color: Kirigami.Theme.textColor
                    }

                    QQC2.TextField {
                        id: remoteUrlField
                        Layout.fillWidth: true
                        enabled: !appController.isPublishing
                        placeholderText: qsTr("https://github.com/user/repo.git or git@github.com:user/repo.git")
                        background: Rectangle {
                            color: CherryStyle.inputBackground
                            border.color: remoteUrlField.activeFocus ? Kirigami.Theme.highlightColor : CherryStyle.borderColor
                            border.width: remoteUrlField.activeFocus ? 2 : 1
                            radius: CherryStyle.radiusSmall
                        }
                    }
                }

                QQC2.Label {
                    text: qsTr("After saving, 'cherrygi' will link the local repository to this remote and track origin branches.")
                    font.pixelSize: CherryStyle.smallFont.pixelSize - 1
                    color: Kirigami.Theme.disabledTextColor
                    wrapMode: Text.Wrap
                    Layout.fillWidth: true
                }

                Item { Layout.fillHeight: true }
            }
        }

        // Dialog Action Buttons
        RowLayout {
            Layout.fillWidth: true
            spacing: Kirigami.Units.smallSpacing

            Item { Layout.fillWidth: true }

            QQC2.Button {
                text: qsTr("Cancel")
                enabled: !appController.isPublishing
                onClicked: root.close()
            }

            QQC2.Button {
                id: publishActionButton
                text: {
                    if (root.activeTab === 0) {
                        return appController.isPublishing ? qsTr("Publishing to GitHub...") : qsTr("Publish Repository");
                    }
                    return qsTr("Set Remote URL");
                }
                icon.name: root.activeTab === 0 ? "cloud-upload" : "document-save"
                highlighted: true
                enabled: {
                    if (appController.isPublishing) return false;
                    if (root.activeTab === 0) {
                        return repoNameField.text.trim().length > 0;
                    } else {
                        return remoteUrlField.text.trim().length > 0;
                    }
                }
                onClicked: {
                    if (root.activeTab === 0) {
                        var name = repoNameField.text.trim();
                        var desc = descField.text.trim();
                        var isPriv = privateCheckBox.checked;
                        appController.publishRepository(name, desc, isPriv);
                    } else {
                        var rname = remoteNameField.text.trim();
                        var url = remoteUrlField.text.trim();
                        if (appController.saveRemoteUrl(url, rname)) {
                            root.close();
                        }
                    }
                }
            }
        }
    }
}
