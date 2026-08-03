import QtQuick
import QtQuick.Controls
import QtQuick.Layouts
import GuangSuo.CT

Item {
    id: root
    signal backRequested()
    signal continueRequested()
    readonly property bool allChecked: door.checked && emergency.checked && detector.checked
                                       && generator.checked && motion.checked && room.checked

    ColumnLayout {
        anchors.fill: parent
        anchors.margins: 40
        spacing: 22

        ColumnLayout {
            spacing: 4
            Text { text: "STEP 02 / 04"; color: Theme.accent; font.pixelSize: 13; font.weight: Font.DemiBold }
            Text { text: "联锁确认与设备检查"; color: Theme.text; font.pixelSize: 27; font.weight: Font.DemiBold }
            Text { text: "曝光与采集前确认安全回路、探测器和机房状态。"; color: Theme.textSecondary; font.pixelSize: 15 }
        }

        RowLayout {
            Layout.fillWidth: true
            Layout.fillHeight: true
            spacing: 24

            Rectangle {
                Layout.fillWidth: true
                Layout.fillHeight: true
                color: "transparent"
                ColumnLayout {
                    anchors.fill: parent
                    spacing: 10
                    Text { text: "设备联锁"; color: Theme.text; font.pixelSize: 18; font.weight: Font.DemiBold }
                    CheckBox { id: door; text: "机房门联锁闭合" }
                    CheckBox { id: emergency; text: "急停回路释放且自检通过" }
                    CheckBox { id: detector; text: "探测器在线，温度与校准状态正常" }
                    CheckBox { id: generator; text: "高压发生器在线且参数范围有效" }
                    CheckBox { id: motion; text: "运动机构回零，无超限或碰撞风险" }
                    CheckBox { id: room; text: "机房清场，患者体位稳定" }
                    Item { Layout.fillHeight: true }
                }
            }

            Rectangle {
                Layout.preferredWidth: 430
                Layout.fillHeight: true
                color: Theme.panel
                border.color: root.allChecked ? Theme.success : Theme.border
                radius: Theme.radius
                ColumnLayout {
                    anchors.fill: parent
                    anchors.margins: 24
                    spacing: 14
                    Text { text: "曝光准备摘要"; color: Theme.text; font.pixelSize: 18; font.weight: Font.DemiBold }
                    StatusPill { text: root.allChecked ? "6 / 6 项通过" : "等待逐项确认"; tone: root.allChecked ? Theme.success : Theme.accent }
                    Text { text: "患者"; color: Theme.textMuted; font.pixelSize: 13 }
                    Text { text: medicalData.patientName + " · " + medicalData.patientId; color: Theme.text; font.pixelSize: 16 }
                    Text { text: "检查数据"; color: Theme.textMuted; font.pixelSize: 13 }
                    Text { text: medicalData.modality + " · " + medicalData.dimensionsText; color: Theme.text; font.pixelSize: 15 }
                    Rectangle { Layout.fillWidth: true; height: 1; color: Theme.border }
                    Text {
                        Layout.fillWidth: true
                        text: root.allChecked
                              ? "联锁条件已确认。进入下一步后仍需完成扫描范围校验。"
                              : "任一未确认项目都会阻止进入扫描范围设置。"
                        color: root.allChecked ? Theme.success : Theme.accent
                        wrapMode: Text.WordWrap
                        font.pixelSize: 14
                    }
                    Item { Layout.fillHeight: true }
                }
            }
        }

        RowLayout {
            Layout.fillWidth: true
            ActionButton { text: "返回患者确认"; onClicked: root.backRequested() }
            Item { Layout.fillWidth: true }
            ActionButton {
                text: "完成联锁检查"
                primary: true
                enabled: root.allChecked
                onClicked: root.continueRequested()
            }
        }
    }
}
