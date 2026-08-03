import QtQuick
import QtQuick.Controls
import QtQuick.Layouts
import GuangSuo.CT

Rectangle {
    id: root
    property bool mip: false
    property bool showSegmentation: true
    property real cropMinimum: 0.0
    property real cropMaximum: 1.0
    signal mipRequested(bool enabled)
    signal segmentationVisibilityRequested(bool visible)
    signal cropRequested(real minimum, real maximum)
    signal requestExport()
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
            TabButton { text: "分割" }
            TabButton { text: "3D" }
            TabButton { text: "导出" }
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
                        from: 1; to: 3000
                        value: medicalData.windowWidth
                        onMoved: medicalData.windowWidth = value
                    }
                    Text { text: "窗位  " + Math.round(medicalData.windowLevel); color: Theme.textSecondary; font.pixelSize: 13 }
                    Slider {
                        Layout.fillWidth: true
                        from: -1200; to: 1800
                        value: medicalData.windowLevel
                        onMoved: medicalData.windowLevel = value
                    }
                    Text { text: "预设"; color: Theme.text; font.pixelSize: 15; font.weight: Font.DemiBold }
                    ComboBox {
                        Layout.fillWidth: true
                        model: ["软组织  W400 / L40", "肺窗  W1500 / L-600", "骨窗  W1800 / L400"]
                        onActivated: index => {
                            if (index === 0) { medicalData.windowWidth = 400; medicalData.windowLevel = 40 }
                            if (index === 1) { medicalData.windowWidth = 1500; medicalData.windowLevel = -600 }
                            if (index === 2) { medicalData.windowWidth = 1800; medicalData.windowLevel = 400 }
                        }
                    }
                    Text { text: "图像信息"; color: Theme.text; font.pixelSize: 15; font.weight: Font.DemiBold }
                    Text { text: medicalData.dimensionsText; color: Theme.textSecondary; font.pixelSize: 13 }
                    Text { text: medicalData.spacingText; color: Theme.textSecondary; font.pixelSize: 13 }
                }
            }

            Flickable {
                contentHeight: segmentationColumn.implicitHeight
                clip: true
                ColumnLayout {
                    id: segmentationColumn
                    width: parent.width
                    spacing: 10
                    Text { text: "阈值分割"; color: Theme.text; font.pixelSize: 15; font.weight: Font.DemiBold }
                    RowLayout {
                        Layout.fillWidth: true
                        TextField { id: thresholdLow; Layout.fillWidth: true; text: "300"; placeholderText: "下限 HU"; validator: DoubleValidator {} }
                        TextField { id: thresholdHigh; Layout.fillWidth: true; text: "2500"; placeholderText: "上限 HU"; validator: DoubleValidator {} }
                    }
                    ActionButton {
                        text: "应用阈值分割"
                        primary: true
                        Layout.fillWidth: true
                        enabled: medicalData.loaded
                        onClicked: medicalData.applyThreshold(Number(thresholdLow.text), Number(thresholdHigh.text))
                    }
                    Rectangle { Layout.fillWidth: true; height: 1; color: Theme.border }
                    Text { text: "种子生长"; color: Theme.text; font.pixelSize: 15; font.weight: Font.DemiBold }
                    RowLayout {
                        Layout.fillWidth: true
                        TextField { id: seedX; Layout.fillWidth: true; text: "96"; placeholderText: "X"; validator: IntValidator { bottom: 0 } }
                        TextField { id: seedY; Layout.fillWidth: true; text: "96"; placeholderText: "Y"; validator: IntValidator { bottom: 0 } }
                        TextField { id: seedZ; Layout.fillWidth: true; text: "80"; placeholderText: "Z"; validator: IntValidator { bottom: 0 } }
                    }
                    RowLayout {
                        Layout.fillWidth: true
                        TextField { id: growLow; Layout.fillWidth: true; text: "-100"; placeholderText: "下限"; validator: DoubleValidator {} }
                        TextField { id: growHigh; Layout.fillWidth: true; text: "200"; placeholderText: "上限"; validator: DoubleValidator {} }
                    }
                    ActionButton {
                        text: "执行种子生长"
                        Layout.fillWidth: true
                        enabled: medicalData.loaded && medicalBackendEnabled
                        onClicked: medicalData.applyRegionGrowing(Number(seedX.text), Number(seedY.text), Number(seedZ.text), Number(growLow.text), Number(growHigh.text))
                    }
                    CheckBox {
                        text: "显示分割叠加"
                        checked: root.showSegmentation
                        enabled: medicalData.segmentationAvailable
                        onToggled: root.segmentationVisibilityRequested(checked)
                    }
                    ActionButton { text: "清除分割"; Layout.fillWidth: true; enabled: medicalData.segmentationAvailable; onClicked: medicalData.clearSegmentation() }
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
                        text: "拖动 3D 视图旋转，滚轮缩放。分割结果以橙色半透明表面叠加。"
                        color: Theme.textSecondary
                        font.pixelSize: 13
                        wrapMode: Text.WordWrap
                    }
                }
            }

            ColumnLayout {
                spacing: 10
                Text { text: "DICOM 导出"; color: Theme.text; font.pixelSize: 15; font.weight: Font.DemiBold }
                Text {
                    Layout.fillWidth: true
                    text: "当前阶段导出经过完整性核对的原始 DICOM 实例副本，不修改患者标签或像素数据。"
                    color: Theme.textSecondary
                    font.pixelSize: 13
                    wrapMode: Text.WordWrap
                }
                ActionButton { text: "导出 DICOM 副本"; primary: true; Layout.fillWidth: true; enabled: medicalData.loaded; onClicked: root.requestExport() }
                Item { Layout.fillHeight: true }
            }
        }
    }
}
