import QtQuick
import QtQuick.Controls as QQC2
import QtQuick.Layouts
import org.kde.kirigami as Kirigami
import org.kde.cherrygi
import "../style"

ColumnLayout {
    id: root
    spacing: 0

    property alias filePath: diffHeader.filePath
    property bool isHistorical: false

    DiffHeader {
        id: diffHeader
        Layout.fillWidth: true
    }

    // Main diff content container
    Rectangle {
        Layout.fillWidth: true
        Layout.fillHeight: true
        color: Kirigami.Theme.backgroundColor

        // Loading state when computing diff
        Item {
            anchors.centerIn: parent
            visible: appController.diffModel.isLoading
            width: parent.width - 60
            height: 140

            ColumnLayout {
                anchors.centerIn: parent
                spacing: Kirigami.Units.mediumSpacing

                QQC2.BusyIndicator {
                    width: 32
                    height: 32
                    Layout.alignment: Qt.AlignHCenter
                    running: appController.diffModel.isLoading
                }

                QQC2.Label {
                    text: qsTr("Loading diff...")
                    font.bold: true
                    font.pixelSize: CherryStyle.basePixelSize + 2
                    Layout.alignment: Qt.AlignHCenter
                    color: Kirigami.Theme.textColor
                }

                QQC2.Label {
                    text: qsTr("Computing line changes and hunk diffs...")
                    font.pixelSize: CherryStyle.smallFont.pixelSize
                    Layout.alignment: Qt.AlignHCenter
                    color: Kirigami.Theme.disabledTextColor
                }
            }
        }

        // Metadata-only state: Git detected a file permission/mode change,
        // but there are no line-level contents to render.
        Item {
            anchors.centerIn: parent
            visible: !appController.diffModel.isLoading && appController.diffModel.metadataOnly
            width: parent.width - 60
            height: 190

            ColumnLayout {
                anchors.centerIn: parent
                spacing: Kirigami.Units.smallSpacing

                Kirigami.Icon {
                    source: "emblem-important"
                    width: 48
                    height: 48
                    Layout.alignment: Qt.AlignHCenter
                    color: Kirigami.Theme.highlightColor
                }

                QQC2.Label {
                    text: qsTr("File metadata changed")
                    font.bold: true
                    font.pixelSize: CherryStyle.basePixelSize + 2
                    Layout.alignment: Qt.AlignHCenter
                    color: Kirigami.Theme.textColor
                }

                QQC2.Label {
                    text: qsTr("Only file permissions changed; there is no content diff to display.")
                    font.pixelSize: CherryStyle.smallFont.pixelSize
                    Layout.alignment: Qt.AlignHCenter
                    color: Kirigami.Theme.disabledTextColor
                    wrapMode: Text.Wrap
                    horizontalAlignment: Text.AlignHCenter
                    Layout.maximumWidth: 420
                }

                QQC2.Button {
                    text: qsTr("Ignore metadata changes in this repository")
                    icon.name: "document-save"
                    Layout.alignment: Qt.AlignHCenter
                    highlighted: true
                    onClicked: appController.setIgnoreFileModeChanges(true, false)
                }
            }
        }

        // Empty state when no file or empty diff
        Item {
            anchors.centerIn: parent
            visible: !appController.diffModel.isLoading && !appController.diffModel.metadataOnly && !appController.diffModel.isImage && (appController.diffModel.count === 0 || !root.filePath || root.filePath.length === 0)
            width: parent.width - 60
            height: 140

            ColumnLayout {
                anchors.centerIn: parent
                spacing: Kirigami.Units.smallSpacing

                Kirigami.Icon {
                    source: "view-list-text"
                    width: 56
                    height: 56
                    Layout.alignment: Qt.AlignHCenter
                    color: Kirigami.Theme.disabledTextColor
                }

                QQC2.Label {
                    text: (root.filePath && root.filePath.length > 0) ? qsTr("No diff to display") : qsTr("No file selected")
                    font.bold: true
                    font.pixelSize: CherryStyle.basePixelSize + 2
                    Layout.alignment: Qt.AlignHCenter
                    color: Kirigami.Theme.textColor
                }

                QQC2.Label {
                    text: (root.filePath && root.filePath.length > 0) ? qsTr("This file has no uncommitted changes or difference to display.") : qsTr("Select a modified file from the changes list to view the diff.")
                    font.pixelSize: CherryStyle.smallFont.pixelSize
                    Layout.alignment: Qt.AlignHCenter
                    color: Kirigami.Theme.disabledTextColor
                }
            }
        }

        // ==========================================
        // IMAGE DIFF VIEW
        // ==========================================
        ImageDiffViewer {
            anchors.fill: parent
            visible: !appController.diffModel.isLoading && appController.diffModel.isImage && Boolean(root.filePath && root.filePath.length > 0)
        }

        // ==========================================
        // UNIFIED DIFF VIEW
        // ==========================================
        QQC2.ScrollView {
            anchors.fill: parent
            visible: !appController.diffModel.isLoading && !appController.diffModel.isImage && appController.diffModel.count > 0 && appController.diffViewMode === "unified" && Boolean(root.filePath && root.filePath.length > 0)
            clip: true

            ListView {
                id: unifiedListView
                width: parent.width
                model: appController.diffModel
                spacing: 0
                reuseItems: false

                delegate: Rectangle {
                    id: unifiedLineDelegate
                    width: unifiedListView.width
                    height: CherryStyle.diffLineHeight

                    required property int index
                    required property int lineType
                    required property string oldLineNumStr
                    required property string newLineNumStr
                    required property string marker
                    required property string content

                    // Background color based on line type
                    color: {
                        if (unifiedLineDelegate.lineType === 1) return CherryStyle.additionBg;       // Addition (+)
                        if (unifiedLineDelegate.lineType === 2) return CherryStyle.deletionBg;       // Deletion (-)
                        if (unifiedLineDelegate.lineType === 3) return CherryStyle.hunkHeaderBg;     // Hunk Header (@@)
                        return "transparent";
                    }

                    RowLayout {
                        anchors.fill: parent
                        spacing: 0

                        // Old Line Number Gutter
                        Rectangle {
                            Layout.preferredWidth: CherryStyle.diffGutterWidth
                            Layout.fillHeight: true
                            color: {
                                if (unifiedLineDelegate.lineType === 1) return CherryStyle.additionGutterBg;
                                if (unifiedLineDelegate.lineType === 2) return CherryStyle.deletionGutterBg;
                                if (unifiedLineDelegate.lineType === 3) return CherryStyle.hunkHeaderBg;
                                return CherryStyle.gutterBg;
                            }

                            QQC2.Label {
                                anchors.centerIn: parent
                                text: unifiedLineDelegate.oldLineNumStr
                                font: CherryStyle.codeFont
                                color: Kirigami.Theme.disabledTextColor
                            }
                        }

                        // New Line Number Gutter
                        Rectangle {
                            Layout.preferredWidth: CherryStyle.diffGutterWidth
                            Layout.fillHeight: true
                            color: {
                                if (unifiedLineDelegate.lineType === 1) return CherryStyle.additionGutterBg;
                                if (unifiedLineDelegate.lineType === 2) return CherryStyle.deletionGutterBg;
                                if (unifiedLineDelegate.lineType === 3) return CherryStyle.hunkHeaderBg;
                                return CherryStyle.gutterBg;
                            }

                            QQC2.Label {
                                anchors.centerIn: parent
                                text: unifiedLineDelegate.newLineNumStr
                                font: CherryStyle.codeFont
                                color: Kirigami.Theme.disabledTextColor
                            }
                        }

                        // Gutter Divider Line
                        Rectangle {
                            Layout.preferredWidth: 1
                            Layout.fillHeight: true
                            color: CherryStyle.borderColor
                        }

                        // Marker Column (+, -, @@, space)
                        Rectangle {
                            Layout.preferredWidth: 28
                            Layout.fillHeight: true
                            color: "transparent"

                            QQC2.Label {
                                anchors.centerIn: parent
                                text: unifiedLineDelegate.marker
                                font: CherryStyle.codeFontBold
                                color: {
                                    if (unifiedLineDelegate.lineType === 1) return CherryStyle.additionColor;
                                    if (unifiedLineDelegate.lineType === 2) return CherryStyle.deletionColor;
                                    if (unifiedLineDelegate.lineType === 3) return CherryStyle.hunkHeaderColor;
                                    return Kirigami.Theme.disabledTextColor;
                                }
                            }
                        }

                        // Code Text Line
                        Item {
                            Layout.fillWidth: true
                            Layout.fillHeight: true

                            QQC2.Label {
                                anchors.left: parent.left
                                anchors.leftMargin: Kirigami.Units.mediumSpacing
                                anchors.verticalCenter: parent.verticalCenter
                                text: unifiedLineDelegate.content
                                font: CherryStyle.codeFont
                                color: {
                                    if (unifiedLineDelegate.lineType === 3) return CherryStyle.hunkHeaderColor;
                                    if (unifiedLineDelegate.content.trim().startsWith("//") || unifiedLineDelegate.content.trim().startsWith("/*") || unifiedLineDelegate.content.trim().startsWith("*")) {
                                        return "#2ec27e"; // comment styling
                                    }
                                    if (unifiedLineDelegate.lineType === 1) return Kirigami.Theme.textColor;
                                    if (unifiedLineDelegate.lineType === 2) return Kirigami.Theme.textColor;
                                    return Kirigami.Theme.textColor;
                                }
                                elide: Text.ElideNone
                            }
                        }
                    }
                }
            }
        }

        // ==========================================
        // SPLIT DIFF VIEW (Side-by-side)
        // ==========================================
        QQC2.ScrollView {
            anchors.fill: parent
            visible: !appController.diffModel.isLoading && !appController.diffModel.isImage && appController.diffModel.count > 0 && appController.diffViewMode === "split" && Boolean(root.filePath && root.filePath.length > 0)
            clip: true

            RowLayout {
                width: parent.width
                spacing: 0

                // LEFT PANE (Old Version)
                ListView {
                    id: splitLeftListView
                    Layout.fillWidth: true
                    Layout.fillHeight: true
                    model: appController.diffModel
                    spacing: 0
                    reuseItems: false

                    delegate: Rectangle {
                        id: splitLeftDelegate
                        width: splitLeftListView.width
                        height: CherryStyle.diffLineHeight

                        required property int index
                        required property int lineType
                        required property string oldLineNumStr
                        required property string content

                        color: (splitLeftDelegate.lineType === 2) ? CherryStyle.deletionBg : (splitLeftDelegate.lineType === 3 ? CherryStyle.hunkHeaderBg : (splitLeftDelegate.lineType === 1 ? CherryStyle.surfaceCard : "transparent"))

                        RowLayout {
                            anchors.fill: parent
                            spacing: 0

                            Rectangle {
                                Layout.preferredWidth: CherryStyle.diffGutterWidth
                                Layout.fillHeight: true
                                color: (splitLeftDelegate.lineType === 2) ? CherryStyle.deletionGutterBg : CherryStyle.gutterBg

                                QQC2.Label {
                                    anchors.centerIn: parent
                                    text: splitLeftDelegate.oldLineNumStr
                                    font: CherryStyle.codeFont
                                    color: Kirigami.Theme.disabledTextColor
                                }
                            }

                            Rectangle {
                                Layout.preferredWidth: 1
                                Layout.fillHeight: true
                                color: CherryStyle.borderColor
                            }

                            QQC2.Label {
                                Layout.fillWidth: true
                                Layout.alignment: Qt.AlignVCenter
                                Layout.leftMargin: Kirigami.Units.mediumSpacing
                                text: (splitLeftDelegate.lineType !== 1) ? splitLeftDelegate.content : ""
                                font: CherryStyle.codeFont
                                color: (splitLeftDelegate.lineType === 2) ? CherryStyle.deletionColor : Kirigami.Theme.textColor
                            }
                        }
                    }
                }

                // Vertical Divider
                Rectangle {
                    Layout.preferredWidth: 1
                    Layout.fillHeight: true
                    color: CherryStyle.strongBorderColor
                }

                // RIGHT PANE (New Version)
                ListView {
                    id: splitRightListView
                    Layout.fillWidth: true
                    Layout.fillHeight: true
                    model: appController.diffModel
                    spacing: 0
                    reuseItems: false

                    delegate: Rectangle {
                        id: splitRightDelegate
                        width: splitRightListView.width
                        height: CherryStyle.diffLineHeight

                        required property int index
                        required property int lineType
                        required property string newLineNumStr
                        required property string content

                        color: (splitRightDelegate.lineType === 1) ? CherryStyle.additionBg : (splitRightDelegate.lineType === 3 ? CherryStyle.hunkHeaderBg : (splitRightDelegate.lineType === 2 ? CherryStyle.surfaceCard : "transparent"))

                        RowLayout {
                            anchors.fill: parent
                            spacing: 0

                            Rectangle {
                                Layout.preferredWidth: CherryStyle.diffGutterWidth
                                Layout.fillHeight: true
                                color: (splitRightDelegate.lineType === 1) ? CherryStyle.additionGutterBg : CherryStyle.gutterBg

                                QQC2.Label {
                                    anchors.centerIn: parent
                                    text: splitRightDelegate.newLineNumStr
                                    font: CherryStyle.codeFont
                                    color: Kirigami.Theme.disabledTextColor
                                }
                            }

                            Rectangle {
                                Layout.preferredWidth: 1
                                Layout.fillHeight: true
                                color: CherryStyle.borderColor
                            }

                            QQC2.Label {
                                Layout.fillWidth: true
                                Layout.alignment: Qt.AlignVCenter
                                Layout.leftMargin: Kirigami.Units.mediumSpacing
                                text: (splitRightDelegate.lineType !== 2) ? splitRightDelegate.content : ""
                                font: CherryStyle.codeFont
                                color: (splitRightDelegate.lineType === 1) ? CherryStyle.additionColor : Kirigami.Theme.textColor
                            }
                        }
                    }
                }
            }
        }
    }
}

