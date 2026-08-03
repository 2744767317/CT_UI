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
    property bool mip: false
    property bool showSegmentation: true
    property real cropMinimum: 0.0
    property real cropMaximum: 1.0
    property real slicePosition: sliceSlider.value
    property point measureStart: Qt.point(-1, -1)
    property point measureEnd: Qt.point(-1, -1)
    property real measuredDistance: 0.0

    color: Theme.image
    border.width: 1
    border.color: activeArea.containsMouse ? root.viewColor : Theme.border
    clip: true

    MedicalViewport {
        id: viewport
        anchors.fill: parent
        anchors.margins: 1
        viewType: root.viewType
        controller: medicalData
        slicePosition: root.slicePosition
        mip: root.mip
        showSegmentation: root.showSegmentation
        cropMinimum: root.cropMinimum
        cropMaximum: root.cropMaximum
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

    Text {
        anchors.right: parent.right
        anchors.top: parent.top
        anchors.margins: 12
        text: medicalData.loaded
              ? (root.viewType === MedicalViewport.Volume3D
                 ? (root.mip ? "MIP" : "VOLUME")
                 : "WW " + Math.round(medicalData.windowWidth) + "  WL " + Math.round(medicalData.windowLevel))
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
        enabled: root.toolMode === "测量" && root.viewType !== MedicalViewport.Volume3D
        preventStealing: true
        onPressed: mouse => {
            root.measureStart = Qt.point(mouse.x, mouse.y)
            root.measureEnd = root.measureStart
            measureCanvas.requestPaint()
        }
        onPositionChanged: mouse => {
            if (pressed) {
                root.measureEnd = Qt.point(mouse.x, mouse.y)
                root.measuredDistance = medicalData.estimateDistanceMm(
                    root.viewType,
                    root.measureEnd.x - root.measureStart.x,
                    root.measureEnd.y - root.measureStart.y,
                    width, height)
                measureCanvas.requestPaint()
            }
        }
    }

    Canvas {
        id: measureCanvas
        anchors.fill: parent
        visible: root.measureStart.x >= 0 && root.measureEnd.x >= 0
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
        visible: root.viewType !== MedicalViewport.Volume3D && medicalData.loaded
        anchors.left: parent.left
        anchors.right: parent.right
        anchors.bottom: parent.bottom
        anchors.margins: 10
        from: 0
        to: 1
        value: 0.5
    }
}
