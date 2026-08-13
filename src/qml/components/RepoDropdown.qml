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
                border.color: searchField.activeFocus ? Kirigami.Theme.highlightColor : CherryStyle.borderColor
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
                color: searchField.activeFocus ? Kirigami.Theme.highlightColor : Kirigami.Theme.disabledTextColor
            }

            QQC2.ToolButton {
                visible: searchField.text.length > 0
                icon.name: "edit-clear"
                icon.width: 12
                icon.height: 12
                anchors.right: parent.right
                anchors.verticalCenter: parent.verticalCenter
                onClicked: searchField.text = ""
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
                text: qsTr("Repositories")
                font.bold: true
                font.pixelSize: CherryStyle.smallFont.pixelSize
                color: Kirigami.Theme.disabledTextColor
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

                signal requestClose()
                onRequestClose: repoDropdownPopup.close()

                delegate: QQC2.ItemDelegate {
                    id: repoDelegate
                    width: repoListView.width
                    height: 52

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

                    highlighted: repoDelegate.isCurrent

                    background: Rectangle {
                        color: repoDelegate.highlighted ? CherryStyle.activeBackground : (repoDelegate.hovered ? CherryStyle.hoverBackground : CherryStyle.surfaceCard)
                        radius: CherryStyle.radiusSmall
                        border.color: repoDelegate.highlighted ? Kirigami.Theme.highlightColor : (repoDelegate.hovered ? CherryStyle.borderColor : CherryStyle.subtleBorderColor)
                        border.width: 1
                    }

                    contentItem: RowLayout {
                        anchors.fill: parent
                        anchors.leftMargin: Kirigami.Units.smallSpacing
                        anchors.rightMargin: Kirigami.Units.smallSpacing
                        spacing: Kirigami.Units.smallSpacing

                        Kirigami.Icon {
                            source: "folder-git"
                            width: 20
                            height: 20
                            color: repoDelegate.isCurrent ? Kirigami.Theme.highlightColor : Kirigami.Theme.textColor
                        }

                        ColumnLayout {
                            Layout.fillWidth: true
                            spacing: 1

                            RowLayout {
                                QQC2.Label {
                                    text: repoDelegate.name
                                    font.bold: repoDelegate.isCurrent
                                    color: repoDelegate.isCurrent ? Kirigami.Theme.highlightColor : Kirigami.Theme.textColor
                                    elide: Text.ElideRight
                                }

                                Item { Layout.fillWidth: true }

                                // Changed files badge if any
                                Rectangle {
                                    visible: repoDelegate.changedFilesCount > 0
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
                                color: Kirigami.Theme.disabledTextColor
                                elide: Text.ElideMiddle
                                Layout.fillWidth: true
                            }
                        }
                    }

                    onClicked: {
                        appController.switchRepository(repoDelegate.repoId);
                        ListView.view.requestClose();
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
                    appController.showToast(qsTr("Add repository dialog opened"));
                    repoDropdownPopup.close();
                }
            }

            QQC2.Button {
                Layout.fillWidth: true
                text: qsTr("Clone...")
                icon.name: "vcs-clone"
                onClicked: {
                    appController.showToast(qsTr("Clone repository dialog opened"));
                    repoDropdownPopup.close();
                }
            }
        }
    }
}
