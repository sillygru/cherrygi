import QtQuick
import QtQuick.Controls as QQC2
import QtQuick.Layouts
import org.kde.kirigami as Kirigami
import org.kde.cherrygi
import "../style"

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
            enabled: !appController.isOperating
            onTriggered: appController.popStash(stashContextMenu.targetStashId)
        }

        QQC2.MenuItem {
            text: qsTr("Discard Stash")
            icon.name: "edit-delete"
            enabled: !appController.isOperating
            onTriggered: appController.dropStash(stashContextMenu.targetStashId)
        }
    }

    // File context menu
    QQC2.Menu {
        id: fileContextMenu
        property string targetFilePath: ""

        QQC2.MenuItem {
            text: qsTr("Open in External Editor")
            icon.name: "accessories-text-editor"
            onTriggered: appController.openInEditor(fileContextMenu.targetFilePath)
        }

        QQC2.MenuItem {
            text: qsTr("Discard Changes...")
            icon.name: "edit-delete"
            enabled: !appController.isOperating
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
        color: CherryStyle.surfaceHeader

        RowLayout {
            anchors.fill: parent
            anchors.leftMargin: Kirigami.Units.smallSpacing + 2
            anchors.rightMargin: Kirigami.Units.smallSpacing + 2
            spacing: Kirigami.Units.smallSpacing

            QQC2.CheckBox {
                id: selectAllBox
                checked: appController.changedFiles.allSelected
                enabled: !appController.isOperating
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
                enabled: !appController.isOperating
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
            spacing: 2

            // Empty state placeholder
            Item {
                anchors.centerIn: parent
                visible: appController.changedFiles.count === 0
                width: parent.width - 40
                height: 140

                ColumnLayout {
                    anchors.centerIn: parent
                    spacing: Kirigami.Units.smallSpacing

                    Kirigami.Icon {
                        source: "dialog-ok"
                        width: 40
                        height: 40
                        Layout.alignment: Qt.AlignHCenter
                        color: Kirigami.Theme.positiveTextColor
                    }

                    QQC2.Label {
                        text: qsTr("No uncommitted changes")
                        font.bold: true
                        font.pixelSize: CherryStyle.basePixelSize
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

                highlighted: appController.selectedStashId === "" && appController.selectedFilePath === fileDelegate.filePath

                background: Rectangle {
                    color: fileDelegate.highlighted ? CherryStyle.activeBackground : (fileDelegate.hovered ? CherryStyle.hoverBackground : "transparent")
                    radius: CherryStyle.radiusMedium

                    // Left accent bar when active
                    Rectangle {
                        visible: fileDelegate.highlighted
                        anchors.left: parent.left
                        anchors.top: parent.top
                        anchors.bottom: parent.bottom
                        anchors.margins: 2
                        width: 2
                        radius: 1.5
                        color: Kirigami.Theme.highlightColor
                    }
                }

                contentItem: RowLayout {
                    anchors.fill: parent
                    anchors.leftMargin: Kirigami.Units.smallSpacing
                    anchors.rightMargin: Kirigami.Units.smallSpacing
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
                        width: 20
                        height: 20
                        radius: 4
                        color: Qt.rgba(fileDelegate.statusColor.r, fileDelegate.statusColor.g, fileDelegate.statusColor.b, 0.15)
                        border.color: Qt.rgba(fileDelegate.statusColor.r, fileDelegate.statusColor.g, fileDelegate.statusColor.b, 0.5)
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
                        font.pixelSize: CherryStyle.smallFont.pixelSize
                        font.bold: fileDelegate.highlighted
                        color: fileDelegate.highlighted ? Kirigami.Theme.highlightColor : Kirigami.Theme.textColor
                        elide: Text.ElideMiddle
                        Layout.fillWidth: true
                    }

                    // Additions / Deletions count tag
                    RowLayout {
                        spacing: 3
                        visible: fileDelegate.additions > 0 || fileDelegate.deletions > 0

                        Rectangle {
                            visible: fileDelegate.additions > 0
                            implicitWidth: addTag.implicitWidth + 8
                            implicitHeight: 18
                            radius: 4
                            color: CherryStyle.additionBg
                            border.color: Qt.rgba(CherryStyle.additionColor.r, CherryStyle.additionColor.g, CherryStyle.additionColor.b, 0.4)
                            border.width: 1

                            QQC2.Label {
                                id: addTag
                                anchors.centerIn: parent
                                text: "+" + fileDelegate.additions
                                font.pixelSize: CherryStyle.smallFont.pixelSize - 1
                                font.bold: true
                                color: CherryStyle.additionColor
                            }
                        }

                        Rectangle {
                            visible: fileDelegate.deletions > 0
                            implicitWidth: delTag.implicitWidth + 8
                            implicitHeight: 18
                            radius: 4
                            color: CherryStyle.deletionBg
                            border.color: Qt.rgba(CherryStyle.deletionColor.r, CherryStyle.deletionColor.g, CherryStyle.deletionColor.b, 0.4)
                            border.width: 1

                            QQC2.Label {
                                id: delTag
                                anchors.centerIn: parent
                                text: "-" + fileDelegate.deletions
                                font.pixelSize: CherryStyle.smallFont.pixelSize - 1
                                font.bold: true
                                color: CherryStyle.deletionColor
                            }
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

    // Stashed Changes Section (Clean non-overlapping row with quick actions)
    Rectangle {
        id: stashSection
        Layout.fillWidth: true
        height: 42
        visible: appController.stashes.count > 0

        property bool isSelected: appController.selectedStashId !== ""
        property bool isHovered: stashMouseArea.containsMouse

        color: isSelected ? CherryStyle.activeBackground : (isHovered ? CherryStyle.hoverBackground : CherryStyle.surfaceCardElevated)

        // Top separator
        Rectangle {
            anchors.top: parent.top
            anchors.left: parent.left  
            anchors.right: parent.right
            height: 1
            color: CherryStyle.borderColor
        }

        RowLayout {
            anchors.fill: parent
            anchors.leftMargin: Kirigami.Units.smallSpacing + 2
            anchors.rightMargin: Kirigami.Units.smallSpacing + 2
            spacing: Kirigami.Units.smallSpacing

            Kirigami.Icon {
                source: "document-save"
                width: 16
                height: 16
                color: "#e5a50a"
            }

            QQC2.Label {
                text: qsTr("Stashed Changes")
                font.bold: true
                font.pixelSize: CherryStyle.smallFont.pixelSize
                color: stashSection.isSelected ? Kirigami.Theme.highlightColor : Kirigami.Theme.textColor
                elide: Text.ElideRight
                Layout.fillWidth: true
            }

            // Stash count pill
            Rectangle {
                implicitWidth: stashCountLabel.implicitWidth + 10
                implicitHeight: 18
                radius: 9
                color: CherryStyle.modifiedBg
                border.color: CherryStyle.modifiedColor
                border.width: 1

                QQC2.Label {
                    id: stashCountLabel
                    anchors.centerIn: parent
                    text: "" + appController.stashes.count
                    font.pixelSize: CherryStyle.smallFont.pixelSize - 1
                    font.bold: true
                    color: CherryStyle.modifiedColor
                }
            }

            // Compact Action Buttons with no text collision
            QQC2.Button {
                text: qsTr("Restore")
                icon.name: "edit-undo"
                implicitHeight: 28
                QQC2.ToolTip.text: qsTr("Restore stashed changes to working directory")
                QQC2.ToolTip.visible: hovered
                onClicked: appController.popStash("")
            }

            QQC2.ToolButton {
                icon.name: "edit-delete"
                icon.width: 14
                icon.height: 14
                implicitHeight: 28
                implicitWidth: 28
                QQC2.ToolTip.text: qsTr("Discard stash")
                QQC2.ToolTip.visible: hovered
                onClicked: appController.dropStash("")
            }

            QQC2.ToolButton {
                icon.name: "go-next-symbolic"
                icon.width: 12
                icon.height: 12
                implicitHeight: 28
                implicitWidth: 28
                QQC2.ToolTip.text: qsTr("View stashed changes in detail")
                QQC2.ToolTip.visible: hovered
                onClicked: {
                    if (appController.selectedStashId !== "") {
                        appController.clearStashSelection();
                    } else {
                        appController.selectStash("");
                    }
                }
            }
        }

        MouseArea {
            id: stashMouseArea
            anchors.fill: parent
            anchors.rightMargin: 150 // leave room for buttons
            hoverEnabled: true
            cursorShape: Qt.PointingHandCursor
            onClicked: {
                if (appController.selectedStashId !== "") {
                    appController.clearStashSelection();
                } else {
                    appController.selectStash("");
                }
            }
        }
    }
}

