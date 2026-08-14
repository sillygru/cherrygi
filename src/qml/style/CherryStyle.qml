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
    // Theme accents are not always readable on custom Plasma palettes.  Keep the
    // original hue where possible, but move it toward a contrasting endpoint
    // when it would otherwise be too dark/light for the app surface.
    function relativeLuminance(c) {
        return 0.2126 * Math.pow(c.r, 2.2) + 0.7152 * Math.pow(c.g, 2.2) + 0.0722 * Math.pow(c.b, 2.2);
    }

    function contrastRatio(a, b) {
        var aLum = relativeLuminance(a);
        var bLum = relativeLuminance(b);
        var lighter = Math.max(aLum, bLum);
        var darker = Math.min(aLum, bLum);
        return (lighter + 0.05) / (darker + 0.05);
    }

    function contrastSafeColor(candidate, surface, minimumRatio) {
        if (contrastRatio(candidate, surface) >= minimumRatio) return candidate;

        var target = relativeLuminance(surface) > 0.45 ? Qt.rgba(0, 0, 0, 1) : Qt.rgba(1, 1, 1, 1);
        var result = candidate;
        for (var step = 1; step <= 20; ++step) {
            var amount = step / 20.0;
            result = Qt.rgba(candidate.r + (target.r - candidate.r) * amount,
                             candidate.g + (target.g - candidate.g) * amount,
                             candidate.b + (target.b - candidate.b) * amount, 1.0);
            if (contrastRatio(result, surface) >= minimumRatio) return result;
        }
        return target;
    }

    readonly property color highlightColor: contrastSafeColor(Kirigami.Theme.highlightColor, Qt.tint(background, activeBackground), 4.5)
    readonly property color accentColor: highlightColor
    readonly property color highlightedTextColor: Kirigami.Theme.highlightedTextColor
    readonly property color secondaryTextColor: contrastSafeColor(Kirigami.Theme.disabledTextColor, background, 3.0)

    // Surface elevation levels for hierarchy and visual weight
    readonly property color surfaceBackground: Qt.tint(Kirigami.Theme.backgroundColor, Qt.rgba(Kirigami.Theme.textColor.r, Kirigami.Theme.textColor.g, Kirigami.Theme.textColor.b, 0.04))
    readonly property color surfaceHeader: Qt.tint(Kirigami.Theme.backgroundColor, Qt.rgba(Kirigami.Theme.textColor.r, Kirigami.Theme.textColor.g, Kirigami.Theme.textColor.b, 0.08))
    readonly property color surfaceSidebar: Qt.tint(Kirigami.Theme.backgroundColor, Qt.rgba(Kirigami.Theme.textColor.r, Kirigami.Theme.textColor.g, Kirigami.Theme.textColor.b, 0.05))
    readonly property color surfacePopup: Qt.tint(Kirigami.Theme.backgroundColor, Qt.rgba(Kirigami.Theme.textColor.r, Kirigami.Theme.textColor.g, Kirigami.Theme.textColor.b, 0.12))
    readonly property color surfaceCard: Qt.rgba(Kirigami.Theme.textColor.r, Kirigami.Theme.textColor.g, Kirigami.Theme.textColor.b, 0.08)
    readonly property color surfaceCardElevated: Qt.rgba(Kirigami.Theme.textColor.r, Kirigami.Theme.textColor.g, Kirigami.Theme.textColor.b, 0.14)

    // Interactive states
    readonly property color cardBackground: Qt.rgba(Kirigami.Theme.textColor.r, Kirigami.Theme.textColor.g, Kirigami.Theme.textColor.b, 0.08)
    readonly property color hoverBackground: Qt.rgba(Kirigami.Theme.textColor.r, Kirigami.Theme.textColor.g, Kirigami.Theme.textColor.b, 0.12)
    // A neutral selection surface keeps the text readable even when the
    // desktop highlight color is a dark custom accent.
    readonly property color activeBackground: Qt.rgba(Kirigami.Theme.textColor.r, Kirigami.Theme.textColor.g, Kirigami.Theme.textColor.b, 0.14)
    readonly property color inputBackground: Qt.rgba(Kirigami.Theme.textColor.r, Kirigami.Theme.textColor.g, Kirigami.Theme.textColor.b, 0.08)

    // Borders with distinct physical weight & contrast
    readonly property color borderColor: Qt.rgba(Kirigami.Theme.textColor.r, Kirigami.Theme.textColor.g, Kirigami.Theme.textColor.b, 0.20)
    readonly property color subtleBorderColor: Qt.rgba(Kirigami.Theme.textColor.r, Kirigami.Theme.textColor.g, Kirigami.Theme.textColor.b, 0.12)
    readonly property color strongBorderColor: Qt.rgba(Kirigami.Theme.textColor.r, Kirigami.Theme.textColor.g, Kirigami.Theme.textColor.b, 0.30)
    readonly property color popupBorderColor: Qt.rgba(Kirigami.Theme.textColor.r, Kirigami.Theme.textColor.g, Kirigami.Theme.textColor.b, 0.28)
    readonly property color shadowColor: Qt.rgba(0, 0, 0, 0.50)

    // Git-specific semantic colors. The minimum contrast guard is especially
    // important for status icons and badges on dark Plasma themes.
    readonly property color additionColor: contrastSafeColor(Kirigami.Theme.positiveTextColor, background, 3.0)
    readonly property color additionBg: Qt.rgba(additionColor.r, additionColor.g, additionColor.b, 0.24)
    readonly property color additionGutterBg: Qt.rgba(additionColor.r, additionColor.g, additionColor.b, 0.35)

    readonly property color deletionColor: contrastSafeColor(Kirigami.Theme.negativeTextColor, background, 3.0)
    readonly property color deletionBg: Qt.rgba(deletionColor.r, deletionColor.g, deletionColor.b, 0.24)
    readonly property color deletionGutterBg: Qt.rgba(deletionColor.r, deletionColor.g, deletionColor.b, 0.35)

    readonly property color modifiedColor: contrastSafeColor(Kirigami.Theme.neutralTextColor, background, 3.0)
    readonly property color modifiedBg: Qt.rgba(modifiedColor.r, modifiedColor.g, modifiedColor.b, 0.24)

    readonly property color renamedColor: accentColor
    readonly property color renamedBg: Qt.rgba(renamedColor.r, renamedColor.g, renamedColor.b, 0.24)
    readonly property color warningColor: contrastSafeColor(Kirigami.Theme.neutralTextColor, background, 3.0)
    readonly property color warningBg: Qt.rgba(warningColor.r, warningColor.g, warningColor.b, 0.18)
    readonly property color errorTextColor: contrastSafeColor(Kirigami.Theme.textColor, deletionColor, 4.5)

    readonly property color hunkHeaderBg: Qt.rgba(accentColor.r, accentColor.g, accentColor.b, 0.20)
    readonly property color hunkHeaderColor: accentColor
    readonly property color gutterBg: Qt.rgba(Kirigami.Theme.textColor.r, Kirigami.Theme.textColor.g, Kirigami.Theme.textColor.b, 0.08)

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

    readonly property font defaultFont: Qt.font({
        family: Kirigami.Theme.defaultFont ? Kirigami.Theme.defaultFont.family : "sans-serif",
        pixelSize: basePixelSize
    })

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

    readonly property font largeFont: Qt.font({
        family: Kirigami.Theme.defaultFont ? Kirigami.Theme.defaultFont.family : "sans-serif",
        pixelSize: basePixelSize + 3,
        bold: true
    })

    readonly property font titleFont: Qt.font({
        family: Kirigami.Theme.defaultFont ? Kirigami.Theme.defaultFont.family : "sans-serif",
        pixelSize: basePixelSize + 5,
        bold: true
    })

    readonly property font smallFont: Qt.font({
        family: Kirigami.Theme.defaultFont ? Kirigami.Theme.defaultFont.family : "sans-serif",
        pixelSize: Math.max(9, basePixelSize - 2)
    })
}
