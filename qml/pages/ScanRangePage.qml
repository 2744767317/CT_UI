import QtQuick
import QtQuick.Controls
import QtQuick.Layouts
import GuangSuo.CT

Item {
    id: root
    signal backRequested()
    signal continueRequested()

    ColumnLayout {
        anchors.fill: parent
        anchors.margins: 32
        spacing: 18

        ColumnLayout {
            spacing: 4
            Text { text: "STEP 03 / 04"; color: Theme.accent; font.pixelSize: 13; font.weight: Font.DemiBold }
            Text { text: "扫描范围与参考平面"; color: Theme.text; font.pixelSize: 27; font.weight: Font.DemiBold }
            Text { text: "在 AP/LAT 定位图上设置上下界；当前版本使用数据范围预览，不触发真实曝光。"; color: Theme.textSecondary; font.pixelSize: 15 }
        }

        RowLayout {
            Layout.fillWidth: true
            Layout.fillHeight: true
            spacing: 12

            Repeater {
                model: ["AP 定位", "LAT 定位"]
                delegate: Rectangle {
                    required property string modelData
                    Layout.fillWidth: true
                    Layout.fillHeight: true
                    color: Theme.image
                    border.color: Theme.border
                    radius: Theme.radius
                    Canvas {
                        anchors.fill: parent
                        anchors.margins: 24
                        onPaint: {
                            const ctx = getContext("2d")
                            ctx.clearRect(0, 0, width, height)
                            ctx.strokeStyle = "#536067"
                            ctx.lineWidth = 2
                            ctx.beginPath()
                            ctx.arc(width * 0.5, height * 0.16, Math.min(width, height) * 0.075, 0, Math.PI * 2)
                            ctx.moveTo(width * 0.5, height * 0.24)
                            ctx.lineTo(width * 0.5, height * 0.88)
                            ctx.moveTo(width * 0.24, height * 0.34)
                            ctx.lineTo(width * 0.76, height * 0.34)
                            ctx.stroke()
                            ctx.fillStyle = "#22DD8335"
                            const upper = height * (1.0 - upperRange.value)
                            const lower = height * (1.0 - lowerRange.value)
                            ctx.fillRect(0, upper, width, lower - upper)
                            ctx.strokeStyle = Theme.accent
                            ctx.beginPath()
                            ctx.moveTo(0, upper); ctx.lineTo(width, upper)
                            ctx.moveTo(0, lower); ctx.lineTo(width, lower)
                            ctx.stroke()
                        }
                        Connections {
                            target: upperRange
                            function onValueChanged() { parent.requestPaint() }
                        }
                        Connections {
                            target: lowerRange
                            function onValueChanged() { parent.requestPaint() }
                        }
                    }
                    Text { anchors.left: parent.left; anchors.top: parent.top; anchors.margins: 12; text: modelData; color: Theme.text; font.pixelSize: 14; font.weight: Font.DemiBold }
                }
            }

            Rectangle {
                Layout.preferredWidth: 360
                Layout.fillHeight: true
                color: Theme.panel
                border.color: Theme.border
                radius: Theme.radius
                ColumnLayout {
                    anchors.fill: parent
                    anchors.margins: 22
                    spacing: 12
                    Text { text: "范围参数"; color: Theme.text; font.pixelSize: 18; font.weight: Font.DemiBold }
                    Text { text: "上界  " + Math.round(upperRange.value * 200) + " mm"; color: Theme.text; font.pixelSize: 14 }
                    Slider { id: upperRange; Layout.fillWidth: true; from: 0.55; to: 0.95; value: 0.84 }
                    Text { text: "下界  " + Math.round(lowerRange.value * 200) + " mm"; color: Theme.text; font.pixelSize: 14 }
                    Slider { id: lowerRange; Layout.fillWidth: true; from: 0.05; to: 0.5; value: 0.18 }
                    Rectangle { Layout.fillWidth: true; height: 1; color: Theme.border }
                    Text { text: "参考平面"; color: Theme.text; font.pixelSize: 15; font.weight: Font.DemiBold }
                    ComboBox { Layout.fillWidth: true; model: ["椎体中心平面", "冠状面", "设备等中心平面"] }
                    CheckBox { text: "头先进"; checked: true }
                    Text {
                        Layout.fillWidth: true
                        text: upperRange.value > lowerRange.value + 0.1
                              ? "扫描范围有效"
                              : "上下界距离不足"
                        color: upperRange.value > lowerRange.value + 0.1 ? Theme.success : Theme.danger
                        font.pixelSize: 14
                    }
                    Item { Layout.fillHeight: true }
                }
            }
        }

        RowLayout {
            Layout.fillWidth: true
            ActionButton { text: "返回联锁检查"; onClicked: root.backRequested() }
            Item { Layout.fillWidth: true }
            ActionButton {
                text: "确认范围并进入工作站"
                primary: true
                enabled: upperRange.value > lowerRange.value + 0.1
                onClicked: root.continueRequested()
            }
        }
    }
}
