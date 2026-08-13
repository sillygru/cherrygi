import QtQuick
import QtQuick.Controls as QQC2
import QtQuick.Layouts
import org.kde.kirigami as Kirigami
import org.kde.cherrygi

QQC2.Menu {
    id: menu

    QQC2.MenuItem {
        text: qsTr("Fetch origin")
        icon.name: "view-refresh"
        onTriggered: appController.fetchOrigin()
    }

    QQC2.MenuItem {
        text: appController.behindCount > 0 ? qsTr("Pull origin (%1 commits behind)").arg(appController.behindCount) : qsTr("Pull origin")
        icon.name: "vcs-pull-symbolic"
        enabled: appController.behindCount > 0
        onTriggered: appController.pullOrigin()
    }

    QQC2.MenuItem {
        text: appController.aheadCount > 0 ? qsTr("Push origin (%1 commits ahead)").arg(appController.aheadCount) : qsTr("Push origin")
        icon.name: "vcs-push-symbolic"
        enabled: appController.aheadCount > 0
        onTriggered: appController.pushOrigin()
    }

    QQC2.MenuSeparator {}

    QQC2.MenuItem {
        text: qsTr("Create Pull Request")
        icon.name: "vcs-merge-request"
        onTriggered: appController.createPullRequest()
    }

    QQC2.MenuItem {
        text: qsTr("View on GitHub")
        icon.name: "globe"
        onTriggered: appController.openOnGitHub()
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
}
