import QtQuick
import QtQuick.Controls as QQC2
import QtQuick.Layouts
import org.kde.kirigami as Kirigami
import org.kde.cherrygi
import "../style"

Rectangle {
    id: root
    color: CherryStyle.surfaceSidebar
    border.color: CherryStyle.borderColor
    border.width: 1

    ColumnLayout {
        anchors.fill: parent
        spacing: 0

        // ==========================================
        // TOP SEGMENTED TAB SWITCHER
        // ==========================================
        Rectangle {
            Layout.fillWidth: true
            height: 44
            color: CherryStyle.surfaceHeader

            // Bottom border
            Rectangle {
                anchors.bottom: parent.bottom
                anchors.left: parent.left
                anchors.right: parent.right
                height: 1
                color: CherryStyle.borderColor
            }

            RowLayout {
                anchors.fill: parent
                spacing: 0

                // CHANGES TAB BUTTON
                Item {
                    Layout.fillWidth: true
                    Layout.fillHeight: true

                    Rectangle {
                        anchors.fill: parent
                        anchors.margins: 4
                        radius: CherryStyle.radiusSmall
                        color: appController.activeTab === "changes" ? CherryStyle.surfaceCardElevated : (changesTabMouse.containsMouse ? CherryStyle.hoverBackground : "transparent")
                        border.color: appController.activeTab === "changes" ? CherryStyle.strongBorderColor : "transparent"
                        border.width: 1
                    }

                    // Bottom accent indicator line when active
                    Rectangle {
                        anchors.bottom: parent.bottom
                        anchors.left: parent.left
                        anchors.right: parent.right
                        anchors.leftMargin: 12
                        anchors.rightMargin: 12
                        height: 3
                        radius: 1.5
                        color: appController.activeTab === "changes" ? Kirigami.Theme.highlightColor : "transparent"
                    }

                    RowLayout {
                        anchors.centerIn: parent
                        spacing: 6

                        QQC2.Label {
                            text: qsTr("Changes")
                            font.bold: appController.activeTab === "changes"
                            color: appController.activeTab === "changes" ? Kirigami.Theme.textColor : Kirigami.Theme.disabledTextColor
                        }

                        // Badge count
                        Rectangle {
                            visible: appController.changedFiles.count > 0
                            implicitWidth: countBadge.implicitWidth + 10
                            implicitHeight: 18
                            radius: 9
                            color: appController.activeTab === "changes" ? CherryStyle.activeBackground : CherryStyle.surfaceCard
                            border.color: appController.activeTab === "changes" ? Kirigami.Theme.highlightColor : CherryStyle.borderColor
                            border.width: 1

                            QQC2.Label {
                                id: countBadge
                                anchors.centerIn: parent
                                text: "" + appController.changedFiles.count
                                font.pixelSize: CherryStyle.smallFont.pixelSize - 1
                                font.bold: true
                                color: appController.activeTab === "changes" ? Kirigami.Theme.highlightColor : Kirigami.Theme.disabledTextColor
                            }
                        }
                    }

                    MouseArea {
                        id: changesTabMouse
                        anchors.fill: parent
                        hoverEnabled: true
                        cursorShape: Qt.PointingHandCursor
                        onClicked: appController.activeTab = "changes"
                    }
                }

                // Vertical Divider
                Rectangle {
                    width: 1
                    Layout.fillHeight: true
                    Layout.topMargin: 8
                    Layout.bottomMargin: 8
                    color: CherryStyle.subtleBorderColor
                }

                // HISTORY TAB BUTTON
                Item {
                    Layout.fillWidth: true
                    Layout.fillHeight: true

                    Rectangle {
                        anchors.fill: parent
                        anchors.margins: 4
                        radius: CherryStyle.radiusSmall
                        color: appController.activeTab === "history" ? CherryStyle.surfaceCardElevated : (historyTabMouse.containsMouse ? CherryStyle.hoverBackground : "transparent")
                        border.color: appController.activeTab === "history" ? CherryStyle.strongBorderColor : "transparent"
                        border.width: 1
                    }

                    Rectangle {
                        anchors.bottom: parent.bottom
                        anchors.left: parent.left
                        anchors.right: parent.right
                        anchors.leftMargin: 12
                        anchors.rightMargin: 12
                        height: 3
                        radius: 1.5
                        color: appController.activeTab === "history" ? Kirigami.Theme.highlightColor : "transparent"
                    }

                    RowLayout {
                        anchors.centerIn: parent
                        spacing: Kirigami.Units.smallSpacing

                        QQC2.Label {
                            text: qsTr("History")
                            font.bold: appController.activeTab === "history"
                            color: appController.activeTab === "history" ? Kirigami.Theme.textColor : Kirigami.Theme.disabledTextColor
                        }
                    }

                    MouseArea {
                        id: historyTabMouse
                        anchors.fill: parent
                        hoverEnabled: true
                        cursorShape: Qt.PointingHandCursor
                        onClicked: appController.activeTab = "history"
                    }
                }
            }
        }

        // ==========================================
        // MIDDLE LIST VIEWS (Changes vs History)
        // ==========================================
        StackLayout {
            Layout.fillWidth: true
            Layout.fillHeight: true
            currentIndex: appController.activeTab === "changes" ? 0 : 1

            ChangesTab {
                id: changesTab
            }

            HistoryTab {
                id: historyTab
            }
        }

        // ==========================================
        // BOTTOM COMMIT BOX (Changes mode only)
        // ==========================================
        CommitBox {
            Layout.fillWidth: true
            visible: appController.activeTab === "changes"
        }
    }
}

