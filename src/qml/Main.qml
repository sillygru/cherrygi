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
        // MAIN WORKSPACE (SplitView: Left Sidebar + Right Content)
        // ==========================================
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
}
