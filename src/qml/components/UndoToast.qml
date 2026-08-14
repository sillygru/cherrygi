import QtQuick
import QtQuick.Controls as QQC2
import QtQuick.Layouts
import org.kde.kirigami as Kirigami
import org.kde.cherrygi
import "../style"

Rectangle {
    id: root
    visible: appController.toastVisible
    opacity: visible ? 1.0 : 0.0

    Behavior on opacity {
        NumberAnimation { duration: 200 }
    }

    implicitWidth: Math.min(toastLayout.implicitWidth + 36, 520)
    implicitHeight: 44
    radius: CherryStyle.radiusMedium

    color: appController.toastIsError ?
           Qt.rgba(CherryStyle.deletionColor.r, CherryStyle.deletionColor.g, CherryStyle.deletionColor.b, 0.96) :
           CherryStyle.surfacePopup

    border.color: appController.toastIsError ? CherryStyle.deletionColor : CherryStyle.popupBorderColor
    border.width: 1

    // Layered floating shadow
    Rectangle {
        anchors.fill: parent
        anchors.margins: -1
        z: -1
        color: "transparent"
        border.color: Qt.rgba(0, 0, 0, 0.4)
        radius: CherryStyle.radiusMedium + 1
    }

    Rectangle {
        anchors.fill: parent
        anchors.margins: -3
        z: -2
        color: "transparent"
        border.color: Qt.rgba(0, 0, 0, 0.18)
        radius: CherryStyle.radiusMedium + 3
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
        anchors.leftMargin: Kirigami.Units.mediumSpacing + 2
        anchors.rightMargin: Kirigami.Units.mediumSpacing + 2
        spacing: Kirigami.Units.smallSpacing

        Kirigami.Icon {
            source: appController.toastIsError ? "dialog-error" : "dialog-information"
            width: 16
            height: 16
            color: appController.toastIsError ? CherryStyle.errorTextColor : CherryStyle.accentColor
        }

        QQC2.Label {
            text: appController.toastMessage
            font.bold: true
            font.pixelSize: CherryStyle.smallFont.pixelSize
            color: appController.toastIsError ? CherryStyle.errorTextColor : Kirigami.Theme.textColor
            Layout.fillWidth: true
            elide: Text.ElideRight
        }

        Kirigami.Icon {
            source: "window-close"
            width: 14
            height: 14
            color: appController.toastIsError ? CherryStyle.errorTextColor : CherryStyle.secondaryTextColor

            MouseArea {
                anchors.fill: parent
                cursorShape: Qt.PointingHandCursor
                onClicked: appController.hideToast()
            }
        }
    }
}

