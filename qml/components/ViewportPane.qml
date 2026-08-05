import QtQuick
import QtQuick.Controls
import GuangSuo.CT
import GuangSuo.CT.Rendering

Rectangle {
    id: root
    property int viewType: MedicalViewport.Axial
    property string title: "AXIAL"
    property color viewColor: Theme.axial
    property string toolMode: "浏览"
    property int toolModeIndex: 0
    property bool mip: false
    property int volumePreset: MedicalViewport.BonePreset
    property bool showSegmentation: true
    property bool pairedProjection: false
    property bool showImage: true
    property bool showMeasurements: true
    property real segmentationOpacity: 0.72
    property int rotationQuarterTurns: 0
    property bool flipHorizontal: false
    property bool flipVertical: false
    property bool seedPicking: false
    property bool seedMarkerVisible: false
    property bool activeViewport: false
    property real seedMarkerX: 0.5
    property real seedMarkerY: 0.5
    property real cropMinimum: 0.0
    property real cropMaximum: 1.0
    property real slicePosition: sliceSlider.value
    property point measureStart: Qt.point(-1, -1)
    property point measureEnd: Qt.point(-1, -1)
    property real measuredDistance: 0.0
    property point windowStart: Qt.point(0, 0)
    property real windowStartWidth: 400.0
    property real windowStartLevel: 40.0
    signal seedSelected(int viewType, real normalizedX, real normalizedY, real slicePosition)
    signal activated()

    color: Theme.image
    border.width: 1
    border.color: root.activeViewport || activeArea.containsMouse ? root.viewColor : Theme.border
    clip: true

    MedicalViewport {
        id: viewport
        anchors.fill: parent
        anchors.margins: 1
        viewType: root.viewType
        controller: medicalData
        slicePosition: root.slicePosition
        mip: root.mip
        volumePreset: root.volumePreset
        pairedProjection: root.pairedProjection
        showImage: root.showImage
        showSegmentation: root.showSegmentation
        segmentationOpacity: root.segmentationOpacity
        rotationQuarterTurns: root.rotationQuarterTurns
        flipHorizontal: root.flipHorizontal
        flipVertical: root.flipVertical
        cropMinimum: root.cropMinimum
        cropMaximum: root.cropMaximum
        onVoxelPicked: (voxelX, voxelY, voxelZ, hu, normalizedX, normalizedY) => {
            root.seedSelected(root.viewType, normalizedX, normalizedY, root.slicePosition)
        }
        onVoxelPickFailed: message => {
            pickError.text = message
            pickError.visible = true
            pickErrorTimer.restart()
        }
    }

    Rectangle {
        anchors.left: parent.left
        anchors.top: parent.top
        anchors.margins: 10
        width: titleText.implicitWidth + 18
        height: 26
        radius: 3
        color: "#B312171A"
        border.color: root.viewColor
        Text {
            id: titleText
            anchors.centerIn: parent
            text: root.title
            color: root.viewColor
            font.pixelSize: 13
            font.weight: Font.DemiBold
        }
    }

    Rectangle {
        visible: root.activeViewport
        anchors.fill: parent
        anchors.margins: 3
        color: "transparent"
        border.width: 2
        border.color: root.viewColor
        z: 6
    }

    MouseArea {
        anchors.fill: parent
        z: 7
        enabled: medicalData.projectionData && !root.seedPicking
                 && root.toolModeIndex !== 1 && root.toolModeIndex !== 4
        acceptedButtons: Qt.LeftButton
        propagateComposedEvents: true
        onPressed: mouse => {
            root.activated()
            mouse.accepted = false
        }
    }

    Text {
        anchors.right: parent.right
        anchors.top: parent.top
        anchors.margins: 12
        text: medicalData.loaded
              ? (root.viewType === MedicalViewport.Volume3D
                 ? (root.mip ? "MIP" : "VOLUME")
                 : "WW " + Math.round(medicalData.windowWidth) + "  WL " + Math.round(medicalData.displayWindowLevel))
              : "NO DATA"
        color: Theme.textSecondary
        font.pixelSize: 12
    }

    Text {
        visible: !medicalData.loaded
        anchors.centerIn: parent
        text: medicalBackendEnabled
              ? "导入 DICOM CT 或 X 线影像"
              : "MinGW UI 兼容模式\n需要 MinGW 版 VTK / ITK"
        color: Theme.textMuted
        font.pixelSize: 15
        horizontalAlignment: Text.AlignHCenter
        lineHeight: 1.35
    }

    MouseArea {
        id: activeArea
        anchors.fill: parent
        hoverEnabled: true
        enabled: root.viewType !== MedicalViewport.Volume3D
                 && (root.seedPicking || root.toolModeIndex === 1 || root.toolModeIndex === 4)
        preventStealing: true
        cursorShape: root.seedPicking || root.toolModeIndex === 4
                     ? Qt.CrossCursor : Qt.SizeAllCursor
        onPressed: mouse => {
            root.activated()
            if (root.seedPicking) {
                viewport.pickVoxel(mouse.x, mouse.y)
                return
            }
            if (root.toolModeIndex === 1) {
                root.windowStart = Qt.point(mouse.x, mouse.y)
                root.windowStartWidth = medicalData.windowWidth
                root.windowStartLevel = medicalData.windowLevel
                return
            }
            root.measureStart = Qt.point(mouse.x, mouse.y)
            root.measureEnd = root.measureStart
            measureCanvas.requestPaint()
        }
        onPositionChanged: mouse => {
            if (pressed) {
                if (root.toolModeIndex === 1) {
                    const dx = mouse.x - root.windowStart.x
                    const dy = mouse.y - root.windowStart.y
                    medicalData.windowWidth = Math.max(
                        1, root.windowStartWidth * Math.exp(dx / Math.max(1, width) * 2.0))
                    medicalData.windowLevel = root.windowStartLevel
                        - dy / Math.max(1, height) * root.windowStartWidth * 2.0
                    return
                }
                root.measureEnd = Qt.point(mouse.x, mouse.y)
                root.measuredDistance = medicalData.estimateDistanceMm(
                    root.viewType,
                    root.measureEnd.x - root.measureStart.x,
                    root.measureEnd.y - root.measureStart.y,
                    width, height, root.pairedProjection)
                measureCanvas.requestPaint()
            }
        }
    }

    Item {
        visible: root.seedMarkerVisible && medicalData.regionGrowingSeedValid
        x: root.seedMarkerX * root.width - width * 0.5
        y: root.seedMarkerY * root.height - height * 0.5
        width: 34
        height: 34
        z: 5

        Rectangle {
            anchors.centerIn: parent
            width: 2
            height: parent.height
            color: Theme.accent
        }
        Rectangle {
            anchors.centerIn: parent
            width: parent.width
            height: 2
            color: Theme.accent
        }
        Rectangle {
            anchors.centerIn: parent
            width: 10
            height: 10
            radius: 5
            color: "transparent"
            border.width: 2
            border.color: Theme.accent
        }
    }

    Rectangle {
        id: pickError
        visible: false
        anchors.centerIn: parent
        width: Math.min(parent.width - 32, pickErrorText.implicitWidth + 24)
        height: 36
        radius: Theme.radius
        color: "#E61A1F22"
        border.color: Theme.danger
        z: 7
        property alias text: pickErrorText.text
        Text {
            id: pickErrorText
            anchors.centerIn: parent
            color: Theme.text
            font.pixelSize: 13
        }
    }
    Timer {
        id: pickErrorTimer
        interval: 2500
        onTriggered: pickError.visible = false
    }

    Canvas {
        id: measureCanvas
        anchors.fill: parent
        visible: root.showMeasurements && root.measureStart.x >= 0 && root.measureEnd.x >= 0
        onPaint: {
            const ctx = getContext("2d")
            ctx.reset()
            ctx.strokeStyle = Theme.accent
            ctx.fillStyle = Theme.accent
            ctx.lineWidth = 2
            ctx.beginPath()
            ctx.moveTo(root.measureStart.x, root.measureStart.y)
            ctx.lineTo(root.measureEnd.x, root.measureEnd.y)
            ctx.stroke()
            ctx.beginPath()
            ctx.arc(root.measureStart.x, root.measureStart.y, 4, 0, Math.PI * 2)
            ctx.arc(root.measureEnd.x, root.measureEnd.y, 4, 0, Math.PI * 2)
            ctx.fill()
        }
    }

    Rectangle {
        visible: measureCanvas.visible
        anchors.horizontalCenter: parent.horizontalCenter
        anchors.bottom: sliceSlider.visible ? sliceSlider.top : parent.bottom
        anchors.bottomMargin: 10
        height: 28
        width: measurementLabel.implicitWidth + 20
        radius: 3
        color: "#D91A1F22"
        border.color: Theme.accent
        Text {
            id: measurementLabel
            anchors.centerIn: parent
            text: root.measuredDistance.toFixed(1) + " mm"
            color: Theme.text
            font.pixelSize: 13
        }
    }

    Slider {
        id: sliceSlider
        visible: medicalData.volumeData && root.viewType !== MedicalViewport.Volume3D
        anchors.left: parent.left
        anchors.right: parent.right
        anchors.bottom: parent.bottom
        anchors.margins: 10
        from: 0
        to: 1
        value: 0.5
    }
}
