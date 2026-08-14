import QtQuick
import QtQuick.Controls as QQC2
import QtQuick.Layouts
import org.kde.kirigami as Kirigami
import org.kde.cherrygi
import "components"
import "style"

Kirigami.ApplicationWindow {
    id: window
    readonly property var appController: AppController
    title: (appController && appController.currentRepoName !== "") ?
           qsTr("%1 - cherrygi").arg(appController.currentRepoName) :
           qsTr("cherrygi - KDE Plasma Git Client")

    width: 1200
    height: 780
    minimumWidth: 900
    minimumHeight: 600

    // Refresh shortcuts (F5, Ctrl+R, StandardKey.Refresh)
    QQC2.Action {
        shortcut: "F5"
        onTriggered: appController.refresh()
    }

    QQC2.Action {
        shortcut: "Ctrl+R"
        onTriggered: appController.refresh()
    }

    QQC2.Action {
        shortcut: StandardKey.Refresh
        onTriggered: appController.refresh()
    }

    // Main Column Layout
    ColumnLayout {
        anchors.fill: parent
        spacing: 0

        // ==========================================
        // TOP 3-SEGMENT HEADER BAR (GitHub Desktop Style)
        // ==========================================
        HeaderBar {
            id: headerBar
            Layout.fillWidth: true
            z: 10
        }

        // ==========================================
        // MAIN WORKSPACE (Normal SplitView vs Missing Repository View)
        // ==========================================
        StackLayout {
            Layout.fillWidth: true
            Layout.fillHeight: true
            currentIndex: appController.isCurrentRepoMissing ? 1 : 0

            // Normal Workspace
            QQC2.SplitView {
                id: mainSplitView
                Layout.fillWidth: true
                Layout.fillHeight: true
                orientation: Qt.Horizontal

                // Left Sidebar
                SidebarPanel {
                    id: sidebar
                    QQC2.SplitView.preferredWidth: 380
                    QQC2.SplitView.minimumWidth: 300
                    QQC2.SplitView.maximumWidth: 550
                    QQC2.SplitView.fillHeight: true
                }

                // Right Main Content (Diff / Commit Inspector)
                MainContentArea {
                    id: contentArea
                    QQC2.SplitView.fillWidth: true
                    QQC2.SplitView.fillHeight: true
                }
            }

            // Missing Repository Full-Screen View
            MissingRepositoryView {
                id: missingRepoView
                Layout.fillWidth: true
                Layout.fillHeight: true
            }
        }
    }

    // ==========================================
    // FLOATING TOAST NOTIFICATION
    // ==========================================
    UndoToast {
        id: toast
        anchors.horizontalCenter: parent.horizontalCenter
        anchors.bottom: parent.bottom
        anchors.bottomMargin: Kirigami.Units.largeSpacing
        z: 99
    }

    // ==========================================
    // STARTUP BACKEND SELECTION MODAL
    // ==========================================
    BackendSelectionDialog {
        id: backendDialog
        visible: appController.isBackendDialogVisible
        onClosed: {
            appController.isBackendDialogVisible = false;
        }
    }

    // ==========================================
    // APP SETTINGS MODAL DIALOG
    // ==========================================
    AppSettingsDialog {
        id: settingsDialog
        visible: appController.isSettingsDialogVisible
        onClosed: {
            appController.isSettingsDialogVisible = false;
        }
    }

    // ==========================================
    // PUBLISH REPOSITORY MODAL DIALOG
    // ==========================================
    PublishRepositoryDialog {
        id: publishDialog
        visible: appController.isPublishDialogVisible
        onClosed: {
            appController.isPublishDialogVisible = false;
        }
    }

    // ==========================================
    // CLONE REPOSITORY MODAL DIALOG
    // ==========================================
    CloneRepositoryDialog {
        id: cloneDialog
        visible: appController.isCloneDialogVisible
        onClosed: {
            appController.isCloneDialogVisible = false;
        }
    }

    // ==========================================
    // GLOBAL PROGRESS STRIP (GitHub Desktop Style)
    // ==========================================
    Rectangle {
        id: globalProgressStrip
        anchors.top: parent.top
        anchors.left: parent.left
        anchors.right: parent.right
        height: 2
        z: 999
        color: "transparent"
        visible: appController.isOperating

        Rectangle {
            id: globalProgressThumb
            height: parent.height
            width: parent.width * 0.35
            color: CherryStyle.accentColor

            SequentialAnimation on x {
                running: globalProgressStrip.visible
                loops: Animation.Infinite
                NumberAnimation {
                    from: -globalProgressThumb.width
                    to: globalProgressStrip.width
                    duration: 1100
                    easing.type: Easing.InOutCubic
                }
            }
        }
    }

    // ==========================================
    // REPOSITORY LOADING OVERLAY (Breeze / Kirigami)
    // ==========================================
    Rectangle {
        id: repoLoadingOverlay
        anchors.fill: parent
        z: 1000
        visible: opacity > 0
        opacity: appController.isLoadingRepository ? 1.0 : 0.0
        color: Qt.rgba(0, 0, 0, 0.5)

        Behavior on opacity {
            NumberAnimation { duration: 180; easing.type: Easing.InOutQuad }
        }

        // Intercept clicks while loading
        MouseArea {
            anchors.fill: parent
            hoverEnabled: true
            preventStealing: true
            onClicked: {}
        }

        Rectangle {
            anchors.centerIn: parent
            width: 360
            height: 205
            radius: CherryStyle.radiusLarge
            color: CherryStyle.surfacePopup
            border.color: CherryStyle.popupBorderColor
            border.width: 1

            // Floating elevation shadow
            Rectangle {
                anchors.fill: parent
                anchors.margins: -2
                z: -1
                radius: CherryStyle.radiusLarge + 2
                color: "transparent"
                border.color: Qt.rgba(0, 0, 0, 0.3)
            }

            ColumnLayout {
                anchors.fill: parent
                anchors.margins: Kirigami.Units.largeSpacing
                spacing: Kirigami.Units.mediumSpacing

                QQC2.BusyIndicator {
                    Layout.alignment: Qt.AlignHCenter
                    running: appController.isLoadingRepository
                    implicitWidth: 42
                    implicitHeight: 42
                }

                ColumnLayout {
                    Layout.alignment: Qt.AlignHCenter
                    Layout.fillWidth: true
                    spacing: 4

                    QQC2.Label {
                        Layout.alignment: Qt.AlignHCenter
                        text: (appController.loadingRepositoryName && appController.loadingRepositoryName !== "") ?
                              appController.loadingRepositoryName : qsTr("Opening Repository")
                        font.bold: true
                        font.pixelSize: CherryStyle.largeFont.pixelSize
                        color: Kirigami.Theme.textColor
                        elide: Text.ElideMiddle
                        Layout.maximumWidth: 300
                    }

                    QQC2.Label {
                        Layout.alignment: Qt.AlignHCenter
                        text: (appController.loadingRepositoryMessage && appController.loadingRepositoryMessage !== "") ?
                              appController.loadingRepositoryMessage : qsTr("Scanning repository files...")
                        font.pixelSize: CherryStyle.smallFont.pixelSize
                        color: CherryStyle.secondaryTextColor
                    }

                    // Indeterminate progress bar: repository size is unknown, but
                    // progress remains visibly active for very large worktrees.
                    Rectangle {
                        id: progressTrack
                        Layout.fillWidth: true
                        Layout.topMargin: Kirigami.Units.smallSpacing
                        height: 4
                        radius: 2
                        color: Qt.rgba(CherryStyle.accentColor.r,
                                       CherryStyle.accentColor.g,
                                       CherryStyle.accentColor.b, 0.18)
                        clip: true

                        Rectangle {
                            id: progressThumb
                            width: progressTrack.width * 0.32
                            height: parent.height
                            radius: parent.radius
                            color: CherryStyle.accentColor

                            SequentialAnimation on x {
                                running: appController.isLoadingRepository
                                loops: Animation.Infinite
                                NumberAnimation {
                                    from: -progressThumb.width
                                    to: progressTrack.width
                                    duration: 900
                                    easing.type: Easing.InOutQuad
                                }
                            }
                        }
                    }
                }
            }
        }
    }
}
