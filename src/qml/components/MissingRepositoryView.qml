import QtQuick
import QtQuick.Controls as QQC2
import QtQuick.Layouts
import org.kde.kirigami as Kirigami
import org.kde.cherrygi
import "../style"

Rectangle {
    id: root
    color: Kirigami.Theme.backgroundColor

    QQC2.ScrollView {
        anchors.fill: parent
        contentWidth: availableWidth

        Item {
            width: root.width
            implicitHeight: contentColumn.implicitHeight + 80

            ColumnLayout {
                id: contentColumn
                anchors.centerIn: parent
                width: Math.min(parent.width - 48, 640)
                spacing: Kirigami.Units.largeSpacing

                // Header & Icon Section
                ColumnLayout {
                    Layout.fillWidth: true
                    spacing: Kirigami.Units.smallSpacing
                    Layout.alignment: Qt.AlignHCenter

                    Rectangle {
                        Layout.alignment: Qt.AlignHCenter
                        width: 72
                        height: 72
                        radius: 36
                        color: Qt.rgba(Kirigami.Theme.negativeTextColor.r, Kirigami.Theme.negativeTextColor.g, Kirigami.Theme.negativeTextColor.b, 0.12)
                        border.color: Qt.rgba(Kirigami.Theme.negativeTextColor.r, Kirigami.Theme.negativeTextColor.g, Kirigami.Theme.negativeTextColor.b, 0.3)
                        border.width: 1

                        Kirigami.Icon {
                            anchors.centerIn: parent
                            width: 40
                            height: 40
                            source: "folder-missing"
                            color: Kirigami.Theme.negativeTextColor
                        }
                    }

                    QQC2.Label {
                        Layout.fillWidth: true
                        Layout.topMargin: Kirigami.Units.smallSpacing
                        text: qsTr("Can't find \"%1\"").arg(appController.missingRepoName !== "" ? appController.missingRepoName : appController.currentRepoName)
                        font.bold: true
                        font.pixelSize: CherryStyle.largeFont.pixelSize + 4
                        horizontalAlignment: Text.AlignHCenter
                        wrapMode: Text.Wrap
                        color: Kirigami.Theme.textColor
                    }

                    QQC2.Label {
                        Layout.fillWidth: true
                        text: qsTr("This repository could not be found at its previous path:\n%1\n\nIt may have been moved, deleted, or stored on an unmounted disk.").arg(appController.missingRepoPath !== "" ? appController.missingRepoPath : appController.currentRepoPath)
                        font.pixelSize: CherryStyle.defaultFont.pixelSize
                        horizontalAlignment: Text.AlignHCenter
                        wrapMode: Text.Wrap
                        color: Kirigami.Theme.disabledTextColor
                    }
                }

                Kirigami.Separator {
                    Layout.fillWidth: true
                    Layout.topMargin: Kirigami.Units.smallSpacing
                    Layout.bottomMargin: Kirigami.Units.smallSpacing
                }

                // Action Cards
                ColumnLayout {
                    Layout.fillWidth: true
                    spacing: Kirigami.Units.mediumSpacing

                    // 1. LOCATE ACTION
                    Rectangle {
                        Layout.fillWidth: true
                        implicitHeight: locateRow.implicitHeight + Kirigami.Units.largeSpacing * 2
                        radius: CherryStyle.radiusMedium
                        color: locateMouse.containsMouse ? CherryStyle.hoverBackground : CherryStyle.surfaceCard
                        border.color: locateMouse.containsMouse ? Kirigami.Theme.highlightColor : CherryStyle.borderColor
                        border.width: 1

                        RowLayout {
                            id: locateRow
                            anchors.fill: parent
                            anchors.margins: Kirigami.Units.largeSpacing
                            spacing: Kirigami.Units.mediumSpacing

                            Kirigami.Icon {
                                source: "folder-open"
                                width: 28
                                height: 28
                                color: Kirigami.Theme.highlightColor
                            }

                            ColumnLayout {
                                Layout.fillWidth: true
                                spacing: 2

                                QQC2.Label {
                                    text: qsTr("Locate...")
                                    font.bold: true
                                    font.pixelSize: CherryStyle.defaultFont.pixelSize + 1
                                    color: Kirigami.Theme.textColor
                                }

                                QQC2.Label {
                                    text: qsTr("Browse to select the new folder location if this repository was moved or renamed.")
                                    font.pixelSize: CherryStyle.smallFont.pixelSize
                                    color: Kirigami.Theme.disabledTextColor
                                    wrapMode: Text.Wrap
                                    Layout.fillWidth: true
                                }
                            }

                            QQC2.Button {
                                text: qsTr("Locate...")
                                icon.name: "folder-open"
                                highlighted: true
                                onClicked: appController.locateMissingRepository()
                            }
                        }

                        MouseArea {
                            id: locateMouse
                            anchors.fill: parent
                            hoverEnabled: true
                            cursorShape: Qt.PointingHandCursor
                            onClicked: appController.locateMissingRepository()
                        }
                    }

                    // 2. CLONE AGAIN ACTION
                    Rectangle {
                        Layout.fillWidth: true
                        implicitHeight: cloneRow.implicitHeight + Kirigami.Units.largeSpacing * 2
                        radius: CherryStyle.radiusMedium
                        color: cloneMouse.containsMouse ? CherryStyle.hoverBackground : CherryStyle.surfaceCard
                        border.color: cloneMouse.containsMouse ? Kirigami.Theme.highlightColor : CherryStyle.borderColor
                        border.width: 1

                        RowLayout {
                            id: cloneRow
                            anchors.fill: parent
                            anchors.margins: Kirigami.Units.largeSpacing
                            spacing: Kirigami.Units.mediumSpacing

                            Kirigami.Icon {
                                source: "download"
                                width: 28
                                height: 28
                                color: Kirigami.Theme.highlightColor
                            }

                            ColumnLayout {
                                Layout.fillWidth: true
                                spacing: 2

                                QQC2.Label {
                                    text: qsTr("Clone Again...")
                                    font.bold: true
                                    font.pixelSize: CherryStyle.defaultFont.pixelSize + 1
                                    color: Kirigami.Theme.textColor
                                }

                                QQC2.Label {
                                    text: (appController.missingRepoRemoteUrl !== "") ?
                                          qsTr("Clone a fresh copy from '%1' into a new directory.").arg(appController.missingRepoRemoteUrl) :
                                          qsTr("Clone a fresh copy of this repository from its remote URL into a new directory.")
                                    font.pixelSize: CherryStyle.smallFont.pixelSize
                                    color: Kirigami.Theme.disabledTextColor
                                    wrapMode: Text.Wrap
                                    Layout.fillWidth: true
                                }
                            }

                            QQC2.Button {
                                text: qsTr("Clone Again...")
                                icon.name: "download"
                                onClicked: appController.showCloneDialog(appController.missingRepoRemoteUrl)
                            }
                        }

                        MouseArea {
                            id: cloneMouse
                            anchors.fill: parent
                            hoverEnabled: true
                            cursorShape: Qt.PointingHandCursor
                            onClicked: appController.showCloneDialog(appController.missingRepoRemoteUrl)
                        }
                    }

                    // 3. RE-CHECK ACTION
                    Rectangle {
                        Layout.fillWidth: true
                        implicitHeight: recheckRow.implicitHeight + Kirigami.Units.largeSpacing * 2
                        radius: CherryStyle.radiusMedium
                        color: recheckMouse.containsMouse ? CherryStyle.hoverBackground : CherryStyle.surfaceCard
                        border.color: recheckMouse.containsMouse ? Kirigami.Theme.highlightColor : CherryStyle.borderColor
                        border.width: 1

                        RowLayout {
                            id: recheckRow
                            anchors.fill: parent
                            anchors.margins: Kirigami.Units.largeSpacing
                            spacing: Kirigami.Units.mediumSpacing

                            Kirigami.Icon {
                                source: "view-refresh"
                                width: 28
                                height: 28
                                color: Kirigami.Theme.textColor
                            }

                            ColumnLayout {
                                Layout.fillWidth: true
                                spacing: 2

                                QQC2.Label {
                                    text: qsTr("Re-check")
                                    font.bold: true
                                    font.pixelSize: CherryStyle.defaultFont.pixelSize + 1
                                    color: Kirigami.Theme.textColor
                                }

                                QQC2.Label {
                                    text: qsTr("Test if the repository folder is now accessible (e.g. external drive reconnected).")
                                    font.pixelSize: CherryStyle.smallFont.pixelSize
                                    color: Kirigami.Theme.disabledTextColor
                                    wrapMode: Text.Wrap
                                    Layout.fillWidth: true
                                }
                            }

                            QQC2.Button {
                                text: qsTr("Re-check")
                                icon.name: "view-refresh"
                                onClicked: appController.recheckMissingRepository()
                            }
                        }

                        MouseArea {
                            id: recheckMouse
                            anchors.fill: parent
                            hoverEnabled: true
                            cursorShape: Qt.PointingHandCursor
                            onClicked: appController.recheckMissingRepository()
                        }
                    }

                    // 4. REMOVE ACTION
                    Rectangle {
                        Layout.fillWidth: true
                        implicitHeight: removeRow.implicitHeight + Kirigami.Units.largeSpacing * 2
                        radius: CherryStyle.radiusMedium
                        color: removeMouse.containsMouse ? Qt.rgba(Kirigami.Theme.negativeTextColor.r, Kirigami.Theme.negativeTextColor.g, Kirigami.Theme.negativeTextColor.b, 0.08) : CherryStyle.surfaceCard
                        border.color: removeMouse.containsMouse ? Kirigami.Theme.negativeTextColor : CherryStyle.subtleBorderColor
                        border.width: 1

                        RowLayout {
                            id: removeRow
                            anchors.fill: parent
                            anchors.margins: Kirigami.Units.largeSpacing
                            spacing: Kirigami.Units.mediumSpacing

                            Kirigami.Icon {
                                source: "edit-delete"
                                width: 28
                                height: 28
                                color: Kirigami.Theme.negativeTextColor
                            }

                            ColumnLayout {
                                Layout.fillWidth: true
                                spacing: 2

                                QQC2.Label {
                                    text: qsTr("Remove from Cherrygi")
                                    font.bold: true
                                    font.pixelSize: CherryStyle.defaultFont.pixelSize + 1
                                    color: Kirigami.Theme.textColor
                                }

                                QQC2.Label {
                                    text: qsTr("Remove this entry from the repository list without altering any remaining files.")
                                    font.pixelSize: CherryStyle.smallFont.pixelSize
                                    color: Kirigami.Theme.disabledTextColor
                                    wrapMode: Text.Wrap
                                    Layout.fillWidth: true
                                }
                            }

                            QQC2.Button {
                                text: qsTr("Remove")
                                icon.name: "edit-delete"
                                onClicked: appController.removeCurrentMissingRepository()
                            }
                        }

                        MouseArea {
                            id: removeMouse
                            anchors.fill: parent
                            hoverEnabled: true
                            cursorShape: Qt.PointingHandCursor
                            onClicked: appController.removeCurrentMissingRepository()
                        }
                    }
                }
            }
        }
    }
}
