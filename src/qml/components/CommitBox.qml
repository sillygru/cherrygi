import QtQuick
import QtQuick.Controls as QQC2
import QtQuick.Layouts
import org.kde.kirigami as Kirigami
import org.kde.cherrygi
import "../style"

Rectangle {
    id: root
    implicitHeight: layout.implicitHeight + Kirigami.Units.mediumSpacing * 2
    color: CherryStyle.surfaceHeader

    // Generate a stable hue-based avatar color from the author name
    function avatarColor(name) {
        if (!name || name.length === 0) return CherryStyle.secondaryTextColor;
        var hash = 0;
        for (var i = 0; i < name.length; i++) {
            hash = name.charCodeAt(i) + ((hash << 5) - hash);
        }
        var hue = Math.abs(hash % 360);
        return Qt.hsla(hue / 360.0, 0.55, 0.45, 1.0);
    }

    // Top separator
    Rectangle {
        anchors.top: parent.top
        anchors.left: parent.left
        anchors.right: parent.right
        height: 1
        color: CherryStyle.borderColor
        z: 2
    }

    property var coAuthorsList: []

    // Popup for adding co-author
    QQC2.Popup {
        id: addCoAuthorPopup
        width: 280
        height: 130
        padding: Kirigami.Units.mediumSpacing
        modal: true
        dim: false
        focus: true
        closePolicy: QQC2.Popup.CloseOnEscape | QQC2.Popup.CloseOnPressOutside

        background: Rectangle {
            color: CherryStyle.surfacePopup
            border.color: CherryStyle.popupBorderColor
            border.width: 1
            radius: CherryStyle.radiusMedium

            Rectangle {
                anchors.fill: parent
                anchors.margins: -2
                z: -1
                color: "transparent"
                border.color: Qt.rgba(0, 0, 0, 0.3)
                radius: CherryStyle.radiusMedium + 2
            }
        }

        ColumnLayout {
            anchors.fill: parent
            spacing: Kirigami.Units.smallSpacing

            QQC2.Label {
                text: qsTr("Add Co-Author")
                font.bold: true
                color: Kirigami.Theme.textColor
            }

            QQC2.TextField {
                id: coAuthorInput
                Layout.fillWidth: true
                placeholderText: "@username or Name <email>"
                background: Rectangle {
                    color: CherryStyle.inputBackground
                    border.color: coAuthorInput.activeFocus ? CherryStyle.accentColor : CherryStyle.borderColor
                    border.width: coAuthorInput.activeFocus ? 2 : 1
                    radius: CherryStyle.radiusSmall
                }
                onAccepted: addBtn.clicked()
            }

            RowLayout {
                Layout.alignment: Qt.AlignRight
                spacing: Kirigami.Units.smallSpacing

                QQC2.Button {
                    text: qsTr("Cancel")
                    onClicked: addCoAuthorPopup.close()
                }

                QQC2.Button {
                    id: addBtn
                    text: qsTr("Add")
                    highlighted: true
                    enabled: coAuthorInput.text.trim().length > 0
                    onClicked: {
                        var list = root.coAuthorsList.slice();
                        list.push(coAuthorInput.text.trim());
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
        anchors.margins: Kirigami.Units.mediumSpacing
        spacing: Kirigami.Units.smallSpacing

        // Undo Banner if available (GitHub Desktop style: active for unpushed commits)
        Rectangle {
            id: undoBanner
            Layout.fillWidth: true
            implicitHeight: undoRow.implicitHeight + 12
            visible: appController.canUndoCommit
            radius: CherryStyle.radiusMedium
            color: Qt.rgba(CherryStyle.accentColor.r, CherryStyle.accentColor.g, CherryStyle.accentColor.b, 0.15)
            border.color: Qt.rgba(CherryStyle.accentColor.r, CherryStyle.accentColor.g, CherryStyle.accentColor.b, 0.6)
            border.width: 1

            RowLayout {
                id: undoRow
                anchors.fill: parent
                anchors.leftMargin: Kirigami.Units.smallSpacing + 4
                anchors.rightMargin: Kirigami.Units.smallSpacing + 4
                spacing: Kirigami.Units.smallSpacing

                Rectangle {
                    width: 24
                    height: 24
                    radius: 12
                    color: Qt.rgba(CherryStyle.accentColor.r, CherryStyle.accentColor.g, CherryStyle.accentColor.b, 0.25)
                    Layout.alignment: Qt.AlignVCenter

                    Kirigami.Icon {
                        anchors.centerIn: parent
                        source: "edit-undo"
                        width: 14
                        height: 14
                        color: CherryStyle.accentColor
                    }
                }

                ColumnLayout {
                    Layout.fillWidth: true
                    spacing: 1

                    QQC2.Label {
                        text: qsTr("Unpushed Commit Available to Undo")
                        font.pixelSize: CherryStyle.smallFont.pixelSize - 1
                        color: CherryStyle.secondaryTextColor
                    }

                    QQC2.Label {
                        text: appController.lastUndoCommitSummary.length > 0 ?
                              appController.lastUndoCommitSummary : qsTr("(No summary)")
                        font.bold: true
                        font.pixelSize: CherryStyle.smallFont.pixelSize
                        color: Kirigami.Theme.textColor
                        elide: Text.ElideRight
                        Layout.fillWidth: true
                    }
                }

                QQC2.Button {
                    text: qsTr("Undo")
                    icon.name: "edit-undo"
                    font.bold: true
                    highlighted: true
                    implicitHeight: 28
                    enabled: !appController.isOperating
                    onClicked: {
                        if (appController.undoLastCommit()) {
                            summaryField.text = appController.lastUndoCommitSummary;
                            descArea.text = appController.lastUndoCommitDescription;
                            root.coAuthorsList = appController.lastUndoCommitCoAuthors;
                        }
                    }
                }
            }
        }

        // Summary row: Avatar + Summary Input
        RowLayout {
            Layout.fillWidth: true
            spacing: Kirigami.Units.smallSpacing

            // User avatar circle
            CherryAvatar {
                implicitWidth: 28
                implicitHeight: 28
                name: appController.currentAuthorName
                source: appController.currentAuthorAvatarUrl
                color: avatarColor(appController.currentAuthorName)
                Layout.alignment: Qt.AlignVCenter

                QQC2.ToolTip.text: appController.currentAuthorEmail.length > 0 ? (appController.currentAuthorName + " <" + appController.currentAuthorEmail + ">") : appController.currentAuthorName
                QQC2.ToolTip.visible: avatarMouse.containsMouse

                MouseArea {
                    id: avatarMouse
                    anchors.fill: parent
                    hoverEnabled: true
                }
            }

            QQC2.TextField {
                id: summaryField
                Layout.fillWidth: true
                placeholderText: qsTr("Summary (required)")
                text: ""
                font.bold: true
                enabled: !appController.isCommitting

                background: Rectangle {
                    color: CherryStyle.inputBackground
                    border.color: summaryField.activeFocus ? CherryStyle.accentColor : CherryStyle.borderColor
                    border.width: summaryField.activeFocus ? 2 : 1
                    radius: CherryStyle.radiusMedium
                }
            }
        }

        // Description Text Area with distinct contrast
        Rectangle {
            Layout.fillWidth: true
            Layout.preferredHeight: 60
            Layout.minimumHeight: 45
            color: CherryStyle.inputBackground
            border.color: descArea.activeFocus ? CherryStyle.accentColor : CherryStyle.borderColor
            border.width: descArea.activeFocus ? 2 : 1
            radius: CherryStyle.radiusMedium

            QQC2.ScrollView {
                anchors.fill: parent
                anchors.margins: 4
                clip: true

                QQC2.TextArea {
                    id: descArea
                    placeholderText: qsTr("Description (optional)")
                    wrapMode: Text.Wrap
                    font.pixelSize: CherryStyle.smallFont.pixelSize
                    enabled: !appController.isCommitting
                    background: Item {}
                }
            }
        }

        // Co-Authors Strip & Add button
        RowLayout {
            Layout.fillWidth: true
            spacing: 4

            QQC2.ToolButton {
                id: coAuthorBtn
                icon.name: "contact-new"
                icon.width: 14
                icon.height: 14
                text: qsTr("Add Co-Author")
                font.pixelSize: CherryStyle.smallFont.pixelSize - 1
                enabled: !appController.isCommitting
                onClicked: addCoAuthorPopup.open()
            }

            // Chips of added co-authors
            ListView {
                Layout.fillWidth: true
                implicitHeight: 24
                orientation: ListView.Horizontal
                spacing: 4
                clip: true
                model: root.coAuthorsList

                delegate: Rectangle {
                    height: 22
                    implicitWidth: chipRow.implicitWidth + 12
                    radius: 11
                    color: CherryStyle.surfaceCardElevated
                    border.color: CherryStyle.borderColor
                    border.width: 1

                    RowLayout {
                        id: chipRow
                        anchors.centerIn: parent
                        spacing: 4

                        QQC2.Label {
                            text: modelData
                            font.pixelSize: CherryStyle.smallFont.pixelSize - 1
                            color: Kirigami.Theme.textColor
                        }

                        Kirigami.Icon {
                            source: "window-close"
                            width: 10
                            height: 10
                            color: CherryStyle.secondaryTextColor

                            MouseArea {
                                anchors.fill: parent
                                enabled: !appController.isCommitting
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
        }

        // Commit Button (Solid high-contrast styling with loading state)
        QQC2.Button {
            id: commitBtn
            Layout.fillWidth: true
            implicitHeight: 40
            highlighted: true
            enabled: summaryField.text.trim().length > 0 && appController.changedFiles.selectedCount > 0 && !appController.isCommitting

            contentItem: RowLayout {
                spacing: 6
                Layout.alignment: Qt.AlignCenter

                Item { Layout.fillWidth: true }

                QQC2.BusyIndicator {
                    width: 14
                    height: 14
                    visible: appController.isCommitting
                    running: appController.isCommitting
                }

                Kirigami.Icon {
                    source: "vcs-commit"
                    width: 14
                    height: 14
                    visible: !appController.isCommitting
                    color: commitBtn.enabled ? Kirigami.Theme.highlightedTextColor : CherryStyle.secondaryTextColor
                }

                QQC2.Label {
                    text: appController.isCommitting ? qsTr("Committing to") : qsTr("Commit to")
                    font.bold: true
                    color: commitBtn.enabled ? Kirigami.Theme.highlightedTextColor : CherryStyle.secondaryTextColor
                }

                QQC2.Label {
                    text: appController.currentBranchName
                    font.bold: true
                    color: commitBtn.enabled ? Kirigami.Theme.highlightedTextColor : CherryStyle.secondaryTextColor
                    elide: Text.ElideRight
                    Layout.maximumWidth: 160
                }

                Item { Layout.fillWidth: true }
            }

            onClicked: {
                var summary = summaryField.text.trim();
                var desc = descArea.text.trim();
                if (appController.commit(summary, desc, root.coAuthorsList)) {
                    summaryField.text = "";
                    descArea.text = "";
                }
            }
        }
    }
}

