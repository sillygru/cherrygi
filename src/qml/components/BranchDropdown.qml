import QtQuick
import QtQuick.Controls as QQC2
import QtQuick.Layouts
import org.kde.kirigami as Kirigami
import "../style"

QQC2.Popup {
    id: popup
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
            visible: !popup.isCreatingBranch
            spacing: Kirigami.Units.smallSpacing

            QQC2.TextField {
                id: searchField
                Layout.fillWidth: true
                placeholderText: i18n("Filter branches...")
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
                text: i18n("New Branch")
                icon.name: "list-add"
                onClicked: {
                    popup.isCreatingBranch = true;
                    newBranchField.forceActiveFocus();
                }
            }
        }

        // Inline New Branch Creator
        Rectangle {
            Layout.fillWidth: true
            implicitHeight: newBranchLayout.implicitHeight + Kirigami.Units.smallSpacing * 2
            visible: popup.isCreatingBranch
            color: CherryStyle.cardBackground
            border.color: CherryStyle.borderColor
            radius: CherryStyle.radiusSmall

            ColumnLayout {
                id: newBranchLayout
                anchors.fill: parent
                anchors.margins: Kirigami.Units.smallSpacing
                spacing: Kirigami.Units.smallSpacing

                QQC2.Label {
                    text: i18n("Create a branch from '%1'", appController.currentBranchName)
                    font.bold: true
                }

                QQC2.TextField {
                    id: newBranchField
                    Layout.fillWidth: true
                    placeholderText: i18n("Name")
                    onAccepted: createBtn.clicked()
                }

                RowLayout {
                    Layout.alignment: Qt.AlignRight
                    spacing: Kirigami.Units.smallSpacing

                    QQC2.Button {
                        text: i18n("Cancel")
                        onClicked: {
                            popup.isCreatingBranch = false;
                        }
                    }

                    QQC2.Button {
                        id: createBtn
                        text: i18n("Create Branch")
                        highlighted: true
                        enabled: newBranchField.text.trimmed().length > 0
                        onClicked: {
                            appController.createBranch(newBranchField.text.trimmed());
                            popup.isCreatingBranch = false;
                            popup.close();
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
                text: i18n("Branches (%1)", appController.branches.count)
                font.bold: true
                font.pixelSize: Kirigami.Units.fontMetrics.font.pixelSize - 1
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

                delegate: QQC2.ItemDelegate {
                    width: branchListView.width
                    height: 40
                    highlighted: model.isCurrent

                    background: Rectangle {
                        color: parent.highlighted ? CherryStyle.activeBackground : (parent.hovered ? CherryStyle.hoverBackground : "transparent")
                        radius: CherryStyle.radiusSmall
                        border.color: parent.highlighted ? Kirigami.Theme.highlightColor : "transparent"
                        border.width: parent.highlighted ? 1 : 0
                    }

                    contentItem: RowLayout {
                        spacing: Kirigami.Units.smallSpacing

                        Kirigami.Icon {
                            source: model.isCurrent ? "vcs-branch" : "vcs-branch"
                            width: Kirigami.Units.iconSizes.small
                            height: width
                            color: model.isCurrent ? Kirigami.Theme.highlightColor : Kirigami.Theme.textColor
                        }

                        QQC2.Label {
                            text: model.name
                            font.bold: model.isCurrent
                            color: model.isCurrent ? Kirigami.Theme.highlightColor : Kirigami.Theme.textColor
                            elide: Text.ElideRight
                            Layout.fillWidth: true
                        }

                        // PR badge if present
                        Rectangle {
                            visible: model.prNumber !== ""
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
                                    text: model.prNumber
                                    font.pixelSize: CherryStyle.smallFont.pixelSize
                                    font.bold: true
                                    color: "#3584e4"
                                }

                                Kirigami.Icon {
                                    source: "dialog-ok"
                                    width: 12
                                    height: 12
                                    color: "#2ec27e"
                                    visible: model.prMergedOrActive
                                }
                            }
                        }

                        // Default badge
                        Rectangle {
                            visible: model.isDefault
                            width: defLabel.width + 8
                            height: 18
                            radius: 4
                            color: CherryStyle.cardBackground
                            border.color: CherryStyle.borderColor
                            border.width: 1

                            QQC2.Label {
                                id: defLabel
                                anchors.centerIn: parent
                                text: i18n("default")
                                font.pixelSize: CherryStyle.smallFont.pixelSize - 1
                                color: Kirigami.Theme.disabledTextColor
                            }
                        }
                    }

                    onClicked: {
                        appController.switchBranch(model.name);
                        popup.close();
                    }
                }
            }
        }
    }
}
