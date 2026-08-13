import QtQuick
import QtQuick.Controls as QQC2
import QtQuick.Layouts
import org.kde.kirigami as Kirigami
import org.kde.cherrygi

ColumnLayout {
    id: root
    spacing: 0

    // Stash detail / drop popup
    QQC2.Menu {
        id: stashContextMenu
        property string targetStashId: ""

        QQC2.MenuItem {
            text: qsTr("Restore Stash")
            icon.name: "edit-undo"
            onTriggered: appController.popStash(stashContextMenu.targetStashId)
        }

        QQC2.MenuItem {
            text: qsTr("Drop Stash")
            icon.name: "edit-delete"
            onTriggered: appController.dropStash(stashContextMenu.targetStashId)
        }
    }

    // File context menu
    QQC2.Menu {
        id: fileContextMenu
        property string targetFilePath: ""

        QQC2.MenuItem {
            text: qsTr("Discard Changes...")
            icon.name: "edit-delete"
            onTriggered: appController.discardFileChanges(fileContextMenu.targetFilePath)
        }

        QQC2.MenuItem {
            text: qsTr("Copy Relative Path")
            icon.name: "edit-copy"
            onTriggered: {
                appController.showToast(qsTr("Path copied to clipboard"));
            }
        }
    }

    // Header bar: Select all checkbox + Changed files count
    Rectangle {
        Layout.fillWidth: true
        height: 36
        color: CherryStyle.cardBackground
        border.color: CherryStyle.subtleBorderColor
        border.width: 1

        RowLayout {
            anchors.fill: parent
            anchors.leftMargin: Kirigami.Units.smallSpacing
            anchors.rightMargin: Kirigami.Units.smallSpacing
            spacing: Kirigami.Units.smallSpacing

            QQC2.CheckBox {
                id: selectAllBox
                checked: appController.changedFiles.allSelected
                checkState: appController.changedFiles.hasPartialSelection ? Qt.PartiallyChecked : (appController.changedFiles.allSelected ? Qt.Checked : Qt.Unchecked)
                onClicked: {
                    appController.changedFiles.selectAll(checkState === Qt.Checked);
                }
            }

            QQC2.Label {
                text: appController.changedFiles.count > 0 ?
                      qsTr("%1 changed files").arg(appController.changedFiles.count) :
                      qsTr("No local changes")
                font.bold: true
                font.pixelSize: CherryStyle.smallFont.pixelSize
                color: Kirigami.Theme.textColor
                Layout.fillWidth: true
            }

            QQC2.ToolButton {
                icon.name: "edit-delete"
                icon.width: 14
                icon.height: 14
                visible: appController.changedFiles.count > 0
                QQC2.ToolTip.text: qsTr("Discard all changes")
                QQC2.ToolTip.visible: hovered
                onClicked: appController.discardAllChanges()
            }
        }
    }

    // Changed Files List View
    QQC2.ScrollView {
        Layout.fillWidth: true
        Layout.fillHeight: true
        clip: true

        ListView {
            id: fileListView
            model: appController.changedFiles
            spacing: 1

            // Empty state placeholder
            Item {
                anchors.centerIn: parent
                visible: appController.changedFiles.count === 0
                width: parent.width - 40
                height: 120

                ColumnLayout {
                    anchors.centerIn: parent
                    spacing: Kirigami.Units.smallSpacing

                    Kirigami.Icon {
                        source: "dialog-ok"
                        width: 36
                        height: 36
                        Layout.alignment: Qt.AlignHCenter
                        color: Kirigami.Theme.positiveTextColor
                    }

                    QQC2.Label {
                        text: qsTr("No uncommitted changes")
                        font.bold: true
                        Layout.alignment: Qt.AlignHCenter
                        color: Kirigami.Theme.textColor
                    }

                    QQC2.Label {
                        text: qsTr("Your working directory is clean.")
                        font.pixelSize: CherryStyle.smallFont.pixelSize
                        Layout.alignment: Qt.AlignHCenter
                        color: Kirigami.Theme.disabledTextColor
                    }
                }
            }

            delegate: QQC2.ItemDelegate {
                id: fileDelegate
                width: fileListView.width
                height: 36

                required property int index
                required property string fileId
                required property string filePath
                required property string fileName
                required property string fileDir
                required property int status
                required property string statusText
                required property string statusIcon
                required property color statusColor
                required property bool isSelected
                required property int additions
                required property int deletions

                highlighted: appController.selectedFilePath === fileDelegate.filePath

                background: Rectangle {
                    color: fileDelegate.highlighted ? CherryStyle.activeBackground : (fileDelegate.hovered ? CherryStyle.hoverBackground : "transparent")
                    radius: CherryStyle.radiusSmall
                    border.color: fileDelegate.highlighted ? Kirigami.Theme.highlightColor : "transparent"
                    border.width: fileDelegate.highlighted ? 1 : 0
                }

                contentItem: RowLayout {
                    spacing: Kirigami.Units.smallSpacing

                    // Inclusion Checkbox
                    QQC2.CheckBox {
                        checked: fileDelegate.isSelected
                        onClicked: {
                            appController.changedFiles.toggleSelected(fileDelegate.index);
                        }
                    }

                    // File status badge icon
                    Rectangle {
                        width: 18
                        height: 18
                        radius: 3
                        color: Qt.rgba(0, 0, 0, 0.05)
                        border.color: fileDelegate.statusColor
                        border.width: 1

                        Kirigami.Icon {
                            anchors.centerIn: parent
                            source: fileDelegate.statusIcon
                            width: 12
                            height: 12
                            color: fileDelegate.statusColor
                        }
                    }

                    // File path
                    QQC2.Label {
                        text: fileDelegate.filePath
                        font.pixelSize: CherryStyle.basePixelSize - 1
                        color: fileDelegate.highlighted ? Kirigami.Theme.highlightColor : Kirigami.Theme.textColor
                        elide: Text.ElideMiddle
                        Layout.fillWidth: true
                    }

                    // Additions / Deletions count tag
                    RowLayout {
                        spacing: 2
                        visible: fileDelegate.additions > 0 || fileDelegate.deletions > 0

                        QQC2.Label {
                            text: "+" + fileDelegate.additions
                            font.pixelSize: CherryStyle.smallFont.pixelSize - 1
                            font.bold: true
                            color: CherryStyle.additionColor
                            visible: fileDelegate.additions > 0
                        }

                        QQC2.Label {
                            text: "-" + fileDelegate.deletions
                            font.pixelSize: CherryStyle.smallFont.pixelSize - 1
                            font.bold: true
                            color: CherryStyle.deletionColor
                            visible: fileDelegate.deletions > 0
                        }
                    }
                }

                onClicked: {
                    appController.selectFileForDiff(fileDelegate.filePath);
                }

                MouseArea {
                    anchors.fill: parent
                    acceptedButtons: Qt.RightButton
                    onClicked: {
                        fileContextMenu.targetFilePath = fileDelegate.filePath;
                        fileContextMenu.popup();
                    }
                }
            }
        }
    }

    // Stashed Changes Section
    Rectangle {
        Layout.fillWidth: true
        height: 38
        visible: appController.stashes.count > 0
        color: CherryStyle.cardBackground
        border.color: CherryStyle.borderColor
        border.width: 1

        RowLayout {
            anchors.fill: parent
            anchors.leftMargin: Kirigami.Units.smallSpacing
            anchors.rightMargin: Kirigami.Units.smallSpacing
            spacing: Kirigami.Units.smallSpacing

            Kirigami.Icon {
                source: "document-save"
                width: Kirigami.Units.iconSizes.small
                height: width
                color: "#e5a50a"
            }

            QQC2.Label {
                text: qsTr("Stashed Changes (%1)").arg(appController.stashes.count)
                font.bold: true
                font.pixelSize: CherryStyle.smallFont.pixelSize
                color: Kirigami.Theme.textColor
                Layout.fillWidth: true
            }

            QQC2.Button {
                text: qsTr("Restore")
                onClicked: appController.popStash()
            }

            QQC2.ToolButton {
                icon.name: "arrow-right"
                icon.width: 14
                icon.height: 14
                onClicked: {
                    stashContextMenu.targetStashId = "";
                    stashContextMenu.popup();
                }
            }
        }
    }
}
