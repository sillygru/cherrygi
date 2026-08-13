import QtQuick
import QtQuick.Controls as QQC2
import QtQuick.Layouts
import org.kde.kirigami as Kirigami
import org.kde.cherrygi

ColumnLayout {
    id: root
    spacing: 0

    // Search bar
    Rectangle {
        Layout.fillWidth: true
        height: 44
        color: CherryStyle.cardBackground
        border.color: CherryStyle.subtleBorderColor
        border.width: 1

        RowLayout {
            anchors.fill: parent
            anchors.margins: Kirigami.Units.smallSpacing
            spacing: Kirigami.Units.smallSpacing

            QQC2.TextField {
                id: searchInput
                Layout.fillWidth: true
                placeholderText: qsTr("Search commits by message, author, or SHA...")
                leftPadding: Kirigami.Units.largeSpacing + 10

                Kirigami.Icon {
                    source: "search"
                    width: Kirigami.Units.iconSizes.small
                    height: width
                    anchors.left: parent.left
                    anchors.leftMargin: Kirigami.Units.smallSpacing
                    anchors.verticalCenter: parent.verticalCenter
                    color: Kirigami.Theme.disabledTextColor
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
                height: 64

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

                highlighted: appController.selectedCommitSha === commitDelegate.sha

                background: Rectangle {
                    color: commitDelegate.highlighted ? CherryStyle.activeBackground : (commitDelegate.hovered ? CherryStyle.hoverBackground : "transparent")
                    radius: CherryStyle.radiusSmall
                    border.color: commitDelegate.highlighted ? Kirigami.Theme.highlightColor : "transparent"
                    border.width: commitDelegate.highlighted ? 1 : 0
                }

                contentItem: RowLayout {
                    spacing: Kirigami.Units.smallSpacing

                    // Avatar Circle
                    Rectangle {
                        width: 32
                        height: 32
                        radius: 16
                        color: Kirigami.Theme.highlightColor

                        QQC2.Label {
                            anchors.centerIn: parent
                            text: commitDelegate.authorName.length > 0 ? commitDelegate.authorName.charAt(0).toUpperCase() : "G"
                            font.bold: true
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
                                width: shaLabel.width + 8
                                height: 18
                                radius: 4
                                color: CherryStyle.cardBackground
                                border.color: CherryStyle.borderColor
                                border.width: 1

                                QQC2.Label {
                                    id: shaLabel
                                    anchors.centerIn: parent
                                    text: commitDelegate.shortSha
                                    font.family: CherryStyle.codeFont.family
                                    font.pixelSize: CherryStyle.smallFont.pixelSize - 1
                                    color: Kirigami.Theme.disabledTextColor
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
