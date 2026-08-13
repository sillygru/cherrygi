pragma Singleton
import QtQuick
import org.kde.kirigami as Kirigami

QtObject {
    id: root

    // Palette & Accents matching KDE Plasma Breeze
    readonly property color background: Kirigami.Theme.backgroundColor
    readonly property color alternateBackground: Kirigami.Theme.alternateBackgroundColor
    readonly property color textColor: Kirigami.Theme.textColor
    readonly property color disabledTextColor: Kirigami.Theme.disabledTextColor
    readonly property color highlightColor: Kirigami.Theme.highlightColor
    readonly property color highlightedTextColor: Kirigami.Theme.highlightedTextColor
    readonly property color cardBackground: Qt.rgba(Kirigami.Theme.textColor.r, Kirigami.Theme.textColor.g, Kirigami.Theme.textColor.b, 0.04)
    readonly property color hoverBackground: Qt.rgba(Kirigami.Theme.textColor.r, Kirigami.Theme.textColor.g, Kirigami.Theme.textColor.b, 0.07)
    readonly property color activeBackground: Qt.rgba(Kirigami.Theme.highlightColor.r, Kirigami.Theme.highlightColor.g, Kirigami.Theme.highlightColor.b, 0.15)
    readonly property color borderColor: Qt.rgba(Kirigami.Theme.textColor.r, Kirigami.Theme.textColor.g, Kirigami.Theme.textColor.b, 0.12)
    readonly property color subtleBorderColor: Qt.rgba(Kirigami.Theme.textColor.r, Kirigami.Theme.textColor.g, Kirigami.Theme.textColor.b, 0.06)

    // Git specific semantic colors
    readonly property color additionColor: Kirigami.Theme.positiveTextColor
    readonly property color additionBg: Qt.rgba(Kirigami.Theme.positiveTextColor.r, Kirigami.Theme.positiveTextColor.g, Kirigami.Theme.positiveTextColor.b, 0.14)
    readonly property color additionGutterBg: Qt.rgba(Kirigami.Theme.positiveTextColor.r, Kirigami.Theme.positiveTextColor.g, Kirigami.Theme.positiveTextColor.b, 0.22)

    readonly property color deletionColor: Kirigami.Theme.negativeTextColor
    readonly property color deletionBg: Qt.rgba(Kirigami.Theme.negativeTextColor.r, Kirigami.Theme.negativeTextColor.g, Kirigami.Theme.negativeTextColor.b, 0.14)
    readonly property color deletionGutterBg: Qt.rgba(Kirigami.Theme.negativeTextColor.r, Kirigami.Theme.negativeTextColor.g, Kirigami.Theme.negativeTextColor.b, 0.22)

    readonly property color modifiedColor: "#e5a50a"
    readonly property color modifiedBg: Qt.rgba(0.9, 0.65, 0.04, 0.15)

    readonly property color hunkHeaderBg: Qt.rgba(Kirigami.Theme.highlightColor.r, Kirigami.Theme.highlightColor.g, Kirigami.Theme.highlightColor.b, 0.10)
    readonly property color hunkHeaderColor: Kirigami.Theme.highlightColor

    // Spacing & Radii
    readonly property int radiusSmall: Kirigami.Units.smallSpacing
    readonly property int radiusMedium: 6
    readonly property int radiusLarge: 8
    readonly property int headerHeight: 52
    readonly property int sidebarWidth: 360

    // Fonts
    readonly property font codeFont: Qt.font({
        family: "Monospace, Source Code Pro, Hack, JetBrains Mono, Fira Code, monospace",
        pixelSize: Kirigami.Units.fontMetrics.font.pixelSize - 1
    })

    readonly property font boldFont: Qt.font({
        family: Kirigami.Theme.defaultFont.family,
        pixelSize: Kirigami.Theme.defaultFont.pixelSize,
        bold: true
    })

    readonly property font smallFont: Qt.font({
        family: Kirigami.Theme.defaultFont.family,
        pixelSize: Kirigami.Units.fontMetrics.font.pixelSize - 2
    })
}
