import QtQuick
import QtQuick.Controls as QQC2
import QtQuick.Layouts
import org.kde.kirigami as Kirigami
import org.kde.cherrygi
import "../style"

Rectangle {
    id: root
    implicitHeight: 44
    color: CherryStyle.surfaceHeader

    property string filePath: appController.selectedFilePath
    property int additions: appController.diffModel.additions
    property int deletions: appController.diffModel.deletions

    QQC2.Menu {
        id: diffOptionsMenu

        QQC2.MenuItem {
            text: qsTr("Discard Changes...")
            icon.name: "edit-delete"
            onTriggered: appController.discardFileChanges(root.filePath)
        }

        QQC2.MenuItem {
            text: qsTr("Copy Relative Path")
            icon.name: "edit-copy"
            onTriggered: appController.showToast(qsTr("Path copied to clipboard"))
        }

        QQC2.MenuItem {
            text: qsTr("Open in External Editor")
            icon.name: "accessories-text-editor"
            onTriggered: appController.showToast(qsTr("Opening file in editor..."))
        }
    }

    // Bottom separator
    Rectangle {
        anchors.bottom: parent.bottom
        anchors.left: parent.left
        anchors.right: parent.right
        height: 1
        color: CherryStyle.borderColor
        z: 2
    }

    RowLayout {
        anchors.fill: parent
        anchors.leftMargin: Kirigami.Units.smallSpacing + 2
        anchors.rightMargin: Kirigami.Units.smallSpacing + 2
        spacing: Kirigami.Units.smallSpacing

        // Up arrow (previous file)
        QQC2.ToolButton {
            icon.name: "go-up-symbolic"
            icon.width: 14
            icon.height: 14
            QQC2.ToolTip.text: qsTr("Previous changed file")
            QQC2.ToolTip.visible: hovered
            onClicked: {
                var count = appController.changedFiles.count;
                if (count <= 1) return;
                for (var i = 0; i < count; ++i) {
                    if (appController.changedFiles.getFilePath(i) === root.filePath) {
                        var prevIndex = (i - 1 + count) % count;
                        appController.selectFileForDiff(appController.changedFiles.getFilePath(prevIndex));
                        break;
                    }
                }
            }
        }

        // Down arrow (next file)
        QQC2.ToolButton {
            icon.name: "go-down-symbolic"
            icon.width: 14
            icon.height: 14
            QQC2.ToolTip.text: qsTr("Next changed file")
            QQC2.ToolTip.visible: hovered
            onClicked: {
                var count = appController.changedFiles.count;
                if (count <= 1) return;
                for (var i = 0; i < count; ++i) {
                    if (appController.changedFiles.getFilePath(i) === root.filePath) {
                        var nextIndex = (i + 1) % count;
                        appController.selectFileForDiff(appController.changedFiles.getFilePath(nextIndex));
                        break;
                    }
                }
            }
        }

        // File Path Title
        QQC2.Label {
            text: root.filePath.length > 0 ? root.filePath : qsTr("No file selected")
            font.bold: true
            font.pixelSize: CherryStyle.smallFont.pixelSize
            color: Kirigami.Theme.textColor
            elide: Text.ElideMiddle
            Layout.fillWidth: true
        }

        // Additions / Deletions Stats
        RowLayout {
            spacing: 3
            visible: root.additions > 0 || root.deletions > 0

            Rectangle {
                visible: root.additions > 0
                implicitWidth: addLbl.implicitWidth + 10
                implicitHeight: 20
                radius: 4
                color: CherryStyle.additionBg
                border.color: Qt.rgba(CherryStyle.additionColor.r, CherryStyle.additionColor.g, CherryStyle.additionColor.b, 0.4)
                border.width: 1

                QQC2.Label {
                    id: addLbl
                    anchors.centerIn: parent
                    text: "+" + root.additions
                    font.bold: true
                    font.pixelSize: CherryStyle.smallFont.pixelSize - 1
                    color: CherryStyle.additionColor
                }
            }

            Rectangle {
                visible: root.deletions > 0
                implicitWidth: delLbl.implicitWidth + 10
                implicitHeight: 20
                radius: 4
                color: CherryStyle.deletionBg
                border.color: Qt.rgba(CherryStyle.deletionColor.r, CherryStyle.deletionColor.g, CherryStyle.deletionColor.b, 0.4)
                border.width: 1

                QQC2.Label {
                    id: delLbl
                    anchors.centerIn: parent
                    text: "-" + root.deletions
                    font.bold: true
                    font.pixelSize: CherryStyle.smallFont.pixelSize - 1
                    color: CherryStyle.deletionColor
                }
            }
        }

        // Split / Unified Toggle Button
        QQC2.ToolButton {
            icon.name: appController.diffViewMode === "split" ? "view-split-left-right" : "view-list-details"
            checkable: true
            checked: appController.diffViewMode === "split"
            QQC2.ToolTip.text: appController.diffViewMode === "split" ? qsTr("Switch to Unified Diff") : qsTr("Switch to Split Diff")
            QQC2.ToolTip.visible: hovered
            onClicked: {
                appController.diffViewMode = (appController.diffViewMode === "split") ? "unified" : "split";
            }
        }

        // Whitespace Toggle
        QQC2.ToolButton {
            icon.name: "format-indent-more"
            checkable: true
            checked: appController.showWhitespace
            QQC2.ToolTip.text: appController.showWhitespace ? qsTr("Hide Whitespace Changes") : qsTr("Show Whitespace Changes")
            QQC2.ToolTip.visible: hovered
            onClicked: {
                appController.showWhitespace = !appController.showWhitespace;
            }
        }

        // Gear options menu
        QQC2.ToolButton {
            icon.name: "configure"
            onClicked: diffOptionsMenu.popup(this, 0, this.height)
        }
    }
}

