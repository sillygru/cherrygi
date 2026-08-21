import QtQuick
import QtQuick.Controls as QQC2
import QtQuick.Layouts
import org.kde.kirigami as Kirigami
import org.kde.cherrygi
import "../style"

ColumnLayout {
    id: root
    spacing: 0

    QQC2.Dialog {
        id: discardAllDialog
        title: qsTr("Discard all changes?")
        modal: true
        standardButtons: QQC2.Dialog.Yes | QQC2.Dialog.No
        parent: QQC2.Overlay.overlay
        anchors.centerIn: parent

        contentItem: QQC2.Label {
            text: qsTr("This will discard all staged and unstaged tracked changes and permanently delete untracked files and directories. This cannot be undone.")
            wrapMode: Text.WordWrap
            padding: Kirigami.Units.largeSpacing
            color: Kirigami.Theme.textColor
        }

        onAccepted: appController.discardAllChanges()
    }

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

    QQC2.Dialog {
        id: discardFileDialog
        property string targetFilePath: ""
        title: qsTr("Discard changes?")
        modal: true
        standardButtons: QQC2.Dialog.Yes | QQC2.Dialog.No
        parent: QQC2.Overlay.overlay
        anchors.centerIn: parent

        contentItem: QQC2.Label {
            text: qsTr("Discard all staged and unstaged changes for '%1'? This cannot be undone.").arg(discardFileDialog.targetFilePath)
            wrapMode: Text.WordWrap
            padding: Kirigami.Units.largeSpacing
            color: Kirigami.Theme.textColor
        }

        onAccepted: appController.discardFileChanges(targetFilePath)
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
            onTriggered: {
                discardFileDialog.targetFilePath = fileContextMenu.targetFilePath;
                discardFileDialog.open();
            }
        }

        QQC2.MenuItem {
            text: qsTr("Copy Relative Path")
            icon.name: "edit-copy"
            onTriggered: {
                appController.copyToClipboard(fileContextMenu.targetFilePath, qsTr("Path copied to clipboard"));
            }
        }
    }

    // Header bar context menu
    QQC2.Menu {
        id: changesHeaderContextMenu

        QQC2.MenuItem {
            text: qsTr("Discard All Changes...")
            icon.name: "edit-delete"
            enabled: !appController.isOperating && appController.changedFiles.count > 0
            onTriggered: discardAllDialog.open()
        }

        QQC2.MenuItem {
            text: qsTr("Stash All Changes")
            icon.name: "archive-insert"
            enabled: !appController.isOperating && appController.changedFiles.count > 0
            onTriggered: appController.stashChanges()
        }
    }

    // Header bar: Select all checkbox + Changed files count
    Rectangle {
        Layout.fillWidth: true
        height: 36
        color: CherryStyle.surfaceHeader

        TapHandler {
            acceptedButtons: Qt.RightButton
            enabled: appController.changedFiles.count > 0
            onTapped: changesHeaderContextMenu.popup()
        }

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
                icon.name: "view-refresh"
                icon.width: 14
                icon.height: 14
                enabled: !appController.isOperating
                QQC2.ToolTip.text: qsTr("Refresh repository (F5)")
                QQC2.ToolTip.visible: hovered
                onClicked: appController.refresh()
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
            reuseItems: true

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
                        color: CherryStyle.additionColor
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
                        color: CherryStyle.secondaryTextColor
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
                required property string oldFilePath
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
                        color: CherryStyle.accentColor
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
                        Layout.alignment: Qt.AlignVCenter
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
                        text: (fileDelegate.oldFilePath && fileDelegate.oldFilePath !== fileDelegate.filePath)
                            ? (fileDelegate.oldFilePath + " → " + fileDelegate.filePath)
                            : fileDelegate.filePath
                        font.pixelSize: CherryStyle.smallFont.pixelSize
                        font.bold: fileDelegate.highlighted
                        color: fileDelegate.highlighted ? CherryStyle.accentColor : Kirigami.Theme.textColor
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

                TapHandler {
                    acceptedButtons: Qt.RightButton
                    onTapped: {
                        fileContextMenu.targetFilePath = fileDelegate.filePath;
                        fileContextMenu.popup();
                    }
                }
            }
        }
    }

    // Stashed Changes Section (With expandable files view)
    Rectangle {
        id: stashSection
        Layout.fillWidth: true
        implicitHeight: stashColumn.implicitHeight + 4
        visible: appController.stashes.count > 0

        property bool isSelected: appController.selectedStashId !== ""
        property bool isExpanded: true

        color: isSelected ? CherryStyle.activeBackground : CherryStyle.surfaceCardElevated

        // Top separator
        Rectangle {
            anchors.top: parent.top
            anchors.left: parent.left  
            anchors.right: parent.right
            height: 1
            color: CherryStyle.borderColor
        }

        ColumnLayout {
            id: stashColumn
            anchors.fill: parent
            anchors.margins: 4
            spacing: 2

            // Main Stash Header Row
            Rectangle {
                Layout.fillWidth: true
                height: 36
                color: "transparent"

                RowLayout {
                    anchors.fill: parent
                    anchors.leftMargin: Kirigami.Units.smallSpacing
                    anchors.rightMargin: Kirigami.Units.smallSpacing
                    spacing: Kirigami.Units.smallSpacing

                    Kirigami.Icon {
                        source: "document-save"
                        width: 16
                        height: 16
                        Layout.alignment: Qt.AlignVCenter
                        color: CherryStyle.warningColor
                    }

                    QQC2.Label {
                        text: qsTr("Stashed Changes")
                        font.bold: true
                        font.pixelSize: CherryStyle.smallFont.pixelSize
                        color: stashSection.isSelected ? CherryStyle.accentColor : Kirigami.Theme.textColor
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

                    // Restore & Discard Quick Buttons
                    QQC2.Button {
                        text: qsTr("Restore")
                        icon.name: "edit-undo"
                        implicitHeight: 26
                        QQC2.ToolTip.text: qsTr("Restore stashed changes to working directory")
                        QQC2.ToolTip.visible: hovered
                        onClicked: appController.popStash("")
                    }

                    QQC2.ToolButton {
                        icon.name: "edit-delete"
                        icon.width: 14
                        icon.height: 14
                        implicitHeight: 26
                        implicitWidth: 26
                        QQC2.ToolTip.text: qsTr("Discard stash")
                        QQC2.ToolTip.visible: hovered
                        onClicked: appController.dropStash("")
                    }

                    QQC2.ToolButton {
                        icon.name: stashSection.isExpanded ? "go-up-symbolic" : "go-down-symbolic"
                        icon.width: 12
                        icon.height: 12
                        implicitHeight: 26
                        implicitWidth: 26
                        QQC2.ToolTip.text: stashSection.isExpanded ? qsTr("Collapse stashed files") : qsTr("Expand stashed files")
                        QQC2.ToolTip.visible: hovered
                        onClicked: {
                            stashSection.isExpanded = !stashSection.isExpanded;
                        }
                    }
                }

                MouseArea {
                    anchors.fill: parent
                    anchors.rightMargin: 150
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

            // Stashed files list
            ListView {
                id: stashedFilesList
                Layout.fillWidth: true
                implicitHeight: Math.min(count * 28, 120)
                visible: Boolean(stashSection.isExpanded && appController.selectedStashData && appController.selectedStashData.files && appController.selectedStashData.files.length > 0)
                model: (appController.selectedStashData && appController.selectedStashData.files) ? appController.selectedStashData.files : []
                clip: true
                spacing: 1

                delegate: QQC2.ItemDelegate {
                    width: stashedFilesList.width
                    height: 26

                    required property int index
                    required property var modelData

                    background: Rectangle {
                        color: hovered ? CherryStyle.hoverBackground : "transparent"
                        radius: CherryStyle.radiusSmall
                    }

                    contentItem: RowLayout {
                        anchors.fill: parent
                        anchors.leftMargin: Kirigami.Units.mediumSpacing
                        anchors.rightMargin: Kirigami.Units.smallSpacing
                        spacing: 4

                        Kirigami.Icon {
                            source: {
                                var isPathChange = Boolean(modelData.oldFilePath && modelData.oldFilePath !== modelData.filePath);
                                var st = modelData.status;
                                if (isPathChange || st === 3) return "arrow-right";
                                if (st === 1 || st === 4) return "list-add";
                                if (st === 2) return "list-remove";
                                return "document-edit";
                            }
                            width: 11
                            height: 11
                            Layout.alignment: Qt.AlignVCenter
                            color: {
                                var isPathChange = Boolean(modelData.oldFilePath && modelData.oldFilePath !== modelData.filePath);
                                var st = modelData.status;
                                if (isPathChange || st === 3) return CherryStyle.renamedColor;
                                if (st === 1 || st === 4) return CherryStyle.additionColor;
                                if (st === 2) return CherryStyle.deletionColor;
                                return CherryStyle.modifiedColor;
                            }
                        }

                        QQC2.Label {
                            text: (modelData.oldFilePath && modelData.oldFilePath !== modelData.filePath)
                                ? (modelData.oldFilePath + " → " + modelData.filePath)
                                : modelData.filePath
                            font.pixelSize: CherryStyle.smallFont.pixelSize - 1
                            color: Kirigami.Theme.textColor
                            elide: Text.ElideMiddle
                            Layout.fillWidth: true
                        }

                        QQC2.Label {
                            text: "+" + modelData.additions
                            font.pixelSize: CherryStyle.smallFont.pixelSize - 2
                            color: CherryStyle.additionColor
                            visible: modelData.additions > 0
                        }

                        QQC2.Label {
                            text: "-" + modelData.deletions
                            font.pixelSize: CherryStyle.smallFont.pixelSize - 2
                            color: CherryStyle.deletionColor
                            visible: modelData.deletions > 0
                        }
                    }

                    onClicked: {
                        appController.selectStash(appController.selectedStashId);
                        appController.diffModel.loadDiffForStash(appController.selectedStashId, modelData.filePath);
                    }
                }
            }
        }
    }
}

