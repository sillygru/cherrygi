import QtQuick
import QtQuick.Controls as QQC2
import QtQuick.Layouts
import org.kde.kirigami as Kirigami
import "../style"

Rectangle {
    id: root
    visible: appController.toastVisible
    opacity: visible ? 1.0 : 0.0

    Behavior on opacity {
        NumberAnimation { duration: 200 }
    }

    implicitWidth: Math.min(toastLayout.implicitWidth + 32, 500)
    implicitHeight: 44
    radius: CherryStyle.radiusMedium

    color: appController.toastIsError ?
           Qt.rgba(Kirigami.Theme.negativeTextColor.r, Kirigami.Theme.negativeTextColor.g, Kirigami.Theme.negativeTextColor.b, 0.95) :
           Qt.rgba(Kirigami.Theme.backgroundColor.r, Kirigami.Theme.backgroundColor.g, Kirigami.Theme.backgroundColor.b, 0.95)

    border.color: appController.toastIsError ? Kirigami.Theme.negativeTextColor : CherryStyle.borderColor
    border.width: 1

    // Shadow simulation
    Rectangle {
        anchors.fill: parent
        anchors.margins: -2
        z: -1
        color: "transparent"
        border.color: Qt.rgba(0, 0, 0, 0.2)
        radius: CherryStyle.radiusMedium + 2
    }

    Timer {
        id: autoHideTimer
        interval: 4000
        running: root.visible
        repeat: false
        onTriggered: appController.hideToast()
    }

    RowLayout {
        id: toastLayout
        anchors.fill: parent
        anchors.leftMargin: Kirigami.Units.mediumSpacing
        anchors.rightMargin: Kirigami.Units.mediumSpacing
        spacing: Kirigami.Units.smallSpacing

        Kirigami.Icon {
            source: appController.toastIsError ? "dialog-error" : "dialog-information"
            width: Kirigami.Units.iconSizes.small
            height: width
            color: appController.toastIsError ? "#ffffff" : Kirigami.Theme.highlightColor
        }

        QQC2.Label {
            text: appController.toastMessage
            font.bold: true
            font.pixelSize: CherryStyle.smallFont.pixelSize
            color: appController.toastIsError ? "#ffffff" : Kirigami.Theme.textColor
            Layout.fillWidth: true
            elide: Text.ElideRight
        }

        Kirigami.Icon {
            source: "window-close"
            width: 14
            height: 14
            color: appController.toastIsError ? "#ffffff" : Kirigami.Theme.disabledTextColor

            MouseArea {
                anchors.fill: parent
                cursorShape: Qt.PointingHandCursor
                onClicked: appController.hideToast()
            }
        }
    }
}
