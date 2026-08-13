import QtQuick
import QtQuick.Controls as QQC2
import QtQuick.Layouts
import org.kde.kirigami as Kirigami
import org.kde.cherrygi

QQC2.Popup {
    id: popup
    width: 360
    height: 420
    padding: Kirigami.Units.smallSpacing
    modal: true
    focus: true
    closePolicy: QQC2.Popup.CloseOnEscape | QQC2.Popup.CloseOnPressOutsideParent

    background: Rectangle {
        color: Kirigami.Theme.backgroundColor
        border.color: CherryStyle.borderColor
        border.width: 1
        radius: CherryStyle.radiusMedium

        // Breeze shadow simulation
        Rectangle {
            anchors.fill: parent
            anchors.margins: -1
            z: -1
            color: "transparent"
            border.color: Qt.rgba(0, 0, 0, 0.15)
            radius: CherryStyle.radiusMedium + 1
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
            leftPadding: Kirigami.Units.largeSpacing + 10
            rightPadding: text.length > 0 ? Kirigami.Units.largeSpacing + 10 : Kirigami.Units.smallSpacing

            Kirigami.Icon {
                source: "search"
                width: Kirigami.Units.iconSizes.small
                height: width
                anchors.left: parent.left
                anchors.leftMargin: Kirigami.Units.smallSpacing
                anchors.verticalCenter: parent.verticalCenter
                color: Kirigami.Theme.disabledTextColor
            }

            QQC2.ToolButton {
                visible: searchField.text.length > 0
                icon.name: "edit-clear"
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
                anchors.leftMargin: Kirigami.Units.smallSpacing
                anchors.verticalCenter: parent.verticalCenter
                text: qsTr("Repositories")
                font.bold: true
                font.pixelSize: CherryStyle.basePixelSize - 1
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
                spacing: 2

                delegate: QQC2.ItemDelegate {
                    id: repoDelegate
                    width: repoListView.width
                    height: 48

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
                        color: repoDelegate.highlighted ? CherryStyle.activeBackground : (repoDelegate.hovered ? CherryStyle.hoverBackground : "transparent")
                        radius: CherryStyle.radiusSmall
                        border.color: repoDelegate.highlighted ? Kirigami.Theme.highlightColor : "transparent"
                        border.width: repoDelegate.highlighted ? 1 : 0
                    }

                    contentItem: RowLayout {
                        spacing: Kirigami.Units.smallSpacing

                        Kirigami.Icon {
                            source: "folder-git"
                            width: Kirigami.Units.iconSizes.medium
                            height: width
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
                        popup.close();
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
                    popup.close();
                }
            }

            QQC2.Button {
                Layout.fillWidth: true
                text: qsTr("Clone...")
                icon.name: "vcs-clone"
                onClicked: {
                    appController.showToast(qsTr("Clone repository dialog opened"));
                    popup.close();
                }
            }
        }
    }
}
