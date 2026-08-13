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
        if (!name || name.length === 0) return Kirigami.Theme.disabledTextColor;
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
                    border.color: coAuthorInput.activeFocus ? Kirigami.Theme.highlightColor : CherryStyle.borderColor
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

        // Undo Banner if available
        Rectangle {
            id: undoBanner
            Layout.fillWidth: true
            height: 34
            visible: appController.hasUndoCommit
            radius: CherryStyle.radiusMedium
            color: Qt.rgba(0.2, 0.6, 1.0, 0.18)
            border.color: Qt.rgba(0.2, 0.6, 1.0, 0.5)
            border.width: 1

            RowLayout {
                anchors.fill: parent
                anchors.leftMargin: Kirigami.Units.smallSpacing + 2
                anchors.rightMargin: Kirigami.Units.smallSpacing + 2
                spacing: Kirigami.Units.smallSpacing

                Kirigami.Icon {
                    source: "edit-undo"
                    width: 16
                    height: 16
                    color: "#4595f6"
                }

                QQC2.Label {
                    text: qsTr("Commit created!")
                    font.bold: true
                    font.pixelSize: CherryStyle.smallFont.pixelSize
                    color: Kirigami.Theme.textColor
                    Layout.fillWidth: true
                    elide: Text.ElideRight
                }

                QQC2.Button {
                    text: qsTr("Undo")
                    font.bold: true
                    implicitHeight: 26
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

            // User avatar circle
            Rectangle {
                width: 28
                height: 28
                radius: 14
                color: avatarColor(appController.currentAuthorName)
                Layout.alignment: Qt.AlignVCenter

                QQC2.ToolTip.text: appController.currentAuthorName
                QQC2.ToolTip.visible: avatarMouse.containsMouse

                MouseArea {
                    id: avatarMouse
                    anchors.fill: parent
                    hoverEnabled: true
                }

                QQC2.Label {
                    anchors.centerIn: parent
                    text: appController.currentAuthorInitial
                    font.bold: true
                    font.pixelSize: 12
                    color: Kirigami.Theme.highlightedTextColor
                }
            }

            QQC2.TextField {
                id: summaryField
                Layout.fillWidth: true
                placeholderText: qsTr("Summary (required)")
                text: ""
                font.bold: true

                background: Rectangle {
                    color: CherryStyle.inputBackground
                    border.color: summaryField.activeFocus ? Kirigami.Theme.highlightColor : CherryStyle.borderColor
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
            border.color: descArea.activeFocus ? Kirigami.Theme.highlightColor : CherryStyle.borderColor
            border.width: descArea.activeFocus ? 2 : 1
            radius: CherryStyle.radiusMedium
            clip: true

            QQC2.ScrollView {
                anchors.fill: parent
                anchors.margins: 4

                QQC2.TextArea {
                    id: descArea
                    placeholderText: qsTr("Description")
                    wrapMode: TextEdit.Wrap
                    font.pixelSize: CherryStyle.smallFont.pixelSize
                    background: null
                }
            }
        }

        // Co-Authors Row
        RowLayout {
            Layout.fillWidth: true
            spacing: 6

            // Co-author icon trigger button
            Rectangle {
                width: 24
                height: 24
                radius: CherryStyle.radiusMedium
                color: coAuthorBtnMouse.containsMouse ? CherryStyle.hoverBackground : CherryStyle.surfaceCard
                border.color: CherryStyle.borderColor
                border.width: 1

                Kirigami.Icon {
                    anchors.centerIn: parent
                    source: "user-identity"
                    width: 14
                    height: 14
                    color: Kirigami.Theme.disabledTextColor
                }

                MouseArea {
                    id: coAuthorBtnMouse
                    anchors.fill: parent
                    hoverEnabled: true
                    cursorShape: Qt.PointingHandCursor
                    QQC2.ToolTip.text: qsTr("Add Co-Author")
                    QQC2.ToolTip.visible: containsMouse
                    onClicked: {
                        addCoAuthorPopup.x = parent.x;
                        addCoAuthorPopup.y = parent.y - addCoAuthorPopup.height - 4;
                        addCoAuthorPopup.open();
                    }
                }
            }

            QQC2.Label {
                text: qsTr("Co-Authors:")
                font.pixelSize: CherryStyle.smallFont.pixelSize - 1
                color: Kirigami.Theme.disabledTextColor
                visible: root.coAuthorsList.length > 0
            }

            // Co-author chips (Flow layout so it wraps gracefully)
            Flow {
                Layout.fillWidth: true
                spacing: 4

                Repeater {
                    model: root.coAuthorsList

                    delegate: Rectangle {
                        height: 24
                        implicitWidth: chipRow.implicitWidth + 12
                        radius: 12
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
            }
        }

        // Commit Button (Solid high-contrast styling)
        QQC2.Button {
            id: commitBtn
            Layout.fillWidth: true
            implicitHeight: 40
            highlighted: true
            enabled: summaryField.text.trim().length > 0 && appController.changedFiles.selectedCount > 0

            contentItem: RowLayout {
                spacing: 4
                Layout.alignment: Qt.AlignCenter

                Item { Layout.fillWidth: true }

                Kirigami.Icon {
                    source: "vcs-commit"
                    width: 14
                    height: 14
                    color: commitBtn.enabled ? Kirigami.Theme.highlightedTextColor : Kirigami.Theme.disabledTextColor
                }

                QQC2.Label {
                    text: qsTr("Commit to")
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

