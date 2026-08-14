import QtQuick
import QtQuick.Controls as QQC2
import QtQuick.Layouts
import org.kde.kirigami as Kirigami
import org.kde.cherrygi
import "../style"

ColumnLayout {
    id: root
    spacing: 0

    property var stashData: appController.selectedStashData
    property string activeFilePath: ""

    onStashDataChanged: {
        if (stashData && stashData.files && stashData.files.length > 0) {
            activeFilePath = stashData.files[0].filePath;
            appController.diffModel.loadDiffForStash(stashData.stashId, activeFilePath);
        }
    }

    // Top Stash Summary & Actions Card
    Rectangle {
        Layout.fillWidth: true
        implicitHeight: headerLayout.implicitHeight + Kirigami.Units.mediumSpacing * 2
        color: CherryStyle.surfaceHeader
        
        Rectangle {
            anchors.bottom: parent.bottom
            anchors.left: parent.left
            anchors.right: parent.right
            height: 1
            color: CherryStyle.borderColor
        }

        ColumnLayout {
            id: headerLayout
            anchors.fill: parent
            anchors.margins: Kirigami.Units.mediumSpacing
            spacing: Kirigami.Units.smallSpacing

            // Title & Close Button
            RowLayout {
                Layout.fillWidth: true
                spacing: Kirigami.Units.smallSpacing

                Kirigami.Icon {
                    source: "document-save"
                    width: 20
                    height: 20
                    Layout.alignment: Qt.AlignVCenter
                    color: CherryStyle.warningColor
                }

                QQC2.Label {
                    text: (root.stashData && root.stashData.message) ? root.stashData.message : qsTr("Stashed Changes")
                    font.bold: true
                    font.pixelSize: CherryStyle.basePixelSize + 2
                    color: Kirigami.Theme.textColor
                    Layout.fillWidth: true
                    wrapMode: Text.Wrap
                }

                QQC2.ToolButton {
                    icon.name: "window-close"
                    QQC2.ToolTip.text: qsTr("Close Stash View")
                    QQC2.ToolTip.visible: hovered
                    onClicked: appController.clearStashSelection()
                }
            }

            // Stash metadata (branch, timestamp)
            RowLayout {
                Layout.fillWidth: true
                spacing: Kirigami.Units.mediumSpacing

                RowLayout {
                    spacing: 4
                    Kirigami.Icon {
                        source: "vcs-branch"
                        width: 14
                        height: 14
                        Layout.alignment: Qt.AlignVCenter
                        color: CherryStyle.secondaryTextColor
                    }
                    QQC2.Label {
                        text: qsTr("On %1").arg((root.stashData && root.stashData.branchName) ? root.stashData.branchName : appController.currentBranchName)
                        font.pixelSize: CherryStyle.smallFont.pixelSize
                        color: CherryStyle.secondaryTextColor
                    }
                }

                QQC2.Label {
                    text: "•"
                    font.pixelSize: CherryStyle.smallFont.pixelSize
                    color: CherryStyle.secondaryTextColor
                }

                QQC2.Label {
                    text: qsTr("Created %1").arg((root.stashData && root.stashData.timestamp) ? root.stashData.timestamp : "")
                    font.pixelSize: CherryStyle.smallFont.pixelSize
                    color: CherryStyle.secondaryTextColor
                }

                Item { Layout.fillWidth: true }

                // Restore & Discard Action Buttons
                RowLayout {
                    spacing: Kirigami.Units.smallSpacing

                    QQC2.Button {
                        text: qsTr("Discard...")
                        icon.name: "edit-delete"
                        implicitHeight: 28
                        enabled: !appController.isOperating
                        onClicked: {
                            if (root.stashData && root.stashData.stashId) {
                                appController.dropStash(root.stashData.stashId);
                            }
                        }
                    }

                    QQC2.Button {
                        text: qsTr("Restore Stash")
                        icon.name: "edit-undo"
                        highlighted: true
                        implicitHeight: 28
                        enabled: !appController.isOperating
                        onClicked: {
                            if (root.stashData && root.stashData.stashId) {
                                appController.popStash(root.stashData.stashId);
                            }
                        }
                    }
                }
            }
        }
    }

    // Splitter: Changed Files in Stash vs Diff Viewer
    QQC2.SplitView {
        Layout.fillWidth: true
        Layout.fillHeight: true
        orientation: Qt.Vertical

        // Stashed Files List
        Rectangle {
            QQC2.SplitView.preferredHeight: 120
            QQC2.SplitView.minimumHeight: 80
            color: CherryStyle.surfaceSidebar

            ColumnLayout {
                anchors.fill: parent
                spacing: 0

                // Header
                Rectangle {
                    Layout.fillWidth: true
                    height: 30
                    color: CherryStyle.surfaceCardElevated

                    RowLayout {
                        anchors.fill: parent
                        anchors.leftMargin: Kirigami.Units.smallSpacing + 2
                        anchors.rightMargin: Kirigami.Units.smallSpacing + 2

                        QQC2.Label {
                            text: qsTr("Stashed Files (%1)").arg(root.stashData && root.stashData.files ? root.stashData.files.length : 0)
                            font.bold: true
                            font.pixelSize: CherryStyle.smallFont.pixelSize
                            color: CherryStyle.secondaryTextColor
                        }
                    }
                }

                QQC2.ScrollView {
                    Layout.fillWidth: true
                    Layout.fillHeight: true
                    clip: true

                    ListView {
                        id: stashFilesListView
                        model: (root.stashData && root.stashData.files) ? root.stashData.files : []
                        spacing: 2
                        reuseItems: true

                        delegate: QQC2.ItemDelegate {
                            id: stashFileDelegate
                            width: stashFilesListView.width
                            height: 32

                            required property int index
                            required property var modelData

                            highlighted: root.activeFilePath === stashFileDelegate.modelData.filePath

                            background: Rectangle {
                                color: stashFileDelegate.highlighted ? CherryStyle.activeBackground : (stashFileDelegate.hovered ? CherryStyle.hoverBackground : "transparent")
                                radius: CherryStyle.radiusMedium

                                Rectangle {
                                    visible: stashFileDelegate.highlighted
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

                                Rectangle {
                                    width: 18
                                    height: 18
                                    Layout.alignment: Qt.AlignVCenter
                                    radius: 3
                                    color: {
                                        var isPathChange = Boolean(stashFileDelegate.modelData.oldFilePath && stashFileDelegate.modelData.oldFilePath !== stashFileDelegate.modelData.filePath);
                                        var st = stashFileDelegate.modelData.status;
                                        if (isPathChange || st === 3) return CherryStyle.renamedBg;
                                        if (st === 1 || st === 4) return CherryStyle.additionBg;
                                        if (st === 2) return CherryStyle.deletionBg;
                                        return CherryStyle.modifiedBg;
                                    }
                                    border.color: {
                                        var isPathChange = Boolean(stashFileDelegate.modelData.oldFilePath && stashFileDelegate.modelData.oldFilePath !== stashFileDelegate.modelData.filePath);
                                        var st = stashFileDelegate.modelData.status;
                                        if (isPathChange || st === 3) return Qt.rgba(CherryStyle.renamedColor.r, CherryStyle.renamedColor.g, CherryStyle.renamedColor.b, 0.5);
                                        if (st === 1 || st === 4) return Qt.rgba(CherryStyle.additionColor.r, CherryStyle.additionColor.g, CherryStyle.additionColor.b, 0.5);
                                        if (st === 2) return Qt.rgba(CherryStyle.deletionColor.r, CherryStyle.deletionColor.g, CherryStyle.deletionColor.b, 0.5);
                                        return Qt.rgba(CherryStyle.modifiedColor.r, CherryStyle.modifiedColor.g, CherryStyle.modifiedColor.b, 0.5);
                                    }
                                    border.width: 1

                                    Kirigami.Icon {
                                        anchors.centerIn: parent
                                        source: {
                                            var isPathChange = Boolean(stashFileDelegate.modelData.oldFilePath && stashFileDelegate.modelData.oldFilePath !== stashFileDelegate.modelData.filePath);
                                            var st = stashFileDelegate.modelData.status;
                                            if (isPathChange || st === 3) return "arrow-right";
                                            if (st === 1 || st === 4) return "list-add";
                                            if (st === 2) return "list-remove";
                                            return "document-edit";
                                        }
                                        width: 11
                                        height: 11
                                        color: {
                                            var isPathChange = Boolean(stashFileDelegate.modelData.oldFilePath && stashFileDelegate.modelData.oldFilePath !== stashFileDelegate.modelData.filePath);
                                            var st = stashFileDelegate.modelData.status;
                                            if (isPathChange || st === 3) return CherryStyle.renamedColor;
                                            if (st === 1 || st === 4) return CherryStyle.additionColor;
                                            if (st === 2) return CherryStyle.deletionColor;
                                            return CherryStyle.modifiedColor;
                                        }
                                    }
                                }

                                QQC2.Label {
                                    text: (stashFileDelegate.modelData.oldFilePath && stashFileDelegate.modelData.oldFilePath !== stashFileDelegate.modelData.filePath)
                                        ? (stashFileDelegate.modelData.oldFilePath + " → " + stashFileDelegate.modelData.filePath)
                                        : stashFileDelegate.modelData.filePath
                                    font.pixelSize: CherryStyle.smallFont.pixelSize
                                    font.bold: stashFileDelegate.highlighted
                                    color: stashFileDelegate.highlighted ? CherryStyle.accentColor : Kirigami.Theme.textColor
                                    Layout.fillWidth: true
                                    elide: Text.ElideMiddle
                                }

                                RowLayout {
                                    spacing: 3

                                    Rectangle {
                                        visible: stashFileDelegate.modelData.additions > 0
                                        implicitWidth: sAddTag.implicitWidth + 8
                                        implicitHeight: 18
                                        radius: 4
                                        color: CherryStyle.additionBg
                                        border.color: Qt.rgba(CherryStyle.additionColor.r, CherryStyle.additionColor.g, CherryStyle.additionColor.b, 0.4)
                                        border.width: 1

                                        QQC2.Label {
                                            id: sAddTag
                                            anchors.centerIn: parent
                                            text: "+" + stashFileDelegate.modelData.additions
                                            font.pixelSize: CherryStyle.smallFont.pixelSize - 1
                                            font.bold: true
                                            color: CherryStyle.additionColor
                                        }
                                    }

                                    Rectangle {
                                        visible: stashFileDelegate.modelData.deletions > 0
                                        implicitWidth: sDelTag.implicitWidth + 8
                                        implicitHeight: 18
                                        radius: 4
                                        color: CherryStyle.deletionBg
                                        border.color: Qt.rgba(CherryStyle.deletionColor.r, CherryStyle.deletionColor.g, CherryStyle.deletionColor.b, 0.4)
                                        border.width: 1

                                        QQC2.Label {
                                            id: sDelTag
                                            anchors.centerIn: parent
                                            text: "-" + stashFileDelegate.modelData.deletions
                                            font.pixelSize: CherryStyle.smallFont.pixelSize - 1
                                            font.bold: true
                                            color: CherryStyle.deletionColor
                                        }
                                    }
                                }
                            }

                            onClicked: {
                                root.activeFilePath = stashFileDelegate.modelData.filePath;
                                if (root.stashData && root.stashData.stashId) {
                                    appController.diffModel.loadDiffForStash(root.stashData.stashId, stashFileDelegate.modelData.filePath);
                                }
                            }
                        }
                    }
                }
            }
        }

        // Stashed Diff Viewer
        DiffViewer {
            QQC2.SplitView.fillHeight: true
            filePath: root.activeFilePath
            isHistorical: true
        }
    }
}

