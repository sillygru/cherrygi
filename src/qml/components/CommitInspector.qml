import QtQuick
import QtQuick.Controls as QQC2
import QtQuick.Layouts
import org.kde.kirigami as Kirigami
import org.kde.cherrygi
import "../style"

ColumnLayout {
    id: root
    spacing: 0

    property var commitData: appController.selectedCommitData
    property string activeFilePath: ""

    onCommitDataChanged: {
        if (commitData && commitData.files && commitData.files.length > 0) {
            activeFilePath = commitData.files[0].filePath;
            appController.diffModel.loadDiffForCommit(commitData.sha, activeFilePath);
        }
    }

    // Top Commit Info Card
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

            // Commit Title / Summary
            QQC2.Label {
                text: (root.commitData && root.commitData.summary) ? root.commitData.summary : qsTr("No commit selected")
                font.bold: true
                font.pixelSize: CherryStyle.basePixelSize + 2
                color: Kirigami.Theme.textColor
                Layout.fillWidth: true
                wrapMode: Text.Wrap
            }

            // Description
            QQC2.Label {
                text: (root.commitData && root.commitData.description) ? root.commitData.description : ""
                visible: root.commitData && root.commitData.description && root.commitData.description.length > 0
                font.pixelSize: CherryStyle.smallFont.pixelSize
                color: Kirigami.Theme.textColor
                Layout.fillWidth: true
                wrapMode: Text.Wrap
            }

            Kirigami.Separator {
                Layout.fillWidth: true
            }

            // Author & Commit Metadata Row
            RowLayout {
                Layout.fillWidth: true
                spacing: Kirigami.Units.mediumSpacing

                // Author Avatar
                Rectangle {
                    width: 34
                    height: 34
                    radius: 17
                    color: Kirigami.Theme.highlightColor

                    QQC2.Label {
                        anchors.centerIn: parent
                        text: (root.commitData && root.commitData.authorName && root.commitData.authorName.length > 0) ? root.commitData.authorName.charAt(0).toUpperCase() : "G"
                        font.bold: true
                        font.pixelSize: 14
                        color: Kirigami.Theme.highlightedTextColor
                    }
                }

                // Author details
                ColumnLayout {
                    spacing: 1

                    RowLayout {
                        spacing: 6

                        QQC2.Label {
                            text: (root.commitData && root.commitData.authorName) ? root.commitData.authorName : ""
                            font.bold: true
                            color: Kirigami.Theme.textColor
                        }

                        QQC2.Label {
                            text: "<" + ((root.commitData && root.commitData.authorEmail) ? root.commitData.authorEmail : "") + ">"
                            font.pixelSize: CherryStyle.smallFont.pixelSize
                            color: Kirigami.Theme.disabledTextColor
                        }
                    }

                    QQC2.Label {
                        text: qsTr("Committed %1 (%2)").arg((root.commitData && root.commitData.relativeTime) ? root.commitData.relativeTime : "").arg((root.commitData && root.commitData.timestamp) ? root.commitData.timestamp : "")
                        font.pixelSize: CherryStyle.smallFont.pixelSize - 1
                        color: Kirigami.Theme.disabledTextColor
                    }
                }

                Item { Layout.fillWidth: true }

                // Full SHA Badge with Copy button
                Rectangle {
                    implicitHeight: 28
                    implicitWidth: shaRow.implicitWidth + 12
                    radius: CherryStyle.radiusMedium
                    color: CherryStyle.surfaceCardElevated

                    RowLayout {
                        id: shaRow
                        anchors.centerIn: parent
                        spacing: Kirigami.Units.smallSpacing

                        QQC2.Label {
                            text: (root.commitData && root.commitData.shortSha) ? root.commitData.shortSha : ""
                            font: CherryStyle.codeFontBold
                            color: Kirigami.Theme.textColor
                        }

                        Kirigami.Icon {
                            source: "edit-copy"
                            width: 14
                            height: 14
                            color: Kirigami.Theme.disabledTextColor

                            MouseArea {
                                anchors.fill: parent
                                cursorShape: Qt.PointingHandCursor
                                onClicked: {
                                    appController.showToast(qsTr("Commit SHA copied to clipboard"));
                                }
                            }
                        }
                    }
                }

                // Actions Menu Button
                QQC2.Button {
                    text: qsTr("Revert Commit")
                    icon.name: "edit-undo"
                    implicitHeight: 28
                    onClicked: {
                        if (root.commitData && root.commitData.sha) {
                            appController.revertCommit(root.commitData.sha);
                        }
                    }
                }
            }
        }
    }

    // Splitter: Changed Files in Commit vs Diff Viewer
    QQC2.SplitView {
        Layout.fillWidth: true
        Layout.fillHeight: true
        orientation: Qt.Vertical

        // Changed Files List in this commit
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
                            text: qsTr("Changed Files (%1)").arg(root.commitData && root.commitData.files ? root.commitData.files.length : 0)
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
                        id: commitFilesListView
                        model: (root.commitData && root.commitData.files) ? root.commitData.files : []
                        spacing: 2

                        delegate: QQC2.ItemDelegate {
                            id: commitFileDelegate
                            width: commitFilesListView.width
                            height: 32

                            required property int index
                            required property var modelData

                            highlighted: root.activeFilePath === commitFileDelegate.modelData.filePath

                            background: Rectangle {
                                color: commitFileDelegate.highlighted ? CherryStyle.activeBackground : (commitFileDelegate.hovered ? CherryStyle.hoverBackground : "transparent")
                                radius: CherryStyle.radiusMedium

                                Rectangle {
                                    visible: commitFileDelegate.highlighted
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

                                Rectangle {
                                    width: 18
                                    height: 18
                                    radius: 3
                                    color: Qt.rgba(0.9, 0.65, 0.04, 0.15)
                                    border.color: Qt.rgba(0.9, 0.65, 0.04, 0.5)
                                    border.width: 1

                                    Kirigami.Icon {
                                        anchors.centerIn: parent
                                        source: "document-edit"
                                        width: 11
                                        height: 11
                                        color: "#e5a50a"
                                    }
                                }

                                QQC2.Label {
                                    text: commitFileDelegate.modelData.filePath
                                    font.pixelSize: CherryStyle.smallFont.pixelSize
                                    font.bold: commitFileDelegate.highlighted
                                    color: commitFileDelegate.highlighted ? Kirigami.Theme.highlightColor : Kirigami.Theme.textColor
                                    Layout.fillWidth: true
                                    elide: Text.ElideMiddle
                                }

                                RowLayout {
                                    spacing: 3

                                    Rectangle {
                                        visible: commitFileDelegate.modelData.additions > 0
                                        implicitWidth: cAddTag.implicitWidth + 8
                                        implicitHeight: 18
                                        radius: 4
                                        color: CherryStyle.additionBg
                                        border.color: Qt.rgba(CherryStyle.additionColor.r, CherryStyle.additionColor.g, CherryStyle.additionColor.b, 0.4)
                                        border.width: 1

                                        QQC2.Label {
                                            id: cAddTag
                                            anchors.centerIn: parent
                                            text: "+" + commitFileDelegate.modelData.additions
                                            font.pixelSize: CherryStyle.smallFont.pixelSize - 1
                                            font.bold: true
                                            color: CherryStyle.additionColor
                                        }
                                    }

                                    Rectangle {
                                        visible: commitFileDelegate.modelData.deletions > 0
                                        implicitWidth: cDelTag.implicitWidth + 8
                                        implicitHeight: 18
                                        radius: 4
                                        color: CherryStyle.deletionBg
                                        border.color: Qt.rgba(CherryStyle.deletionColor.r, CherryStyle.deletionColor.g, CherryStyle.deletionColor.b, 0.4)
                                        border.width: 1

                                        QQC2.Label {
                                            id: cDelTag
                                            anchors.centerIn: parent
                                            text: "-" + commitFileDelegate.modelData.deletions
                                            font.pixelSize: CherryStyle.smallFont.pixelSize - 1
                                            font.bold: true
                                            color: CherryStyle.deletionColor
                                        }
                                    }
                                }
                            }

                            onClicked: {
                                root.activeFilePath = commitFileDelegate.modelData.filePath;
                                if (root.commitData && root.commitData.sha) {
                                    appController.diffModel.loadDiffForCommit(root.commitData.sha, commitFileDelegate.modelData.filePath);
                                }
                            }
                        }
                    }
                }
            }
        }

        // Commit Diff Viewer
        DiffViewer {
            QQC2.SplitView.fillHeight: true
            filePath: root.activeFilePath
            isHistorical: true
        }
    }
}

