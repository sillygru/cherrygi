import QtQuick
import QtQuick.Controls as QQC2
import QtQuick.Layouts
import org.kde.kirigami as Kirigami
import "../style"

Rectangle {
    id: root
    color: Kirigami.Theme.backgroundColor
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
            height: 42
            color: CherryStyle.cardBackground
            border.color: CherryStyle.borderColor
            border.width: 1

            RowLayout {
                anchors.fill: parent
                spacing: 0

                // CHANGES TAB BUTTON
                Rectangle {
                    Layout.fillWidth: true
                    Layout.fillHeight: true
                    color: appController.activeTab === "changes" ? Kirigami.Theme.backgroundColor : "transparent"

                    // Bottom indicator line when active
                    Rectangle {
                        anchors.bottom: parent.bottom
                        anchors.left: parent.left
                        anchors.right: parent.right
                        height: 2
                        color: appController.activeTab === "changes" ? Kirigami.Theme.highlightColor : "transparent"
                    }

                    RowLayout {
                        anchors.centerIn: parent
                        spacing: Kirigami.Units.smallSpacing

                        QQC2.Label {
                            text: i18n("Changes")
                            font.bold: appController.activeTab === "changes"
                            color: appController.activeTab === "changes" ? Kirigami.Theme.textColor : Kirigami.Theme.disabledTextColor
                        }

                        // Badge count
                        Rectangle {
                            visible: appController.changedFiles.count > 0
                            width: countBadge.width + 10
                            height: 18
                            radius: 9
                            color: appController.activeTab === "changes" ? Qt.rgba(Kirigami.Theme.textColor.r, Kirigami.Theme.textColor.g, Kirigami.Theme.textColor.b, 0.12) : "transparent"

                            QQC2.Label {
                                id: countBadge
                                anchors.centerIn: parent
                                text: "" + appController.changedFiles.count
                                font.pixelSize: CherryStyle.smallFont.pixelSize
                                font.bold: true
                                color: appController.activeTab === "changes" ? Kirigami.Theme.textColor : Kirigami.Theme.disabledTextColor
                            }
                        }
                    }

                    MouseArea {
                        anchors.fill: parent
                        cursorShape: Qt.PointingHandCursor
                        onClicked: appController.activeTab = "changes"
                    }
                }

                // Vertical Divider
                Rectangle {
                    width: 1
                    Layout.fillHeight: true
                    color: CherryStyle.borderColor
                }

                // HISTORY TAB BUTTON
                Rectangle {
                    Layout.fillWidth: true
                    Layout.fillHeight: true
                    color: appController.activeTab === "history" ? Kirigami.Theme.backgroundColor : "transparent"

                    Rectangle {
                        anchors.bottom: parent.bottom
                        anchors.left: parent.left
                        anchors.right: parent.right
                        height: 2
                        color: appController.activeTab === "history" ? Kirigami.Theme.highlightColor : "transparent"
                    }

                    RowLayout {
                        anchors.centerIn: parent
                        spacing: Kirigami.Units.smallSpacing

                        QQC2.Label {
                            text: i18n("History")
                            font.bold: appController.activeTab === "history"
                            color: appController.activeTab === "history" ? Kirigami.Theme.textColor : Kirigami.Theme.disabledTextColor
                        }
                    }

                    MouseArea {
                        anchors.fill: parent
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
