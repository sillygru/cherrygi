import QtQuick
import QtQuick.Controls as QQC2
import QtQuick.Layouts
import org.kde.kirigami as Kirigami
import org.kde.cherrygi
import "../style"

ColumnLayout {
    id: root
    spacing: 0

    // Commit Context Menu
    QQC2.Menu {
        id: commitContextMenu
        property string targetSha: ""
        property string targetSummary: ""
        property string targetDescription: ""
        property bool targetIsLocal: false
        property bool targetIsHead: false

        QQC2.MenuItem {
            text: qsTr("Undo Commit")
            icon.name: "edit-undo"
            visible: commitContextMenu.targetIsHead && commitContextMenu.targetIsLocal && appController.canUndoCommit
            onTriggered: appController.undoLastCommit()
        }

        QQC2.MenuItem {
            text: qsTr("Revert Commit (Create new commit undoing changes)")
            icon.name: "vcs-diff"
            onTriggered: appController.revertCommit(commitContextMenu.targetSha)
        }

        QQC2.MenuItem {
            text: qsTr("Checkout Commit (Reset Hard)")
            icon.name: "vcs-branch"
            onTriggered: appController.checkoutCommit(commitContextMenu.targetSha)
        }

        QQC2.MenuSeparator {}

        QQC2.MenuItem {
            text: qsTr("Copy SHA (%1)").arg(commitContextMenu.targetSha.substring(0, 7))
            icon.name: "edit-copy"
            onTriggered: appController.copyToClipboard(commitContextMenu.targetSha, qsTr("Commit SHA copied"))
        }

        QQC2.MenuItem {
            text: qsTr("Copy Commit Message")
            icon.name: "edit-copy"
            onTriggered: {
                var msg = commitContextMenu.targetSummary;
                if (commitContextMenu.targetDescription && commitContextMenu.targetDescription.length > 0) {
                    msg += "\n\n" + commitContextMenu.targetDescription;
                }
                appController.copyToClipboard(msg, qsTr("Commit message copied"));
            }
        }

        QQC2.MenuSeparator {}

        QQC2.MenuItem {
            text: qsTr("Open Repository in Editor")
            icon.name: "accessories-text-editor"
            onTriggered: appController.openInEditor()
        }

        QQC2.MenuItem {
            text: qsTr("View on GitHub")
            icon.name: "globe"
            visible: appController.isGitHubRemote
            onTriggered: appController.openOnGitHub()
        }
    }

    // Search bar
    Rectangle {
        Layout.fillWidth: true
        height: 48
        color: CherryStyle.surfaceHeader

        RowLayout {
            anchors.fill: parent
            anchors.margins: Kirigami.Units.smallSpacing
            spacing: Kirigami.Units.smallSpacing

            QQC2.TextField {
                id: searchInput
                Layout.fillWidth: true
                placeholderText: qsTr("Search commits by message, author, or SHA...")
                leftPadding: Kirigami.Units.largeSpacing + 12
                rightPadding: text.length > 0 ? Kirigami.Units.largeSpacing + 10 : Kirigami.Units.smallSpacing

                background: Rectangle {
                    color: CherryStyle.inputBackground
                    border.color: searchInput.activeFocus ? CherryStyle.accentColor : CherryStyle.borderColor
                    border.width: searchInput.activeFocus ? 2 : 1
                    radius: CherryStyle.radiusMedium
                }

                Kirigami.Icon {
                    source: "search"
                    width: 14
                    height: 14
                    anchors.left: parent.left
                    anchors.leftMargin: Kirigami.Units.smallSpacing + 2
                    anchors.verticalCenter: parent.verticalCenter
                    color: searchInput.activeFocus ? CherryStyle.accentColor : CherryStyle.secondaryTextColor
                }

                QQC2.ToolButton {
                    visible: searchInput.text.length > 0
                    icon.name: "edit-clear"
                    icon.width: 12
                    icon.height: 12
                    anchors.right: parent.right
                    anchors.verticalCenter: parent.verticalCenter
                    onClicked: {
                        searchInput.text = "";
                        appController.commitHistory.filterText = "";
                    }
                }

                onTextChanged: {
                    appController.commitHistory.filterText = text;
                }
            }
        }
    }

    // Commit list
    QQC2.ScrollView {
        Layout.fillWidth: true
        Layout.fillHeight: true
        clip: true

        ListView {
            id: commitListView
            model: appController.commitHistory
            spacing: 1
            reuseItems: true
            boundsBehavior: Flickable.StopAtBounds
            highlightFollowsCurrentItem: false
            currentIndex: -1

            delegate: QQC2.ItemDelegate {
                id: commitDelegate
                width: commitListView.width
                height: 72

                required property int index
                required property string sha
                required property string shortSha
                required property string summary
                required property string description
                required property string authorName
                required property string authorEmail
                required property string authorAvatarUrl
                required property string relativeTime
                required property string timestamp
                required property var coAuthors
                required property string coAuthorsText
                required property int changedFilesCount
                required property bool isLocal
                required property color authorColor
                required property string authorInitial
                required property var tags

                highlighted: appController.selectedCommitSha === commitDelegate.sha

                background: Rectangle {
                    color: commitDelegate.highlighted ? CherryStyle.activeBackground : (commitDelegate.hovered ? CherryStyle.hoverBackground : "transparent")
                    radius: CherryStyle.radiusMedium

                    // Left accent indicator on selected commit
                    Rectangle {
                        visible: commitDelegate.highlighted
                        anchors.left: parent.left
                        anchors.top: parent.top
                        anchors.bottom: parent.bottom
                        anchors.margins: 2
                        width: 2
                        radius: 1.5
                        color: CherryStyle.accentColor
                    }
                }

                contentItem: RowLayout {
                    anchors.fill: parent
                    anchors.leftMargin: Kirigami.Units.smallSpacing + 2
                    anchors.rightMargin: Kirigami.Units.smallSpacing + 2
                    spacing: Kirigami.Units.smallSpacing

                    // Avatar Circle
                    CherryAvatar {
                        Layout.alignment: Qt.AlignVCenter
                        implicitWidth: 36
                        implicitHeight: 36
                        name: commitDelegate.authorName
                        source: commitDelegate.authorAvatarUrl
                        color: commitDelegate.authorColor
                    }

                    // Metadata
                    ColumnLayout {
                        Layout.fillWidth: true
                        spacing: 3

                        // Commit Title / Summary (Full width, not crowded)
                        QQC2.Label {
                            text: commitDelegate.summary
                            font.bold: true
                            color: commitDelegate.highlighted ? CherryStyle.accentColor : Kirigami.Theme.textColor
                            elide: Text.ElideRight
                            Layout.fillWidth: true
                        }

                        // Subtitle Metadata Line (Author, Date, Tags, Unpushed, Short SHA)
                        RowLayout {
                            Layout.fillWidth: true
                            spacing: 4

                            QQC2.Label {
                                text: commitDelegate.authorName
                                font.pixelSize: CherryStyle.smallFont.pixelSize
                                color: CherryStyle.secondaryTextColor
                                elide: Text.ElideRight
                            }

                            QQC2.Label {
                                text: "•"
                                font.pixelSize: CherryStyle.smallFont.pixelSize
                                color: CherryStyle.secondaryTextColor
                            }

                            QQC2.Label {
                                text: commitDelegate.relativeTime
                                font.pixelSize: CherryStyle.smallFont.pixelSize
                                color: CherryStyle.secondaryTextColor
                            }

                            // Tag Badges
                            Repeater {
                                model: commitDelegate.tags
                                Rectangle {
                                    implicitWidth: tagRow.implicitWidth + 8
                                    implicitHeight: 18
                                    radius: CherryStyle.radiusSmall
                                    color: commitDelegate.highlighted ? Qt.rgba(CherryStyle.accentColor.r, CherryStyle.accentColor.g, CherryStyle.accentColor.b, 0.25) : CherryStyle.surfaceCardElevated
                                    border.color: commitDelegate.highlighted ? CherryStyle.accentColor : CherryStyle.borderColor
                                    border.width: 1

                                    RowLayout {
                                        id: tagRow
                                        anchors.centerIn: parent
                                        spacing: 3

                                        Kirigami.Icon {
                                            source: "tag"
                                            implicitWidth: 10
                                            implicitHeight: 10
                                            color: commitDelegate.highlighted ? CherryStyle.accentColor : CherryStyle.secondaryTextColor
                                        }

                                        QQC2.Label {
                                            text: modelData
                                            font.pixelSize: CherryStyle.smallFont.pixelSize - 1
                                            font.bold: true
                                            color: commitDelegate.highlighted ? CherryStyle.accentColor : CherryStyle.secondaryTextColor
                                            elide: Text.ElideRight
                                            Layout.maximumWidth: 100
                                        }
                                    }
                                }
                            }

                            // Unpushed commit indicator
                            Rectangle {
                                visible: commitDelegate.isLocal
                                implicitWidth: localRow.implicitWidth + 8
                                implicitHeight: 16
                                radius: 8
                                color: Qt.rgba(CherryStyle.additionColor.r, CherryStyle.additionColor.g, CherryStyle.additionColor.b, 0.15)

                                RowLayout {
                                    id: localRow
                                    anchors.centerIn: parent
                                    spacing: 2

                                    Kirigami.Icon {
                                        source: "vcs-push-symbolic"
                                        width: 10
                                        height: 10
                                        color: CherryStyle.additionColor
                                    }
                                }
                            }

                            Item { Layout.fillWidth: true }

                            // Short SHA Badge (right-aligned in metadata row)
                            Rectangle {
                                implicitWidth: shaLabel.implicitWidth + 8
                                implicitHeight: 18
                                radius: CherryStyle.radiusSmall
                                color: CherryStyle.surfaceCardElevated

                                QQC2.Label {
                                    id: shaLabel
                                    anchors.centerIn: parent
                                    text: commitDelegate.shortSha
                                    font.family: CherryStyle.codeFont.family
                                    font.pixelSize: CherryStyle.smallFont.pixelSize - 1
                                    font.bold: true
                                    color: commitDelegate.highlighted ? CherryStyle.accentColor : CherryStyle.secondaryTextColor
                                }
                            }
                        }
                    }
                }

                onClicked: {
                    appController.selectCommit(commitDelegate.sha);
                }

                TapHandler {
                    acceptedButtons: Qt.RightButton
                    onTapped: {
                        commitContextMenu.targetSha = commitDelegate.sha;
                        commitContextMenu.targetSummary = commitDelegate.summary;
                        commitContextMenu.targetDescription = commitDelegate.description;
                        commitContextMenu.targetIsLocal = commitDelegate.isLocal;
                        commitContextMenu.targetIsHead = (index === 0);
                        commitContextMenu.popup();
                    }
                }
            }
        }
    }
}
