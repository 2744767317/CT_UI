import QtQuick
import QtQuick.Controls
import QtQuick.Layouts
import GuangSuo.CT

Rectangle {
    id: root
    color: "transparent"

    // 标注列表数据来自 annotationController.items（当前活动数据集的 scene）。
    // 逐个标注的显隐/删除直接调用 annotationController 的 Q_INVOKABLE 方法，
    // 渲染管线已按 item["visible"] 过滤，无需额外接线。

    ColumnLayout {
        anchors.fill: parent
        spacing: 6

        RowLayout {
            Layout.fillWidth: true
            spacing: 6
            Text {
                text: "标注"
                color: Theme.text
                font.pixelSize: 15
                font.weight: Font.DemiBold
            }
            Text {
                Layout.fillWidth: true
                text: medicalData.loaded ? medicalData.seriesDescription : ""
                color: Theme.textSecondary
                font.pixelSize: 11
                elide: Text.ElideRight
            }
            // 全部显隐总开关：与“测量与标注”图层开关联动。
            Switch {
                checked: annotationController.visible
                onToggled: annotationController.visible = checked
            }
            ActionButton {
                text: "清空"
                enabled: annotationController.items.length > 0
                onClicked: annotationController.clearAll()
            }
        }

        ListView {
            id: tree
            Layout.fillWidth: true
            Layout.preferredHeight: 200
            clip: true
            spacing: 2
            model: annotationController.items

            delegate: Rectangle {
                required property var modelData
                width: ListView.view.width
                height: 30
                color: rowMouse.containsMouse ? Theme.panelRaised : "transparent"
                border.color: Theme.border
                border.width: 0

                RowLayout {
                    anchors.fill: parent
                    anchors.leftMargin: 4
                    anchors.rightMargin: 4
                    spacing: 6

                    // 眼睛图标：逐个显隐
                    CheckBox {
                        checked: modelData.visible
                        onToggled: annotationController.setNodeVisible(modelData.id, checked)
                    }

                    // 颜色块
                    Rectangle {
                        Layout.preferredWidth: 12
                        Layout.preferredHeight: 12
                        radius: 2
                        color: modelData.color
                        border.color: Theme.border
                        border.width: 1
                    }

                    // 类型图标（点/线/角/闭合）
                    Text {
                        Layout.preferredWidth: 16
                        text: {
                            switch (modelData.type) {
                            case 0: return "●"   // Point
                            case 1: return "─"   // Line
                            case 2: return "∠"   // Angle
                            case 3: return "◯"   // ClosedCurve
                            }
                            return "?"
                        }
                        color: Theme.textSecondary
                        font.pixelSize: 12
                        horizontalAlignment: Text.AlignHCenter
                    }

                    Text {
                        Layout.preferredWidth: 36
                        text: modelData.label
                        color: Theme.text
                        font.pixelSize: 12
                        font.weight: Font.DemiBold
                    }

                    Text {
                        Layout.fillWidth: true
                        text: modelData.displayText
                        color: Theme.textSecondary
                        font.pixelSize: 11
                        elide: Text.ElideRight
                    }

                    // 删除单条标注
                    ActionButton {
                        Layout.preferredWidth: 28
                        text: "×"
                        onClicked: annotationController.removeNode(modelData.id)
                    }
                }

                HoverHandler { id: rowMouse }
            }

            Text {
                visible: tree.count === 0
                anchors.centerIn: parent
                text: "无标注"
                color: Theme.textMuted
                font.pixelSize: 12
            }
        }
    }
}
