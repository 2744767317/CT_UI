import QtQuick
import QtQuick.Controls
import QtQuick.Dialogs
import QtQuick.Layouts
import GuangSuo.CT

ApplicationWindow {
    id: window
    width: 1920
    height: 1080
    minimumWidth: 1440
    minimumHeight: 820
    visible: true
    title: "光索科技 CT 影像工作站"
    color: Theme.app
    palette.window: Theme.app
    palette.windowText: Theme.text
    palette.text: Theme.text
    palette.button: Theme.control
    palette.buttonText: Theme.text
    palette.base: Theme.control
    palette.alternateBase: Theme.panelRaised
    palette.highlight: Theme.accent
    palette.highlightedText: "#16191B"
    palette.placeholderText: Theme.textMuted

    function importFolder() { importFolderDialog.open() }
    function importFile() { importFileDialog.open() }
    function exportDicom() { exportFolderDialog.open() }
    function chooseSeries() { seriesDialog.open() }

    FolderDialog {
        id: importFolderDialog
        title: "选择 DICOM 序列目录"
        onAccepted: medicalData.importDicomAsync(selectedFolder)
    }
    FileDialog {
        id: importFileDialog
        title: "选择 DICOM 文件"
        nameFilters: ["DICOM files (*.dcm *.dicom)", "All files (*)"]
        onAccepted: medicalData.importDicomAsync(selectedFile)
    }
    FolderDialog {
        id: exportFolderDialog
        title: "选择 DICOM 副本导出目录"
        onAccepted: medicalData.exportDicomCopy(selectedFolder)
    }

    SeriesSelectionDialog { id: seriesDialog }
    Connections {
        target: medicalData
        function onSeriesChoicesChanged() {
            if (medicalData.seriesChoices.length > 1 && medicalData.selectedSeriesIndex < 0)
                Qt.callLater(function() { seriesDialog.open() })
        }
    }

    ColumnLayout {
        anchors.fill: parent
        spacing: 0

        Rectangle {
            Layout.fillWidth: true
            height: 68
            color: Theme.panelRaised
            border.color: Theme.border

            RowLayout {
                anchors.fill: parent
                anchors.leftMargin: 18
                anchors.rightMargin: 14
                spacing: 18

                ColumnLayout {
                    spacing: 0
                    Text { text: "光索科技"; color: Theme.text; font.pixelSize: 22; font.weight: Font.Bold }
                    Text { text: "CT 影像与三维工作站"; color: Theme.textSecondary; font.pixelSize: 12 }
                }

                Rectangle { width: 1; Layout.fillHeight: true; Layout.topMargin: 12; Layout.bottomMargin: 12; color: Theme.border }

                ColumnLayout {
                    Layout.fillWidth: true
                    spacing: 1
                    Text {
                        text: medicalData.loaded ? medicalData.patientName + "  ·  " + medicalData.patientId : "未载入患者"
                        color: medicalData.loaded ? Theme.text : Theme.textMuted
                        font.pixelSize: 17
                        font.weight: Font.DemiBold
                    }
                    Text {
                        text: medicalData.loaded ? medicalData.studyDescription + "  ·  " + medicalData.seriesDescription : "请从患者确认页导入 DICOM 检查"
                        color: Theme.textSecondary
                        font.pixelSize: 12
                        elide: Text.ElideRight
                        Layout.fillWidth: true
                    }
                }

                StatusPill { text: workflowController.stateName; tone: workflowController.locked ? Theme.danger : Theme.accent }
                StatusPill { text: medicalBackendEnabled ? "成像后端就绪" : "UI 兼容模式"; tone: medicalBackendEnabled ? Theme.success : Theme.accent }
                StatusPill { text: medicalData.loaded ? medicalData.modality + " 已载入" : "无影像"; tone: medicalData.loaded ? Theme.success : Theme.textMuted }
            }
        }

        Rectangle {
            Layout.fillWidth: true
            height: 54
            color: Theme.panel
            border.color: Theme.border

            Row {
                id: workflowRow
                anchors.fill: parent
                anchors.leftMargin: 18
                anchors.rightMargin: 18

                Repeater {
                    model: ["01  患者确认", "02  联锁检查", "03  扫描范围", "04  影像工作站"]
                    delegate: Button {
                        required property int index
                        required property string modelData
                        width: workflowRow.width / 4
                        height: workflowRow.height
                        text: modelData
                        enabled: workflowController.canVisit(index)
                        onClicked: workflowController.goToStep(index)
                        contentItem: Text {
                            text: parent.text
                            color: index === workflowController.currentStep ? Theme.accent
                                 : index < workflowController.currentStep ? Theme.success
                                 : Theme.textMuted
                            horizontalAlignment: Text.AlignHCenter
                            verticalAlignment: Text.AlignVCenter
                            font.pixelSize: 14
                            font.weight: index === workflowController.currentStep ? Font.DemiBold : Font.Medium
                        }
                        background: Rectangle {
                            color: "transparent"
                            Rectangle {
                                visible: index === workflowController.currentStep
                                anchors.left: parent.left
                                anchors.right: parent.right
                                anchors.bottom: parent.bottom
                                height: 3
                                color: Theme.accent
                            }
                        }
                    }
                }
            }
        }

        Loader {
            id: pageLoader
            Layout.fillWidth: true
            Layout.fillHeight: true
            sourceComponent: workflowController.currentStep === 0 ? patientComponent
                           : workflowController.currentStep === 1 ? safetyComponent
                           : workflowController.currentStep === 2 ? rangeComponent
                           : workstationComponent
        }
    }

    Component {
        id: patientComponent
        PatientPage {
            onRequestFolderImport: window.importFolder()
            onRequestFileImport: window.importFile()
            onRequestDemo: medicalData.loadDemoVolume()
            onRequestSeriesSelection: window.chooseSeries()
            onContinueRequested: workflowController.advance()
        }
    }
    Component {
        id: safetyComponent
        SafetyPage {
            onBackRequested: workflowController.back()
            onContinueRequested: workflowController.advance()
        }
    }
    Component {
        id: rangeComponent
        ScanRangePage {
            onBackRequested: workflowController.back()
            onContinueRequested: workflowController.advance()
        }
    }
    Component {
        id: workstationComponent
        WorkstationPage {
            onRequestFolderImport: window.importFolder()
            onRequestFileImport: window.importFile()
            onRequestExport: window.exportDicom()
        }
    }

    Rectangle {
        visible: medicalData.busy
        anchors.fill: parent
        color: "#B0000000"
        z: 100
        Column {
            anchors.centerIn: parent
            spacing: 12
            BusyIndicator { anchors.horizontalCenter: parent.horizontalCenter; running: true }
            Text { text: medicalData.statusMessage; color: Theme.text; font.pixelSize: 16 }
        }
    }
}
