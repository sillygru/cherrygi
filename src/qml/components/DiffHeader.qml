import QtQuick
import QtQuick.Controls as QQC2
import QtQuick.Layouts
import org.kde.kirigami as Kirigami
import "../style"

Rectangle {
    id: root
    implicitHeight: 40
    color: CherryStyle.cardBackground
    border.color: CherryStyle.borderColor
    border.width: 1

    property string filePath: appController.selectedFilePath
    property int additions: appController.diffModel.additions
    property int deletions: appController.diffModel.deletions

    QQC2.Menu {
        id: diffOptionsMenu

        QQC2.MenuItem {
            text: i18n("Discard Changes...")
            icon.name: "edit-delete"
            onTriggered: appController.discardFileChanges(root.filePath)
        }

        QQC2.MenuItem {
            text: i18n("Copy Relative Path")
            icon.name: "edit-copy"
            onTriggered: appController.showToast(i18n("Path copied to clipboard"))
        }

        QQC2.MenuItem {
            text: i18n("Open in External Editor")
            icon.name: "accessories-text-editor"
            onTriggered: appController.showToast(i18n("Opening file in editor..."))
        }
    }

    RowLayout {
        anchors.fill: parent
        anchors.leftMargin: Kirigami.Units.smallSpacing
        anchors.rightMargin: Kirigami.Units.smallSpacing
        spacing: Kirigami.Units.smallSpacing

        // Up arrow (previous file)
        QQC2.ToolButton {
            icon.name: "arrow-up"
            icon.width: 14
            icon.height: 14
            QQC2.ToolTip.text: i18n("Previous changed file")
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
            icon.name: "arrow-down"
            icon.width: 14
            icon.height: 14
            QQC2.ToolTip.text: i18n("Next changed file")
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
            text: root.filePath.length > 0 ? root.filePath : i18n("No file selected")
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
                width: addLbl.width + 8
                height: 18
                radius: 4
                color: CherryStyle.additionBg

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
                width: delLbl.width + 8
                height: 18
                radius: 4
                color: CherryStyle.deletionBg

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
            QQC2.ToolTip.text: appController.diffViewMode === "split" ? i18n("Switch to Unified Diff") : i18n("Switch to Split Diff")
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
            QQC2.ToolTip.text: appController.showWhitespace ? i18n("Hide Whitespace Changes") : i18n("Show Whitespace Changes")
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
