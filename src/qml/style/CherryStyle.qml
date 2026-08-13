pragma Singleton
import QtQuick
import org.kde.kirigami as Kirigami

QtObject {
    id: root

    // Palette & Accents matching KDE Plasma Breeze with distinct surface contrast
    readonly property color background: Kirigami.Theme.backgroundColor
    readonly property color alternateBackground: Kirigami.Theme.alternateBackgroundColor
    readonly property color textColor: Kirigami.Theme.textColor
    readonly property color disabledTextColor: Kirigami.Theme.disabledTextColor
    readonly property color highlightColor: Kirigami.Theme.highlightColor
    readonly property color highlightedTextColor: Kirigami.Theme.highlightedTextColor

    // Surface elevation levels for hierarchy and visual weight
    readonly property color surfaceHeader: Qt.tint(Kirigami.Theme.backgroundColor, Qt.rgba(Kirigami.Theme.textColor.r, Kirigami.Theme.textColor.g, Kirigami.Theme.textColor.b, 0.04))
    readonly property color surfaceSidebar: Qt.tint(Kirigami.Theme.backgroundColor, Qt.rgba(Kirigami.Theme.textColor.r, Kirigami.Theme.textColor.g, Kirigami.Theme.textColor.b, 0.03))
    readonly property color surfacePopup: Qt.tint(Kirigami.Theme.backgroundColor, Qt.rgba(Kirigami.Theme.textColor.r, Kirigami.Theme.textColor.g, Kirigami.Theme.textColor.b, 0.08))
    readonly property color surfaceCard: Qt.rgba(Kirigami.Theme.textColor.r, Kirigami.Theme.textColor.g, Kirigami.Theme.textColor.b, 0.06)
    readonly property color surfaceCardElevated: Qt.rgba(Kirigami.Theme.textColor.r, Kirigami.Theme.textColor.g, Kirigami.Theme.textColor.b, 0.09)

    // Interactive states
    readonly property color cardBackground: Qt.rgba(Kirigami.Theme.textColor.r, Kirigami.Theme.textColor.g, Kirigami.Theme.textColor.b, 0.06)
    readonly property color hoverBackground: Qt.rgba(Kirigami.Theme.textColor.r, Kirigami.Theme.textColor.g, Kirigami.Theme.textColor.b, 0.08)
    readonly property color activeBackground: Qt.rgba(Kirigami.Theme.highlightColor.r, Kirigami.Theme.highlightColor.g, Kirigami.Theme.highlightColor.b, 0.18)
    readonly property color inputBackground: Qt.rgba(Kirigami.Theme.textColor.r, Kirigami.Theme.textColor.g, Kirigami.Theme.textColor.b, 0.06)

    // Borders with distinct physical weight & contrast
    readonly property color borderColor: Qt.rgba(Kirigami.Theme.textColor.r, Kirigami.Theme.textColor.g, Kirigami.Theme.textColor.b, 0.12)
    readonly property color subtleBorderColor: Qt.rgba(Kirigami.Theme.textColor.r, Kirigami.Theme.textColor.g, Kirigami.Theme.textColor.b, 0.06)
    readonly property color strongBorderColor: Qt.rgba(Kirigami.Theme.textColor.r, Kirigami.Theme.textColor.g, Kirigami.Theme.textColor.b, 0.20)
    readonly property color popupBorderColor: Qt.rgba(Kirigami.Theme.textColor.r, Kirigami.Theme.textColor.g, Kirigami.Theme.textColor.b, 0.18)
    readonly property color shadowColor: Qt.rgba(0, 0, 0, 0.40)

    // Git specific semantic colors (vibrant & accessible)
    readonly property color additionColor: Kirigami.Theme.positiveTextColor
    readonly property color additionBg: Qt.rgba(Kirigami.Theme.positiveTextColor.r, Kirigami.Theme.positiveTextColor.g, Kirigami.Theme.positiveTextColor.b, 0.18)
    readonly property color additionGutterBg: Qt.rgba(Kirigami.Theme.positiveTextColor.r, Kirigami.Theme.positiveTextColor.g, Kirigami.Theme.positiveTextColor.b, 0.30)

    readonly property color deletionColor: Kirigami.Theme.negativeTextColor
    readonly property color deletionBg: Qt.rgba(Kirigami.Theme.negativeTextColor.r, Kirigami.Theme.negativeTextColor.g, Kirigami.Theme.negativeTextColor.b, 0.18)
    readonly property color deletionGutterBg: Qt.rgba(Kirigami.Theme.negativeTextColor.r, Kirigami.Theme.negativeTextColor.g, Kirigami.Theme.negativeTextColor.b, 0.30)

    readonly property color modifiedColor: "#e5a50a"
    readonly property color modifiedBg: Qt.rgba(0.9, 0.65, 0.04, 0.18)

    readonly property color hunkHeaderBg: Qt.rgba(Kirigami.Theme.highlightColor.r, Kirigami.Theme.highlightColor.g, Kirigami.Theme.highlightColor.b, 0.14)
    readonly property color hunkHeaderColor: Kirigami.Theme.highlightColor
    readonly property color gutterBg: Qt.rgba(Kirigami.Theme.textColor.r, Kirigami.Theme.textColor.g, Kirigami.Theme.textColor.b, 0.05)

    // Line height for diff lines
    readonly property int diffLineHeight: 26
    readonly property int diffGutterWidth: 52

    // Spacing & Radii
    readonly property int radiusSmall: 5
    readonly property int radiusMedium: 8
    readonly property int radiusLarge: 10
    readonly property int radiusRound: 100
    readonly property int headerHeight: 56
    readonly property int sidebarWidth: 380

    // Dynamic font metrics with robust fallbacks
    readonly property int basePixelSize: (Kirigami.Theme.defaultFont && Kirigami.Theme.defaultFont.pixelSize > 0) ? Kirigami.Theme.defaultFont.pixelSize : 13

    readonly property font codeFont: Qt.font({
        family: "Monospace, Source Code Pro, Hack, JetBrains Mono, Fira Code, monospace",
        pixelSize: basePixelSize - 1
    })

    readonly property font codeFontBold: Qt.font({
        family: "Monospace, Source Code Pro, Hack, JetBrains Mono, Fira Code, monospace",
        pixelSize: basePixelSize - 1,
        bold: true
    })

    readonly property font boldFont: Qt.font({
        family: Kirigami.Theme.defaultFont ? Kirigami.Theme.defaultFont.family : "sans-serif",
        pixelSize: basePixelSize,
        bold: true
    })

    readonly property font smallFont: Qt.font({
        family: Kirigami.Theme.defaultFont ? Kirigami.Theme.defaultFont.family : "sans-serif",
        pixelSize: Math.max(9, basePixelSize - 2)
    })
}
