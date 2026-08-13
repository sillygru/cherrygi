import QtQuick
import QtQuick.Controls as QQC2
import QtQuick.Layouts
import org.kde.kirigami as Kirigami
import org.kde.cherrygi

QQC2.Popup {
    id: branchDropdownPopup
    width: 360
    height: 440
    padding: Kirigami.Units.smallSpacing
    modal: true
    focus: true
    closePolicy: QQC2.Popup.CloseOnEscape | QQC2.Popup.CloseOnPressOutsideParent

    property bool isCreatingBranch: false

    onAboutToShow: {
        isCreatingBranch = false;
        newBranchField.text = "";
        searchField.text = "";
        appController.branches.filterText = "";
    }

    background: Rectangle {
        color: Kirigami.Theme.backgroundColor
        border.color: CherryStyle.borderColor
        border.width: 1
        radius: CherryStyle.radiusMedium
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
            color: CherryStyle.cardBackground
            border.color: CherryStyle.borderColor
            radius: CherryStyle.radiusSmall

            ColumnLayout {
                id: newBranchLayout
                anchors.fill: parent
                anchors.margins: Kirigami.Units.smallSpacing
                spacing: Kirigami.Units.smallSpacing

                QQC2.Label {
                    text: qsTr("Create a branch from '%1'").arg(appController.currentBranchName)
                    font.bold: true
                }

                QQC2.TextField {
                    id: newBranchField
                    Layout.fillWidth: true
                    placeholderText: qsTr("Name")
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
                anchors.leftMargin: Kirigami.Units.smallSpacing
                anchors.verticalCenter: parent.verticalCenter
                text: qsTr("Branches (%1)").arg(appController.branches.count)
                font.bold: true
                font.pixelSize: CherryStyle.basePixelSize - 1
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
                    height: 40

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
                        color: branchDelegate.highlighted ? CherryStyle.activeBackground : (branchDelegate.hovered ? CherryStyle.hoverBackground : "transparent")
                        radius: CherryStyle.radiusSmall
                        border.color: branchDelegate.highlighted ? Kirigami.Theme.highlightColor : "transparent"
                        border.width: branchDelegate.highlighted ? 1 : 0
                    }

                    contentItem: RowLayout {
                        spacing: Kirigami.Units.smallSpacing

                        Kirigami.Icon {
                            source: "vcs-branch"
                            width: Kirigami.Units.iconSizes.small
                            height: width
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
                            width: prLabel.width + 12
                            height: 20
                            radius: 10
                            color: Qt.rgba(0.2, 0.6, 1.0, 0.15)
                            border.color: Qt.rgba(0.2, 0.6, 1.0, 0.5)
                            border.width: 1

                            RowLayout {
                                anchors.centerIn: parent
                                spacing: 2

                                QQC2.Label {
                                    id: prLabel
                                    text: branchDelegate.prNumber
                                    font.pixelSize: CherryStyle.smallFont.pixelSize
                                    font.bold: true
                                    color: "#3584e4"
                                }

                                Kirigami.Icon {
                                    source: "dialog-ok"
                                    width: 12
                                    height: 12
                                    color: "#2ec27e"
                                    visible: branchDelegate.prMergedOrActive
                                }
                            }
                        }

                        // Default badge
                        Rectangle {
                            visible: branchDelegate.isDefault
                            width: defLabel.width + 8
                            height: 18
                            radius: 4
                            color: CherryStyle.cardBackground
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
