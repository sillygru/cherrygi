import QtQuick
import QtQuick.Controls as QQC2
import QtQuick.Layouts
import org.kde.kirigami as Kirigami
import org.kde.cherrygi
import "../style"

Rectangle {
    id: root
    implicitHeight: CherryStyle.headerHeight
    color: CherryStyle.surfaceHeader

    // Subtle bottom border with high contrast
    Rectangle {
        anchors.bottom: parent.bottom
        anchors.left: parent.left
        anchors.right: parent.right
        height: 1
        color: CherryStyle.borderColor
        z: 2
    }

    RepoDropdown {
        id: repoDropdown
        y: root.height + 2
        x: repoSegment.x + 4
    }

    BranchDropdown {
        id: branchDropdown
        y: root.height + 2
        x: branchSegment.x + 4
    }

    RemoteDropdown {
        id: remoteMenu
    }

    RowLayout {
        anchors.fill: parent
        spacing: 0

        // ==========================================
        // SEGMENT 1: Current Repository
        // ==========================================
        Item {
            id: repoSegment
            Layout.fillHeight: true
            Layout.preferredWidth: 280
            Layout.minimumWidth: 190

            Rectangle {
                anchors.fill: parent
                anchors.margins: 6
                radius: CherryStyle.radiusMedium
                color: repoDropdown.visible ? CherryStyle.activeBackground : (repoMouse.containsMouse ? CherryStyle.hoverBackground : "transparent")
            }

            ColumnLayout {
                anchors.fill: parent
                anchors.leftMargin: Kirigami.Units.mediumSpacing
                anchors.rightMargin: Kirigami.Units.mediumSpacing
                anchors.topMargin: 8
                anchors.bottomMargin: 8
                spacing: 1

                QQC2.Label {
                    text: qsTr("Current repository")
                    font.pixelSize: CherryStyle.smallFont.pixelSize - 1
                    color: Kirigami.Theme.disabledTextColor
                }

                RowLayout {
                    Layout.fillWidth: true
                    spacing: Kirigami.Units.smallSpacing

                    Kirigami.Icon {
                        source: "folder-git"
                        width: 16
                        height: 16
                        color: repoDropdown.visible ? Kirigami.Theme.highlightColor : Kirigami.Theme.textColor
                    }

                    QQC2.Label {
                        text: appController.currentRepoName
                        font.bold: true
                        color: repoDropdown.visible ? Kirigami.Theme.highlightColor : Kirigami.Theme.textColor
                        elide: Text.ElideRight
                        Layout.fillWidth: true
                    }

                    Kirigami.Icon {
                        source: "go-down-symbolic"
                        width: 12
                        height: 12
                        color: repoDropdown.visible ? Kirigami.Theme.highlightColor : Kirigami.Theme.disabledTextColor
                    }
                }
            }

            MouseArea {
                id: repoMouse
                anchors.fill: parent
                hoverEnabled: true
                cursorShape: Qt.PointingHandCursor
                onClicked: {
                    if (repoDropdown.visible) {
                        repoDropdown.close();
                    } else {
                        branchDropdown.close();
                        repoDropdown.open();
                    }
                }
            }
        }

        // Vertical Separator
        Rectangle {
            Layout.preferredWidth: 1
            Layout.fillHeight: true
            Layout.topMargin: 8
            Layout.bottomMargin: 8
            color: CherryStyle.subtleBorderColor
        }

        // ==========================================
        // SEGMENT 2: Current Branch
        // ==========================================
        Item {
            id: branchSegment
            Layout.fillHeight: true
            Layout.preferredWidth: 310
            Layout.minimumWidth: 210

            Rectangle {
                anchors.fill: parent
                anchors.margins: 6
                radius: CherryStyle.radiusMedium
                color: branchDropdown.visible ? CherryStyle.activeBackground : (branchMouse.containsMouse ? CherryStyle.hoverBackground : "transparent")
            }

            ColumnLayout {
                anchors.fill: parent
                anchors.leftMargin: Kirigami.Units.mediumSpacing
                anchors.rightMargin: Kirigami.Units.mediumSpacing
                anchors.topMargin: 8
                anchors.bottomMargin: 8
                spacing: 1

                QQC2.Label {
                    text: qsTr("Current branch")
                    font.pixelSize: CherryStyle.smallFont.pixelSize - 1
                    color: Kirigami.Theme.disabledTextColor
                }

                RowLayout {
                    Layout.fillWidth: true
                    spacing: Kirigami.Units.smallSpacing

                    Kirigami.Icon {
                        source: "vcs-branch"
                        width: 16
                        height: 16
                        color: branchDropdown.visible ? Kirigami.Theme.highlightColor : Kirigami.Theme.textColor
                    }

                    QQC2.Label {
                        text: appController.currentBranchName
                        font.bold: true
                        color: branchDropdown.visible ? Kirigami.Theme.highlightColor : Kirigami.Theme.textColor
                        elide: Text.ElideRight
                        Layout.fillWidth: true
                    }

                    // PR Badge if present
                    Rectangle {
                        visible: appController.currentBranchPr !== ""
                        implicitWidth: prRow.implicitWidth + 12
                        implicitHeight: 20
                        radius: 10
                        color: Qt.rgba(0.2, 0.6, 1.0, 0.18)

                        RowLayout {
                            id: prRow
                            anchors.centerIn: parent
                            spacing: 3

                            QQC2.Label {
                                id: prHeaderLabel
                                text: appController.currentBranchPr
                                font.pixelSize: CherryStyle.smallFont.pixelSize
                                font.bold: true
                                color: "#4595f6"
                            }

                            Kirigami.Icon {
                                source: "dialog-ok"
                                width: 11
                                height: 11
                                color: "#2ec27e"
                                visible: appController.currentBranchPrActive
                            }
                        }
                    }

                    Kirigami.Icon {
                        source: "go-down-symbolic"
                        width: 12
                        height: 12
                        color: branchDropdown.visible ? Kirigami.Theme.highlightColor : Kirigami.Theme.disabledTextColor
                    }
                }
            }

            MouseArea {
                id: branchMouse
                anchors.fill: parent
                hoverEnabled: true
                cursorShape: Qt.PointingHandCursor
                onClicked: {
                    if (branchDropdown.visible) {
                        branchDropdown.close();
                    } else {
                        repoDropdown.close();
                        branchDropdown.open();
                    }
                }
            }
        }

        // Vertical Separator
        Rectangle {
            Layout.preferredWidth: 1
            Layout.fillHeight: true
            Layout.topMargin: 8
            Layout.bottomMargin: 8
            color: CherryStyle.subtleBorderColor
        }

        // ==========================================
        // SEGMENT 3: Sync & Remote Action
        // ==========================================
        Item {
            id: remoteSegment
            Layout.fillHeight: true
            Layout.preferredWidth: 300
            Layout.minimumWidth: 220

            Rectangle {
                anchors.fill: parent
                anchors.margins: 6
                radius: CherryStyle.radiusMedium
                color: remotePrimaryMouse.containsMouse ? CherryStyle.hoverBackground : "transparent"
            }

            RowLayout {
                anchors.fill: parent
                anchors.leftMargin: Kirigami.Units.mediumSpacing
                anchors.rightMargin: 4
                anchors.topMargin: 8
                anchors.bottomMargin: 8
                spacing: Kirigami.Units.smallSpacing

                // Primary Clickable Action Area
                Item {
                    Layout.fillWidth: true
                    Layout.fillHeight: true

                    RowLayout {
                        anchors.fill: parent
                        spacing: Kirigami.Units.smallSpacing

                        // Sync Icon with controlled rotation
                        Item {
                            width: 22
                            height: 22
                            Layout.alignment: Qt.AlignVCenter

                            Kirigami.Icon {
                                id: syncIcon
                                anchors.centerIn: parent
                                source: {
                                    if (appController.behindCount > 0) return "vcs-pull-symbolic";
                                    if (appController.aheadCount > 0) return "vcs-push-symbolic";
                                    return "view-refresh";
                                }
                                width: 18
                                height: 18
                                color: Kirigami.Theme.textColor
                                rotation: 0

                                NumberAnimation on rotation {
                                    id: spinAnim
                                    running: appController.isFetching || appController.isPulling || appController.isPushing
                                    from: 0
                                    to: 360
                                    loops: Animation.Infinite
                                    duration: 900
                                    alwaysRunToEnd: false
                                }

                                Connections {
                                    target: appController
                                    function onRemoteStatusChanged() {
                                        if (!appController.isFetching && !appController.isPulling && !appController.isPushing) {
                                            syncIcon.rotation = 0;
                                        }
                                    }
                                }
                            }
                        }

                        // Text Stack
                        ColumnLayout {
                            Layout.fillWidth: true
                            spacing: 1

                            RowLayout {
                                Layout.fillWidth: true
                                spacing: 6

                                QQC2.Label {
                                    text: {
                                        if (appController.isPulling) return qsTr("Pulling...");
                                        if (appController.isPushing) return qsTr("Pushing...");
                                        if (appController.isFetching) return qsTr("Fetching...");
                                        if (appController.behindCount > 0) return qsTr("Pull origin");
                                        if (appController.aheadCount > 0) return qsTr("Push origin");
                                        return qsTr("Fetch origin");
                                    }
                                    font.bold: true
                                    color: Kirigami.Theme.textColor
                                    elide: Text.ElideRight
                                    Layout.fillWidth: true
                                }

                                // Clean non-overlapping Badge count (e.g. 3 ↓ or 1 ↑)
                                Rectangle {
                                    visible: appController.behindCount > 0 || appController.aheadCount > 0
                                    implicitWidth: syncBadgeRow.implicitWidth + 10
                                    implicitHeight: 18
                                    radius: 9
                                    color: CherryStyle.surfaceCardElevated

                                    RowLayout {
                                        id: syncBadgeRow
                                        anchors.centerIn: parent
                                        spacing: 2

                                        QQC2.Label {
                                            id: syncCountLabel
                                            text: appController.behindCount > 0 ? ("" + appController.behindCount) : ("" + appController.aheadCount)
                                            font.pixelSize: CherryStyle.smallFont.pixelSize - 1
                                            font.bold: true
                                            color: Kirigami.Theme.textColor
                                        }

                                        Kirigami.Icon {
                                            source: appController.behindCount > 0 ? "arrow-down" : "arrow-up"
                                            width: 10
                                            height: 10
                                            color: Kirigami.Theme.textColor
                                        }
                                    }
                                }
                            }

                            QQC2.Label {
                                text: appController.lastFetchedText
                                font.pixelSize: CherryStyle.smallFont.pixelSize - 1
                                color: Kirigami.Theme.disabledTextColor
                                elide: Text.ElideRight
                                Layout.fillWidth: true
                            }
                        }
                    }

                    MouseArea {
                        id: remotePrimaryMouse
                        anchors.fill: parent
                        hoverEnabled: true
                        cursorShape: Qt.PointingHandCursor
                        onClicked: {
                            if (appController.behindCount > 0) {
                                appController.pullOrigin();
                            } else if (appController.aheadCount > 0) {
                                appController.pushOrigin();
                            } else {
                                appController.fetchOrigin();
                            }
                        }
                    }
                }

                // Sub-menu Trigger Button
                Rectangle {
                    width: 32
                    height: 32
                    radius: CherryStyle.radiusMedium
                    color: remoteMenuMouse.containsMouse ? CherryStyle.hoverBackground : "transparent"

                    Kirigami.Icon {
                        anchors.centerIn: parent
                        source: "go-down-symbolic"
                        width: 12
                        height: 12
                        color: Kirigami.Theme.disabledTextColor
                    }

                    MouseArea {
                        id: remoteMenuMouse
                        anchors.fill: parent
                        hoverEnabled: true
                        cursorShape: Qt.PointingHandCursor
                        onClicked: remoteMenu.popup(remoteSegment, remoteSegment.width - remoteMenu.width, remoteSegment.height + 2)
                    }
                }
            }
        }

        // Vertical Separator
        Rectangle {
            Layout.preferredWidth: 1
            Layout.fillHeight: true
            Layout.topMargin: 8
            Layout.bottomMargin: 8
            color: CherryStyle.subtleBorderColor
        }

        // Fill remaining header space
        Item {
            Layout.fillWidth: true
            Layout.fillHeight: true
        }
    }
}

