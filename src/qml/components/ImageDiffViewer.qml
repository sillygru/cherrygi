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

    readonly property bool hasOld: root.oldImageUrl !== "" && root.oldSize > 0
    readonly property bool hasNew: root.newImageUrl !== "" && root.newSize > 0

    function formatFileSize(bytes) {
        if (bytes <= 0) return qsTr("0 B");
        var units = ["B", "KB", "MB", "GB"];
        var i = Math.floor(Math.log(bytes) / Math.log(1024));
        if (i < 0) i = 0;
        if (i >= units.length) i = units.length - 1;
        var val = (bytes / Math.pow(1024, i)).toFixed(1);
        return val + " " + units[i];
    }

    function formatSizeDiff() {
        if (!hasOld || !hasNew) return "";
        var diff = root.newSize - root.oldSize;
        if (diff === 0) return qsTr("0 B change");
        var pct = ((diff / root.oldSize) * 100).toFixed(1);
        var sign = diff > 0 ? "+" : "";
        return sign + formatFileSize(Math.abs(diff)) + " (" + sign + pct + "%)";
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
                                text: root.hasOld ? formatFileSize(root.oldSize) : qsTr("No previous version")
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
                            source: root.hasOld ? root.oldImageUrl : ""
                            fillMode: root.actualSize ? Image.Pad : Image.PreserveAspectFit
                            width: root.actualSize ? sourceSize.width : Math.min(parent.width - 20, sourceSize.width)
                            height: root.actualSize ? sourceSize.height : Math.min(parent.height - 20, sourceSize.height)
                            smooth: true
                            asynchronous: true
                            visible: root.hasOld && status === Image.Ready
                        }

                        // Placeholder if no old version (New file)
                        ColumnLayout {
                            anchors.centerIn: parent
                            visible: !root.hasOld || oldImage.status !== Image.Ready
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
                                text: (root.hasOld && root.oldDimensions.length > 0) ? root.oldDimensions : qsTr("No dimension data")
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
                                text: root.hasNew ? formatFileSize(root.newSize) : qsTr("Deleted file")
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
                            source: root.hasNew ? root.newImageUrl : ""
                            fillMode: root.actualSize ? Image.Pad : Image.PreserveAspectFit
                            width: root.actualSize ? sourceSize.width : Math.min(parent.width - 20, sourceSize.width)
                            height: root.actualSize ? sourceSize.height : Math.min(parent.height - 20, sourceSize.height)
                            smooth: true
                            asynchronous: true
                            visible: root.hasNew && status === Image.Ready
                        }

                        // Placeholder if file deleted
                        ColumnLayout {
                            anchors.centerIn: parent
                            visible: !root.hasNew || newImage.status !== Image.Ready
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
                                text: (root.hasNew && root.newDimensions.length > 0) ? root.newDimensions : qsTr("No dimension data")
                                font.pixelSize: CherryStyle.smallFont.pixelSize - 1
                                color: Kirigami.Theme.disabledTextColor
                            }

                            Item { Layout.fillWidth: true }

                            QQC2.Label {
                                visible: root.hasOld && root.hasNew
                                text: formatSizeDiff()
                                font.pixelSize: CherryStyle.smallFont.pixelSize - 1
                                color: (root.newSize > root.oldSize) ? CherryStyle.additionColor : ((root.newSize < root.oldSize) ? CherryStyle.deletionColor : Kirigami.Theme.disabledTextColor)
                            }
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
            property real naturalWidth: Math.max(swipeOldImg.sourceSize.width, swipeNewImg.sourceSize.width)
            property real naturalHeight: Math.max(swipeOldImg.sourceSize.height, swipeNewImg.sourceSize.height)
            property real fitScale: (naturalWidth > 0 && naturalHeight > 0) ? Math.min((width - 40) / naturalWidth, (height - 60) / naturalHeight, 1.0) : 1.0
            property real displayWidth: root.actualSize ? naturalWidth : naturalWidth * fitScale
            property real displayHeight: root.actualSize ? naturalHeight : naturalHeight * fitScale

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

            // Notice badge when file is added or deleted
            Rectangle {
                anchors.top: parent.top
                anchors.horizontalCenter: parent.horizontalCenter
                anchors.topMargin: 8
                visible: !root.hasOld || !root.hasNew
                radius: CherryStyle.radiusSmall
                implicitHeight: 28
                implicitWidth: singleNoticeLbl.implicitWidth + 24
                color: Qt.rgba(0, 0, 0, 0.75)
                border.color: CherryStyle.borderColor
                border.width: 1
                z: 10

                RowLayout {
                    anchors.centerIn: parent
                    spacing: 6
                    Kirigami.Icon {
                        source: root.hasNew ? "list-add" : "edit-delete"
                        width: 14
                        height: 14
                        color: root.hasNew ? CherryStyle.additionColor : CherryStyle.deletionColor
                    }
                    QQC2.Label {
                        id: singleNoticeLbl
                        text: root.hasNew ? qsTr("Newly added file (Single image view)") : qsTr("Deleted file (Single image view)")
                        font.pixelSize: CherryStyle.smallFont.pixelSize
                        font.bold: true
                        color: root.hasNew ? CherryStyle.additionColor : CherryStyle.deletionColor
                    }
                }
            }

            // Image Frame Box
            Item {
                id: swipeImageBox
                anchors.centerIn: parent
                width: Math.max(1, swipeContainer.displayWidth)
                height: Math.max(1, swipeContainer.displayHeight)
                visible: (root.hasOld && swipeOldImg.status === Image.Ready) || (root.hasNew && swipeNewImg.status === Image.Ready)

                // Old Image (Underneath)
                Image {
                    id: swipeOldImg
                    anchors.fill: parent
                    source: root.hasOld ? root.oldImageUrl : ""
                    fillMode: Image.PreserveAspectFit
                    smooth: true
                    asynchronous: true
                    visible: root.hasOld
                }

                // New Image (Clipped on top from the left)
                Item {
                    anchors.top: parent.top
                    anchors.bottom: parent.bottom
                    anchors.left: parent.left
                    width: root.hasOld && root.hasNew ? (parent.width * swipeContainer.swipeSplit) : parent.width
                    clip: true
                    visible: root.hasNew

                    Image {
                        id: swipeNewImg
                        x: 0
                        y: 0
                        width: swipeImageBox.width
                        height: swipeImageBox.height
                        source: root.hasNew ? root.newImageUrl : ""
                        fillMode: Image.PreserveAspectFit
                        smooth: true
                        asynchronous: true
                    }
                }

                // Interactive Divider Line (only when both versions exist)
                Rectangle {
                    id: swipeDivider
                    visible: root.hasOld && root.hasNew
                    x: parent.width * swipeContainer.swipeSplit - 1.5
                    y: 0
                    width: 3
                    height: parent.height
                    color: Kirigami.Theme.highlightColor
                    z: 5

                    // Drag Pill Handle
                    Rectangle {
                        anchors.centerIn: parent
                        width: 28
                        height: 28
                        radius: 14
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

                // Mouse interaction for dragging swipe divider
                MouseArea {
                    anchors.fill: parent
                    enabled: root.hasOld && root.hasNew
                    cursorShape: Qt.SplitHCursor
                    onPositionChanged: (mouse) => {
                        if (pressed && width > 0) {
                            var split = Math.max(0.0, Math.min(1.0, mouse.x / width));
                            swipeContainer.swipeSplit = split;
                        }
                    }
                    onPressed: (mouse) => {
                        if (width > 0) {
                            var split = Math.max(0.0, Math.min(1.0, mouse.x / width));
                            swipeContainer.swipeSplit = split;
                        }
                    }
                }

                // Badges indicating sides (only when both versions exist)
                Rectangle {
                    anchors.left: parent.left
                    anchors.top: parent.top
                    anchors.margins: 8
                    radius: 4
                    visible: root.hasOld && root.hasNew
                    implicitWidth: newBadgeLbl.implicitWidth + 12
                    implicitHeight: 22
                    color: Qt.rgba(0, 0, 0, 0.75)
                    z: 6
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
                    anchors.right: parent.right
                    anchors.top: parent.top
                    anchors.margins: 8
                    radius: 4
                    visible: root.hasOld && root.hasNew
                    implicitWidth: oldBadgeLbl.implicitWidth + 12
                    implicitHeight: 22
                    color: Qt.rgba(0, 0, 0, 0.75)
                    z: 6
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
            property real naturalWidth: Math.max(onionOldImg.sourceSize.width, onionNewImg.sourceSize.width)
            property real naturalHeight: Math.max(onionOldImg.sourceSize.height, onionNewImg.sourceSize.height)
            property real fitScale: (naturalWidth > 0 && naturalHeight > 0) ? Math.min((width - 40) / naturalWidth, (height - 100) / naturalHeight, 1.0) : 1.0
            property real displayWidth: root.actualSize ? naturalWidth : naturalWidth * fitScale
            property real displayHeight: root.actualSize ? naturalHeight : naturalHeight * fitScale

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

            // Notice badge when file is added or deleted
            Rectangle {
                anchors.top: parent.top
                anchors.horizontalCenter: parent.horizontalCenter
                anchors.topMargin: 8
                visible: !root.hasOld || !root.hasNew
                radius: CherryStyle.radiusSmall
                implicitHeight: 28
                implicitWidth: singleOnionLbl.implicitWidth + 24
                color: Qt.rgba(0, 0, 0, 0.75)
                border.color: CherryStyle.borderColor
                border.width: 1
                z: 10

                RowLayout {
                    anchors.centerIn: parent
                    spacing: 6
                    Kirigami.Icon {
                        source: root.hasNew ? "list-add" : "edit-delete"
                        width: 14
                        height: 14
                        color: root.hasNew ? CherryStyle.additionColor : CherryStyle.deletionColor
                    }
                    QQC2.Label {
                        id: singleOnionLbl
                        text: root.hasNew ? qsTr("Newly added file (Single image view)") : qsTr("Deleted file (Single image view)")
                        font.pixelSize: CherryStyle.smallFont.pixelSize
                        font.bold: true
                        color: root.hasNew ? CherryStyle.additionColor : CherryStyle.deletionColor
                    }
                }
            }

            // Image Frame Box
            Item {
                id: onionImageBox
                anchors.centerIn: parent
                anchors.verticalCenterOffset: root.hasOld && root.hasNew ? -20 : 0
                width: Math.max(1, onionContainer.displayWidth)
                height: Math.max(1, onionContainer.displayHeight)
                visible: (root.hasOld && onionOldImg.status === Image.Ready) || (root.hasNew && onionNewImg.status === Image.Ready)

                // Old Image (Base layer)
                Image {
                    id: onionOldImg
                    anchors.fill: parent
                    source: root.hasOld ? root.oldImageUrl : ""
                    fillMode: Image.PreserveAspectFit
                    smooth: true
                    asynchronous: true
                    visible: root.hasOld
                }

                // New Image (Overlaid with adjustable opacity)
                Image {
                    id: onionNewImg
                    anchors.fill: parent
                    source: root.hasNew ? root.newImageUrl : ""
                    fillMode: Image.PreserveAspectFit
                    opacity: root.hasOld ? onionContainer.opacityLevel : 1.0
                    smooth: true
                    asynchronous: true
                    visible: root.hasNew
                }
            }

            // Floating Opacity Slider Pill at Bottom (only when both images exist)
            Rectangle {
                anchors.bottom: parent.bottom
                anchors.horizontalCenter: parent.horizontalCenter
                anchors.bottomMargin: 16
                visible: root.hasOld && root.hasNew
                implicitWidth: 340
                implicitHeight: 44
                radius: 22
                color: CherryStyle.surfaceHeader
                border.color: CherryStyle.borderColor
                border.width: 1
                z: 10

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
