import QtQuick
import QtQuick.Controls as QQC2
import QtQuick.Layouts
import org.kde.kirigami as Kirigami
import org.kde.cherrygi

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
        implicitHeight: headerLayout.implicitHeight + Kirigami.Units.smallSpacing * 2
        color: CherryStyle.cardBackground
        border.color: CherryStyle.borderColor
        border.width: 1

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
                    width: 22
                    height: 22
                    color: "#e5a50a"
                }

                QQC2.Label {
                    text: root.stashData.message ? root.stashData.message : qsTr("Stashed Changes")
                    font.bold: true
                    font.pixelSize: CherryStyle.basePixelSize + 3
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
                        color: Kirigami.Theme.disabledTextColor
                    }
                    QQC2.Label {
                        text: qsTr("On %1").arg(root.stashData.branchName ? root.stashData.branchName : appController.currentBranchName)
                        font.pixelSize: CherryStyle.smallFont.pixelSize
                        color: Kirigami.Theme.disabledTextColor
                    }
                }

                QQC2.Label {
                    text: "•"
                    font.pixelSize: CherryStyle.smallFont.pixelSize
                    color: Kirigami.Theme.disabledTextColor
                }

                QQC2.Label {
                    text: qsTr("Created %1").arg(root.stashData.timestamp ? root.stashData.timestamp : "")
                    font.pixelSize: CherryStyle.smallFont.pixelSize
                    color: Kirigami.Theme.disabledTextColor
                }

                Item { Layout.fillWidth: true }

                // Restore & Discard Action Buttons
                RowLayout {
                    spacing: Kirigami.Units.smallSpacing

                    QQC2.Button {
                        text: qsTr("Discard...")
                        icon.name: "edit-delete"
                        onClicked: {
                            if (root.stashData.stashId) {
                                appController.dropStash(root.stashData.stashId);
                            }
                        }
                    }

                    QQC2.Button {
                        text: qsTr("Restore Stash")
                        icon.name: "edit-undo"
                        highlighted: true
                        onClicked: {
                            if (root.stashData.stashId) {
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
            color: Kirigami.Theme.backgroundColor
            border.color: CherryStyle.borderColor
            border.width: 1

            ColumnLayout {
                anchors.fill: parent
                spacing: 0

                // Header
                Rectangle {
                    Layout.fillWidth: true
                    height: 28
                    color: CherryStyle.cardBackground
                    border.color: CherryStyle.subtleBorderColor
                    border.width: 1

                    RowLayout {
                        anchors.fill: parent
                        anchors.leftMargin: Kirigami.Units.smallSpacing
                        anchors.rightMargin: Kirigami.Units.smallSpacing

                        QQC2.Label {
                            text: qsTr("Stashed Files (%1)").arg(root.stashData.files ? root.stashData.files.length : 0)
                            font.bold: true
                            font.pixelSize: CherryStyle.smallFont.pixelSize
                            color: Kirigami.Theme.disabledTextColor
                        }
                    }
                }

                QQC2.ScrollView {
                    Layout.fillWidth: true
                    Layout.fillHeight: true
                    clip: true

                    ListView {
                        id: stashFilesListView
                        model: root.stashData.files ? root.stashData.files : []
                        spacing: 1

                        delegate: QQC2.ItemDelegate {
                            id: stashFileDelegate
                            width: stashFilesListView.width
                            height: 32

                            required property int index
                            required property var modelData

                            highlighted: root.activeFilePath === stashFileDelegate.modelData.filePath

                            background: Rectangle {
                                color: stashFileDelegate.highlighted ? CherryStyle.activeBackground : (stashFileDelegate.hovered ? CherryStyle.hoverBackground : "transparent")
                                radius: CherryStyle.radiusSmall
                                border.color: stashFileDelegate.highlighted ? Kirigami.Theme.highlightColor : "transparent"
                                border.width: stashFileDelegate.highlighted ? 1 : 0
                            }

                            contentItem: RowLayout {
                                spacing: Kirigami.Units.smallSpacing

                                Kirigami.Icon {
                                    source: "document-edit"
                                    width: 14
                                    height: 14
                                    color: "#e5a50a"
                                }

                                QQC2.Label {
                                    text: stashFileDelegate.modelData.filePath
                                    font.pixelSize: CherryStyle.smallFont.pixelSize
                                    color: stashFileDelegate.highlighted ? Kirigami.Theme.highlightColor : Kirigami.Theme.textColor
                                    Layout.fillWidth: true
                                    elide: Text.ElideMiddle
                                }

                                RowLayout {
                                    spacing: 3
                                    QQC2.Label {
                                        text: "+" + stashFileDelegate.modelData.additions
                                        font.pixelSize: CherryStyle.smallFont.pixelSize - 1
                                        font.bold: true
                                        color: CherryStyle.additionColor
                                        visible: stashFileDelegate.modelData.additions > 0
                                    }
                                    QQC2.Label {
                                        text: "-" + stashFileDelegate.modelData.deletions
                                        font.pixelSize: CherryStyle.smallFont.pixelSize - 1
                                        font.bold: true
                                        color: CherryStyle.deletionColor
                                        visible: stashFileDelegate.modelData.deletions > 0
                                    }
                                }
                            }

                            onClicked: {
                                root.activeFilePath = stashFileDelegate.modelData.filePath;
                                appController.diffModel.loadDiffForStash(root.stashData.stashId, stashFileDelegate.modelData.filePath);
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
