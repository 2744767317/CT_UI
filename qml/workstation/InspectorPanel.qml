import QtQuick
import QtQuick.Controls
import QtQuick.Layouts
import GuangSuo.CT

Rectangle {
    id: root
    property bool ctMode: medicalData.modality === "CT"
    property bool mip: false
    property int volumePreset: MedicalViewport.BonePreset
    property bool showSegmentation: true
    property bool seedPicking: false
    property real cropMinimum: 0.0
    property real cropMaximum: 1.0
    property color segmentationColor: "#F0783C"
    property int rotationQuarterTurns: 0
    property bool flipHorizontal: false
    property bool flipVertical: false
    property bool activeProjection: false
    property string projectionViewLabel: medicalData.projectionViewLabel
    property string projectionOrientation: medicalData.patientOrientation
    property string projectionSopClassName: medicalData.sopClassName
    property string projectionImageType: medicalData.imageType
    signal mipRequested(bool enabled)
    signal volumePresetRequested(int preset)
    signal seedPickingRequested(bool enabled)
    signal segmentationVisibilityRequested(bool visible)
    signal segmentationColorRequested(color color)
    signal cropRequested(real minimum, real maximum)
    signal rotationRequested(int turns)
    signal flipHorizontalRequested(bool flipped)
    signal flipVerticalRequested(bool flipped)
    signal requestExport()

    function applySegmentationPreset(index) {
        const presets = [
            ["", "", "#F0783C"],
            ["150", "2500", "#F2C078"],
            ["-150", "250", "#E27D60"],
            ["-1000", "-400", "#55B7D9"],
            ["100", "500", "#D85C8B"]
        ]
        const preset = presets[index]
        if (!preset)
            return
        if (preset[0].length > 0) {
            thresholdLow.text = preset[0]
            thresholdHigh.text = preset[1]
            growLow.text = preset[0]
            growHigh.text = preset[1]
        }
        root.segmentationColorRequested(preset[2])
    }
    color: Theme.panel
    border.color: Theme.border

    ColumnLayout {
        anchors.fill: parent
        anchors.margins: 12
        spacing: 8

        Text { text: "上下文工具"; color: Theme.text; font.pixelSize: 17; font.weight: Font.DemiBold }

        TabBar {
            id: tabs
            Layout.fillWidth: true
            TabButton { text: "查看" }
            TabButton { text: "分割"; enabled: root.ctMode }
            TabButton { text: "3D"; enabled: medicalData.volumeData }
            TabButton { text: "导出" }
        }
        Connections {
            target: medicalData
            function onDataChanged() {
                if ((!medicalData.volumeData && tabs.currentIndex === 2)
                    || (!root.ctMode && tabs.currentIndex === 1))
                    tabs.currentIndex = 0
            }
            function onRegionGrowingSeedChanged() {
                if (!medicalData.regionGrowingSeedValid || !root.seedPicking)
                    return

                const lower = Math.max(-32768, medicalData.regionGrowingSeedValue - 100)
                const upper = Math.min(32767, medicalData.regionGrowingSeedValue + 100)
                growLow.text = String(Math.round(lower))
                growHigh.text = String(Math.round(upper))
                root.segmentationVisibilityRequested(true)
                Qt.callLater(function() {
                    if (medicalData.regionGrowingSeedValid && !medicalData.busy)
                        medicalData.applyRegionGrowingFromSeedAsync(
                            lower, upper, connectivity.currentIndex === 1)
                })
            }
        }

        StackLayout {
            currentIndex: tabs.currentIndex
            Layout.fillWidth: true
            Layout.fillHeight: true

            Flickable {
                contentHeight: displayColumn.implicitHeight
                clip: true
                ColumnLayout {
                    id: displayColumn
                    width: parent.width
                    spacing: 10
                    Text { text: "窗宽窗位"; color: Theme.text; font.pixelSize: 15; font.weight: Font.DemiBold }
                    Text { text: "窗宽  " + Math.round(medicalData.windowWidth); color: Theme.textSecondary; font.pixelSize: 13 }
                    Slider {
                        Layout.fillWidth: true
                        from: 1
                        to: root.ctMode ? 6000 : 65535
                        value: medicalData.windowWidth
                        onMoved: medicalData.setWindowing(value, medicalData.windowLevel)
                    }
                    Text { text: "窗位  " + Math.round(medicalData.displayWindowLevel); color: Theme.textSecondary; font.pixelSize: 13 }
                    Slider {
                        Layout.fillWidth: true
                        from: root.ctMode ? -2000 : (medicalData.projectionUnsigned ? 0 : -32768)
                        to: root.ctMode ? 3000 : (medicalData.projectionUnsigned ? 65535 : 32767)
                        value: medicalData.displayWindowLevel
                        onMoved: medicalData.setWindowing(
                                     medicalData.windowWidth,
                                     value - (root.ctMode || !medicalData.projectionUnsigned ? 0 : 32768))
                    }
                    Text { text: "预设"; color: Theme.text; font.pixelSize: 15; font.weight: Font.DemiBold }
                    ComboBox {
                        Layout.fillWidth: true
                        model: root.ctMode
                            ? ["软组织  W400 / L40", "肺窗  W1500 / L-600", "骨窗  W1800 / L400"]
                            : ["全动态范围  W65535 / L32767", "高对比  W16000 / L32767", "低对比  W32000 / L16384"]
                        onActivated: index => {
                            if (root.ctMode) {
                                if (index === 0) medicalData.setWindowing(400, 40)
                                if (index === 1) medicalData.setWindowing(1500, -600)
                                if (index === 2) medicalData.setWindowing(1800, 400)
                            } else {
                                if (index === 0) medicalData.setWindowing(
                                                    65535, 32767 - (medicalData.projectionUnsigned ? 32768 : 0))
                                if (index === 1) medicalData.setWindowing(
                                                    16000, 32767 - (medicalData.projectionUnsigned ? 32768 : 0))
                                if (index === 2) medicalData.setWindowing(
                                                    32000, 16384 - (medicalData.projectionUnsigned ? 32768 : 0))
                            }
                        }
                    }
                    Text { text: "图像信息"; color: Theme.text; font.pixelSize: 15; font.weight: Font.DemiBold }
                    Text { text: medicalData.dimensionsText; color: Theme.textSecondary; font.pixelSize: 13 }
                    Text { text: medicalData.spacingText; color: Theme.textSecondary; font.pixelSize: 13 }
                    Rectangle { Layout.fillWidth: true; height: 1; color: Theme.border; visible: !root.ctMode && medicalData.projectionData }
                    Text {
                        visible: !root.ctMode && medicalData.projectionData
                        text: "DICOM 方向"
                        color: Theme.text
                        font.pixelSize: 15
                        font.weight: Font.DemiBold
                    }
                    Text {
                        visible: !root.ctMode && medicalData.projectionData
                        Layout.fillWidth: true
                        text: root.projectionViewLabel + "  ·  " + root.projectionOrientation
                        color: Theme.textSecondary
                        font.pixelSize: 13
                        elide: Text.ElideRight
                    }
                    Text {
                        visible: !root.ctMode && medicalData.projectionData
                        Layout.fillWidth: true
                        text: root.projectionSopClassName + "\n" + root.projectionImageType
                        color: Theme.textMuted
                        font.pixelSize: 11
                        wrapMode: Text.WordWrap
                    }
                    RowLayout {
                        visible: !root.ctMode && medicalData.projectionData
                        Layout.fillWidth: true
                        ActionButton { text: "↶ 90°"; Layout.fillWidth: true; onClicked: root.rotationRequested((root.rotationQuarterTurns + 3) % 4) }
                        ActionButton { text: "↷ 90°"; Layout.fillWidth: true; onClicked: root.rotationRequested((root.rotationQuarterTurns + 1) % 4) }
                    }
                    RowLayout {
                        visible: !root.ctMode && medicalData.projectionData
                        Layout.fillWidth: true
                        ActionButton { text: "水平翻转"; Layout.fillWidth: true; active: root.flipHorizontal; onClicked: root.flipHorizontalRequested(!root.flipHorizontal) }
                        ActionButton { text: "垂直翻转"; Layout.fillWidth: true; active: root.flipVertical; onClicked: root.flipVerticalRequested(!root.flipVertical) }
                    }
                    ActionButton {
                        visible: !root.ctMode && medicalData.projectionData
                        text: "恢复 DICOM 方向"
                        Layout.fillWidth: true
                        onClicked: { root.rotationRequested(0); root.flipHorizontalRequested(false); root.flipVerticalRequested(false) }
                    }
                }
            }

            Flickable {
                contentHeight: segmentationColumn.implicitHeight
                clip: true
                ColumnLayout {
                    id: segmentationColumn
                    width: parent.width
                    spacing: 10
                    Text { text: "分割方法"; color: Theme.text; font.pixelSize: 15; font.weight: Font.DemiBold }
                    ComboBox {
                        Layout.fillWidth: true
                        model: ["自定义", "骨骼", "软组织", "肺部", "血管 / 增强"]
                        onActivated: index => root.applySegmentationPreset(index)
                    }
                    Text {
                        Layout.fillWidth: true
                        text: "组织预设会同步阈值/种子生长范围，并设置对应分割颜色；仍可手动微调 HU。"
                        color: Theme.textSecondary
                        font.pixelSize: 12
                        wrapMode: Text.WordWrap
                    }
                    ComboBox {
                        id: segmentationMethod
                        Layout.fillWidth: true
                        model: ["阈值分割", "种子生长"]
                    }
                    StackLayout {
                        Layout.fillWidth: true
                        currentIndex: segmentationMethod.currentIndex

                        ColumnLayout {
                            spacing: 10
                            Text {
                                Layout.fillWidth: true
                                text: "选择整个体数据中位于 HU 范围内的体素。"
                                color: Theme.textSecondary
                                font.pixelSize: 13
                                wrapMode: Text.WordWrap
                            }
                            RowLayout {
                                Layout.fillWidth: true
                                TextField { id: thresholdLow; Layout.fillWidth: true; text: "300"; placeholderText: "下限 HU"; validator: DoubleValidator {} }
                                TextField { id: thresholdHigh; Layout.fillWidth: true; text: "2500"; placeholderText: "上限 HU"; validator: DoubleValidator {} }
                            }
                            ActionButton {
                                text: "应用阈值分割"
                                primary: true
                                Layout.fillWidth: true
                                enabled: medicalData.volumeData && !medicalData.busy
                                onClicked: medicalData.applyThresholdAsync(Number(thresholdLow.text), Number(thresholdHigh.text))
                            }
                        }

                        ColumnLayout {
                            id: regionGrowingControls
                            spacing: 10
                            property real growLower: Number(growLow.text)
                            property real growUpper: Number(growHigh.text)
                            property bool growRangeValid: isFinite(growLower) && isFinite(growUpper)
                                                          && growLower <= growUpper
                            property bool seedInGrowRange: medicalData.regionGrowingSeedValid
                                                           && growRangeValid
                                                           && medicalData.regionGrowingSeedValue >= growLower
                                                           && medicalData.regionGrowingSeedValue <= growUpper
                            Text {
                                Layout.fillWidth: true
                                text: medicalData.regionGrowingSeedValid
                                      ? "IJK  " + medicalData.regionGrowingSeedX + ", "
                                        + medicalData.regionGrowingSeedY + ", "
                                        + medicalData.regionGrowingSeedZ + "    "
                                        + medicalData.regionGrowingSeedValue + " HU"
                                      : "尚未选择种子点"
                                color: medicalData.regionGrowingSeedValid ? Theme.text : Theme.textMuted
                                font.pixelSize: 14
                                font.weight: Font.DemiBold
                            }
                            ActionButton {
                                text: root.seedPicking ? "取消取点" : (medicalData.regionGrowingSeedValid ? "重新选取种子" : "在切片中选取种子")
                                active: root.seedPicking
                                Layout.fillWidth: true
                                enabled: medicalData.volumeData && medicalBackendEnabled && !medicalData.busy
                                onClicked: root.seedPickingRequested(!root.seedPicking)
                            }
                            Text {
                                visible: root.seedPicking
                                Layout.fillWidth: true
                                text: "取点模式已开启：在任一二维切片的目标组织内左键单击一次，不要拖动。"
                                color: Theme.accent
                                font.pixelSize: 13
                                wrapMode: Text.WordWrap
                            }
                            RowLayout {
                                Layout.fillWidth: true
                                TextField { id: growLow; Layout.fillWidth: true; text: "-100"; placeholderText: "下限 HU"; validator: DoubleValidator {} }
                                TextField { id: growHigh; Layout.fillWidth: true; text: "200"; placeholderText: "上限 HU"; validator: DoubleValidator {} }
                            }
                            Text {
                                visible: medicalData.regionGrowingSeedValid
                                         && !regionGrowingControls.seedInGrowRange
                                Layout.fillWidth: true
                                text: regionGrowingControls.growRangeValid
                                      ? "当前种子值不在生长范围内，请调整 HU 上下限。"
                                      : "HU 下限必须小于或等于上限。"
                                color: Theme.danger
                                font.pixelSize: 12
                                wrapMode: Text.WordWrap
                            }
                            ComboBox {
                                id: connectivity
                                Layout.fillWidth: true
                                model: ["6 邻域（面连接）", "26 邻域（全连接）"]
                            }
                            ActionButton {
                                text: "执行种子生长"
                                primary: true
                                Layout.fillWidth: true
                                enabled: regionGrowingControls.seedInGrowRange
                                         && medicalBackendEnabled && !medicalData.busy
                                onClicked: medicalData.applyRegionGrowingFromSeedAsync(
                                    Number(growLow.text), Number(growHigh.text), connectivity.currentIndex === 1)
                            }
                            ActionButton {
                                text: "清除种子点"
                                Layout.fillWidth: true
                                enabled: medicalData.regionGrowingSeedValid && !medicalData.busy
                                onClicked: medicalData.clearRegionGrowingSeed()
                            }
                        }
                    }
                    Rectangle { Layout.fillWidth: true; height: 1; color: Theme.border }
                    CheckBox {
                        text: "显示分割叠加"
                        checked: root.showSegmentation
                        enabled: medicalData.segmentationAvailable
                        onToggled: root.segmentationVisibilityRequested(checked)
                    }
                    ActionButton {
                        text: "清除分割结果"
                        Layout.fillWidth: true
                        enabled: medicalData.segmentationAvailable && !medicalData.busy
                        onClicked: medicalData.clearSegmentation()
                    }
                    Text {
                        visible: medicalData.segmentationAvailable
                        Layout.fillWidth: true
                        text: medicalData.segmentationMethod + "  ·  "
                              + medicalData.segmentationVoxelCount + " 体素  ·  "
                              + Number(medicalData.segmentationVolumeMl).toFixed(2) + " mL"
                        color: Theme.textSecondary
                        font.pixelSize: 12
                        wrapMode: Text.WordWrap
                    }
                    Text { text: "分割显示颜色"; color: Theme.text; font.pixelSize: 13; font.weight: Font.DemiBold }
                    ComboBox {
                        Layout.fillWidth: true
                        model: ["橙色", "米色", "红色", "蓝色", "玫红色"]
                        onActivated: {
                            const colors = ["#F0783C", "#F2C078", "#E27D60", "#55B7D9", "#D85C8B"]
                            root.segmentationColorRequested(colors[index])
                        }
                    }
                }
            }

            Flickable {
                contentHeight: volumeColumn.implicitHeight
                clip: true
                ColumnLayout {
                    id: volumeColumn
                    width: parent.width
                    spacing: 10
                    Text { text: "三维渲染模式"; color: Theme.text; font.pixelSize: 15; font.weight: Font.DemiBold }
                    ComboBox {
                        Layout.fillWidth: true
                        model: ["体绘制", "最大值投影 MIP"]
                        currentIndex: root.mip ? 1 : 0
                        onActivated: index => root.mipRequested(index === 1)
                    }
                    Text { text: "体绘制预设"; color: Theme.text; font.pixelSize: 15; font.weight: Font.DemiBold }
                    ComboBox {
                        Layout.fillWidth: true
                        model: ["胸部增强", "骨骼", "肺部", "软组织", "血管 / 增强"]
                        currentIndex: root.volumePreset
                        enabled: !root.mip
                        onActivated: index => root.volumePresetRequested(index)
                    }
                    Text {
                        Layout.fillWidth: true
                        text: root.mip
                              ? "MIP 沿观察方向保留最高密度体素。"
                              : "三维颜色与透明度曲线已联动当前窗宽窗位，可用顶部窗宽窗位工具在切片中拖动调整。"
                        color: Theme.textSecondary
                        font.pixelSize: 13
                        wrapMode: Text.WordWrap
                    }
                    Text {
                        visible: !root.mip
                        Layout.fillWidth: true
                        text: "3D 映射  W" + Math.round(medicalData.windowWidth)
                              + " / L" + Math.round(medicalData.displayWindowLevel)
                        color: Theme.accent
                        font.pixelSize: 13
                        font.weight: Font.DemiBold
                    }
                    Text { text: "Z 轴裁剪下界  " + Math.round(root.cropMinimum * 100) + "%"; color: Theme.textSecondary; font.pixelSize: 13 }
                    Slider {
                        id: cropLow
                        Layout.fillWidth: true
                        from: 0; to: Math.max(0, cropHigh.value - 0.02)
                        value: root.cropMinimum
                        onMoved: root.cropRequested(value, cropHigh.value)
                    }
                    Text { text: "Z 轴裁剪上界  " + Math.round(root.cropMaximum * 100) + "%"; color: Theme.textSecondary; font.pixelSize: 13 }
                    Slider {
                        id: cropHigh
                        Layout.fillWidth: true
                        from: Math.min(1, cropLow.value + 0.02); to: 1
                        value: root.cropMaximum
                        onMoved: root.cropRequested(cropLow.value, value)
                    }
                    Text {
                        Layout.fillWidth: true
                        text: "拖动 3D 视图旋转，滚轮缩放。分割结果以当前颜色半透明表面叠加。"
                        color: Theme.textSecondary
                        font.pixelSize: 13
                        wrapMode: Text.WordWrap
                    }
                }
            }

            ColumnLayout {
                spacing: 10
                Text { text: "病例包导出"; color: Theme.text; font.pixelSize: 15; font.weight: Font.DemiBold }
                Text {
                    Layout.fillWidth: true
                    text: "导出独立病例文件夹：原始 DICOM、三维分割掩膜、测量/标注、窗宽窗位和关键操作记录。再次导入此文件夹可恢复工作区。"
                    color: Theme.textSecondary
                    font.pixelSize: 13
                    wrapMode: Text.WordWrap
                }
                Text {
                    Layout.fillWidth: true
                    text: "当前为软件病例包，不是 DICOM SEG/SR；不得作为临床互操作或诊断归档使用。"
                    color: Theme.accent
                    font.pixelSize: 12
                    wrapMode: Text.WordWrap
                }
                ActionButton { text: "导出病例包"; primary: true; Layout.fillWidth: true; enabled: medicalData.loaded; onClicked: root.requestExport() }
                Item { Layout.fillHeight: true }
            }
        }
    }
}
