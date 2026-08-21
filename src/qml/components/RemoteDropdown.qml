import QtQuick
import QtQuick.Controls as QQC2
import QtQuick.Layouts
import org.kde.kirigami as Kirigami
import org.kde.cherrygi

QQC2.Menu {
    id: menu

    QQC2.MenuItem {
        text: qsTr("Publish Repository...")
        icon.name: "cloud-upload"
        visible: !appController.hasRemote
        onTriggered: appController.showPublishDialog()
    }

    QQC2.MenuItem {
        text: qsTr("Fetch origin")
        icon.name: "view-refresh"
        enabled: appController.hasRemote
        onTriggered: appController.fetchOrigin()
    }

    QQC2.MenuItem {
        text: appController.behindCount > 0 ? qsTr("Pull origin (%1 commits behind)").arg(appController.behindCount) : qsTr("Pull origin")
        icon.name: "vcs-pull-symbolic"
        enabled: appController.hasRemote
        onTriggered: appController.pullOrigin()
    }

    QQC2.MenuItem {
        text: appController.aheadCount > 0 ? qsTr("Push origin (%1 commits ahead)").arg(appController.aheadCount) : qsTr("Push origin")
        icon.name: "vcs-push-symbolic"
        enabled: appController.hasRemote && appController.aheadCount > 0
        onTriggered: appController.pushOrigin()
    }

    QQC2.MenuItem {
        text: qsTr("Force Push origin")
        icon.name: "vcs-push-symbolic"
        enabled: appController.hasRemote
        onTriggered: appController.forcePushOrigin()
    }

    QQC2.MenuSeparator {}

    QQC2.MenuItem {
        text: qsTr("Create Pull Request")
        icon.name: "vcs-merge-request"
        enabled: appController.hasRemote && appController.isGitHubRemote
        onTriggered: appController.createPullRequest()
    }

    QQC2.MenuItem {
        text: qsTr("View on GitHub")
        icon.name: "globe"
        visible: appController.isGitHubRemote
        enabled: appController.hasRemote && appController.isGitHubRemote
        onTriggered: appController.openOnGitHub()
    }

    QQC2.MenuSeparator {}

    QQC2.MenuItem {
        text: qsTr("Open in External Editor")
        icon.name: "accessories-text-editor"
        onTriggered: appController.openInEditor()
    }

    QQC2.MenuItem {
        text: qsTr("Open in Terminal")
        icon.name: "utilities-terminal"
        onTriggered: appController.openInTerminal()
    }

    QQC2.MenuItem {
        text: qsTr("Open in File Manager")
        icon.name: "folder"
        onTriggered: appController.openInFileManager()
    }

    QQC2.MenuSeparator {}

    QQC2.MenuItem {
        text: qsTr("Repository Settings...")
        icon.name: "folder-git"
        onTriggered: appController.showSettingsDialog("repository")
    }

    QQC2.MenuItem {
        text: qsTr("Application Settings...")
        icon.name: "settings-configure"
        onTriggered: appController.showSettingsDialog("editor")
    }
}
