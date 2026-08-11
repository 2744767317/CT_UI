import QtQuick
import QtQuick.Controls
import QtQuick.Layouts
import GuangSuo.CT
import GuangSuo.CT.Rendering

Item {
    id: root
    signal requestFolderImport()
    signal requestFileImport()
    signal requestExport()

    property string toolMode: "浏览"
    property int toolModeIndex: 0
    property int measureSubMode: AnnotationTool.LineTool
    property int layoutMode: 0
    property bool mip: false
    property int volumePreset: MedicalViewport.BonePreset
    property bool showSegmentation: true
    property bool showImage: true
    property bool showMeasurements: true
    property real segmentationOpacity: 0.72

    function measureToolLabel() {
        if (root.toolModeIndex !== 4)
            return "测量"
        if (root.measureSubMode === AnnotationTool.PointListTool)
            return "测量·标点"
        if (root.measureSubMode === AnnotationTool.AngleTool)
            return "测量·角度"
        if (root.measureSubMode === AnnotationTool.CurveTool)
            return "测量·曲线"
        return "测量·直线"
    }

    function selectMeasureTool(toolType) {
        root.toolMode = "测量"
        root.toolModeIndex = 4
        root.measureSubMode = toolType
        root.showMeasurements = true
        annotationController.toolType = toolType
        annotationController.visible = true
        root.seedPicking = false
    }
    property bool activePairedProjection: false
    property int frontalRotationQuarterTurns: 0
    property bool frontalFlipHorizontal: false
    property bool frontalFlipVertical: false
    property int lateralRotationQuarterTurns: 0
    property bool lateralFlipHorizontal: false
    property bool lateralFlipVertical: false
    property bool seedPicking: false
    property int seedViewType: -1
    property real seedMarkerX: 0.5
    property real seedMarkerY: 0.5
    property real seedSlicePosition: -1.0
    property real cropMinimum: 0.0
    property real cropMaximum: 1.0

    function resetProjectionAdjustments() {
        root.activePairedProjection = false
        root.frontalRotationQuarterTurns = 0
        root.frontalFlipHorizontal = false
        root.frontalFlipVertical = false
        root.lateralRotationQuarterTurns = 0
        root.lateralFlipHorizontal = false
        root.lateralFlipVertical = false
    }

    function acceptSeed(viewType, normalizedX, normalizedY, slicePosition) {
        root.seedViewType = viewType
        root.seedMarkerX = normalizedX
        root.seedMarkerY = normalizedY
        root.seedSlicePosition = slicePosition
        root.seedPicking = false
    }

    Connections {
        target: medicalData
        function onDataChanged() {
            root.resetProjectionAdjustments()
            root.showImage = medicalData.activeVolumeVisible
            root.seedPicking = false
            root.seedViewType = -1
            root.seedSlicePosition = -1.0
        }
        function onRegionGrowingSeedChanged() {
            if (!medicalData.regionGrowingSeedValid) {
                root.seedViewType = -1
                root.seedSlicePosition = -1.0
            }
        }
    }

    onShowMeasurementsChanged: annotationController.visible = root.showMeasurements
    Component.onCompleted: annotationController.visible = root.showMeasurements

    Keys.onPressed: event => {
        if (event.key === Qt.Key_Escape) {
            annotationController.cancelActive()
            event.accepted = true
        }
    }
    focus: true

    ColumnLayout {
        anchors.fill: parent
        spacing: 0

        Rectangle {
            Layout.fillWidth: true
            height: 52
            color: Theme.panelRaised
            border.color: Theme.border
            RowLayout {
                anchors.fill: parent
                anchors.leftMargin: 10
                anchors.rightMargin: 10
                spacing: 6
                ButtonGroup { id: toolGroup }
                Repeater {
                    model: ["浏览", "窗宽窗位", "平移", "缩放"]
                    delegate: ActionButton {
                        required property string modelData
                        required property int index
                        text: modelData
                        checkable: true
                        checked: root.toolModeIndex === index
                        active: checked
                        ButtonGroup.group: toolGroup
                        onClicked: {
                            root.toolMode = modelData
                            root.toolModeIndex = index
                            annotationController.toolType = AnnotationTool.NoneTool
                        }
                    }
                }
                ActionButton {
                    id: measureButton
                    text: root.measureToolLabel()
                    checkable: true
                    checked: root.toolModeIndex === 4
                    active: checked
                    ButtonGroup.group: toolGroup
                    onClicked: {
                        if (root.toolModeIndex !== 4)
                            root.selectMeasureTool(root.measureSubMode || AnnotationTool.LineTool)
                        measureMenu.open()
                    }
                    Menu {
                        id: measureMenu
                        y: measureButton.height + 4
                        MenuItem {
                            text: "标点（Point List）"
                            onTriggered: root.selectMeasureTool(AnnotationTool.PointListTool)
                        }
                        MenuItem {
                            text: "直线（Line）"
                            onTriggered: root.selectMeasureTool(AnnotationTool.LineTool)
                        }
                        MenuItem {
                            text: "角度（Angle）"
                            onTriggered: root.selectMeasureTool(AnnotationTool.AngleTool)
                        }
                        MenuItem {
                            text: "曲线（Curve）"
                            onTriggered: root.selectMeasureTool(AnnotationTool.CurveTool)
                        }
                    }
                }
                Rectangle { width: 1; Layout.fillHeight: true; Layout.topMargin: 8; Layout.bottomMargin: 8; color: Theme.border }
                Text { text: "布局"; color: Theme.textSecondary; font.pixelSize: 13 }
                ComboBox {
                    model: ["四视图", "仅三维", "仅切片"]
                    onActivated: index => root.layoutMode = index
                }
                Item { Layout.fillWidth: true }
                StatusPill {
                    text: medicalData.volumeData ? "CT VOLUME" : (medicalData.projectionData ? "DX PROJECTION" : "NO DATA")
                    tone: medicalData.loaded ? Theme.success : Theme.textMuted
                }
            }
        }

        RowLayout {
            Layout.fillWidth: true
            Layout.fillHeight: true
            spacing: 3

            DataPanel {
                Layout.minimumWidth: 320
                Layout.preferredWidth: 320
                Layout.maximumWidth: 320
                Layout.fillHeight: true
                onRequestFolderImport: root.requestFolderImport()
                onRequestFileImport: root.requestFileImport()
                onRequestExport: root.requestExport()
                onImageVisibilityRequested: visible => {
                    root.showImage = visible
                    if (medicalData.selectedVolumeIndex >= 0)
                        medicalData.setVolumeVisibility(medicalData.selectedVolumeIndex, visible)
                }
                onSegmentationVisibilityRequested: visible => root.showSegmentation = visible
                onMeasurementVisibilityRequested: visible => root.showMeasurements = visible
                onSegmentationOpacityRequested: opacity => root.segmentationOpacity = opacity
                imageVisible: root.showImage
                segmentationVisible: root.showSegmentation
                measurementsVisible: root.showMeasurements
                segmentationOpacity: root.segmentationOpacity
            }

            Item {
                id: canvasHost
                Layout.fillWidth: true
                Layout.fillHeight: true

                RowLayout {
                    anchors.fill: parent
                    spacing: 3
                    visible: medicalData.projectionData

                    ViewportPane {
                        Layout.fillWidth: true
                        Layout.fillHeight: true
                        viewType: MedicalViewport.Axial
                        title: medicalData.projectionViewLabel
                        viewColor: Theme.axial
                        toolMode: root.toolMode
                        toolModeIndex: root.toolModeIndex
                        measureSubMode: root.measureSubMode
                        showImage: root.showImage
                        showSegmentation: false
                        showMeasurements: root.showMeasurements
                        rotationQuarterTurns: root.frontalRotationQuarterTurns
                        flipHorizontal: root.frontalFlipHorizontal
                        flipVertical: root.frontalFlipVertical
                        activeViewport: medicalData.projectionData && !root.activePairedProjection
                        onActivated: root.activePairedProjection = false
                    }
                    ViewportPane {
                        Layout.fillWidth: true
                        Layout.fillHeight: true
                        visible: medicalData.pairedProjectionAvailable
                        pairedProjection: true
                        viewType: MedicalViewport.Axial
                        title: medicalData.projectionPairViewLabel
                        viewColor: Theme.coronal
                        toolMode: root.toolMode
                        toolModeIndex: root.toolModeIndex
                        measureSubMode: root.measureSubMode
                        showImage: root.showImage
                        showSegmentation: false
                        showMeasurements: root.showMeasurements
                        rotationQuarterTurns: root.lateralRotationQuarterTurns
                        flipHorizontal: root.lateralFlipHorizontal
                        flipVertical: root.lateralFlipVertical
                        activeViewport: root.activePairedProjection
                        onActivated: root.activePairedProjection = true
                    }
                    Text {
                        visible: medicalData.loaded && !medicalData.pairedProjectionAvailable
                        Layout.alignment: Qt.AlignBottom | Qt.AlignHCenter
                        Layout.bottomMargin: 12
                        text: "单幅投影：未发现同检查的另一平面"
                        color: Theme.textMuted
                        font.pixelSize: 12
                    }
                }

                GridLayout {
                    anchors.fill: parent
                    visible: medicalData.volumeData
                    columns: root.layoutMode === 2 ? 3 : 2
                    rows: 2
                    rowSpacing: 3
                    columnSpacing: 3

                    ViewportPane {
                        Layout.fillWidth: true
                        Layout.fillHeight: true
                        Layout.row: 0; Layout.column: 0
                        Layout.rowSpan: root.layoutMode === 1 ? 2 : 1
                        Layout.columnSpan: root.layoutMode === 1 ? 2 : 1
                        visible: root.layoutMode !== 1
                        viewType: MedicalViewport.Axial
                        title: "AXIAL"
                        viewColor: Theme.axial
                        toolMode: root.toolMode
                        toolModeIndex: root.toolModeIndex
                        measureSubMode: root.measureSubMode
                        showImage: root.showImage
                        showSegmentation: root.showSegmentation
                        showMeasurements: root.showMeasurements
                        segmentationOpacity: root.segmentationOpacity
                        seedPicking: root.seedPicking
                        seedMarkerVisible: root.seedViewType === viewType && Math.abs(slicePosition - root.seedSlicePosition) < 0.0001
                        seedMarkerX: root.seedMarkerX
                        seedMarkerY: root.seedMarkerY
                        onSeedSelected: (viewType, x, y, slice) => root.acceptSeed(viewType, x, y, slice)
                    }
                    ViewportPane {
                        Layout.fillWidth: true
                        Layout.fillHeight: true
                        Layout.row: root.layoutMode === 2 ? 0 : 1
                        Layout.column: root.layoutMode === 2 ? 1 : 0
                        visible: root.layoutMode !== 1
                        viewType: MedicalViewport.Coronal
                        title: "CORONAL"
                        viewColor: Theme.coronal
                        toolMode: root.toolMode
                        toolModeIndex: root.toolModeIndex
                        measureSubMode: root.measureSubMode
                        showImage: root.showImage
                        showSegmentation: root.showSegmentation
                        showMeasurements: root.showMeasurements
                        segmentationOpacity: root.segmentationOpacity
                        seedPicking: root.seedPicking
                        seedMarkerVisible: root.seedViewType === viewType && Math.abs(slicePosition - root.seedSlicePosition) < 0.0001
                        seedMarkerX: root.seedMarkerX
                        seedMarkerY: root.seedMarkerY
                        onSeedSelected: (viewType, x, y, slice) => root.acceptSeed(viewType, x, y, slice)
                    }
                    ViewportPane {
                        Layout.fillWidth: true
                        Layout.fillHeight: true
                        Layout.row: root.layoutMode === 2 ? 0 : 1
                        Layout.column: root.layoutMode === 2 ? 2 : 1
                        visible: root.layoutMode !== 1
                        viewType: MedicalViewport.Sagittal
                        title: "SAGITTAL"
                        viewColor: Theme.sagittal
                        toolMode: root.toolMode
                        toolModeIndex: root.toolModeIndex
                        measureSubMode: root.measureSubMode
                        showImage: root.showImage
                        showSegmentation: root.showSegmentation
                        showMeasurements: root.showMeasurements
                        segmentationOpacity: root.segmentationOpacity
                        seedPicking: root.seedPicking
                        seedMarkerVisible: root.seedViewType === viewType && Math.abs(slicePosition - root.seedSlicePosition) < 0.0001
                        seedMarkerX: root.seedMarkerX
                        seedMarkerY: root.seedMarkerY
                        onSeedSelected: (viewType, x, y, slice) => root.acceptSeed(viewType, x, y, slice)
                    }
                    ViewportPane {
                        Layout.fillWidth: true
                        Layout.fillHeight: true
                        Layout.row: 0
                        Layout.column: root.layoutMode === 1 ? 0 : 1
                        Layout.rowSpan: root.layoutMode === 1 ? 2 : 1
                        visible: root.layoutMode !== 2
                        viewType: MedicalViewport.Volume3D
                        title: root.mip ? "3D MIP" : "3D VOLUME"
                        viewColor: Theme.volume
                        toolMode: root.toolMode
                        toolModeIndex: root.toolModeIndex
                        measureSubMode: root.measureSubMode
                        showImage: root.showImage
                        mip: root.mip
                        volumePreset: root.volumePreset
                        showSegmentation: root.showSegmentation
                        showMeasurements: root.showMeasurements
                        segmentationOpacity: root.segmentationOpacity
                        cropMinimum: root.cropMinimum
                        cropMaximum: root.cropMaximum
                    }
                }
            }

            InspectorPanel {
                Layout.minimumWidth: 340
                Layout.preferredWidth: 340
                Layout.maximumWidth: 340
                Layout.fillHeight: true
                mip: root.mip
                volumePreset: root.volumePreset
                showSegmentation: root.showSegmentation
                seedPicking: root.seedPicking
                cropMinimum: root.cropMinimum
                cropMaximum: root.cropMaximum
                activeProjection: root.activePairedProjection
                projectionViewLabel: root.activePairedProjection
                                     ? medicalData.projectionPairViewLabel
                                     : medicalData.projectionViewLabel
                projectionOrientation: root.activePairedProjection
                                       ? medicalData.projectionPairOrientation
                                       : medicalData.patientOrientation
                projectionSopClassName: root.activePairedProjection
                                        ? medicalData.projectionPairSopClassName
                                        : medicalData.sopClassName
                projectionImageType: root.activePairedProjection
                                     ? medicalData.projectionPairImageType
                                     : medicalData.imageType
                rotationQuarterTurns: root.activePairedProjection
                                      ? root.lateralRotationQuarterTurns
                                      : root.frontalRotationQuarterTurns
                flipHorizontal: root.activePairedProjection
                                ? root.lateralFlipHorizontal
                                : root.frontalFlipHorizontal
                flipVertical: root.activePairedProjection
                              ? root.lateralFlipVertical
                              : root.frontalFlipVertical
                onMipRequested: enabled => root.mip = enabled
                onVolumePresetRequested: preset => root.volumePreset = preset
                onSeedPickingRequested: enabled => root.seedPicking = enabled
                onSegmentationVisibilityRequested: visible => root.showSegmentation = visible
                onCropRequested: (minimum, maximum) => { root.cropMinimum = minimum; root.cropMaximum = maximum }
                onRotationRequested: turns => {
                    if (root.activePairedProjection)
                        root.lateralRotationQuarterTurns = turns
                    else
                        root.frontalRotationQuarterTurns = turns
                }
                onFlipHorizontalRequested: flipped => {
                    if (root.activePairedProjection)
                        root.lateralFlipHorizontal = flipped
                    else
                        root.frontalFlipHorizontal = flipped
                }
                onFlipVerticalRequested: flipped => {
                    if (root.activePairedProjection)
                        root.lateralFlipVertical = flipped
                    else
                        root.frontalFlipVertical = flipped
                }
                onRequestExport: root.requestExport()
            }
        }

        Rectangle {
            Layout.fillWidth: true
            height: 28
            color: Theme.panelRaised
            border.color: Theme.border
            RowLayout {
                anchors.fill: parent
                anchors.leftMargin: 12
                anchors.rightMargin: 12
                Text { text: medicalData.seriesDescription; color: Theme.textSecondary; font.pixelSize: 12; elide: Text.ElideRight; Layout.maximumWidth: 360 }
                Text { text: medicalData.dimensionsText; color: Theme.textSecondary; font.pixelSize: 12 }
                Text { text: medicalData.spacingText; color: Theme.textSecondary; font.pixelSize: 12 }
                Item { Layout.fillWidth: true }
                Text { text: medicalBackendEnabled ? "VTK / ITK" : "兼容模式"; color: medicalBackendEnabled ? Theme.success : Theme.accent; font.pixelSize: 12 }
            }
        }
    }
}
