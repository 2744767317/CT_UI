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
    property bool seedPickPending: false
    property string seedPickMessage: ""
    property bool seedPickMessageError: false
    property real cropMinimum: 0.0
    property real cropMaximum: 1.0
    property real slicePosition: sliceSlider.value
    readonly property int sliceCount: viewport.sliceCount
    property bool draggingHandle: false
    property int dragNodeId: -1
    property int dragPointIndex: -1
    property bool middlePanning: false
    signal seedSelected(int viewType, real normalizedX, real normalizedY, real slicePosition)
    signal activated()

    readonly property bool measureMode: root.toolModeIndex === 4
            && root.viewType !== MedicalViewport.Volume3D
            && medicalData.volumeData
    // 与窗宽窗位对称：用 toolModeIndex 直接打开 MouseArea，避免 measureMode 复合条件导致测量时 enabled=false
    readonly property bool captureInput: root.seedPicking
            || root.toolModeIndex === 1
            || root.toolModeIndex === 2
            || root.toolModeIndex === 3
            || root.toolModeIndex === 4

    function stepSlice(direction) {
        if (root.sliceCount <= 1 || direction === 0)
            return
        const last = root.sliceCount - 1
        const current = Math.round(sliceSlider.value * last)
        const next = Math.max(0, Math.min(last, current + direction))
        sliceSlider.value = next / last
    }

    function handleWheel(wheel) {
        const local = root.mapToItem(viewport, wheel.x, wheel.y)
        const delta = wheel.angleDelta.y !== 0 ? wheel.angleDelta.y : wheel.pixelDelta.y
        if ((wheel.modifiers & Qt.ControlModifier) && delta !== 0) {
            viewport.zoomBy(Math.pow(1.12, delta / 120.0), local.x, local.y)
        } else {
            root.stepSlice(delta > 0 ? 1 : -1)
        }
        wheel.accepted = true
    }

    function handleZoomShortcut(key) {
        if (key === Qt.Key_0) {
            viewport.resetView()
            return true
        }
        if (key === Qt.Key_Plus || key === Qt.Key_Equal) {
            viewport.zoomBy(1.2, viewport.width * 0.5, viewport.height * 0.5)
            return true
        }
        if (key === Qt.Key_Minus || key === Qt.Key_Underscore) {
            viewport.zoomBy(1.0 / 1.2, viewport.width * 0.5, viewport.height * 0.5)
            return true
        }
        return false
    }

    function beginMiddlePan(mouse) {
        if (mouse.button !== Qt.MiddleButton)
            return false
        root.middlePanning = true
        root.interactionLast = Qt.point(mouse.x, mouse.y)
        return true
    }

    function updateMiddlePan(mouse) {
        if (!root.middlePanning || !(mouse.buttons & Qt.MiddleButton))
            return false
        const dx = mouse.x - root.interactionLast.x
        const dy = mouse.y - root.interactionLast.y
        viewport.panBy(dx, dy)
        root.interactionLast = Qt.point(mouse.x, mouse.y)
        return true
    }

    function endMiddlePan(mouse) {
        if (mouse.button === Qt.MiddleButton)
            root.middlePanning = false
    }

    color: Theme.image
    border.width: 1
    border.color: root.activeViewport || activeArea.containsMouse ? root.viewColor : Theme.border
    clip: true
    focus: false

    Keys.onPressed: event => {
        if (!(event.modifiers & Qt.ControlModifier))
            return
        if (root.handleZoomShortcut(event.key))
            event.accepted = true
    }

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
            pickTimeout.stop()
            root.seedPickPending = false
            root.seedPickMessageError = false
            root.seedPickMessage = "种子点已选择：IJK (" + voxelX + ", " + voxelY + ", "
                    + voxelZ + ")，" + hu + " HU"
            pickMessageTimer.restart()
            root.seedSelected(root.viewType, normalizedX, normalizedY, root.slicePosition)
        }
        onVoxelPickFailed: message => {
            root.draggingHandle = false
            pickTimeout.stop()
            root.seedPickPending = false
            root.seedPickMessageError = true
            root.seedPickMessage = message
            pickMessageTimer.restart()
        }
        onAnnotationControlPointPressed: (nodeId, pointIndex) => {
            root.dragNodeId = nodeId
            root.dragPointIndex = pointIndex
            root.draggingHandle = activeArea.pressed
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
        enabled: medicalData.loaded && !root.seedPicking
                 && root.viewType !== MedicalViewport.Volume3D && !root.captureInput
        acceptedButtons: Qt.AllButtons
        preventStealing: true
        onPressed: mouse => {
            root.activated()
            root.forceActiveFocus()
            root.beginMiddlePan(mouse)
            mouse.accepted = true
        }
        onPositionChanged: mouse => root.updateMiddlePan(mouse)
        onReleased: mouse => root.endMiddlePan(mouse)
        onCanceled: root.middlePanning = false
        onWheel: wheel => {
            root.handleWheel(wheel)
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
        acceptedButtons: Qt.AllButtons
        enabled: root.viewType !== MedicalViewport.Volume3D
                 && !root.seedPickPending
                 && root.captureInput
        preventStealing: true
        cursorShape: root.seedPicking || root.toolModeIndex === 4
                     ? Qt.CrossCursor
                     : (root.toolModeIndex === 3 ? Qt.SizeVerCursor : Qt.SizeAllCursor)

        onPressed: mouse => {
            root.activated()
            root.forceActiveFocus()
            if (root.beginMiddlePan(mouse))
                return
            if (mouse.button === Qt.RightButton) {
                if (root.toolModeIndex === 4
                        && (annotationController.toolType === AnnotationTool.CurveTool
                            || annotationController.toolType === AnnotationTool.PointListTool)) {
                    annotationController.finishActive()
                }
                return
            }
            if (mouse.button !== Qt.LeftButton)
                return
            if (root.seedPicking) {
                const seedLocal = mapToItem(viewport, mouse.x, mouse.y)
                root.seedPickPending = true
                root.seedPickMessageError = false
                root.seedPickMessage = "正在读取种子点…"
                pickMessageTimer.stop()
                pickTimeout.restart()
                viewport.mapClickToVoxel(seedLocal.x, seedLocal.y, true)
                return
            }
            if (root.toolModeIndex === 1) {
                root.windowStart = Qt.point(mouse.x, mouse.y)
                root.windowStartWidth = medicalData.windowWidth
                root.windowStartLevel = medicalData.windowLevel
                return
            }
            if (root.toolModeIndex === 2 || root.toolModeIndex === 3) {
                root.interactionLast = Qt.point(mouse.x, mouse.y)
                return
            }
            if (root.toolModeIndex === 4) {
                const local = mapToItem(viewport, mouse.x, mouse.y)
                viewport.beginAnnotationInteraction(local.x, local.y, 14)
            }
        }
        onPositionChanged: mouse => {
            if (root.updateMiddlePan(mouse))
                return
            if (!(mouse.buttons & Qt.LeftButton))
                return
            if (root.toolModeIndex === 1 && pressed) {
                const dx = mouse.x - root.windowStart.x
                const dy = mouse.y - root.windowStart.y
                medicalData.windowWidth = Math.max(
                    1, root.windowStartWidth * Math.exp(dx / Math.max(1, width) * 2.0))
                medicalData.windowLevel = root.windowStartLevel
                    - dy / Math.max(1, height) * root.windowStartWidth * 2.0
                return
            }
            if (root.toolModeIndex === 2 && pressed) {
                const dx = mouse.x - root.interactionLast.x
                const dy = mouse.y - root.interactionLast.y
                viewport.panBy(dx, dy)
                root.interactionLast = Qt.point(mouse.x, mouse.y)
                return
            }
            if (root.toolModeIndex === 3 && pressed) {
                const dy = mouse.y - root.interactionLast.y
                viewport.zoomBy(Math.exp(-dy / 180.0), mouse.x, mouse.y)
                root.interactionLast = Qt.point(mouse.x, mouse.y)
                return
            }
            if (root.draggingHandle && pressed) {
                const local = mapToItem(viewport, mouse.x, mouse.y)
                viewport.updateAnnotationControlPoint(
                            root.dragNodeId, root.dragPointIndex, local.x, local.y)
            }
        }
        onReleased: mouse => {
            root.endMiddlePan(mouse)
            if (root.draggingHandle) {
                root.draggingHandle = false
                root.dragNodeId = -1
                root.dragPointIndex = -1
            }
        }
        onCanceled: root.middlePanning = false
        onDoubleClicked: mouse => {
            if (root.toolModeIndex === 2 || root.toolModeIndex === 3)
                viewport.resetView()
        }
        onWheel: wheel => {
            root.handleWheel(wheel)
        }
    }

    property point windowStart: Qt.point(0, 0)
    property point interactionLast: Qt.point(0, 0)
    property real windowStartWidth: 400.0
    property real windowStartLevel: 40.0

    onSeedPickingChanged: {
        if (!root.seedPicking)
            root.seedPickPending = false
    }

    Rectangle {
        visible: root.seedPicking && root.seedPickMessage.length === 0
        anchors.horizontalCenter: parent.horizontalCenter
        anchors.top: parent.top
        anchors.topMargin: 46
        width: pickModeText.implicitWidth + 22
        height: 32
        radius: 3
        color: "#E61A1F22"
        border.color: root.viewColor
        z: 9

        Text {
            id: pickModeText
            anchors.centerIn: parent
            text: "单击切片内目标组织选择种子点"
            color: Theme.text
            font.pixelSize: 13
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
        id: pickMessage
        visible: root.seedPickMessage.length > 0
        anchors.centerIn: parent
        width: Math.min(parent.width - 32, pickMessageText.implicitWidth + 24)
        height: Math.max(36, pickMessageText.implicitHeight + 16)
        radius: Theme.radius
        color: "#E61A1F22"
        border.color: root.seedPickMessageError ? Theme.danger : Theme.accent
        z: 20
        Text {
            id: pickMessageText
            anchors.centerIn: parent
            width: Math.min(implicitWidth, pickMessage.parent.width - 20)
            text: root.seedPickMessage
            color: root.seedPickMessageError ? Theme.danger : Theme.text
            font.pixelSize: 13
            wrapMode: Text.WordWrap
            horizontalAlignment: Text.AlignHCenter
        }
    }
    Timer {
        id: pickMessageTimer
        interval: 2600
        onTriggered: root.seedPickMessage = ""
    }
    Timer {
        id: pickTimeout
        interval: 4000
        onTriggered: {
            if (!root.seedPickPending)
                return
            root.seedPickPending = false
            root.seedPickMessageError = true
            root.seedPickMessage = "种子点读取超时，请在切片图像内重新单击。"
            pickMessageTimer.restart()
        }
    }

    Text {
        visible: root.toolModeIndex === 4
                 && root.viewType !== MedicalViewport.Volume3D
                 && annotationController.toolType === AnnotationTool.CurveTool
        anchors.horizontalCenter: parent.horizontalCenter
        anchors.bottom: sliceSlider.visible ? sliceSlider.top : parent.bottom
        anchors.bottomMargin: 10
        z: 11
        text: "单击添加曲线控制点 · 右键完成 · Esc 取消 · 拖拽控制点编辑"
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
        stepSize: root.sliceCount > 1 ? 1 / (root.sliceCount - 1) : 1
        snapMode: Slider.SnapAlways
        value: 0.5
    }
}
