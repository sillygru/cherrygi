import QtQuick
import QtQuick.Controls as QQC2
import QtQuick.Layouts
import org.kde.kirigami as Kirigami
import org.kde.cherrygi
import "../style"

QQC2.Popup {
    id: root
    width: 600
    height: 440
    anchors.centerIn: parent
    padding: Kirigami.Units.largeSpacing
    modal: true
    dim: true
    focus: true
    closePolicy: QQC2.Popup.NoAutoClose

    background: Rectangle {
        color: CherryStyle.surfacePopup
        border.color: CherryStyle.popupBorderColor
        border.width: 1
        radius: CherryStyle.radiusLarge

        // Multi-layer drop shadow
        Rectangle {
            anchors.fill: parent
            anchors.margins: -1
            z: -1
            color: "transparent"
            border.color: Qt.rgba(0, 0, 0, 0.4)
            radius: CherryStyle.radiusLarge + 1
        }
        Rectangle {
            anchors.fill: parent
            anchors.margins: -4
            z: -2
            color: "transparent"
            border.color: Qt.rgba(0, 0, 0, 0.2)
            radius: CherryStyle.radiusLarge + 4
        }
    }

    ColumnLayout {
        anchors.fill: parent
        spacing: Kirigami.Units.largeSpacing

        // Header Title
        ColumnLayout {
            Layout.fillWidth: true
            spacing: 4

            RowLayout {
                spacing: Kirigami.Units.smallSpacing

                Kirigami.Icon {
                    source: "vcs-branch"
                    width: 28
                    height: 28
                    color: Kirigami.Theme.highlightColor
                }

                QQC2.Label {
                    text: qsTr("Welcome to cherrygi")
                    font.bold: true
                    font.pixelSize: CherryStyle.basePixelSize + 6
                    color: Kirigami.Theme.textColor
                }
            }

            QQC2.Label {
                text: qsTr("Select a backend mode to start your session:")
                font.pixelSize: CherryStyle.smallFont.pixelSize
                color: Kirigami.Theme.disabledTextColor
            }
        }

        // Two Cards Side-by-Side
        RowLayout {
            Layout.fillWidth: true
            Layout.fillHeight: true
            spacing: Kirigami.Units.mediumSpacing

            // ==========================================
            // CARD 1: REAL GIT
            // ==========================================
            Rectangle {
                id: realCard
                Layout.fillWidth: true
                Layout.fillHeight: true
                radius: CherryStyle.radiusMedium
                color: realMouse.containsMouse ? CherryStyle.hoverBackground : CherryStyle.surfaceCard
                border.color: realMouse.containsMouse ? Kirigami.Theme.highlightColor : CherryStyle.borderColor
                border.width: realMouse.containsMouse ? 2 : 1

                MouseArea {
                    id: realMouse
                    anchors.fill: parent
                    hoverEnabled: true
                    cursorShape: Qt.PointingHandCursor
                    onClicked: {
                        appController.selectBackend("real");
                    }
                }

                ColumnLayout {
                    anchors.fill: parent
                    anchors.margins: Kirigami.Units.largeSpacing
                    spacing: Kirigami.Units.smallSpacing

                    RowLayout {
                        Layout.fillWidth: true
                        spacing: Kirigami.Units.smallSpacing

                        Kirigami.Icon {
                            source: "folder-git"
                            width: 24
                            height: 24
                            color: Kirigami.Theme.highlightColor
                        }

                        QQC2.Label {
                            text: qsTr("Real Git Backend")
                            font.bold: true
                            font.pixelSize: CherryStyle.basePixelSize + 1
                            color: Kirigami.Theme.textColor
                            Layout.fillWidth: true
                        }

                        Rectangle {
                            implicitWidth: realPill.implicitWidth + 8
                            implicitHeight: 18
                            radius: 9
                            color: Qt.rgba(Kirigami.Theme.highlightColor.r, Kirigami.Theme.highlightColor.g, Kirigami.Theme.highlightColor.b, 0.2)
                            border.color: Kirigami.Theme.highlightColor
                            border.width: 1

                            QQC2.Label {
                                id: realPill
                                anchors.centerIn: parent
                                text: qsTr("Live")
                                font.pixelSize: CherryStyle.smallFont.pixelSize - 1
                                font.bold: true
                                color: Kirigami.Theme.highlightColor
                            }
                        }
                    }

                    QQC2.Label {
                        text: qsTr("Work with real repositories on this machine. Stage files, view line diffs, commit, soft-reset undo, create branches, and stash changes.")
                        font.pixelSize: CherryStyle.smallFont.pixelSize
                        color: Kirigami.Theme.textColor
                        wrapMode: Text.Wrap
                        Layout.fillWidth: true
                        Layout.fillHeight: true
                    }

                    QQC2.Button {
                        Layout.fillWidth: true
                        text: qsTr("Use Real Git")
                        icon.name: "dialog-ok"
                        highlighted: true
                        onClicked: appController.selectBackend("real")
                    }
                }
            }

            // ==========================================
            // CARD 2: MOCK DEMO
            // ==========================================
            Rectangle {
                id: mockCard
                Layout.fillWidth: true
                Layout.fillHeight: true
                radius: CherryStyle.radiusMedium
                color: mockMouse.containsMouse ? CherryStyle.hoverBackground : CherryStyle.surfaceCard
                border.color: mockMouse.containsMouse ? Kirigami.Theme.highlightColor : CherryStyle.borderColor
                border.width: mockMouse.containsMouse ? 2 : 1

                MouseArea {
                    id: mockMouse
                    anchors.fill: parent
                    hoverEnabled: true
                    cursorShape: Qt.PointingHandCursor
                    onClicked: {
                        appController.selectBackend("mock");
                    }
                }

                ColumnLayout {
                    anchors.fill: parent
                    anchors.margins: Kirigami.Units.largeSpacing
                    spacing: Kirigami.Units.smallSpacing

                    RowLayout {
                        Layout.fillWidth: true
                        spacing: Kirigami.Units.smallSpacing

                        Kirigami.Icon {
                            source: "system-run"
                            width: 24
                            height: 24
                            color: "#e5a50a"
                        }

                        QQC2.Label {
                            text: qsTr("Mock Demo Mode")
                            font.bold: true
                            font.pixelSize: CherryStyle.basePixelSize + 1
                            color: Kirigami.Theme.textColor
                            Layout.fillWidth: true
                        }

                        Rectangle {
                            implicitWidth: mockPill.implicitWidth + 8
                            implicitHeight: 18
                            radius: 9
                            color: Qt.rgba(0.9, 0.65, 0.04, 0.2)
                            border.color: "#e5a50a"
                            border.width: 1

                            QQC2.Label {
                                id: mockPill
                                anchors.centerIn: parent
                                text: qsTr("Sandbox")
                                font.pixelSize: CherryStyle.smallFont.pixelSize - 1
                                font.bold: true
                                color: "#e5a50a"
                            }
                        }
                    }

                    QQC2.Label {
                        text: qsTr("Explore cherrygi with sample GitHub Desktop mock repositories (desktop, cherrygi-core, plasma-workspace) without modifying local files.")
                        font.pixelSize: CherryStyle.smallFont.pixelSize
                        color: Kirigami.Theme.textColor
                        wrapMode: Text.Wrap
                        Layout.fillWidth: true
                        Layout.fillHeight: true
                    }

                    QQC2.Button {
                        Layout.fillWidth: true
                        text: qsTr("Explore Mock Demo")
                        icon.name: "media-playback-start"
                        onClicked: appController.selectBackend("mock")
                    }
                }
            }
        }

        // Footer Note
        RowLayout {
            Layout.fillWidth: true
            spacing: Kirigami.Units.smallSpacing

            Kirigami.Icon {
                source: "dialog-information"
                width: 14
                height: 14
                color: Kirigami.Theme.disabledTextColor
            }

            QQC2.Label {
                text: qsTr("Saved repositories are stored cleanly in ~/.config/cherrygi/cherrygi.ini.")
                font.pixelSize: CherryStyle.smallFont.pixelSize - 1
                color: Kirigami.Theme.disabledTextColor
                Layout.fillWidth: true
            }
        }
    }
}
