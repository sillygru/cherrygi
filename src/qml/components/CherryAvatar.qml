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
        // Rectangle's default opaque fill supplies the mask alpha.
        visible: false
    }

    // Explicitly render the mask to a texture. This is required for reliable
    // MultiEffect mask sampling on Qt 6; a hidden plain Rectangle is not a
    // valid texture source on all scene-graph backends.
    ShaderEffectSource {
        id: maskSource
        anchors.fill: maskRect
        sourceItem: maskRect
        hideSource: true
        live: true
        z: -1
    }

    MultiEffect {
        id: avatarEffect
        anchors.fill: parent
        source: avatarImage
        maskEnabled: true
        maskSource: maskSource
        visible: root.source !== "" && avatarImage.status === Image.Ready
    }
}
