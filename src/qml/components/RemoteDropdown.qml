import QtQuick
import QtQuick.Controls as QQC2
import QtQuick.Layouts
import org.kde.kirigami as Kirigami
import "../style"

QQC2.Menu {
    id: menu

    QQC2.MenuItem {
        text: i18n("Fetch origin")
        icon.name: "view-refresh"
        onTriggered: appController.fetchOrigin()
    }

    QQC2.MenuItem {
        text: appController.behindCount > 0 ? i18n("Pull origin (%1 commits behind)", appController.behindCount) : i18n("Pull origin")
        icon.name: "vcs-pull-symbolic"
        enabled: appController.behindCount > 0
        onTriggered: appController.pullOrigin()
    }

    QQC2.MenuItem {
        text: appController.aheadCount > 0 ? i18n("Push origin (%1 commits ahead)", appController.aheadCount) : i18n("Push origin")
        icon.name: "vcs-push-symbolic"
        enabled: appController.aheadCount > 0
        onTriggered: appController.pushOrigin()
    }

    QQC2.MenuSeparator {}

    QQC2.MenuItem {
        text: i18n("Create Pull Request")
        icon.name: "vcs-merge-request"
        onTriggered: appController.showToast(i18n("Opening pull request in browser..."))
    }

    QQC2.MenuItem {
        text: i18n("View on GitHub")
        icon.name: "globe"
        onTriggered: appController.showToast(i18n("Opening repository on GitHub..."))
    }

    QQC2.MenuItem {
        text: i18n("Open in Terminal")
        icon.name: "utilities-terminal"
        onTriggered: appController.showToast(i18n("Opened terminal in repository directory"))
    }
}
