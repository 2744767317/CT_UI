import QtQuick
import QtQuick.Controls
import QtQuick.Layouts
import GuangSuo.CT

Item {
    id: root
    signal requestFolderImport()
    signal requestFileImport()
    signal requestDemo()
    signal requestSeriesSelection()
    signal continueRequested()

    ColumnLayout {
        anchors.fill: parent
        anchors.margins: 32
        spacing: 20

        ColumnLayout {
            spacing: 4
            Text { text: "STEP 01 / 04"; color: Theme.accent; font.pixelSize: 13; font.weight: Font.DemiBold }
            Text { text: "导入检查并确认患者"; color: Theme.text; font.pixelSize: 27; font.weight: Font.DemiBold }
            Text { text: "读取 DICOM 标签后，使用姓名与患者 ID 完成双标识核对。"; color: Theme.textSecondary; font.pixelSize: 15 }
        }

        RowLayout {
            Layout.fillWidth: true
            Layout.fillHeight: true
            spacing: 20

            Rectangle {
                Layout.fillWidth: true
                Layout.fillHeight: true
                color: Theme.panel
                border.color: Theme.border
                radius: Theme.radius

                ColumnLayout {
                    anchors.fill: parent
                    anchors.margins: 24
                    spacing: 16

                    Text { text: "医学数据来源"; color: Theme.text; font.pixelSize: 18; font.weight: Font.DemiBold }
                    Text {
                        Layout.fillWidth: true
                        text: "递归识别多层 DICOM 目录。发现多个患者、CT 序列或 X 线投影时，将先选择影像再载入。"
                        wrapMode: Text.WordWrap
                        color: Theme.textSecondary
                        font.pixelSize: 14
                    }

                    Rectangle {
                        Layout.fillWidth: true
                        Layout.fillHeight: true
                        color: Theme.app
                        border.color: medicalData.loaded ? Theme.success : Theme.border
                        radius: Theme.radius

                        ColumnLayout {
                            anchors.centerIn: parent
                            width: Math.min(parent.width - 64, 620)
                            spacing: 12
                            Text {
                                Layout.alignment: Qt.AlignHCenter
                                text: medicalData.loaded ? "DICOM 数据已载入" : "尚未载入影像"
                                color: medicalData.loaded ? Theme.success : Theme.text
                                font.pixelSize: 20
                                font.weight: Font.DemiBold
                            }
                            Text {
                                Layout.fillWidth: true
                                horizontalAlignment: Text.AlignHCenter
                                text: medicalData.loaded ? medicalData.sourcePath : "选择一个检查目录或单个 DICOM 文件"
                                color: Theme.textSecondary
                                font.pixelSize: 13
                                wrapMode: Text.WrapAnywhere
                            }
                            RowLayout {
                                Layout.alignment: Qt.AlignHCenter
                                spacing: 8
                                ActionButton { text: "导入 DICOM 目录"; enabled: !medicalData.busy; onClicked: root.requestFolderImport() }
                                ActionButton { text: "导入单个文件"; enabled: !medicalData.busy; onClicked: root.requestFileImport() }
                                ActionButton {
                                    text: "选择已发现序列"
                                    visible: medicalData.seriesChoices.length > 1
                                    enabled: !medicalData.busy
                                    onClicked: root.requestSeriesSelection()
                                }
                                ActionButton { text: "载入演示 CT"; enabled: !medicalData.busy; onClicked: root.requestDemo() }
                            }
                            Text {
                                visible: !medicalBackendEnabled
                                Layout.fillWidth: true
                                horizontalAlignment: Text.AlignHCenter
                                text: "MinGW UI 模式未链接医学库，只能载入演示数据。"
                                color: Theme.accent
                                font.pixelSize: 13
                            }
                        }
                    }
                }
            }

            Rectangle {
                Layout.preferredWidth: 410
                Layout.fillHeight: true
                color: Theme.panel
                border.color: Theme.border
                radius: Theme.radius

                ColumnLayout {
                    anchors.fill: parent
                    anchors.margins: 22
                    spacing: 12

                    Text { text: "患者基本信息"; color: Theme.text; font.pixelSize: 18; font.weight: Font.DemiBold }
                    Repeater {
                        model: [
                            ["患者姓名", medicalData.loaded ? medicalData.patientName : "--"],
                            ["患者 ID", medicalData.loaded ? medicalData.patientId : "--"],
                            ["性别", medicalData.loaded ? medicalData.patientSex : "--"],
                            ["出生日期", medicalData.loaded ? medicalData.patientBirthDate : "--"],
                            ["模态", medicalData.loaded ? medicalData.modality : "--"],
                            ["检查", medicalData.loaded ? medicalData.studyDescription : "--"],
                            ["序列", medicalData.loaded ? medicalData.seriesDescription : "--"],
                            ["矩阵", medicalData.loaded ? medicalData.dimensionsText : "--"],
                            ["体素间距", medicalData.loaded ? medicalData.spacingText : "--"]
                        ]
                        delegate: RowLayout {
                            required property var modelData
                            Layout.fillWidth: true
                            Text { text: modelData[0]; color: Theme.textMuted; font.pixelSize: 13; Layout.preferredWidth: 92 }
                            Text {
                                text: modelData[1]
                                color: Theme.text
                                font.pixelSize: 14
                                Layout.fillWidth: true
                                horizontalAlignment: Text.AlignRight
                                elide: Text.ElideRight
                            }
                        }
                    }

                    Rectangle { Layout.fillWidth: true; height: 1; color: Theme.border }
                    Text { text: "双标识核对"; color: Theme.text; font.pixelSize: 16; font.weight: Font.DemiBold }
                    CheckBox { id: nameConfirmed; text: "患者姓名已由患者或腕带确认"; enabled: medicalData.loaded }
                    CheckBox { id: idConfirmed; text: "患者 ID 与申请单一致"; enabled: medicalData.loaded }
                    Text {
                        Layout.fillWidth: true
                        text: "不得使用床号、房间号或图像文件名替代患者身份。"
                        color: Theme.accent
                        wrapMode: Text.WordWrap
                        font.pixelSize: 13
                    }
                    Item { Layout.fillHeight: true }
                }
            }
        }

        RowLayout {
            Layout.fillWidth: true
            Text {
                Layout.fillWidth: true
                text: medicalData.errorMessage.length > 0 ? medicalData.errorMessage : medicalData.statusMessage
                color: medicalData.errorMessage.length > 0 ? Theme.danger : Theme.textSecondary
                font.pixelSize: 13
                elide: Text.ElideRight
            }
            ActionButton {
                text: "确认患者并继续"
                primary: true
                enabled: medicalData.loaded && nameConfirmed.checked && idConfirmed.checked
                onClicked: root.continueRequested()
            }
        }
    }
}
