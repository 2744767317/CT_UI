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
    property int measureSubMode: 0
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
    property bool annotationPicking: false
    property bool draggingHandle: false
    property int dragNodeId: -1
    property int dragPointIndex: -1
    signal seedSelected(int viewType, real normalizedX, real normalizedY, real slicePosition)
    signal activated()

    readonly property bool measureMode: root.toolModeIndex === 4
            && root.viewType !== MedicalViewport.Volume3D
            && medicalData.volumeData
    // 与窗宽窗位对称：用 toolModeIndex 直接打开 MouseArea，避免 measureMode 复合条件导致测量时 enabled=false
    readonly property bool captureInput: root.seedPicking
            || root.toolModeIndex === 1
            || root.toolModeIndex === 4

    color: Theme.image
    border.width: 1
    border.color: root.activeViewport || activeArea.containsMouse ? root.viewColor : Theme.border
    clip: true

    onShowMeasurementsChanged: viewport.showAnnotations = root.showMeasurements
    Component.onCompleted: {
        viewport.annotations = annotationController
        viewport.showAnnotations = root.showMeasurements
    }

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
        showAnnotations: root.showMeasurements
        onVoxelPicked: (voxelX, voxelY, voxelZ, hu, normalizedX, normalizedY) => {
            if (root.draggingHandle && root.dragNodeId >= 0) {
                annotationController.updateControlPointFromVoxel(
                            root.dragNodeId, root.dragPointIndex, voxelX, voxelY, voxelZ)
                return
            }
            if (root.annotationPicking) {
                root.annotationPicking = false
                annotationController.addControlPoint(voxelX, voxelY, voxelZ)
                return
            }
            root.seedSelected(root.viewType, normalizedX, normalizedY, root.slicePosition)
        }
        onVoxelPickFailed: message => {
            root.annotationPicking = false
            root.draggingHandle = false
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
                 && root.toolModeIndex !== 1 && !root.measureMode
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
        z: 10
        hoverEnabled: true
        acceptedButtons: Qt.LeftButton | Qt.RightButton
        enabled: root.viewType !== MedicalViewport.Volume3D && root.captureInput
        preventStealing: true
        cursorShape: root.seedPicking || root.toolModeIndex === 4
                     ? Qt.CrossCursor : Qt.SizeAllCursor

        onPressed: mouse => {
            root.activated()
            if (mouse.button === Qt.RightButton) {
                if (root.toolModeIndex === 4
                        && annotationController.toolType === AnnotationTool.PerimeterTool) {
                    annotationController.finishActive()
                }
                return
            }
            if (root.seedPicking) {
                const seedLocal = mapToItem(viewport, mouse.x, mouse.y)
                viewport.mapClickToVoxel(seedLocal.x, seedLocal.y, true)
                return
            }
            if (root.toolModeIndex === 1) {
                root.windowStart = Qt.point(mouse.x, mouse.y)
                root.windowStartWidth = medicalData.windowWidth
                root.windowStartLevel = medicalData.windowLevel
                return
            }
            if (root.toolModeIndex === 4) {
                const local = mapToItem(viewport, mouse.x, mouse.y)
                if (!annotationController.hasActive) {
                    const hit = viewport.hitTestControlPoint(local.x, local.y, 14)
                    if (hit && hit.nodeId !== undefined) {
                        root.draggingHandle = true
                        root.dragNodeId = hit.nodeId
                        root.dragPointIndex = hit.pointIndex
                        return
                    }
                }
                root.annotationPicking = true
                if (!viewport.mapClickToVoxel(local.x, local.y, false))
                    root.annotationPicking = false
            }
        }
        onPositionChanged: mouse => {
            if (root.toolModeIndex === 1 && pressed) {
                const dx = mouse.x - root.windowStart.x
                const dy = mouse.y - root.windowStart.y
                medicalData.windowWidth = Math.max(
                    1, root.windowStartWidth * Math.exp(dx / Math.max(1, width) * 2.0))
                medicalData.windowLevel = root.windowStartLevel
                    - dy / Math.max(1, height) * root.windowStartWidth * 2.0
                return
            }
            if (root.draggingHandle && pressed) {
                const local = mapToItem(viewport, mouse.x, mouse.y)
                viewport.mapClickToVoxel(local.x, local.y, false)
            }
        }
        onReleased: {
            if (root.draggingHandle) {
                root.draggingHandle = false
                root.dragNodeId = -1
                root.dragPointIndex = -1
            }
        }
        onDoubleClicked: mouse => {
            if (root.toolModeIndex === 4
                    && annotationController.toolType === AnnotationTool.PerimeterTool) {
                annotationController.finishActive()
                mouse.accepted = true
            }
        }
    }

    property point windowStart: Qt.point(0, 0)
    property real windowStartWidth: 400.0
    property real windowStartLevel: 40.0

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
        z: 20
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

    Text {
        visible: root.toolModeIndex === 4
                 && root.viewType !== MedicalViewport.Volume3D
                 && annotationController.toolType === AnnotationTool.PerimeterTool
        anchors.horizontalCenter: parent.horizontalCenter
        anchors.bottom: sliceSlider.visible ? sliceSlider.top : parent.bottom
        anchors.bottomMargin: 10
        z: 11
        text: "单击加点 · 双击/右键闭合 · Esc 取消 · 拖拽控制点编辑"
        color: Theme.textSecondary
        font.pixelSize: 12
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
