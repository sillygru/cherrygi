import QtQuick
import QtQuick.Controls as QQC2
import QtQuick.Layouts
import org.kde.kirigami as Kirigami
import org.kde.cherrygi
import "../style"

QQC2.Popup {
    id: branchDropdownPopup
    width: 380
    height: 440
    padding: Kirigami.Units.mediumSpacing
    modal: true
    dim: false
    focus: true
    closePolicy: QQC2.Popup.CloseOnEscape | QQC2.Popup.CloseOnPressOutside

    property bool isCreatingBranch: false

    onAboutToShow: {
        isCreatingBranch = false;
        newBranchField.text = "";
        searchField.text = "";
        appController.branches.filterText = "";
    }

    background: Rectangle {
        color: CherryStyle.surfacePopup
        border.color: CherryStyle.popupBorderColor
        border.width: 1
        radius: CherryStyle.radiusLarge

        // Multi-layered shadow for realistic floating weight & contrast
        Rectangle {
            anchors.fill: parent
            anchors.margins: -1
            z: -1
            color: "transparent"
            border.color: Qt.rgba(0, 0, 0, 0.35)
            radius: CherryStyle.radiusLarge + 1
        }

        Rectangle {
            anchors.fill: parent
            anchors.margins: -3
            z: -2
            color: "transparent"
            border.color: Qt.rgba(0, 0, 0, 0.15)
            radius: CherryStyle.radiusLarge + 3
        }
    }

    ColumnLayout {
        anchors.fill: parent
        spacing: Kirigami.Units.smallSpacing

        // Search Box & New Branch toggle
        RowLayout {
            Layout.fillWidth: true
            visible: !branchDropdownPopup.isCreatingBranch
            spacing: Kirigami.Units.smallSpacing

            QQC2.TextField {
                id: searchField
                Layout.fillWidth: true
                placeholderText: qsTr("Filter branches...")
                leftPadding: Kirigami.Units.largeSpacing + 12
                rightPadding: text.length > 0 ? Kirigami.Units.largeSpacing + 10 : Kirigami.Units.smallSpacing

                background: Rectangle {
                    color: CherryStyle.inputBackground
                    border.color: searchField.activeFocus ? Kirigami.Theme.highlightColor : CherryStyle.borderColor
                    border.width: searchField.activeFocus ? 2 : 1
                    radius: CherryStyle.radiusSmall
                }

                Kirigami.Icon {
                    source: "search"
                    width: 14
                    height: 14
                    anchors.left: parent.left
                    anchors.leftMargin: Kirigami.Units.smallSpacing + 2
                    anchors.verticalCenter: parent.verticalCenter
                    color: searchField.activeFocus ? Kirigami.Theme.highlightColor : Kirigami.Theme.disabledTextColor
                }

                QQC2.ToolButton {
                    visible: searchField.text.length > 0
                    icon.name: "edit-clear"
                    icon.width: 12
                    icon.height: 12
                    anchors.right: parent.right
                    anchors.verticalCenter: parent.verticalCenter
                    onClicked: {
                        searchField.text = "";
                        appController.branches.filterText = "";
                    }
                }

                onTextChanged: {
                    appController.branches.filterText = text;
                }
            }

            QQC2.Button {
                text: qsTr("New Branch")
                icon.name: "list-add"
                onClicked: {
                    branchDropdownPopup.isCreatingBranch = true;
                    newBranchField.forceActiveFocus();
                }
            }
        }

        // Inline New Branch Creator
        Rectangle {
            Layout.fillWidth: true
            implicitHeight: newBranchLayout.implicitHeight + Kirigami.Units.smallSpacing * 2
            visible: branchDropdownPopup.isCreatingBranch
            color: CherryStyle.surfaceCardElevated
            border.color: CherryStyle.borderColor
            border.width: 1
            radius: CherryStyle.radiusSmall

            ColumnLayout {
                id: newBranchLayout
                anchors.fill: parent
                anchors.margins: Kirigami.Units.smallSpacing
                spacing: Kirigami.Units.smallSpacing

                QQC2.Label {
                    text: qsTr("Create a branch from '%1'").arg(appController.currentBranchName)
                    font.bold: true
                    color: Kirigami.Theme.textColor
                }

                QQC2.TextField {
                    id: newBranchField
                    Layout.fillWidth: true
                    placeholderText: qsTr("Name")
                    background: Rectangle {
                        color: CherryStyle.inputBackground
                        border.color: newBranchField.activeFocus ? Kirigami.Theme.highlightColor : CherryStyle.borderColor
                        border.width: newBranchField.activeFocus ? 2 : 1
                        radius: CherryStyle.radiusSmall
                    }
                    onAccepted: createBtn.clicked()
                }

                RowLayout {
                    Layout.alignment: Qt.AlignRight
                    spacing: Kirigami.Units.smallSpacing

                    QQC2.Button {
                        text: qsTr("Cancel")
                        onClicked: {
                            branchDropdownPopup.isCreatingBranch = false;
                        }
                    }

                    QQC2.Button {
                        id: createBtn
                        text: qsTr("Create Branch")
                        highlighted: true
                        enabled: newBranchField.text.trim().length > 0
                        onClicked: {
                            appController.createBranch(newBranchField.text.trim());
                            branchDropdownPopup.isCreatingBranch = false;
                            branchDropdownPopup.close();
                        }
                    }
                }
            }
        }

        // Section header
        Item {
            Layout.fillWidth: true
            height: 20
            QQC2.Label {
                anchors.left: parent.left
                anchors.leftMargin: 2
                anchors.verticalCenter: parent.verticalCenter
                text: qsTr("Branches (%1)").arg(appController.branches.count)
                font.bold: true
                font.pixelSize: CherryStyle.smallFont.pixelSize
                color: Kirigami.Theme.disabledTextColor
            }
        }

        // Branch list
        QQC2.ScrollView {
            Layout.fillWidth: true
            Layout.fillHeight: true
            clip: true

            ListView {
                id: branchListView
                model: appController.branches
                spacing: 2

                signal requestClose()
                onRequestClose: branchDropdownPopup.close()

                delegate: QQC2.ItemDelegate {
                    id: branchDelegate
                    width: branchListView.width
                    height: 44

                    required property int index
                    required property string name
                    required property bool isCurrent
                    required property bool isDefault
                    required property bool isRemote
                    required property string prNumber
                    required property bool prMergedOrActive
                    required property string tipCommitSha

                    highlighted: branchDelegate.isCurrent

                    background: Rectangle {
                        color: branchDelegate.highlighted ? CherryStyle.activeBackground : (branchDelegate.hovered ? CherryStyle.hoverBackground : CherryStyle.surfaceCard)
                        radius: CherryStyle.radiusSmall
                        border.color: branchDelegate.highlighted ? Kirigami.Theme.highlightColor : (branchDelegate.hovered ? CherryStyle.borderColor : CherryStyle.subtleBorderColor)
                        border.width: 1
                    }

                    contentItem: RowLayout {
                        anchors.fill: parent
                        anchors.leftMargin: Kirigami.Units.smallSpacing
                        anchors.rightMargin: Kirigami.Units.smallSpacing
                        spacing: Kirigami.Units.smallSpacing

                        Kirigami.Icon {
                            source: "vcs-branch"
                            width: 16
                            height: 16
                            color: branchDelegate.isCurrent ? Kirigami.Theme.highlightColor : Kirigami.Theme.textColor
                        }

                        QQC2.Label {
                            text: branchDelegate.name
                            font.bold: branchDelegate.isCurrent
                            color: branchDelegate.isCurrent ? Kirigami.Theme.highlightColor : Kirigami.Theme.textColor
                            elide: Text.ElideRight
                            Layout.fillWidth: true
                        }

                        // PR badge if present
                        Rectangle {
                            visible: branchDelegate.prNumber !== ""
                            implicitWidth: prBadgeInner.implicitWidth + 12
                            implicitHeight: 20
                            radius: 10
                            color: Qt.rgba(0.2, 0.6, 1.0, 0.18)
                            border.color: Qt.rgba(0.2, 0.6, 1.0, 0.6)
                            border.width: 1

                            RowLayout {
                                id: prBadgeInner
                                anchors.centerIn: parent
                                spacing: 3

                                QQC2.Label {
                                    id: prLabel
                                    text: branchDelegate.prNumber
                                    font.pixelSize: CherryStyle.smallFont.pixelSize - 1
                                    font.bold: true
                                    color: "#4595f6"
                                }

                                Kirigami.Icon {
                                    source: "dialog-ok"
                                    width: 11
                                    height: 11
                                    color: "#2ec27e"
                                    visible: branchDelegate.prMergedOrActive
                                }
                            }
                        }

                        // Default badge
                        Rectangle {
                            visible: branchDelegate.isDefault
                            implicitWidth: defLabel.implicitWidth + 10
                            implicitHeight: 18
                            radius: 4
                            color: CherryStyle.surfaceCardElevated
                            border.color: CherryStyle.borderColor
                            border.width: 1

                            QQC2.Label {
                                id: defLabel
                                anchors.centerIn: parent
                                text: qsTr("default")
                                font.pixelSize: CherryStyle.smallFont.pixelSize - 1
                                color: Kirigami.Theme.disabledTextColor
                            }
                        }
                    }

                    onClicked: {
                        appController.switchBranch(branchDelegate.name);
                        ListView.view.requestClose();
                    }
                }
            }
        }
    }
}

