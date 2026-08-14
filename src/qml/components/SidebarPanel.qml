import QtQuick
import QtQuick.Controls as QQC2
import QtQuick.Layouts
import org.kde.kirigami as Kirigami
import org.kde.cherrygi
import "../style"

Rectangle {
    id: root
    color: CherryStyle.surfaceSidebar

    ColumnLayout {
        anchors.fill: parent
        spacing: 0

        // ==========================================
        // TOP SEGMENTED TAB SWITCHER
        // ==========================================
        Rectangle {
            Layout.fillWidth: true
            height: 48
            color: CherryStyle.surfaceHeader

            RowLayout {
                anchors.fill: parent
                spacing: 0

                // CHANGES TAB BUTTON
                Item {
                    Layout.fillWidth: true
                    Layout.fillHeight: true

                    Rectangle {
                        anchors.fill: parent
                        anchors.margins: 6
                        radius: CherryStyle.radiusMedium
                        color: appController.activeTab === "changes" ? CherryStyle.surfaceCardElevated : (changesTabMouse.containsMouse ? CherryStyle.hoverBackground : "transparent")
                    }

                    // Bottom accent indicator line when active
                    Rectangle {
                        anchors.bottom: parent.bottom
                        anchors.left: parent.left
                        anchors.right: parent.right
                        anchors.leftMargin: 16
                        anchors.rightMargin: 16
                        height: 2
                        radius: 1.0
                        color: appController.activeTab === "changes" ? CherryStyle.accentColor : "transparent"
                    }

                    RowLayout {
                        anchors.centerIn: parent
                        spacing: 6

                        QQC2.Label {
                            text: qsTr("Changes")
                            font.bold: appController.activeTab === "changes"
                            color: appController.activeTab === "changes" ? Kirigami.Theme.textColor : CherryStyle.secondaryTextColor
                        }

                        // Badge count
                        Rectangle {
                            visible: appController.changedFiles.count > 0
                            implicitWidth: countBadge.implicitWidth + 10
                            implicitHeight: 18
                            radius: 9
                            color: appController.activeTab === "changes" ? CherryStyle.activeBackground : CherryStyle.surfaceCard
                            border.color: appController.activeTab === "changes" ? CherryStyle.accentColor : CherryStyle.borderColor
                            border.width: 1

                            QQC2.Label {
                                id: countBadge
                                anchors.centerIn: parent
                                text: "" + appController.changedFiles.count
                                font.pixelSize: CherryStyle.smallFont.pixelSize - 1
                                font.bold: true
                                color: appController.activeTab === "changes" ? CherryStyle.accentColor : CherryStyle.secondaryTextColor
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
                    color: Qt.rgba(Kirigami.Theme.textColor.r, Kirigami.Theme.textColor.g, Kirigami.Theme.textColor.b, 0.04)
                }

                // HISTORY TAB BUTTON
                Item {
                    Layout.fillWidth: true
                    Layout.fillHeight: true

                    Rectangle {
                        anchors.fill: parent
                        anchors.margins: 6
                        radius: CherryStyle.radiusMedium
                        color: appController.activeTab === "history" ? CherryStyle.surfaceCardElevated : (historyTabMouse.containsMouse ? CherryStyle.hoverBackground : "transparent")
                    }

                    Rectangle {
                        anchors.bottom: parent.bottom
                        anchors.left: parent.left
                        anchors.right: parent.right
                        anchors.leftMargin: 16
                        anchors.rightMargin: 16
                        height: 2
                        radius: 1.0
                        color: appController.activeTab === "history" ? CherryStyle.accentColor : "transparent"
                    }

                    RowLayout {
                        anchors.centerIn: parent
                        spacing: Kirigami.Units.smallSpacing

                        QQC2.Label {
                            text: qsTr("History")
                            font.bold: appController.activeTab === "history"
                            color: appController.activeTab === "history" ? Kirigami.Theme.textColor : CherryStyle.secondaryTextColor
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

