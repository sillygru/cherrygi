import QtQuick
import QtQuick.Controls as QQC2
import QtQuick.Layouts
import org.kde.kirigami as Kirigami
import org.kde.cherrygi
import "../style"

ColumnLayout {
    id: root
    spacing: 0

    // Generate a stable hue-based avatar color from the author name
    function avatarColor(name) {
        if (!name || name.length === 0) return CherryStyle.secondaryTextColor;
        var hash = 0;
        for (var i = 0; i < name.length; i++) {
            hash = name.charCodeAt(i) + ((hash << 5) - hash);
        }
        var hue = Math.abs(hash % 360);
        return Qt.hsla(hue / 360.0, 0.55, 0.45, 1.0);
    }

    property var commitData: appController.selectedCommitData
    property string activeFilePath: ""

    onCommitDataChanged: {
        if (commitData && commitData.files && commitData.files.length > 0) {
            var firstFile = commitData.files[0];
            activeFilePath = firstFile.filePath;
            if (appController.diffModel.filePath !== activeFilePath || appController.diffModel.commitSha !== commitData.sha) {
                appController.diffModel.loadDiffForCommit(commitData.sha, firstFile.filePath, firstFile.oldFilePath || "");
            }
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
                visible: Boolean(root.commitData && root.commitData.description && root.commitData.description.length > 0)
                font.pixelSize: CherryStyle.smallFont.pixelSize
                color: Kirigami.Theme.textColor
                Layout.fillWidth: true
                wrapMode: Text.Wrap
            }

            // Tags Row
            RowLayout {
                visible: Boolean(root.commitData && root.commitData.tags && root.commitData.tags.length > 0)
                Layout.fillWidth: true
                spacing: Kirigami.Units.smallSpacing

                Kirigami.Icon {
                    source: "tag"
                    implicitWidth: 14
                    implicitHeight: 14
                    color: CherryStyle.secondaryTextColor
                }

                Repeater {
                    model: (root.commitData && root.commitData.tags) ? root.commitData.tags : []
                    Rectangle {
                        implicitWidth: inspectorTagRow.implicitWidth + 10
                        implicitHeight: 22
                        radius: CherryStyle.radiusSmall
                        color: CherryStyle.surfaceCardElevated
                        border.color: CherryStyle.borderColor
                        border.width: 1

                        RowLayout {
                            id: inspectorTagRow
                            anchors.centerIn: parent
                            spacing: 4

                            QQC2.Label {
                                text: modelData
                                font.pixelSize: CherryStyle.smallFont.pixelSize
                                font.bold: true
                                color: Kirigami.Theme.textColor
                            }
                        }
                    }
                }

                Item { Layout.fillWidth: true }
            }

            Kirigami.Separator {
                Layout.fillWidth: true
            }

            // Author & Commit Metadata Row
            RowLayout {
                Layout.fillWidth: true
                spacing: Kirigami.Units.mediumSpacing

                // Author Avatar
                CherryAvatar {
                    implicitWidth: 34
                    implicitHeight: 34
                    name: (root.commitData && root.commitData.authorName) ? root.commitData.authorName : ""
                    source: (root.commitData && root.commitData.authorAvatarUrl) ? root.commitData.authorAvatarUrl : ""
                    color: avatarColor(root.commitData && root.commitData.authorName ? root.commitData.authorName : "")
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
                            color: CherryStyle.secondaryTextColor
                        }
                    }

                    QQC2.Label {
                        text: qsTr("Committed %1 (%2)").arg((root.commitData && root.commitData.relativeTime) ? root.commitData.relativeTime : "").arg((root.commitData && root.commitData.timestamp) ? root.commitData.timestamp : "")
                        font.pixelSize: CherryStyle.smallFont.pixelSize - 1
                        color: CherryStyle.secondaryTextColor
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
                            color: CherryStyle.secondaryTextColor

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

                // Actions Menu Button with crystal clear tooltip and iconography
                QQC2.Button {
                    text: qsTr("Revert Commit")
                    icon.name: "vcs-diff"
                    implicitHeight: 28
                    enabled: !appController.isOperating
                    QQC2.ToolTip.text: qsTr("Create a new commit that reverts the changes introduced in this commit")
                    QQC2.ToolTip.visible: hovered
                    onClicked: {
                        if (root.commitData && root.commitData.sha) {
                            appController.revertCommit(root.commitData.sha);
                        }
                    }
                }
            }
        }
    }

    // Splitter: 3-column layout (Changed Files in Commit Sidebar | Diff Viewer)
    QQC2.SplitView {
        Layout.fillWidth: true
        Layout.fillHeight: true
        orientation: Qt.Horizontal

        // Changed Files List in this commit (Second sidebar)
        Rectangle {
            QQC2.SplitView.preferredWidth: 260
            QQC2.SplitView.minimumWidth: 180
            QQC2.SplitView.maximumWidth: 420
            QQC2.SplitView.fillHeight: true
            color: CherryStyle.surfaceSidebar

            // Right border for the secondary sidebar
            Rectangle {
                anchors.right: parent.right
                anchors.top: parent.top
                anchors.bottom: parent.bottom
                width: 1
                color: CherryStyle.borderColor
            }

            ColumnLayout {
                anchors.fill: parent
                spacing: 0

                // Header
                Rectangle {
                    Layout.fillWidth: true
                    height: 34
                    color: CherryStyle.surfaceCardElevated

                    Rectangle {
                        anchors.bottom: parent.bottom
                        anchors.left: parent.left
                        anchors.right: parent.right
                        height: 1
                        color: CherryStyle.borderColor
                    }

                    RowLayout {
                        anchors.fill: parent
                        anchors.leftMargin: Kirigami.Units.smallSpacing + 4
                        anchors.rightMargin: Kirigami.Units.smallSpacing + 4

                        Kirigami.Icon {
                            source: "view-list-details"
                            width: 14
                            height: 14
                            color: CherryStyle.secondaryTextColor
                        }

                        QQC2.Label {
                            text: qsTr("Files Changed (%1)").arg(root.commitData && root.commitData.files ? root.commitData.files.length : 0)
                            font.bold: true
                            font.pixelSize: CherryStyle.smallFont.pixelSize
                            color: Kirigami.Theme.textColor
                            Layout.fillWidth: true
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
                        reuseItems: true

                        delegate: QQC2.ItemDelegate {
                            id: commitFileDelegate
                            width: commitFilesListView.width
                            height: 34

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
                                    color: CherryStyle.accentColor
                                }
                            }

                            contentItem: RowLayout {
                                anchors.fill: parent
                                anchors.leftMargin: Kirigami.Units.smallSpacing + 2
                                anchors.rightMargin: Kirigami.Units.smallSpacing + 2
                                spacing: Kirigami.Units.smallSpacing

                                Rectangle {
                                    width: 18
                                    height: 18
                                    Layout.alignment: Qt.AlignVCenter
                                    radius: 3
                                    color: {
                                        var isPathChange = Boolean(commitFileDelegate.modelData.oldFilePath && commitFileDelegate.modelData.oldFilePath !== commitFileDelegate.modelData.filePath);
                                        var st = commitFileDelegate.modelData.status;
                                        if (isPathChange || st === 3) return CherryStyle.renamedBg; // Renamed / Path change
                                        if (st === 1 || st === 4) return CherryStyle.additionBg; // Added/Untracked
                                        if (st === 2) return CherryStyle.deletionBg; // Deleted
                                        return CherryStyle.modifiedBg; // Modified
                                    }
                                    border.color: {
                                        var isPathChange = Boolean(commitFileDelegate.modelData.oldFilePath && commitFileDelegate.modelData.oldFilePath !== commitFileDelegate.modelData.filePath);
                                        var st = commitFileDelegate.modelData.status;
                                        if (isPathChange || st === 3) return Qt.rgba(CherryStyle.renamedColor.r, CherryStyle.renamedColor.g, CherryStyle.renamedColor.b, 0.5);
                                        if (st === 1 || st === 4) return Qt.rgba(CherryStyle.additionColor.r, CherryStyle.additionColor.g, CherryStyle.additionColor.b, 0.5);
                                        if (st === 2) return Qt.rgba(CherryStyle.deletionColor.r, CherryStyle.deletionColor.g, CherryStyle.deletionColor.b, 0.5);
                                        return Qt.rgba(CherryStyle.modifiedColor.r, CherryStyle.modifiedColor.g, CherryStyle.modifiedColor.b, 0.5);
                                    }
                                    border.width: 1

                                    Kirigami.Icon {
                                        anchors.centerIn: parent
                                        source: {
                                            var isPathChange = Boolean(commitFileDelegate.modelData.oldFilePath && commitFileDelegate.modelData.oldFilePath !== commitFileDelegate.modelData.filePath);
                                            var st = commitFileDelegate.modelData.status;
                                            if (isPathChange || st === 3) return "arrow-right";
                                            if (st === 1 || st === 4) return "list-add";
                                            if (st === 2) return "list-remove";
                                            return "document-edit";
                                        }
                                        width: 11
                                        height: 11
                                        color: {
                                            var isPathChange = Boolean(commitFileDelegate.modelData.oldFilePath && commitFileDelegate.modelData.oldFilePath !== commitFileDelegate.modelData.filePath);
                                            var st = commitFileDelegate.modelData.status;
                                            if (isPathChange || st === 3) return CherryStyle.renamedColor;
                                            if (st === 1 || st === 4) return CherryStyle.additionColor;
                                            if (st === 2) return CherryStyle.deletionColor;
                                            return CherryStyle.modifiedColor;
                                        }
                                    }
                                }

                                QQC2.Label {
                                    text: (commitFileDelegate.modelData.oldFilePath && commitFileDelegate.modelData.oldFilePath !== commitFileDelegate.modelData.filePath)
                                        ? (commitFileDelegate.modelData.oldFilePath + " → " + commitFileDelegate.modelData.filePath)
                                        : commitFileDelegate.modelData.filePath
                                    font.pixelSize: CherryStyle.smallFont.pixelSize
                                    font.bold: commitFileDelegate.highlighted
                                    color: commitFileDelegate.highlighted ? CherryStyle.accentColor : Kirigami.Theme.textColor
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
                                    appController.diffModel.loadDiffForCommit(root.commitData.sha, commitFileDelegate.modelData.filePath, commitFileDelegate.modelData.oldFilePath || "");
                                }
                            }
                        }
                    }
                }
            }
        }

        // Commit Diff Viewer
        DiffViewer {
            QQC2.SplitView.fillWidth: true
            QQC2.SplitView.fillHeight: true
            filePath: root.activeFilePath
            isHistorical: true
        }
    }
}

