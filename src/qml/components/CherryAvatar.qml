import QtQuick
import QtQuick.Controls as QQC2
import QtQuick.Layouts
import QtQuick.Effects
import org.kde.kirigami as Kirigami
import org.kde.cherrygi
import "../style"

Item {
    id: root

    property string name: ""
    property string source: ""
    property color color: CherryStyle.accentColor
    property string initials: {
        var trimmed = name.trim();
        if (trimmed.length === 0) return "U";
        var parts = trimmed.split(/\s+/);
        if (parts.length >= 2 && parts[0].length > 0 && parts[1].length > 0) {
            return (parts[0].charAt(0) + parts[1].charAt(0)).toUpperCase();
        }
        return trimmed.charAt(0).toUpperCase();
    }

    implicitWidth: 36
    implicitHeight: 36

    // Fallback Circle with Initial(s)
    Rectangle {
        id: fallbackCircle
        anchors.fill: parent
        radius: width / 2
        color: root.color
        visible: !avatarEffect.visible

        QQC2.Label {
            anchors.centerIn: parent
            text: root.initials
            font.bold: true
            font.pixelSize: Math.max(9, Math.round(parent.height * 0.4))
            color: Kirigami.Theme.highlightedTextColor
        }
    }

    // Avatar Image with Circular Mask
    Image {
        id: avatarImage
        anchors.fill: parent
        source: root.source
        asynchronous: true
        cache: true
        fillMode: Image.PreserveAspectCrop
        visible: false
    }

    Rectangle {
        id: maskRect
        anchors.fill: parent
        radius: width / 2
        visible: false
    }

    MultiEffect {
        id: avatarEffect
        anchors.fill: parent
        source: avatarImage
        maskEnabled: true
        maskSource: maskRect
        visible: root.source !== "" && avatarImage.status === Image.Ready
    }
}
