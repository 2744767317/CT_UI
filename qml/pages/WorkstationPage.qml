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
    property int layoutMode: 0
    property bool mip: false
    property int volumePreset: MedicalViewport.BonePreset
    property bool showSegmentation: true
    property bool seedPicking: false
    property int seedViewType: -1
    property real seedMarkerX: 0.5
    property real seedMarkerY: 0.5
    property real seedSlicePosition: -1.0
    property real cropMinimum: 0.0
    property real cropMaximum: 1.0

    function acceptSeed(viewType, normalizedX, normalizedY, slicePosition) {
        root.seedViewType = viewType
        root.seedMarkerX = normalizedX
        root.seedMarkerY = normalizedY
        root.seedSlicePosition = slicePosition
        root.seedPicking = false
    }

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
                    model: ["浏览", "窗宽窗位", "平移", "缩放", "测量"]
                    delegate: ActionButton {
                        required property string modelData
                        text: modelData
                        checkable: true
                        checked: root.toolMode === modelData
                        active: checked
                        ButtonGroup.group: toolGroup
                        onClicked: root.toolMode = modelData
                    }
                }
                Rectangle { width: 1; Layout.fillHeight: true; Layout.topMargin: 8; Layout.bottomMargin: 8; color: Theme.border }
                Text { text: "布局"; color: Theme.textSecondary; font.pixelSize: 13 }
                ComboBox {
                    model: ["四视图", "仅三维", "仅切片"]
                    onActivated: index => root.layoutMode = index
                }
                Item { Layout.fillWidth: true }
                StatusPill { text: medicalData.volumeData ? "CT VOLUME" : "2D X-RAY"; tone: medicalData.loaded ? Theme.success : Theme.textMuted }
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
            }

            GridLayout {
                Layout.fillWidth: true
                Layout.fillHeight: true
                Layout.minimumWidth: 0
                columns: root.layoutMode === 2 ? 3 : 2
                rows: 2
                rowSpacing: 3
                columnSpacing: 3

                ViewportPane {
                    Layout.fillWidth: true
                    Layout.fillHeight: true
                    Layout.row: 0; Layout.column: 0
                    Layout.rowSpan: !medicalData.volumeData ? 2 : 1
                    Layout.columnSpan: !medicalData.volumeData ? 2 : 1
                    visible: root.layoutMode !== 1
                    viewType: MedicalViewport.Axial
                    title: "AXIAL 轴状位"
                    viewColor: Theme.axial
                    toolMode: root.toolMode
                    showSegmentation: root.showSegmentation
                    seedPicking: root.seedPicking
                    seedMarkerVisible: root.seedViewType === viewType
                                       && Math.abs(slicePosition - root.seedSlicePosition) < 0.0001
                    seedMarkerX: root.seedMarkerX
                    seedMarkerY: root.seedMarkerY
                    onSeedSelected: (viewType, x, y, slice) => root.acceptSeed(viewType, x, y, slice)
                }
                ViewportPane {
                    Layout.fillWidth: true
                    Layout.fillHeight: true
                    Layout.row: root.layoutMode === 2 ? 0 : 1
                    Layout.column: root.layoutMode === 2 ? 1 : 0
                    visible: medicalData.volumeData && root.layoutMode !== 1
                    viewType: MedicalViewport.Coronal
                    title: "CORONAL 冠状位"
                    viewColor: Theme.coronal
                    toolMode: root.toolMode
                    showSegmentation: root.showSegmentation
                    seedPicking: root.seedPicking
                    seedMarkerVisible: root.seedViewType === viewType
                                       && Math.abs(slicePosition - root.seedSlicePosition) < 0.0001
                    seedMarkerX: root.seedMarkerX
                    seedMarkerY: root.seedMarkerY
                    onSeedSelected: (viewType, x, y, slice) => root.acceptSeed(viewType, x, y, slice)
                }
                ViewportPane {
                    Layout.fillWidth: true
                    Layout.fillHeight: true
                    Layout.row: root.layoutMode === 2 ? 0 : 1
                    Layout.column: root.layoutMode === 2 ? 2 : 1
                    visible: medicalData.volumeData && root.layoutMode !== 1
                    viewType: MedicalViewport.Sagittal
                    title: "SAGITTAL 矢状位"
                    viewColor: Theme.sagittal
                    toolMode: root.toolMode
                    showSegmentation: root.showSegmentation
                    seedPicking: root.seedPicking
                    seedMarkerVisible: root.seedViewType === viewType
                                       && Math.abs(slicePosition - root.seedSlicePosition) < 0.0001
                    seedMarkerX: root.seedMarkerX
                    seedMarkerY: root.seedMarkerY
                    onSeedSelected: (viewType, x, y, slice) => root.acceptSeed(viewType, x, y, slice)
                }
                ViewportPane {
                    Layout.fillWidth: true
                    Layout.fillHeight: true
                    Layout.row: root.layoutMode === 1 ? 0 : 0
                    Layout.column: root.layoutMode === 1 ? 0 : 1
                    Layout.rowSpan: root.layoutMode === 1 ? 2 : 1
                    Layout.columnSpan: root.layoutMode === 1 ? 2 : 1
                    visible: medicalData.volumeData && root.layoutMode !== 2
                    viewType: MedicalViewport.Volume3D
                    title: root.mip ? "3D MIP" : "3D VOLUME"
                    viewColor: Theme.volume
                    toolMode: root.toolMode
                    mip: root.mip
                    volumePreset: root.volumePreset
                    showSegmentation: root.showSegmentation
                    cropMinimum: root.cropMinimum
                    cropMaximum: root.cropMaximum
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
                onMipRequested: enabled => root.mip = enabled
                onVolumePresetRequested: preset => root.volumePreset = preset
                onSeedPickingRequested: enabled => root.seedPicking = enabled
                onSegmentationVisibilityRequested: visible => root.showSegmentation = visible
                onCropRequested: (minimum, maximum) => {
                    root.cropMinimum = minimum
                    root.cropMaximum = maximum
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
                Text { text: medicalData.seriesDescription; color: Theme.textSecondary; font.pixelSize: 12 }
                Text { text: medicalData.dimensionsText; color: Theme.textSecondary; font.pixelSize: 12 }
                Text { text: medicalData.spacingText; color: Theme.textSecondary; font.pixelSize: 12 }
                Item { Layout.fillWidth: true }
                Text { text: medicalBackendEnabled ? "VTK / ITK 后端就绪" : "MinGW UI 兼容模式"; color: medicalBackendEnabled ? Theme.success : Theme.accent; font.pixelSize: 12 }
            }
        }
    }
}
