import QtQuick
import QtQuick.Controls as QQC2
import QtQuick.Layouts
import org.kde.kirigami as Kirigami
import "../style"

Rectangle {
    id: root
    implicitHeight: layout.implicitHeight + Kirigami.Units.smallSpacing * 2
    color: CherryStyle.cardBackground
    border.color: CherryStyle.borderColor
    border.width: 1

    property var coAuthorsList: ["@sergiou87", "@tidy-dev"]

    // Popup for adding co-author
    QQC2.Popup {
        id: addCoAuthorPopup
        width: 260
        height: 120
        padding: Kirigami.Units.smallSpacing
        modal: true
        focus: true

        background: Rectangle {
            color: Kirigami.Theme.backgroundColor
            border.color: CherryStyle.borderColor
            radius: CherryStyle.radiusSmall
        }

        ColumnLayout {
            anchors.fill: parent
            spacing: Kirigami.Units.smallSpacing

            QQC2.Label {
                text: i18n("Add Co-Author")
                font.bold: true
            }

            QQC2.TextField {
                id: coAuthorInput
                Layout.fillWidth: true
                placeholderText: "@username or Name <email>"
                onAccepted: addBtn.clicked()
            }

            RowLayout {
                Layout.alignment: Qt.AlignRight
                spacing: Kirigami.Units.smallSpacing

                QQC2.Button {
                    text: i18n("Cancel")
                    onClicked: addCoAuthorPopup.close()
                }

                QQC2.Button {
                    id: addBtn
                    text: i18n("Add")
                    highlighted: true
                    enabled: coAuthorInput.text.trimmed().length > 0
                    onClicked: {
                        var list = root.coAuthorsList.slice();
                        list.push(coAuthorInput.text.trimmed());
                        root.coAuthorsList = list;
                        coAuthorInput.text = "";
                        addCoAuthorPopup.close();
                    }
                }
            }
        }
    }

    ColumnLayout {
        id: layout
        anchors.fill: parent
        anchors.margins: Kirigami.Units.smallSpacing
        spacing: Kirigami.Units.smallSpacing

        // Undo Banner if available
        Rectangle {
            id: undoBanner
            Layout.fillWidth: true
            height: 36
            visible: appController.hasUndoCommit
            radius: CherryStyle.radiusSmall
            color: Qt.rgba(0.2, 0.6, 1.0, 0.15)
            border.color: Qt.rgba(0.2, 0.6, 1.0, 0.4)
            border.width: 1

            RowLayout {
                anchors.fill: parent
                anchors.leftMargin: Kirigami.Units.smallSpacing
                anchors.rightMargin: Kirigami.Units.smallSpacing
                spacing: Kirigami.Units.smallSpacing

                Kirigami.Icon {
                    source: "edit-undo"
                    width: Kirigami.Units.iconSizes.small
                    height: width
                    color: "#3584e4"
                }

                QQC2.Label {
                    text: i18n("Commit created!")
                    font.bold: true
                    font.pixelSize: CherryStyle.smallFont.pixelSize
                    color: Kirigami.Theme.textColor
                    Layout.fillWidth: true
                    elide: Text.ElideRight
                }

                QQC2.Button {
                    text: i18n("Undo")
                    font.bold: true
                    onClicked: {
                        var restoredSummary = appController.lastUndoCommitSummary;
                        var restoredDesc = appController.lastUndoCommitDescription;
                        if (appController.undoLastCommit()) {
                            summaryField.text = restoredSummary;
                            descArea.text = restoredDesc;
                        }
                    }
                }
            }
        }

        // Summary row: Avatar + Summary Input
        RowLayout {
            Layout.fillWidth: true
            spacing: Kirigami.Units.smallSpacing

            // Avatar circle
            Rectangle {
                width: 28
                height: 28
                radius: 14
                color: Kirigami.Theme.highlightColor
                Layout.alignment: Qt.AlignTop

                QQC2.Label {
                    anchors.centerIn: parent
                    text: "U"
                    font.bold: true
                    font.pixelSize: 12
                    color: Kirigami.Theme.highlightedTextColor
                }
            }

            QQC2.TextField {
                id: summaryField
                Layout.fillWidth: true
                placeholderText: i18n("Summary (required)")
                text: "Bring `onRowKeyboardFocus` to `SectionList`"
            }
        }

        // Description Text Area
        QQC2.ScrollView {
            Layout.fillWidth: true
            Layout.preferredHeight: 65
            Layout.minimumHeight: 50
            clip: true

            QQC2.TextArea {
                id: descArea
                placeholderText: i18n("Description")
                wrapMode: TextEdit.Wrap
            }
        }

        // Co-Authors Row
        RowLayout {
            Layout.fillWidth: true
            spacing: 4

            // Co-author icon trigger
            QQC2.ToolButton {
                icon.name: "user-identity"
                icon.width: 14
                icon.height: 14
                QQC2.ToolTip.text: i18n("Add Co-Author")
                QQC2.ToolTip.visible: hovered
                onClicked: {
                    addCoAuthorPopup.x = parent.x;
                    addCoAuthorPopup.y = parent.y - addCoAuthorPopup.height - 4;
                    addCoAuthorPopup.open();
                }
            }

            QQC2.Label {
                text: i18n("Co-Authors")
                font.pixelSize: CherryStyle.smallFont.pixelSize - 1
                color: Kirigami.Theme.disabledTextColor
                visible: root.coAuthorsList.length > 0
            }

            // Co-author chips flow
            Repeater {
                model: root.coAuthorsList

                delegate: Rectangle {
                    height: 22
                    width: chipRow.width + 12
                    radius: 11
                    color: Qt.rgba(Kirigami.Theme.textColor.r, Kirigami.Theme.textColor.g, Kirigami.Theme.textColor.b, 0.08)
                    border.color: CherryStyle.borderColor
                    border.width: 1

                    RowLayout {
                        id: chipRow
                        anchors.centerIn: parent
                        spacing: 3

                        QQC2.Label {
                            text: modelData
                            font.pixelSize: CherryStyle.smallFont.pixelSize - 1
                            color: Kirigami.Theme.textColor
                        }

                        Kirigami.Icon {
                            source: "window-close"
                            width: 10
                            height: 10
                            color: Kirigami.Theme.disabledTextColor

                            MouseArea {
                                anchors.fill: parent
                                cursorShape: Qt.PointingHandCursor
                                onClicked: {
                                    var list = root.coAuthorsList.slice();
                                    list.splice(index, 1);
                                    root.coAuthorsList = list;
                                }
                            }
                        }
                    }
                }
            }

            Item { Layout.fillWidth: true }
        }

        // Commit Button
        QQC2.Button {
            id: commitBtn
            Layout.fillWidth: true
            implicitHeight: 36
            highlighted: true
            enabled: summaryField.text.trimmed().length > 0 && appController.changedFiles.selectedCount > 0

            contentItem: RowLayout {
                anchors.centerIn: parent
                spacing: 4

                QQC2.Label {
                    text: i18n("Commit to")
                    font.bold: true
                    color: commitBtn.enabled ? Kirigami.Theme.highlightedTextColor : Kirigami.Theme.disabledTextColor
                }

                QQC2.Label {
                    text: appController.currentBranchName
                    font.bold: true
                    color: commitBtn.enabled ? Kirigami.Theme.highlightedTextColor : Kirigami.Theme.disabledTextColor
                    elide: Text.ElideRight
                    Layout.maximumWidth: 160
                }
            }

            onClicked: {
                var summary = summaryField.text.trimmed();
                var desc = descArea.text.trim();
                if (appController.commit(summary, desc, root.coAuthorsList)) {
                    summaryField.text = "";
                    descArea.text = "";
                }
            }
        }
    }
}
