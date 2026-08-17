import QtQuick
import QtQuick.Controls as QQC2
import QtQuick.Layouts
import org.kde.kirigami as Kirigami
import org.kde.cherrygi
import "../style"

QQC2.Popup {
    id: root
    width: Math.min(520, parent ? parent.width - 32 : 520)
    implicitHeight: layout.implicitHeight + Kirigami.Units.largeSpacing * 2
    anchors.centerIn: parent
    padding: Kirigami.Units.largeSpacing
    modal: true
    dim: true
    focus: true
    closePolicy: QQC2.Popup.CloseOnEscape | QQC2.Popup.CloseOnPressOutside

    property string selectedStyle: appController.aiCommitStyle
    property bool includeDesc: appController.aiIncludeDescription
    property bool followRepo: appController.aiFollowRepoStyle
    property string apiKeyText: appController.aiApiKey

    signal accepted()

    onAboutToShow: {
        selectedStyle = appController.aiCommitStyle.length > 0 ? appController.aiCommitStyle : "conventional";
        includeDesc = appController.aiIncludeDescription;
        followRepo = appController.aiFollowRepoStyle;
        apiKeyText = appController.aiApiKey;
    }

    background: Rectangle {
        color: CherryStyle.surfacePopup
        border.color: CherryStyle.popupBorderColor
        border.width: 1
        radius: CherryStyle.radiusLarge

        // Multi-layer drop shadow
        Rectangle {
            anchors.fill: parent
            anchors.margins: -2
            z: -1
            color: "transparent"
            border.color: Qt.rgba(0, 0, 0, 0.35)
            radius: CherryStyle.radiusLarge + 2
        }
    }

    contentItem: ColumnLayout {
        id: layout
        spacing: Kirigami.Units.mediumSpacing

        // Header
        RowLayout {
            Layout.fillWidth: true
            spacing: Kirigami.Units.smallSpacing

            Kirigami.Icon {
                source: "tools-wizard"
                width: 22
                height: 22
                color: CherryStyle.accentColor
            }

            ColumnLayout {
                Layout.fillWidth: true
                spacing: 1

                QQC2.Label {
                    text: qsTr("AI Commit Style Preference")
                    font.bold: true
                    font.pixelSize: CherryStyle.basePixelSize + 1
                    color: Kirigami.Theme.textColor
                }

                QQC2.Label {
                    text: qsTr("Choose how you want AI to format your commit messages.")
                    font.pixelSize: CherryStyle.smallFont.pixelSize
                    color: CherryStyle.secondaryTextColor
                }
            }

            QQC2.ToolButton {
                icon.name: "window-close"
                icon.width: 14
                icon.height: 14
                onClicked: root.close()
            }
        }

        Rectangle {
            Layout.fillWidth: true
            height: 1
            color: CherryStyle.subtleBorderColor
        }

        // Commit Style Selector
        ColumnLayout {
            Layout.fillWidth: true
            spacing: 6

            QQC2.Label {
                text: qsTr("Commit Format Style")
                font.bold: true
                color: Kirigami.Theme.textColor
            }

            QQC2.ButtonGroup { id: styleGroup }

            QQC2.RadioButton {
                text: qsTr("Conventional Commits (feat:, fix:, chore:, refactor:, docs:, ...)")
                checked: root.selectedStyle === "conventional"
                QQC2.ButtonGroup.group: styleGroup
                onClicked: root.selectedStyle = "conventional"
            }

            QQC2.RadioButton {
                text: qsTr("Imperative Summary (e.g. \"Add support for dark mode\", \"Fix memory leak\")")
                checked: root.selectedStyle === "summary"
                QQC2.ButtonGroup.group: styleGroup
                onClicked: root.selectedStyle = "summary"
            }

            QQC2.RadioButton {
                text: qsTr("Concise Summary (Direct, brief plain text explanation)")
                checked: root.selectedStyle === "concise"
                QQC2.ButtonGroup.group: styleGroup
                onClicked: root.selectedStyle = "concise"
            }

            QQC2.RadioButton {
                text: qsTr("Gitmoji (Starts with relevant emoji: :sparkles:, :bug:, :recycle:, ...)")
                checked: root.selectedStyle === "gitmoji"
                QQC2.ButtonGroup.group: styleGroup
                onClicked: root.selectedStyle = "gitmoji"
            }
        }

        Rectangle {
            Layout.fillWidth: true
            height: 1
            color: CherryStyle.subtleBorderColor
        }

        // Length & Output Mode
        ColumnLayout {
            Layout.fillWidth: true
            spacing: 6

            QQC2.Label {
                text: qsTr("Output Structure")
                font.bold: true
                color: Kirigami.Theme.textColor
            }

            QQC2.ButtonGroup { id: structureGroup }

            QQC2.RadioButton {
                id: titleAndDescRadio
                text: qsTr("Title and Description (Title strictly under 40 chars + bullet points)")
                checked: root.includeDesc
                QQC2.ButtonGroup.group: structureGroup
                onClicked: root.includeDesc = true
            }

            QQC2.RadioButton {
                id: titleOnlyRadio
                text: qsTr("Title Only (Single line commit summary)")
                checked: !root.includeDesc
                QQC2.ButtonGroup.group: structureGroup
                onClicked: root.includeDesc = false
            }
        }

        // Context / History
        QQC2.CheckBox {
            id: followHistoryCheck
            text: qsTr("Follow existing repository commit history (sends last 5 commits for style)")
            checked: root.followRepo
            onToggled: root.followRepo = checked
        }

        // API Key entry if missing
        ColumnLayout {
            Layout.fillWidth: true
            visible: appController.aiApiKey.trim().length === 0 && appController.aiProvider !== "custom"
            spacing: 4

            QQC2.Label {
                text: qsTr("%1 API Key (Required for AI generation):").arg(appController.aiProvider.toUpperCase())
                font.bold: true
                color: CherryStyle.warningColor
            }

            QQC2.TextField {
                id: keyInput
                Layout.fillWidth: true
                echoMode: TextInput.Password
                placeholderText: qsTr("Enter API key...")
                text: root.apiKeyText
                background: Rectangle {
                    color: CherryStyle.inputBackground
                    border.color: keyInput.activeFocus ? CherryStyle.accentColor : CherryStyle.borderColor
                    border.width: keyInput.activeFocus ? 2 : 1
                    radius: CherryStyle.radiusSmall
                }
                onTextChanged: root.apiKeyText = text
            }
        }

        // Actions
        RowLayout {
            Layout.fillWidth: true
            Layout.topMargin: Kirigami.Units.smallSpacing
            spacing: Kirigami.Units.smallSpacing

            QQC2.Button {
                text: qsTr("Full AI Settings...")
                icon.name: "settings-configure"
                onClicked: {
                    root.close();
                    appController.showSettingsDialog("ai");
                }
            }

            Item { Layout.fillWidth: true }

            QQC2.Button {
                text: qsTr("Cancel")
                onClicked: root.close()
            }

            QQC2.Button {
                text: qsTr("Save & Generate")
                icon.name: "tools-wizard"
                highlighted: true
                onClicked: {
                    var apiKey = root.apiKeyText.trim();
                    appController.saveAiSettings(
                        true, // enabled
                        appController.aiProvider,
                        apiKey.length > 0 ? apiKey : appController.aiApiKey,
                        appController.aiEndpoint,
                        appController.aiModel,
                        root.selectedStyle,
                        root.includeDesc,
                        root.followRepo,
                        true // firstRunConfigured
                    );
                    root.close();
                    root.accepted();
                    appController.generateAiCommit();
                }
            }
        }
    }
}
