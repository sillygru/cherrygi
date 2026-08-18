import QtQuick
import QtQuick.Controls as QQC2
import QtQuick.Layouts
import org.kde.kirigami as Kirigami
import org.kde.cherrygi
import "../style"

QQC2.Popup {
    id: repoDropdownPopup
    width: 380
    height: 440
    padding: Kirigami.Units.mediumSpacing
    modal: true
    dim: false
    focus: true
    closePolicy: QQC2.Popup.CloseOnEscape | QQC2.Popup.CloseOnPressOutside

    property string filterText: ""

    onAboutToShow: {
        searchField.text = "";
        filterText = "";
    }

    // Repository Context Menu
    QQC2.Menu {
        id: repoContextMenu
        property string targetRepoId: ""
        property string targetRepoPath: ""
        property string targetRepoName: ""
        modal: true
        closePolicy: QQC2.Popup.CloseOnEscape | QQC2.Popup.CloseOnPressOutside

        QQC2.MenuItem {
            text: qsTr("Open in External Editor")
            icon.name: "accessories-text-editor"
            onTriggered: appController.openInEditor(repoContextMenu.targetRepoPath)
        }

        QQC2.MenuItem {
            text: qsTr("Open in File Manager")
            icon.name: "folder"
            onTriggered: appController.openInFileManager(repoContextMenu.targetRepoPath)
        }

        QQC2.MenuItem {
            text: qsTr("Open in Terminal")
            icon.name: "utilities-terminal"
            onTriggered: appController.openInTerminal(repoContextMenu.targetRepoPath)
        }

        QQC2.MenuItem {
            text: qsTr("Copy Path")
            icon.name: "edit-copy"
            onTriggered: {
                appController.showToast(qsTr("Path copied to clipboard"));
            }
        }

        QQC2.MenuSeparator {}

        QQC2.MenuItem {
            text: qsTr("Repository Settings...")
            icon.name: "settings-configure"
            onTriggered: {
                if (repoContextMenu.targetRepoId !== "" && repoContextMenu.targetRepoId !== appController.currentRepoPath) {
                    appController.switchRepository(repoContextMenu.targetRepoId);
                }
                appController.showSettingsDialog("repository");
            }
        }

        QQC2.MenuItem {
            text: qsTr("Remove from Cherrygi")
            icon.name: "edit-delete"
            onTriggered: {
                appController.removeRepository(repoContextMenu.targetRepoId);
            }
        }
    }

    background: Rectangle {
        color: CherryStyle.surfacePopup
        border.color: CherryStyle.popupBorderColor
        border.width: 1
        radius: CherryStyle.radiusLarge

        // Multi-layered shadow for realistic floating weight & contrast
        Rectangle {
            anchors.fill: parent
            anchors.margins: -1
            z: -1
            color: "transparent"
            border.color: Qt.rgba(0, 0, 0, 0.35)
            radius: CherryStyle.radiusLarge + 1
        }

        Rectangle {
            anchors.fill: parent
            anchors.margins: -3
            z: -2
            color: "transparent"
            border.color: Qt.rgba(0, 0, 0, 0.15)
            radius: CherryStyle.radiusLarge + 3
        }
    }

    ColumnLayout {
        anchors.fill: parent
        spacing: Kirigami.Units.smallSpacing

        // Search Box
        QQC2.TextField {
            id: searchField
            Layout.fillWidth: true
            placeholderText: qsTr("Filter repositories...")
            leftPadding: Kirigami.Units.largeSpacing + 12
            rightPadding: text.length > 0 ? Kirigami.Units.largeSpacing + 10 : Kirigami.Units.smallSpacing

            background: Rectangle {
                color: CherryStyle.inputBackground
                border.color: searchField.activeFocus ? CherryStyle.accentColor : CherryStyle.borderColor
                border.width: searchField.activeFocus ? 2 : 1
                radius: CherryStyle.radiusSmall
            }

            Kirigami.Icon {
                source: "search"
                width: 14
                height: 14
                anchors.left: parent.left
                anchors.leftMargin: Kirigami.Units.smallSpacing + 2
                anchors.verticalCenter: parent.verticalCenter
                color: searchField.activeFocus ? CherryStyle.accentColor : CherryStyle.secondaryTextColor
            }

            QQC2.ToolButton {
                visible: searchField.text.length > 0
                icon.name: "edit-clear"
                icon.width: 12
                icon.height: 12
                anchors.right: parent.right
                anchors.verticalCenter: parent.verticalCenter
                onClicked: {
                    searchField.text = "";
                    repoDropdownPopup.filterText = "";
                }
            }

            onTextChanged: {
                repoDropdownPopup.filterText = text.trim().toLowerCase();
            }
        }

        // Section header
        Item {
            Layout.fillWidth: true
            height: 20
            QQC2.Label {
                anchors.left: parent.left
                anchors.leftMargin: 2
                anchors.verticalCenter: parent.verticalCenter
                text: qsTr("Repositories (%1)").arg(appController.repositories.count)
                font.bold: true
                font.pixelSize: CherryStyle.smallFont.pixelSize
                color: CherryStyle.secondaryTextColor
            }
        }

        // Repository list
        QQC2.ScrollView {
            Layout.fillWidth: true
            Layout.fillHeight: true
            clip: true

            ListView {
                id: repoListView
                model: appController.repositories
                spacing: 3
                reuseItems: false

                signal requestClose()
                onRequestClose: repoDropdownPopup.close()

                delegate: QQC2.ItemDelegate {
                    id: repoDelegate
                    width: repoListView.width
                    height: visible ? 52 : 0
                    visible: {
                        if (repoDropdownPopup.filterText === "") return true;
                        return repoDelegate.name.toLowerCase().includes(repoDropdownPopup.filterText) ||
                               repoDelegate.path.toLowerCase().includes(repoDropdownPopup.filterText);
                    }

                    required property int index
                    required property string repoId
                    required property string name
                    required property string path
                    required property bool isCurrent
                    required property string currentBranch
                    required property int changedFilesCount
                    required property int aheadCount
                    required property int behindCount
                    required property string lastFetchedTime
                    required property bool isMissing
                    required property string remoteUrl

                    highlighted: repoDelegate.isCurrent

                    background: Rectangle {
                        color: repoDelegate.highlighted ? CherryStyle.activeBackground : (repoDelegate.hovered ? CherryStyle.hoverBackground : CherryStyle.surfaceCard)
                        radius: CherryStyle.radiusSmall
                        border.color: repoDelegate.isMissing ? Qt.rgba(CherryStyle.deletionColor.r, CherryStyle.deletionColor.g, CherryStyle.deletionColor.b, 0.5) : (repoDelegate.highlighted ? CherryStyle.accentColor : (repoDelegate.hovered ? CherryStyle.borderColor : CherryStyle.subtleBorderColor))
                        border.width: 1
                    }

                    contentItem: RowLayout {
                        anchors.fill: parent
                        anchors.leftMargin: Kirigami.Units.smallSpacing
                        anchors.rightMargin: Kirigami.Units.smallSpacing
                        spacing: Kirigami.Units.smallSpacing

                        Kirigami.Icon {
                            source: repoDelegate.isMissing ? "dialog-warning" : "folder-git"
                            width: 20
                            height: 20
                            Layout.alignment: Qt.AlignVCenter
                            color: repoDelegate.isMissing ? CherryStyle.deletionColor : (repoDelegate.isCurrent ? CherryStyle.accentColor : Kirigami.Theme.textColor)
                        }

                        ColumnLayout {
                            Layout.fillWidth: true
                            spacing: 1

                            RowLayout {
                                QQC2.Label {
                                    text: repoDelegate.name
                                    font.bold: repoDelegate.isCurrent
                                    color: repoDelegate.isMissing ? CherryStyle.deletionColor : (repoDelegate.isCurrent ? CherryStyle.accentColor : Kirigami.Theme.textColor)
                                    elide: Text.ElideRight
                                }

                                // Missing badge
                                Rectangle {
                                    visible: repoDelegate.isMissing
                                    width: missingLabel.width + 10
                                    height: 18
                                    radius: 9
                                    color: Qt.rgba(CherryStyle.deletionColor.r, CherryStyle.deletionColor.g, CherryStyle.deletionColor.b, 0.15)
                                    border.color: CherryStyle.deletionColor
                                    border.width: 1

                                    QQC2.Label {
                                        id: missingLabel
                                        anchors.centerIn: parent
                                        text: qsTr("Missing")
                                        font.pixelSize: CherryStyle.smallFont.pixelSize - 1
                                        font.bold: true
                                        color: CherryStyle.deletionColor
                                    }
                                }

                                Item { Layout.fillWidth: true }

                                // Changed files badge if any
                                Rectangle {
                                    visible: !repoDelegate.isMissing && repoDelegate.changedFilesCount > 0
                                    width: countLabel.width + 10
                                    height: 18
                                    radius: 9
                                    color: CherryStyle.modifiedBg
                                    border.color: CherryStyle.modifiedColor
                                    border.width: 1

                                    QQC2.Label {
                                        id: countLabel
                                        anchors.centerIn: parent
                                        text: repoDelegate.changedFilesCount
                                        font.pixelSize: CherryStyle.smallFont.pixelSize
                                        font.bold: true
                                        color: CherryStyle.modifiedColor
                                    }
                                }
                            }

                            QQC2.Label {
                                text: repoDelegate.path
                                font.pixelSize: CherryStyle.smallFont.pixelSize
                                color: CherryStyle.secondaryTextColor
                                elide: Text.ElideMiddle
                                Layout.fillWidth: true
                            }
                        }
                    }

                    onClicked: {
                        appController.switchRepository(repoDelegate.repoId);
                        ListView.view.requestClose();
                    }

                    // Use a right-button MouseArea instead of TapHandler so the
                    // event is accepted before an underlying file view can see
                    // the same native right-click.
                    MouseArea {
                        id: repoContextMouse
                        anchors.fill: parent
                        acceptedButtons: Qt.RightButton
                        preventStealing: true
                        z: 2

                        onPressed: function(mouse) {
                            mouse.accepted = true;
                            repoContextMenu.targetRepoId = repoDelegate.repoId;
                            repoContextMenu.targetRepoPath = repoDelegate.path;
                            repoContextMenu.targetRepoName = repoDelegate.name;
                            repoContextMenu.popup();
                        }
                    }
                }
            }
        }

        Kirigami.Separator {
            Layout.fillWidth: true
        }

        // Bottom Actions
        RowLayout {
            Layout.fillWidth: true
            spacing: Kirigami.Units.smallSpacing

            QQC2.Button {
                Layout.fillWidth: true
                text: qsTr("Add Local...")
                icon.name: "list-add"
                onClicked: {
                    repoDropdownPopup.close();
                    appController.openLocalRepositoryDialog();
                }
            }

            QQC2.Button {
                Layout.fillWidth: true
                text: qsTr("Clone...")
                icon.name: "download"
                highlighted: true
                onClicked: {
                    repoDropdownPopup.close();
                    appController.showCloneDialog();
                }
            }

            QQC2.Button {
                Layout.fillWidth: true
                text: qsTr("Backend...")
                icon.name: "view-refresh"
                onClicked: {
                    repoDropdownPopup.close();
                    appController.showBackendSelectionDialog();
                }
            }
        }
    }
}
