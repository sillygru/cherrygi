import QtQuick
import QtQuick.Controls as QQC2
import QtQuick.Layouts
import org.kde.kirigami as Kirigami
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
        implicitHeight: headerLayout.implicitHeight + Kirigami.Units.smallSpacing * 2
        color: CherryStyle.cardBackground
        border.color: CherryStyle.borderColor
        border.width: 1

        ColumnLayout {
            id: headerLayout
            anchors.fill: parent
            anchors.margins: Kirigami.Units.mediumSpacing
            spacing: Kirigami.Units.smallSpacing

            // Commit Title / Summary
            QQC2.Label {
                text: root.commitData.summary ? root.commitData.summary : i18n("No commit selected")
                font.bold: true
                font.pixelSize: Kirigami.Units.fontMetrics.font.pixelSize + 3
                color: Kirigami.Theme.textColor
                Layout.fillWidth: true
                wrapMode: Text.Wrap
            }

            // Description
            QQC2.Label {
                text: root.commitData.description ? root.commitData.description : ""
                visible: root.commitData.description && root.commitData.description.length > 0
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
                    width: 36
                    height: 36
                    radius: 18
                    color: Kirigami.Theme.highlightColor

                    QQC2.Label {
                        anchors.centerIn: parent
                        text: (root.commitData.authorName && root.commitData.authorName.length > 0) ? root.commitData.authorName.charAt(0).toUpperCase() : "G"
                        font.bold: true
                        color: Kirigami.Theme.highlightedTextColor
                    }
                }

                // Author details
                ColumnLayout {
                    spacing: 1

                    RowLayout {
                        spacing: 4

                        QQC2.Label {
                            text: root.commitData.authorName ? root.commitData.authorName : ""
                            font.bold: true
                            color: Kirigami.Theme.textColor
                        }

                        QQC2.Label {
                            text: "<" + (root.commitData.authorEmail ? root.commitData.authorEmail : "") + ">"
                            font.pixelSize: CherryStyle.smallFont.pixelSize
                            color: Kirigami.Theme.disabledTextColor
                        }
                    }

                    QQC2.Label {
                        text: i18n("Committed %1 (%2)", root.commitData.relativeTime ? root.commitData.relativeTime : "", root.commitData.timestamp ? root.commitData.timestamp : "")
                        font.pixelSize: CherryStyle.smallFont.pixelSize - 1
                        color: Kirigami.Theme.disabledTextColor
                    }
                }

                Item { Layout.fillWidth: true }

                // Full SHA Badge with Copy button
                Rectangle {
                    height: 28
                    width: shaRow.width + 12
                    radius: 4
                    color: CherryStyle.hoverBackground
                    border.color: CherryStyle.borderColor
                    border.width: 1

                    RowLayout {
                        id: shaRow
                        anchors.centerIn: parent
                        spacing: Kirigami.Units.smallSpacing

                        QQC2.Label {
                            text: root.commitData.shortSha ? root.commitData.shortSha : ""
                            font: CherryStyle.codeFont
                            font.bold: true
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
                                    appController.showToast(i18n("Commit SHA copied to clipboard"));
                                }
                            }
                        }
                    }
                }

                // Actions Menu Button
                QQC2.Button {
                    text: i18n("Revert Commit")
                    icon.name: "edit-undo"
                    onClicked: {
                        if (root.commitData.sha) {
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
                            text: i18n("Changed Files (%1)", root.commitData.files ? root.commitData.files.length : 0)
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
                        model: root.commitData.files ? root.commitData.files : []
                        spacing: 1

                        delegate: QQC2.ItemDelegate {
                            width: commitFilesListView.width
                            height: 32
                            highlighted: root.activeFilePath === modelData.filePath

                            background: Rectangle {
                                color: parent.highlighted ? CherryStyle.activeBackground : (parent.hovered ? CherryStyle.hoverBackground : "transparent")
                                radius: CherryStyle.radiusSmall
                                border.color: parent.highlighted ? Kirigami.Theme.highlightColor : "transparent"
                                border.width: parent.highlighted ? 1 : 0
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
                                    text: modelData.filePath
                                    font.pixelSize: CherryStyle.smallFont.pixelSize
                                    color: parent.highlighted ? Kirigami.Theme.highlightColor : Kirigami.Theme.textColor
                                    Layout.fillWidth: true
                                    elide: Text.ElideMiddle
                                }

                                RowLayout {
                                    spacing: 3
                                    QQC2.Label {
                                        text: "+" + modelData.additions
                                        font.pixelSize: CherryStyle.smallFont.pixelSize - 1
                                        font.bold: true
                                        color: CherryStyle.additionColor
                                        visible: modelData.additions > 0
                                    }
                                    QQC2.Label {
                                        text: "-" + modelData.deletions
                                        font.pixelSize: CherryStyle.smallFont.pixelSize - 1
                                        font.bold: true
                                        color: CherryStyle.deletionColor
                                        visible: modelData.deletions > 0
                                    }
                                }
                            }

                            onClicked: {
                                root.activeFilePath = modelData.filePath;
                                appController.diffModel.loadDiffForCommit(root.commitData.sha, modelData.filePath);
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
