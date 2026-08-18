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
                    color: CherryStyle.secondaryTextColor
                }

                RowLayout {
                    Layout.fillWidth: true
                    spacing: Kirigami.Units.smallSpacing

                    Kirigami.Icon {
                        source: appController.isCurrentRepoMissing ? "dialog-warning" : "folder-git"
                        width: 16
                        height: 16
                        Layout.alignment: Qt.AlignVCenter
                        color: appController.isCurrentRepoMissing ? CherryStyle.deletionColor : (repoDropdown.visible ? CherryStyle.accentColor : Kirigami.Theme.textColor)
                    }

                    QQC2.Label {
                        text: appController.isCurrentRepoMissing ? (appController.missingRepoName !== "" ? appController.missingRepoName : appController.currentRepoName) : appController.currentRepoName
                        font.bold: true
                        color: appController.isCurrentRepoMissing ? CherryStyle.deletionColor : (repoDropdown.visible ? CherryStyle.accentColor : Kirigami.Theme.textColor)
                        elide: Text.ElideRight
                        Layout.fillWidth: true
                    }

                    Kirigami.Icon {
                        source: "go-down-symbolic"
                        width: 12
                        height: 12
                        color: repoDropdown.visible ? CherryStyle.accentColor : CherryStyle.secondaryTextColor
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
            enabled: !appController.isCurrentRepoMissing
            opacity: appController.isCurrentRepoMissing ? 0.4 : 1.0

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
                    color: CherryStyle.secondaryTextColor
                }

                RowLayout {
                    Layout.fillWidth: true
                    spacing: Kirigami.Units.smallSpacing

                    Kirigami.Icon {
                        source: "vcs-branch"
                        width: 16
                        height: 16
                        Layout.alignment: Qt.AlignVCenter
                        color: branchDropdown.visible ? CherryStyle.accentColor : Kirigami.Theme.textColor
                    }

                    QQC2.Label {
                        text: appController.currentBranchName
                        font.bold: true
                        color: branchDropdown.visible ? CherryStyle.accentColor : Kirigami.Theme.textColor
                        elide: Text.ElideRight
                        Layout.fillWidth: true
                    }

                    // PR Badge if present
                    Rectangle {
                        visible: appController.currentBranchPr !== ""
                        implicitWidth: prRow.implicitWidth + 12
                        implicitHeight: 20
                        radius: 10
                        color: Qt.rgba(CherryStyle.accentColor.r, CherryStyle.accentColor.g, CherryStyle.accentColor.b, 0.18)

                        RowLayout {
                            id: prRow
                            anchors.centerIn: parent
                            spacing: 3

                            QQC2.Label {
                                id: prHeaderLabel
                                text: appController.currentBranchPr
                                font.pixelSize: CherryStyle.smallFont.pixelSize
                                font.bold: true
                                color: CherryStyle.accentColor
                            }

                            Kirigami.Icon {
                                source: "dialog-ok"
                                width: 11
                                height: 11
                                Layout.alignment: Qt.AlignVCenter
                                color: CherryStyle.additionColor
                                visible: appController.currentBranchPrActive
                            }
                        }
                    }

                    Kirigami.Icon {
                        source: "go-down-symbolic"
                        width: 12
                        height: 12
                        color: branchDropdown.visible ? CherryStyle.accentColor : CherryStyle.secondaryTextColor
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
            enabled: !appController.isCurrentRepoMissing
            opacity: appController.isCurrentRepoMissing ? 0.4 : 1.0

            readonly property bool isSyncActive: appController.isFetching || appController.isPulling || appController.isPushing || appController.isPublishing

            Rectangle {
                anchors.fill: parent
                anchors.margins: 6
                radius: CherryStyle.radiusMedium
                color: remotePrimaryMouse.containsMouse && !remoteSegment.isSyncActive ? CherryStyle.hoverBackground : "transparent"
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

                        // Sync Icon with clean status indicator
                        Item {
                            width: 22
                            height: 22
                            Layout.alignment: Qt.AlignVCenter

                            Kirigami.Icon {
                                id: syncIcon
                                anchors.centerIn: parent
                                source: {
                                    if (!appController.hasRemote) return "cloud-upload";
                                    if (appController.isPushing || appController.aheadCount > 0) return "vcs-push-symbolic";
                                    if (appController.isPulling || appController.behindCount > 0) return "vcs-pull-symbolic";
                                    return "view-refresh";
                                }
                                width: 18
                                height: 18
                                color: (!appController.hasRemote || remoteSegment.isSyncActive) ? CherryStyle.accentColor : Kirigami.Theme.textColor
                                opacity: 1.0

                                SequentialAnimation on opacity {
                                    running: remoteSegment.isSyncActive
                                    loops: Animation.Infinite
                                    NumberAnimation { from: 1.0; to: 0.35; duration: 600; easing.type: Easing.InOutQuad }
                                    NumberAnimation { from: 0.35; to: 1.0; duration: 600; easing.type: Easing.InOutQuad }
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
                                        if (!appController.hasRemote) {
                                            if (appController.isPublishing) return qsTr("Publishing repository...");
                                            return qsTr("Publish repository");
                                        }
                                        if (appController.isPushing) {
                                            return appController.aheadCount > 0 ?
                                                (appController.aheadCount === 1 ? qsTr("Pushing 1 commit...") : qsTr("Pushing %1 commits...").arg(appController.aheadCount)) :
                                                qsTr("Pushing origin...");
                                        }
                                        if (appController.isPulling) {
                                            return appController.behindCount > 0 ?
                                                (appController.behindCount === 1 ? qsTr("Pulling 1 commit...") : qsTr("Pulling %1 commits...").arg(appController.behindCount)) :
                                                qsTr("Pulling origin...");
                                        }
                                        if (appController.isFetching) return qsTr("Fetching origin...");
                                        if (appController.behindCount > 0) return qsTr("Pull origin");
                                        if (appController.aheadCount > 0) return qsTr("Push origin");
                                        return qsTr("Fetch origin");
                                    }
                                    font.bold: true
                                    color: (!appController.hasRemote || remoteSegment.isSyncActive) ? CherryStyle.accentColor : Kirigami.Theme.textColor
                                    elide: Text.ElideRight
                                    Layout.fillWidth: true
                                }

                                // Clean non-overlapping Badge count (e.g. 3 ↓ or 1 ↑)
                                Rectangle {
                                    visible: !remoteSegment.isSyncActive && appController.hasRemote && (appController.behindCount > 0 || appController.aheadCount > 0)
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
                                text: {
                                    if (appController.isPublishing) return qsTr("Creating repository on GitHub...");
                                    if (appController.isPushing) return qsTr("Uploading commits to remote...");
                                    if (appController.isPulling) return qsTr("Downloading changes from remote...");
                                    if (appController.isFetching) return qsTr("Checking for remote updates...");
                                    if (!appController.hasRemote) return qsTr("Publish to GitHub or add remote");
                                    return appController.lastFetchedText;
                                }
                                font.pixelSize: CherryStyle.smallFont.pixelSize - 1
                                color: CherryStyle.secondaryTextColor
                                elide: Text.ElideRight
                                Layout.fillWidth: true
                            }
                        }
                    }

                    MouseArea {
                        id: remotePrimaryMouse
                        anchors.fill: parent
                        enabled: !remoteSegment.isSyncActive
                        hoverEnabled: !remoteSegment.isSyncActive
                        cursorShape: remoteSegment.isSyncActive ? Qt.ArrowCursor : Qt.PointingHandCursor
                        onClicked: {
                            if (!appController.hasRemote) {
                                appController.showPublishDialog();
                            } else if (appController.behindCount > 0) {
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
                    color: remoteMenuMouse.containsMouse && !remoteSegment.isSyncActive ? CherryStyle.hoverBackground : "transparent"
                    opacity: remoteSegment.isSyncActive ? 0.4 : 1.0

                    Kirigami.Icon {
                        anchors.centerIn: parent
                        source: "go-down-symbolic"
                        width: 12
                        height: 12
                        color: CherryStyle.secondaryTextColor
                    }

                    MouseArea {
                        id: remoteMenuMouse
                        anchors.fill: parent
                        enabled: !remoteSegment.isSyncActive
                        hoverEnabled: !remoteSegment.isSyncActive
                        cursorShape: remoteSegment.isSyncActive ? Qt.ArrowCursor : Qt.PointingHandCursor
                        onClicked: remoteMenu.popup(remoteSegment, remoteSegment.width - remoteMenu.width, remoteSegment.height + 2)
                    }
                }
            }

            // GitHub Desktop style Progress Bar for Push/Pull/Fetch/Publish
            Rectangle {
                id: syncProgressBarContainer
                anchors.bottom: parent.bottom
                anchors.left: parent.left
                anchors.right: parent.right
                anchors.margins: 8
                height: 3
                radius: 1.5
                clip: true
                color: Qt.rgba(CherryStyle.accentColor.r, CherryStyle.accentColor.g, CherryStyle.accentColor.b, 0.2)
                visible: remoteSegment.isSyncActive

                Rectangle {
                    id: syncProgressBarThumb
                    height: parent.height
                    width: parent.width * 0.4
                    radius: 1.5
                    color: CherryStyle.accentColor

                    SequentialAnimation on x {
                        running: syncProgressBarContainer.visible
                        loops: Animation.Infinite
                        NumberAnimation {
                            from: -syncProgressBarThumb.width
                            to: syncProgressBarContainer.width
                            duration: 1100
                            easing.type: Easing.InOutCubic
                        }
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

        // ==========================================
        // QUICK ACTION BUTTONS (Editor & Settings)
        // ==========================================
        RowLayout {
            Layout.alignment: Qt.AlignVCenter
            Layout.rightMargin: Kirigami.Units.largeSpacing
            spacing: 2

            QQC2.ToolButton {
                icon.name: "accessories-text-editor"
                icon.width: 16
                icon.height: 16
                QQC2.ToolTip.text: qsTr("Open repository in default text editor")
                QQC2.ToolTip.visible: hovered
                onClicked: appController.openInEditor()
            }

            QQC2.ToolButton {
                icon.name: "utilities-terminal"
                icon.width: 16
                icon.height: 16
                QQC2.ToolTip.text: qsTr("Open repository in terminal")
                QQC2.ToolTip.visible: hovered
                onClicked: appController.openInTerminal()
            }

            QQC2.ToolButton {
                icon.name: "settings-configure"
                icon.width: 16
                icon.height: 16
                QQC2.ToolTip.text: qsTr("Settings")
                QQC2.ToolTip.visible: hovered
                onClicked: appController.showSettingsDialog()
            }
        }
    }
}

