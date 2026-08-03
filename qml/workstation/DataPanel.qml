import QtQuick
import QtQuick.Controls
import QtQuick.Layouts
import GuangSuo.CT

Rectangle {
    id: root
    signal requestFolderImport()
    signal requestFileImport()
    signal requestExport()
    color: Theme.panel
    border.color: Theme.border

    ColumnLayout {
        anchors.fill: parent
        anchors.margins: 14
        spacing: 10

        Text { text: "当前检查"; color: Theme.text; font.pixelSize: 17; font.weight: Font.DemiBold }
        Text { text: medicalData.patientName; color: Theme.text; font.pixelSize: 20; font.weight: Font.DemiBold; elide: Text.ElideRight; Layout.fillWidth: true }
        Text { text: medicalData.patientId + " · " + medicalData.modality; color: Theme.textSecondary; font.pixelSize: 13 }

        RowLayout {
            Layout.fillWidth: true
            spacing: 6
            ActionButton { text: "导入"; Layout.fillWidth: true; onClicked: root.requestFolderImport() }
            ActionButton { text: "单文件"; Layout.fillWidth: true; onClicked: root.requestFileImport() }
            ActionButton { text: "导出"; Layout.fillWidth: true; enabled: medicalData.loaded; onClicked: root.requestExport() }
        }

        Rectangle { Layout.fillWidth: true; height: 1; color: Theme.border }
        Text { text: "数据与派生对象"; color: Theme.text; font.pixelSize: 15; font.weight: Font.DemiBold }

        ListView {
            Layout.fillWidth: true
            Layout.fillHeight: true
            clip: true
            model: [
                [medicalData.patientName, 0, true],
                [medicalData.studyDescription, 1, true],
                [medicalData.seriesDescription, 2, true],
                [medicalData.dimensionsText, 3, false],
                [medicalData.segmentationAvailable ? "Segmentation · 可见" : "Segmentation · 空", 2, medicalData.segmentationAvailable],
                ["Measurements", 2, false]
            ]
            delegate: Rectangle {
                required property var modelData
                required property int index
                width: ListView.view.width
                height: 36
                color: index === 2 ? Theme.control : "transparent"
                Text {
                    anchors.left: parent.left
                    anchors.leftMargin: 8 + modelData[1] * 16
                    anchors.right: parent.right
                    anchors.verticalCenter: parent.verticalCenter
                    text: (modelData[2] ? "●  " : "○  ") + modelData[0]
                    color: modelData[2] ? Theme.text : Theme.textMuted
                    font.pixelSize: 13
                    elide: Text.ElideRight
                }
            }
        }

        Rectangle {
            Layout.fillWidth: true
            implicitHeight: statusText.implicitHeight + 18
            color: medicalData.errorMessage.length > 0 ? "#2C1D1D" : Theme.app
            border.color: medicalData.errorMessage.length > 0 ? Theme.danger : Theme.border
            radius: Theme.radius
            Text {
                id: statusText
                anchors.fill: parent
                anchors.margins: 9
                text: medicalData.errorMessage.length > 0 ? medicalData.errorMessage : medicalData.statusMessage
                color: medicalData.errorMessage.length > 0 ? Theme.danger : Theme.textSecondary
                font.pixelSize: 12
                wrapMode: Text.WordWrap
            }
        }
    }
}
