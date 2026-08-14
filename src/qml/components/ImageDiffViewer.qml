import QtQuick
import QtQuick.Controls as QQC2
import QtQuick.Layouts
import org.kde.kirigami as Kirigami
import org.kde.cherrygi
import "../style"

Item {
    id: root

    property string oldImageUrl: appController.diffModel.oldImageUrl
    property string newImageUrl: appController.diffModel.newImageUrl
    property string oldDimensions: appController.diffModel.oldImageDimensions
    property string newDimensions: appController.diffModel.newImageDimensions
    property real oldSize: appController.diffModel.oldImageSize
    property real newSize: appController.diffModel.newImageSize
    property string diffMode: appController.diffModel.imageDiffMode // "2-up", "swipe", "onion"
    property bool actualSize: false

    function formatFileSize(bytes) {
        if (bytes <= 0) return qsTr("0 B");
        var units = ["B", "KB", "MB", "GB"];
        var i = Math.floor(Math.log(bytes) / Math.log(1024));
        if (i < 0) i = 0;
        if (i >= units.length) i = units.length - 1;
        var val = (bytes / Math.pow(1024, i)).toFixed(1);
        return val + " " + units[i];
    }

    // Main Container
    Rectangle {
        anchors.fill: parent
        color: Kirigami.Theme.backgroundColor

        // =================================================================
        // 2-UP MODE (Side-by-Side)
        // =================================================================
        RowLayout {
            anchors.fill: parent
            anchors.margins: Kirigami.Units.mediumSpacing
            spacing: Kirigami.Units.mediumSpacing
            visible: root.diffMode === "2-up"

            // Old Image Card
            Rectangle {
                Layout.fillWidth: true
                Layout.fillHeight: true
                radius: 6
                color: CherryStyle.surfaceBackground
                border.color: CherryStyle.borderColor
                border.width: 1
                clip: true

                ColumnLayout {
                    anchors.fill: parent
                    spacing: 0

                    // Card Header
                    Rectangle {
                        Layout.fillWidth: true
                        implicitHeight: 32
                        color: Qt.rgba(CherryStyle.deletionColor.r, CherryStyle.deletionColor.g, CherryStyle.deletionColor.b, 0.12)
                        border.color: Qt.rgba(CherryStyle.deletionColor.r, CherryStyle.deletionColor.g, CherryStyle.deletionColor.b, 0.3)
                        border.width: 1

                        RowLayout {
                            anchors.fill: parent
                            anchors.leftMargin: Kirigami.Units.smallSpacing + 2
                            anchors.rightMargin: Kirigami.Units.smallSpacing + 2

                            Kirigami.Icon {
                                source: "edit-delete"
                                width: 14
                                height: 14
                                color: CherryStyle.deletionColor
                            }

                            QQC2.Label {
                                text: qsTr("Previous Version (Old)")
                                font.bold: true
                                font.pixelSize: CherryStyle.smallFont.pixelSize
                                color: CherryStyle.deletionColor
                            }

                            Item { Layout.fillWidth: true }

                            QQC2.Label {
                                text: (root.oldSize > 0) ? formatFileSize(root.oldSize) : qsTr("Not in HEAD")
                                font.pixelSize: CherryStyle.smallFont.pixelSize - 1
                                color: Kirigami.Theme.disabledTextColor
                            }
                        }
                    }

                    // Canvas Container
                    Item {
                        Layout.fillWidth: true
                        Layout.fillHeight: true
                        clip: true

                        Canvas {
                            anchors.fill: parent
                            onPaint: {
                                var ctx = getContext("2d");
                                var size = 16;
                                ctx.fillStyle = Kirigami.Theme.backgroundColor;
                                ctx.fillRect(0, 0, width, height);
                                ctx.fillStyle = Qt.rgba(Kirigami.Theme.textColor.r, Kirigami.Theme.textColor.g, Kirigami.Theme.textColor.b, 0.06);
                                for (var x = 0; x < width; x += size) {
                                    for (var y = 0; y < height; y += size) {
                                        if ((Math.floor(x / size) + Math.floor(y / size)) % 2 === 1) {
                                            ctx.fillRect(x, y, size, size);
                                        }
                                    }
                                }
                            }
                            onWidthChanged: requestPaint()
                            onHeightChanged: requestPaint()
                        }

                        Image {
                            id: oldImage
                            anchors.centerIn: parent
                            source: root.oldImageUrl
                            fillMode: root.actualSize ? Image.Pad : Image.PreserveAspectFit
                            width: root.actualSize ? implicitWidth : Math.min(parent.width - 20, implicitWidth)
                            height: root.actualSize ? implicitHeight : Math.min(parent.height - 20, implicitHeight)
                            smooth: true
                            asynchronous: true
                            visible: status === Image.Ready
                        }

                        // Placeholder if no old version (New file)
                        ColumnLayout {
                            anchors.centerIn: parent
                            visible: oldImage.status !== Image.Ready
                            spacing: Kirigami.Units.smallSpacing

                            Kirigami.Icon {
                                source: "list-add"
                                width: 40
                                height: 40
                                Layout.alignment: Qt.AlignHCenter
                                color: Kirigami.Theme.disabledTextColor
                            }

                            QQC2.Label {
                                text: qsTr("No previous version (Added file)")
                                font.pixelSize: CherryStyle.smallFont.pixelSize
                                color: Kirigami.Theme.disabledTextColor
                                Layout.alignment: Qt.AlignHCenter
                            }
                        }
                    }

                    // Card Footer
                    Rectangle {
                        Layout.fillWidth: true
                        implicitHeight: 28
                        color: CherryStyle.surfaceHeader
                        border.color: CherryStyle.borderColor
                        border.width: 1

                        RowLayout {
                            anchors.fill: parent
                            anchors.leftMargin: Kirigami.Units.smallSpacing + 2
                            anchors.rightMargin: Kirigami.Units.smallSpacing + 2

                            QQC2.Label {
                                text: root.oldDimensions.length > 0 ? root.oldDimensions : qsTr("No dimension data")
                                font.pixelSize: CherryStyle.smallFont.pixelSize - 1
                                color: Kirigami.Theme.disabledTextColor
                            }

                            Item { Layout.fillWidth: true }
                        }
                    }
                }
            }

            // New Image Card
            Rectangle {
                Layout.fillWidth: true
                Layout.fillHeight: true
                radius: 6
                color: CherryStyle.surfaceBackground
                border.color: CherryStyle.borderColor
                border.width: 1
                clip: true

                ColumnLayout {
                    anchors.fill: parent
                    spacing: 0

                    // Card Header
                    Rectangle {
                        Layout.fillWidth: true
                        implicitHeight: 32
                        color: Qt.rgba(CherryStyle.additionColor.r, CherryStyle.additionColor.g, CherryStyle.additionColor.b, 0.12)
                        border.color: Qt.rgba(CherryStyle.additionColor.r, CherryStyle.additionColor.g, CherryStyle.additionColor.b, 0.3)
                        border.width: 1

                        RowLayout {
                            anchors.fill: parent
                            anchors.leftMargin: Kirigami.Units.smallSpacing + 2
                            anchors.rightMargin: Kirigami.Units.smallSpacing + 2

                            Kirigami.Icon {
                                source: "list-add"
                                width: 14
                                height: 14
                                color: CherryStyle.additionColor
                            }

                            QQC2.Label {
                                text: qsTr("Current Version (New)")
                                font.bold: true
                                font.pixelSize: CherryStyle.smallFont.pixelSize
                                color: CherryStyle.additionColor
                            }

                            Item { Layout.fillWidth: true }

                            QQC2.Label {
                                text: (root.newSize > 0) ? formatFileSize(root.newSize) : qsTr("Deleted file")
                                font.pixelSize: CherryStyle.smallFont.pixelSize - 1
                                color: Kirigami.Theme.disabledTextColor
                            }
                        }
                    }

                    // Canvas Container
                    Item {
                        Layout.fillWidth: true
                        Layout.fillHeight: true
                        clip: true

                        Canvas {
                            anchors.fill: parent
                            onPaint: {
                                var ctx = getContext("2d");
                                var size = 16;
                                ctx.fillStyle = Kirigami.Theme.backgroundColor;
                                ctx.fillRect(0, 0, width, height);
                                ctx.fillStyle = Qt.rgba(Kirigami.Theme.textColor.r, Kirigami.Theme.textColor.g, Kirigami.Theme.textColor.b, 0.06);
                                for (var x = 0; x < width; x += size) {
                                    for (var y = 0; y < height; y += size) {
                                        if ((Math.floor(x / size) + Math.floor(y / size)) % 2 === 1) {
                                            ctx.fillRect(x, y, size, size);
                                        }
                                    }
                                }
                            }
                            onWidthChanged: requestPaint()
                            onHeightChanged: requestPaint()
                        }

                        Image {
                            id: newImage
                            anchors.centerIn: parent
                            source: root.newImageUrl
                            fillMode: root.actualSize ? Image.Pad : Image.PreserveAspectFit
                            width: root.actualSize ? implicitWidth : Math.min(parent.width - 20, implicitWidth)
                            height: root.actualSize ? implicitHeight : Math.min(parent.height - 20, implicitHeight)
                            smooth: true
                            asynchronous: true
                            visible: status === Image.Ready
                        }

                        // Placeholder if file deleted
                        ColumnLayout {
                            anchors.centerIn: parent
                            visible: newImage.status !== Image.Ready
                            spacing: Kirigami.Units.smallSpacing

                            Kirigami.Icon {
                                source: "edit-delete"
                                width: 40
                                height: 40
                                Layout.alignment: Qt.AlignHCenter
                                color: Kirigami.Theme.disabledTextColor
                            }

                            QQC2.Label {
                                text: qsTr("File was removed (Deleted)")
                                font.pixelSize: CherryStyle.smallFont.pixelSize
                                color: Kirigami.Theme.disabledTextColor
                                Layout.alignment: Qt.AlignHCenter
                            }
                        }
                    }

                    // Card Footer
                    Rectangle {
                        Layout.fillWidth: true
                        implicitHeight: 28
                        color: CherryStyle.surfaceHeader
                        border.color: CherryStyle.borderColor
                        border.width: 1

                        RowLayout {
                            anchors.fill: parent
                            anchors.leftMargin: Kirigami.Units.smallSpacing + 2
                            anchors.rightMargin: Kirigami.Units.smallSpacing + 2

                            QQC2.Label {
                                text: root.newDimensions.length > 0 ? root.newDimensions : qsTr("No dimension data")
                                font.pixelSize: CherryStyle.smallFont.pixelSize - 1
                                color: Kirigami.Theme.disabledTextColor
                            }

                            Item { Layout.fillWidth: true }
                        }
                    }
                }
            }
        }

        // =================================================================
        // SWIPE MODE
        // =================================================================
        Item {
            id: swipeContainer
            anchors.fill: parent
            anchors.margins: Kirigami.Units.mediumSpacing
            visible: root.diffMode === "swipe"

            property real swipeSplit: 0.5

            Canvas {
                anchors.fill: parent
                onPaint: {
                    var ctx = getContext("2d");
                    var size = 16;
                    ctx.fillStyle = Kirigami.Theme.backgroundColor;
                    ctx.fillRect(0, 0, width, height);
                    ctx.fillStyle = Qt.rgba(Kirigami.Theme.textColor.r, Kirigami.Theme.textColor.g, Kirigami.Theme.textColor.b, 0.06);
                    for (var x = 0; x < width; x += size) {
                        for (var y = 0; y < height; y += size) {
                            if ((Math.floor(x / size) + Math.floor(y / size)) % 2 === 1) {
                                ctx.fillRect(x, y, size, size);
                            }
                        }
                    }
                }
                onWidthChanged: requestPaint()
                onHeightChanged: requestPaint()
            }

            // Old Image (Underneath)
            Image {
                id: swipeOldImg
                anchors.centerIn: parent
                source: root.oldImageUrl
                fillMode: Image.PreserveAspectFit
                width: Math.min(parent.width - 40, implicitWidth)
                height: Math.min(parent.height - 40, implicitHeight)
                smooth: true
                asynchronous: true
            }

            // New Image (Clipped on top)
            Item {
                anchors.top: swipeOldImg.top
                anchors.bottom: swipeOldImg.bottom
                anchors.left: swipeOldImg.left
                width: swipeOldImg.width * swipeContainer.swipeSplit
                clip: true

                Image {
                    id: swipeNewImg
                    x: 0
                    y: 0
                    width: swipeOldImg.width
                    height: swipeOldImg.height
                    source: root.newImageUrl
                    fillMode: Image.PreserveAspectFit
                    smooth: true
                    asynchronous: true
                }
            }

            // Interactive Divider Line
            Rectangle {
                id: swipeDivider
                x: swipeOldImg.x + swipeOldImg.width * swipeContainer.swipeSplit - 1.5
                y: swipeOldImg.y
                width: 3
                height: swipeOldImg.height
                color: Kirigami.Theme.highlightColor

                // Drag Pill Handle
                Rectangle {
                    anchors.centerIn: parent
                    width: 26
                    height: 26
                    radius: 13
                    color: Kirigami.Theme.highlightColor
                    border.color: Kirigami.Theme.highlightedTextColor
                    border.width: 2

                    Kirigami.Icon {
                        anchors.centerIn: parent
                        source: "view-split-left-right"
                        width: 14
                        height: 14
                        color: Kirigami.Theme.highlightedTextColor
                    }
                }
            }

            MouseArea {
                anchors.fill: swipeOldImg
                cursorShape: Qt.SplitHCursor
                onPositionChanged: (mouse) => {
                    if (pressed && swipeOldImg.width > 0) {
                        var split = Math.max(0.0, Math.min(1.0, mouse.x / swipeOldImg.width));
                        swipeContainer.swipeSplit = split;
                    }
                }
                onPressed: (mouse) => {
                    if (swipeOldImg.width > 0) {
                        var split = Math.max(0.0, Math.min(1.0, mouse.x / swipeOldImg.width));
                        swipeContainer.swipeSplit = split;
                    }
                }
            }

            // Badges indicating sides
            Rectangle {
                anchors.left: swipeOldImg.left
                anchors.top: swipeOldImg.top
                anchors.margins: 8
                radius: 4
                implicitWidth: newBadgeLbl.implicitWidth + 12
                implicitHeight: 22
                color: Qt.rgba(0, 0, 0, 0.75)
                QQC2.Label {
                    id: newBadgeLbl
                    anchors.centerIn: parent
                    text: qsTr("New (Left)")
                    font.bold: true
                    font.pixelSize: 11
                    color: CherryStyle.additionColor
                }
            }

            Rectangle {
                anchors.right: swipeOldImg.right
                anchors.top: swipeOldImg.top
                anchors.margins: 8
                radius: 4
                implicitWidth: oldBadgeLbl.implicitWidth + 12
                implicitHeight: 22
                color: Qt.rgba(0, 0, 0, 0.75)
                QQC2.Label {
                    id: oldBadgeLbl
                    anchors.centerIn: parent
                    text: qsTr("Old (Right)")
                    font.bold: true
                    font.pixelSize: 11
                    color: CherryStyle.deletionColor
                }
            }
        }

        // =================================================================
        // ONION SKIN MODE
        // =================================================================
        Item {
            id: onionContainer
            anchors.fill: parent
            anchors.margins: Kirigami.Units.mediumSpacing
            visible: root.diffMode === "onion"

            property real opacityLevel: 0.5

            Canvas {
                anchors.fill: parent
                onPaint: {
                    var ctx = getContext("2d");
                    var size = 16;
                    ctx.fillStyle = Kirigami.Theme.backgroundColor;
                    ctx.fillRect(0, 0, width, height);
                    ctx.fillStyle = Qt.rgba(Kirigami.Theme.textColor.r, Kirigami.Theme.textColor.g, Kirigami.Theme.textColor.b, 0.06);
                    for (var x = 0; x < width; x += size) {
                        for (var y = 0; y < height; y += size) {
                            if ((Math.floor(x / size) + Math.floor(y / size)) % 2 === 1) {
                                ctx.fillRect(x, y, size, size);
                            }
                        }
                    }
                }
                onWidthChanged: requestPaint()
                onHeightChanged: requestPaint()
            }

            // Old Image (Base layer)
            Image {
                id: onionOldImg
                anchors.centerIn: parent
                source: root.oldImageUrl
                fillMode: Image.PreserveAspectFit
                width: Math.min(parent.width - 40, implicitWidth)
                height: Math.min(parent.height - 80, implicitHeight)
                smooth: true
                asynchronous: true
            }

            // New Image (Overlaid with adjustable opacity)
            Image {
                id: onionNewImg
                anchors.centerIn: parent
                source: root.newImageUrl
                fillMode: Image.PreserveAspectFit
                width: onionOldImg.width
                height: onionOldImg.height
                opacity: onionContainer.opacityLevel
                smooth: true
                asynchronous: true
            }

            // Floating Opacity Slider Pill at Bottom
            Rectangle {
                anchors.bottom: parent.bottom
                anchors.horizontalCenter: parent.horizontalCenter
                anchors.bottomMargin: 16
                implicitWidth: 340
                implicitHeight: 44
                radius: 22
                color: CherryStyle.surfaceHeader
                border.color: CherryStyle.borderColor
                border.width: 1

                RowLayout {
                    anchors.fill: parent
                    anchors.margins: 8
                    spacing: Kirigami.Units.smallSpacing

                    QQC2.Label {
                        text: qsTr("Old")
                        font.bold: true
                        font.pixelSize: CherryStyle.smallFont.pixelSize
                        color: CherryStyle.deletionColor
                    }

                    QQC2.Slider {
                        Layout.fillWidth: true
                        from: 0.0
                        to: 1.0
                        value: onionContainer.opacityLevel
                        onMoved: onionContainer.opacityLevel = value
                    }

                    QQC2.Label {
                        text: qsTr("New")
                        font.bold: true
                        font.pixelSize: CherryStyle.smallFont.pixelSize
                        color: CherryStyle.additionColor
                    }

                    QQC2.Label {
                        text: Math.round(onionContainer.opacityLevel * 100) + "%"
                        font.pixelSize: CherryStyle.smallFont.pixelSize - 1
                        color: Kirigami.Theme.disabledTextColor
                        Layout.minimumWidth: 35
                    }
                }
            }
        }
    }
}
