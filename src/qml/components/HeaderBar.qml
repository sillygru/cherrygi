import QtQuick
import QtQuick.Controls as QQC2
import QtQuick.Layouts
import org.kde.kirigami as Kirigami
import "../style"

Rectangle {
    id: root
    implicitHeight: CherryStyle.headerHeight
    color: Kirigami.Theme.backgroundColor

    border.color: CherryStyle.borderColor
    border.width: 1

    RepoDropdown {
        id: repoDropdown
        y: root.height + 4
        x: repoSegment.x
    }

    BranchDropdown {
        id: branchDropdown
        y: root.height + 4
        x: branchSegment.x
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
            Layout.preferredWidth: 260
            Layout.minimumWidth: 180

            Rectangle {
                anchors.fill: parent
                anchors.margins: 4
                radius: CherryStyle.radiusSmall
                color: repoMouse.containsMouse ? CherryStyle.hoverBackground : "transparent"
            }

            ColumnLayout {
                anchors.fill: parent
                anchors.leftMargin: Kirigami.Units.mediumSpacing
                anchors.rightMargin: Kirigami.Units.mediumSpacing
                anchors.topMargin: 6
                anchors.bottomMargin: 6
                spacing: 1

                QQC2.Label {
                    text: i18n("Current repository")
                    font.pixelSize: CherryStyle.smallFont.pixelSize - 1
                    color: Kirigami.Theme.disabledTextColor
                }

                RowLayout {
                    Layout.fillWidth: true
                    spacing: Kirigami.Units.smallSpacing

                    Kirigami.Icon {
                        source: "folder-git"
                        width: Kirigami.Units.iconSizes.small
                        height: width
                        color: Kirigami.Theme.textColor
                    }

                    QQC2.Label {
                        text: appController.currentRepoName
                        font.bold: true
                        color: Kirigami.Theme.textColor
                        elide: Text.ElideRight
                        Layout.fillWidth: true
                    }

                    Kirigami.Icon {
                        source: "arrow-down"
                        width: 12
                        height: 12
                        color: Kirigami.Theme.disabledTextColor
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
            color: CherryStyle.borderColor
        }

        // ==========================================
        // SEGMENT 2: Current Branch
        // ==========================================
        Item {
            id: branchSegment
            Layout.fillHeight: true
            Layout.preferredWidth: 280
            Layout.minimumWidth: 200

            Rectangle {
                anchors.fill: parent
                anchors.margins: 4
                radius: CherryStyle.radiusSmall
                color: branchMouse.containsMouse ? CherryStyle.hoverBackground : "transparent"
            }

            ColumnLayout {
                anchors.fill: parent
                anchors.leftMargin: Kirigami.Units.mediumSpacing
                anchors.rightMargin: Kirigami.Units.mediumSpacing
                anchors.topMargin: 6
                anchors.bottomMargin: 6
                spacing: 1

                QQC2.Label {
                    text: i18n("Current branch")
                    font.pixelSize: CherryStyle.smallFont.pixelSize - 1
                    color: Kirigami.Theme.disabledTextColor
                }

                RowLayout {
                    Layout.fillWidth: true
                    spacing: Kirigami.Units.smallSpacing

                    Kirigami.Icon {
                        source: "vcs-branch"
                        width: Kirigami.Units.iconSizes.small
                        height: width
                        color: Kirigami.Theme.textColor
                    }

                    QQC2.Label {
                        text: appController.currentBranchName
                        font.bold: true
                        color: Kirigami.Theme.textColor
                        elide: Text.ElideRight
                        Layout.fillWidth: true
                    }

                    // PR Badge if present
                    Rectangle {
                        visible: appController.currentBranchPr !== ""
                        width: prHeaderLabel.width + 12
                        height: 20
                        radius: 10
                        color: Qt.rgba(0.2, 0.6, 1.0, 0.15)
                        border.color: Qt.rgba(0.2, 0.6, 1.0, 0.5)
                        border.width: 1

                        RowLayout {
                            anchors.centerIn: parent
                            spacing: 2

                            QQC2.Label {
                                id: prHeaderLabel
                                text: appController.currentBranchPr
                                font.pixelSize: CherryStyle.smallFont.pixelSize
                                font.bold: true
                                color: "#3584e4"
                            }

                            Kirigami.Icon {
                                source: "dialog-ok"
                                width: 12
                                height: 12
                                color: "#2ec27e"
                                visible: appController.currentBranchPrActive
                            }
                        }
                    }

                    Kirigami.Icon {
                        source: "arrow-down"
                        width: 12
                        height: 12
                        color: Kirigami.Theme.disabledTextColor
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
            color: CherryStyle.borderColor
        }

        // ==========================================
        // SEGMENT 3: Sync & Remote Action
        // ==========================================
        Item {
            id: remoteSegment
            Layout.fillHeight: true
            Layout.preferredWidth: 260
            Layout.minimumWidth: 200

            Rectangle {
                anchors.fill: parent
                anchors.margins: 4
                radius: CherryStyle.radiusSmall
                color: remoteMouse.containsMouse ? CherryStyle.hoverBackground : "transparent"
            }

            RowLayout {
                anchors.fill: parent
                anchors.leftMargin: Kirigami.Units.mediumSpacing
                anchors.rightMargin: Kirigami.Units.mediumSpacing
                anchors.topMargin: 4
                anchors.bottomMargin: 4
                spacing: Kirigami.Units.smallSpacing

                // Sync Icon
                Kirigami.Icon {
                    source: {
                        if (appController.behindCount > 0) return "vcs-pull-symbolic";
                        if (appController.aheadCount > 0) return "vcs-push-symbolic";
                        return "view-refresh";
                    }
                    width: Kirigami.Units.iconSizes.medium
                    height: width
                    color: Kirigami.Theme.textColor

                    // Spinning animation when fetching/pulling/pushing
                    RotationAnimator on rotation {
                        running: appController.isFetching || appController.isPulling || appController.isPushing
                        from: 0
                        to: 360
                        loops: Animation.Infinite
                        duration: 800
                    }
                }

                // Text Stack
                ColumnLayout {
                    Layout.fillWidth: true
                    spacing: 1

                    RowLayout {
                        spacing: 4

                        QQC2.Label {
                            text: {
                                if (appController.isPulling) return i18n("Pulling...");
                                if (appController.isPushing) return i18n("Pushing...");
                                if (appController.isFetching) return i18n("Fetching...");
                                if (appController.behindCount > 0) return i18n("Pull origin");
                                if (appController.aheadCount > 0) return i18n("Push origin");
                                return i18n("Fetch origin");
                            }
                            font.bold: true
                            color: Kirigami.Theme.textColor
                        }

                        // Badge count (e.g. 3 v or 1 ^)
                        Rectangle {
                            visible: appController.behindCount > 0 || appController.aheadCount > 0
                            width: syncCountLabel.width + 10
                            height: 18
                            radius: 9
                            color: CherryStyle.cardBackground
                            border.color: CherryStyle.borderColor
                            border.width: 1

                            RowLayout {
                                anchors.centerIn: parent
                                spacing: 2

                                QQC2.Label {
                                    id: syncCountLabel
                                    text: appController.behindCount > 0 ? ("" + appController.behindCount) : ("" + appController.aheadCount)
                                    font.pixelSize: CherryStyle.smallFont.pixelSize
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
                    }
                }

                // Sub-menu trigger
                QQC2.ToolButton {
                    icon.name: "arrow-down"
                    icon.width: 12
                    icon.height: 12
                    onClicked: remoteMenu.popup(this, 0, this.height)
                }
            }

            MouseArea {
                id: remoteMouse
                anchors.fill: parent
                anchors.rightMargin: 30
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

        // Vertical Separator
        Rectangle {
            Layout.preferredWidth: 1
            Layout.fillHeight: true
            Layout.topMargin: 8
            Layout.bottomMargin: 8
            color: CherryStyle.borderColor
        }

        // Fill remaining header space
        Item {
            Layout.fillWidth: true
            Layout.fillHeight: true
        }
    }
}
