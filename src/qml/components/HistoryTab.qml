import QtQuick
import QtQuick.Controls as QQC2
import QtQuick.Layouts
import org.kde.kirigami as Kirigami
import org.kde.cherrygi
import "../style"

ColumnLayout {
    id: root
    spacing: 0

    // Generate a stable hue-based avatar color from the author name
    function avatarColor(name) {
        if (!name || name.length === 0) return Kirigami.Theme.disabledTextColor;
        var hash = 0;
        for (var i = 0; i < name.length; i++) {
            hash = name.charCodeAt(i) + ((hash << 5) - hash);
        }
        var hue = Math.abs(hash % 360);
        return Qt.hsla(hue / 360.0, 0.55, 0.45, 1.0);
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
                    border.color: searchInput.activeFocus ? Kirigami.Theme.highlightColor : CherryStyle.borderColor
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
                    color: searchInput.activeFocus ? Kirigami.Theme.highlightColor : Kirigami.Theme.disabledTextColor
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
                        color: Kirigami.Theme.highlightColor
                    }
                }

                contentItem: RowLayout {
                    anchors.fill: parent
                    anchors.leftMargin: Kirigami.Units.smallSpacing + 2
                    anchors.rightMargin: Kirigami.Units.smallSpacing + 2
                    spacing: Kirigami.Units.smallSpacing

                    // Avatar Circle
                    Rectangle {
                        width: 36
                        height: 36
                        radius: 18
                        color: avatarColor(commitDelegate.authorName)
                        Layout.alignment: Qt.AlignVCenter

                        QQC2.Label {
                            anchors.centerIn: parent
                            text: commitDelegate.authorName.length > 0 ? commitDelegate.authorName.charAt(0).toUpperCase() : "G"
                            font.bold: true
                            font.pixelSize: 14
                            color: Kirigami.Theme.highlightedTextColor
                        }
                    }

                    // Metadata
                    ColumnLayout {
                        Layout.fillWidth: true
                        spacing: 2

                        RowLayout {
                            Layout.fillWidth: true
                            spacing: Kirigami.Units.smallSpacing

                            QQC2.Label {
                                text: commitDelegate.summary
                                font.bold: true
                                color: commitDelegate.highlighted ? Kirigami.Theme.highlightColor : Kirigami.Theme.textColor
                                elide: Text.ElideRight
                                Layout.fillWidth: true
                            }

                            // Short SHA Badge
                            Rectangle {
                                implicitWidth: shaLabel.implicitWidth + 10
                                implicitHeight: 20
                                radius: CherryStyle.radiusSmall
                                color: CherryStyle.surfaceCardElevated

                                QQC2.Label {
                                    id: shaLabel
                                    anchors.centerIn: parent
                                    text: commitDelegate.shortSha
                                    font.family: CherryStyle.codeFont.family
                                    font.pixelSize: CherryStyle.smallFont.pixelSize - 1
                                    font.bold: true
                                    color: commitDelegate.highlighted ? Kirigami.Theme.highlightColor : Kirigami.Theme.disabledTextColor
                                }
                            }
                        }

                        RowLayout {
                            Layout.fillWidth: true
                            spacing: 4

                            QQC2.Label {
                                text: commitDelegate.authorName
                                font.pixelSize: CherryStyle.smallFont.pixelSize
                                color: Kirigami.Theme.disabledTextColor
                                elide: Text.ElideRight
                            }

                            QQC2.Label {
                                text: "•"
                                font.pixelSize: CherryStyle.smallFont.pixelSize
                                color: Kirigami.Theme.disabledTextColor
                            }

                            QQC2.Label {
                                text: commitDelegate.relativeTime
                                font.pixelSize: CherryStyle.smallFont.pixelSize
                                color: Kirigami.Theme.disabledTextColor
                            }

                            // Unpushed commit indicator
                            Rectangle {
                                visible: commitDelegate.isLocal
                                implicitWidth: localRow.implicitWidth + 8
                                implicitHeight: 16
                                radius: 8
                                color: Qt.rgba(Kirigami.Theme.positiveTextColor.r, Kirigami.Theme.positiveTextColor.g, Kirigami.Theme.positiveTextColor.b, 0.15)

                                RowLayout {
                                    id: localRow
                                    anchors.centerIn: parent
                                    spacing: 2

                                    Kirigami.Icon {
                                        source: "vcs-push-symbolic"
                                        width: 10
                                        height: 10
                                        color: Kirigami.Theme.positiveTextColor
                                    }
                                }
                            }

                            Item { Layout.fillWidth: true }
                        }
                    }
                }

                onClicked: {
                    appController.selectCommit(commitDelegate.sha);
                }
            }
        }
    }
}

