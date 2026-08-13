import QtQuick
import QtQuick.Controls as QQC2
import QtQuick.Layouts
import org.kde.kirigami as Kirigami
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

        // Empty state when no file or empty diff
        Item {
            anchors.centerIn: parent
            visible: appController.diffModel.count === 0
            width: parent.width - 60
            height: 140

            ColumnLayout {
                anchors.centerIn: parent
                spacing: Kirigami.Units.smallSpacing

                Kirigami.Icon {
                    source: "document-preview"
                    width: 48
                    height: 48
                    Layout.alignment: Qt.AlignHCenter
                    color: Kirigami.Theme.disabledTextColor
                }

                QQC2.Label {
                    text: i18n("No diff to display")
                    font.bold: true
                    font.pixelSize: Kirigami.Units.fontMetrics.font.pixelSize + 2
                    Layout.alignment: Qt.AlignHCenter
                    color: Kirigami.Theme.textColor
                }

                QQC2.Label {
                    text: i18n("Select a modified file from the changes list to view the diff.")
                    font.pixelSize: CherryStyle.smallFont.pixelSize
                    Layout.alignment: Qt.AlignHCenter
                    color: Kirigami.Theme.disabledTextColor
                }
            }
        }

        // ==========================================
        // UNIFIED DIFF VIEW
        // ==========================================
        QQC2.ScrollView {
            anchors.fill: parent
            visible: appController.diffModel.count > 0 && appController.diffViewMode === "unified"
            clip: true

            ListView {
                id: unifiedListView
                width: parent.width
                model: appController.diffModel
                spacing: 0

                delegate: Rectangle {
                    width: unifiedListView.width
                    height: 22

                    // Background color based on line type
                    color: {
                        if (model.lineType === 1) return CherryStyle.additionBg;       // Addition (+)
                        if (model.lineType === 2) return CherryStyle.deletionBg;       // Deletion (-)
                        if (model.lineType === 3) return CherryStyle.hunkHeaderBg;     // Hunk Header (@@)
                        return "transparent";
                    }

                    RowLayout {
                        anchors.fill: parent
                        spacing: 0

                        // Old Line Number Gutter
                        Rectangle {
                            Layout.preferredWidth: 46
                            Layout.fillHeight: true
                            color: {
                                if (model.lineType === 1) return CherryStyle.additionGutterBg;
                                if (model.lineType === 2) return CherryStyle.deletionGutterBg;
                                if (model.lineType === 3) return CherryStyle.hunkHeaderBg;
                                return CherryStyle.cardBackground;
                            }
                            border.color: CherryStyle.subtleBorderColor
                            border.width: 1

                            QQC2.Label {
                                anchors.centerIn: parent
                                text: model.oldLineNumStr
                                font: CherryStyle.codeFont
                                color: Kirigami.Theme.disabledTextColor
                            }
                        }

                        // New Line Number Gutter
                        Rectangle {
                            Layout.preferredWidth: 46
                            Layout.fillHeight: true
                            color: {
                                if (model.lineType === 1) return CherryStyle.additionGutterBg;
                                if (model.lineType === 2) return CherryStyle.deletionGutterBg;
                                if (model.lineType === 3) return CherryStyle.hunkHeaderBg;
                                return CherryStyle.cardBackground;
                            }
                            border.color: CherryStyle.subtleBorderColor
                            border.width: 1

                            QQC2.Label {
                                anchors.centerIn: parent
                                text: model.newLineNumStr
                                font: CherryStyle.codeFont
                                color: Kirigami.Theme.disabledTextColor
                            }
                        }

                        // Marker Column (+, -, @@, space)
                        Rectangle {
                            Layout.preferredWidth: 24
                            Layout.fillHeight: true
                            color: "transparent"

                            QQC2.Label {
                                anchors.centerIn: parent
                                text: model.marker
                                font: CherryStyle.codeFont
                                font.bold: true
                                color: {
                                    if (model.lineType === 1) return CherryStyle.additionColor;
                                    if (model.lineType === 2) return CherryStyle.deletionColor;
                                    if (model.lineType === 3) return CherryStyle.hunkHeaderColor;
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
                                anchors.leftMargin: Kirigami.Units.smallSpacing
                                anchors.verticalCenter: parent.verticalCenter
                                text: model.content
                                font: CherryStyle.codeFont
                                color: {
                                    if (model.lineType === 3) return CherryStyle.hunkHeaderColor;
                                    if (model.content.trim().startsWith("//") || model.content.trim().startsWith("/*") || model.content.trim().startsWith("*")) {
                                        return "#2ec27e"; // comment styling
                                    }
                                    if (model.content.includes("readonly") || model.content.includes("interface") || model.content.includes("return") || model.content.includes("const") || model.content.includes("export")) {
                                        return Kirigami.Theme.textColor;
                                    }
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
            visible: appController.diffModel.count > 0 && appController.diffViewMode === "split"
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

                    delegate: Rectangle {
                        width: splitLeftListView.width
                        height: 22
                        color: (model.lineType === 2) ? CherryStyle.deletionBg : (model.lineType === 3 ? CherryStyle.hunkHeaderBg : (model.lineType === 1 ? CherryStyle.cardBackground : "transparent"))

                        RowLayout {
                            anchors.fill: parent
                            spacing: 0

                            Rectangle {
                                Layout.preferredWidth: 44
                                Layout.fillHeight: true
                                color: CherryStyle.cardBackground
                                border.color: CherryStyle.subtleBorderColor
                                border.width: 1

                                QQC2.Label {
                                    anchors.centerIn: parent
                                    text: model.oldLineNumStr
                                    font: CherryStyle.codeFont
                                    color: Kirigami.Theme.disabledTextColor
                                }
                            }

                            QQC2.Label {
                                anchors.left: parent.left
                                anchors.leftMargin: 48
                                anchors.verticalCenter: parent.verticalCenter
                                text: (model.lineType !== 1) ? model.content : ""
                                font: CherryStyle.codeFont
                                color: (model.lineType === 2) ? CherryStyle.deletionColor : Kirigami.Theme.textColor
                            }
                        }
                    }
                }

                // Vertical Divider
                Rectangle {
                    Layout.preferredWidth: 1
                    Layout.fillHeight: true
                    color: CherryStyle.borderColor
                }

                // RIGHT PANE (New Version)
                ListView {
                    id: splitRightListView
                    Layout.fillWidth: true
                    Layout.fillHeight: true
                    model: appController.diffModel
                    spacing: 0

                    delegate: Rectangle {
                        width: splitRightListView.width
                        height: 22
                        color: (model.lineType === 1) ? CherryStyle.additionBg : (model.lineType === 3 ? CherryStyle.hunkHeaderBg : (model.lineType === 2 ? CherryStyle.cardBackground : "transparent"))

                        RowLayout {
                            anchors.fill: parent
                            spacing: 0

                            Rectangle {
                                Layout.preferredWidth: 44
                                Layout.fillHeight: true
                                color: CherryStyle.cardBackground
                                border.color: CherryStyle.subtleBorderColor
                                border.width: 1

                                QQC2.Label {
                                    anchors.centerIn: parent
                                    text: model.newLineNumStr
                                    font: CherryStyle.codeFont
                                    color: Kirigami.Theme.disabledTextColor
                                }
                            }

                            QQC2.Label {
                                anchors.left: parent.left
                                anchors.leftMargin: 48
                                anchors.verticalCenter: parent.verticalCenter
                                text: (model.lineType !== 2) ? model.content : ""
                                font: CherryStyle.codeFont
                                color: (model.lineType === 1) ? CherryStyle.additionColor : Kirigami.Theme.textColor
                            }
                        }
                    }
                }
            }
        }
    }
}
