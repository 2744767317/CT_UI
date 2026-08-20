import QtQuick
import QtQuick.Controls
import QtQuick.Layouts
import GuangSuo.CT

Rectangle {
    id: root
    signal requestFolderImport()
    signal requestFileImport()
    signal requestExport()
    signal imageVisibilityRequested(bool visible)
    signal segmentationVisibilityRequested(bool visible)
    signal measurementVisibilityRequested(bool visible)
    signal segmentationOpacityRequested(real opacity)
    signal layerSelected(string layerId)

    property bool imageVisible: true
    property bool segmentationVisible: true
    property bool measurementsVisible: true
    property real segmentationOpacity: 0.72
    property string selectedLayer: "image"
    property int pendingRemoveVolume: -1

    color: Theme.panel
    border.color: Theme.border

    ColumnLayout {
        anchors.fill: parent
        anchors.margins: 14
        spacing: 10

        Text { text: "当前检查"; color: Theme.text; font.pixelSize: 17; font.weight: Font.DemiBold }
        Text { text: medicalData.patientName; color: Theme.text; font.pixelSize: 20; font.weight: Font.DemiBold; elide: Text.ElideRight; Layout.fillWidth: true }
        Text { text: medicalData.patientId + "  ·  " + medicalData.modality; color: Theme.textSecondary; font.pixelSize: 13 }

        RowLayout {
            Layout.fillWidth: true
            spacing: 6
            ActionButton { text: "导入"; Layout.fillWidth: true; enabled: !medicalData.busy; onClicked: root.requestFolderImport() }
            ActionButton { text: "单文件"; Layout.fillWidth: true; enabled: !medicalData.busy; onClicked: root.requestFileImport() }
            ActionButton { text: "导出"; Layout.fillWidth: true; enabled: medicalData.loaded && !medicalData.busy; onClicked: root.requestExport() }
        }

        Rectangle { Layout.fillWidth: true; height: 1; color: Theme.border }
        Text { text: "数据集"; color: Theme.text; font.pixelSize: 15; font.weight: Font.DemiBold }
        Text {
            visible: medicalData.seriesChoices.length > 0
            text: "已发现序列（点击载入）"
            color: Theme.textSecondary
            font.pixelSize: 12
        }
        ListView {
            id: seriesList
            visible: medicalData.seriesChoices.length > 0
            Layout.fillWidth: true
            Layout.preferredHeight: Math.min(78, Math.max(40, medicalData.seriesChoices.length * 38))
            clip: true
            spacing: 2
            model: medicalData.seriesChoices
            delegate: Rectangle {
                required property var modelData
                required property int index
                width: ListView.view.width
                height: 36
                color: modelData.index === medicalData.selectedSeriesIndex
                       ? Theme.control : "transparent"
                border.color: modelData.index === medicalData.selectedSeriesIndex
                              ? Theme.accent : Theme.border
                radius: 2
                RowLayout {
                    anchors.fill: parent
                    anchors.leftMargin: 7
                    anchors.rightMargin: 7
                    spacing: 6
                    Text {
                        text: modelData.modality
                        color: modelData.modality === "CT" ? Theme.volume : Theme.accent
                        font.pixelSize: 11
                        font.weight: Font.DemiBold
                    }
                    Text {
                        Layout.fillWidth: true
                        text: modelData.description + " · " + modelData.dimensions
                        color: Theme.text
                        font.pixelSize: 11
                        elide: Text.ElideRight
                    }
                    Text {
                        text: modelData.instanceCount
                        color: Theme.textMuted
                        font.pixelSize: 10
                    }
                }
                TapHandler {
                    enabled: !medicalData.busy
                    onTapped: medicalData.selectSeriesAsync(modelData.index)
                }
            }
            ScrollBar.vertical: ScrollBar {}
        }
        ListView {
            id: volumeList
            Layout.fillWidth: true
            Layout.preferredHeight: 126
            clip: true
            spacing: 3
            model: medicalData.volumeNodes
            delegate: Rectangle {
                required property var modelData
                required property int index
                width: ListView.view.width
                height: 50
                color: modelData.active ? Theme.control
                                        : (nodeMouse.containsMouse ? Theme.panelRaised : "transparent")
                border.color: modelData.active ? Theme.accent : Theme.border
                RowLayout {
                    anchors.fill: parent
                    anchors.leftMargin: 6
                    anchors.rightMargin: 8
                    spacing: 6
                    CheckBox {
                        checked: modelData.visible
                        enabled: !medicalData.busy
                        onClicked: medicalData.setVolumeVisibility(index, checked)
                    }
                    ColumnLayout {
                        Layout.fillWidth: true
                        spacing: 1
                        Text {
                            Layout.fillWidth: true
                            text: modelData.name
                            color: modelData.visible ? Theme.text : Theme.textMuted
                            font.pixelSize: 13
                            font.weight: modelData.active ? Font.DemiBold : Font.Normal
                            elide: Text.ElideRight
                        }
                        Text {
                            Layout.fillWidth: true
                            text: {
                                var base = modelData.modality + "  ·  " + modelData.dimensions
                                var mk = annotationController.markCountFor(modelData.id)
                                var ms = annotationController.measureCountFor(modelData.id)
                                if (mk + ms > 0)
                                    base += "  ·  标记:" + mk + " 测量:" + ms
                                return base
                            }
                            color: Theme.textSecondary
                            font.pixelSize: 11
                            elide: Text.ElideRight
                        }
                    }
                    Text {
                        text: modelData.segmentation ? "SEG" : (modelData.pairedProjection ? "PAIR" : "VOL")
                        color: modelData.segmentation ? Theme.accent : Theme.textMuted
                        font.pixelSize: 10
                    }
                }
                HoverHandler { id: nodeMouse }
                TapHandler {
                    enabled: !medicalData.busy
                    acceptedButtons: Qt.LeftButton
                    onTapped: medicalData.selectVolume(index)
                }
            }
            Text {
                visible: volumeList.count === 0
                anchors.centerIn: parent
                text: "尚未载入 Volume"
                color: Theme.textMuted
                font.pixelSize: 12
            }
        }

        RowLayout {
            Layout.fillWidth: true
            spacing: 6
            ActionButton {
                text: "重命名"
                Layout.fillWidth: true
                enabled: medicalData.selectedVolumeIndex >= 0 && !medicalData.busy
                onClicked: {
                    renameField.text = medicalData.volumeNodes[medicalData.selectedVolumeIndex].name
                    renameRow.visible = true
                    renameField.forceActiveFocus()
                    renameField.selectAll()
                }
            }
            ActionButton {
                text: "移除"
                Layout.fillWidth: true
                enabled: medicalData.selectedVolumeIndex >= 0 && !medicalData.busy
                onClicked: {
                    root.pendingRemoveVolume = medicalData.selectedVolumeIndex
                    removeDialog.open()
                }
            }
        }

        RowLayout {
            id: renameRow
            Layout.fillWidth: true
            visible: false
            TextField {
                id: renameField
                Layout.fillWidth: true
                selectByMouse: true
                enabled: !medicalData.busy
                onAccepted: {
                    if (medicalData.renameVolume(medicalData.selectedVolumeIndex, text))
                        renameRow.visible = false
                }
            }
            ActionButton {
                text: "保存"
                enabled: !medicalData.busy
                onClicked: {
                    if (medicalData.renameVolume(medicalData.selectedVolumeIndex, renameField.text))
                        renameRow.visible = false
                }
            }
        }

        // Slicer 风格标注树：逐个标注显隐/删除（作用于当前活动数据集）。
        MarkupsTreePanel {
            Layout.fillWidth: true
            Layout.preferredHeight: 240
        }

        Rectangle { Layout.fillWidth: true; height: 1; color: Theme.border }
        Text { text: "图层"; color: Theme.text; font.pixelSize: 15; font.weight: Font.DemiBold }

        ColumnLayout {
            Layout.fillWidth: true
            spacing: 4

            Rectangle {
                Layout.fillWidth: true
                height: 38
                color: root.selectedLayer === "image" ? Theme.control
                                                        : (imageLayerMouse.containsMouse ? Theme.control : "transparent")
                border.color: Theme.border
                RowLayout {
                    anchors.fill: parent
                    anchors.leftMargin: 8
                    anchors.rightMargin: 6
                    spacing: 6
                    CheckBox {
                        checked: root.imageVisible
                        enabled: medicalData.loaded && !medicalData.busy
                        onToggled: root.imageVisibilityRequested(checked)
                    }
                    Text { text: "原始影像"; color: Theme.text; Layout.fillWidth: true; font.pixelSize: 13 }
                    Text { text: medicalData.modality; color: Theme.textSecondary; font.pixelSize: 11 }
                }
                MouseArea { id: imageLayerMouse; anchors.fill: parent; z: -1; hoverEnabled: true; onClicked: { root.selectedLayer = "image"; root.layerSelected("image") } }
            }
            Rectangle {
                Layout.fillWidth: true
                height: 38
                color: root.selectedLayer === "segmentation" ? Theme.control
                                                               : (segmentationLayerMouse.containsMouse ? Theme.control : "transparent")
                border.color: Theme.border
                opacity: medicalData.segmentationAvailable ? 1.0 : 0.55
                RowLayout {
                    anchors.fill: parent
                    anchors.leftMargin: 8
                    anchors.rightMargin: 6
                    spacing: 6
                    CheckBox {
                        checked: root.segmentationVisible
                        enabled: medicalData.segmentationAvailable && !medicalData.busy
                        onToggled: root.segmentationVisibilityRequested(checked)
                    }
                    Text { text: "分割结果"; color: Theme.text; Layout.fillWidth: true; font.pixelSize: 13 }
                    Text { text: medicalData.segmentationAvailable ? "ITK" : "空"; color: Theme.textSecondary; font.pixelSize: 11 }
                }
                MouseArea { id: segmentationLayerMouse; anchors.fill: parent; z: -1; hoverEnabled: true; onClicked: { root.selectedLayer = "segmentation"; root.layerSelected("segmentation") } }
            }
            Rectangle {
                Layout.fillWidth: true
                height: 38
                color: root.selectedLayer === "measurements" ? Theme.control
                                                               : (measurementLayerMouse.containsMouse ? Theme.control : "transparent")
                border.color: Theme.border
                RowLayout {
                    anchors.fill: parent
                    anchors.leftMargin: 8
                    anchors.rightMargin: 6
                    spacing: 6
                    CheckBox {
                        checked: root.measurementsVisible
                        onToggled: root.measurementVisibilityRequested(checked)
                    }
                    Text { text: "测量与标注"; color: Theme.text; Layout.fillWidth: true; font.pixelSize: 13 }
                    Text { text: "局部"; color: Theme.textSecondary; font.pixelSize: 11 }
                }
                MouseArea { id: measurementLayerMouse; anchors.fill: parent; z: -1; hoverEnabled: true; onClicked: { root.selectedLayer = "measurements"; root.layerSelected("measurements") } }
            }
        }

        RowLayout {
            Layout.fillWidth: true
            visible: medicalData.segmentationAvailable
            Text { text: "分割不透明度"; color: Theme.textSecondary; font.pixelSize: 12 }
            Slider { Layout.fillWidth: true; from: 0.1; to: 1.0; value: root.segmentationOpacity; onMoved: root.segmentationOpacityRequested(value) }
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
        Item { Layout.fillHeight: true }
    }

    Dialog {
        id: removeDialog
        anchors.centerIn: parent
        width: 360
        modal: true
        title: "从工作区移除 Volume"
        standardButtons: Dialog.Cancel | Dialog.Ok
        onAccepted: {
            medicalData.removeVolume(root.pendingRemoveVolume)
            root.pendingRemoveVolume = -1
        }
        onRejected: root.pendingRemoveVolume = -1
        contentItem: Text {
            anchors.fill: parent
            anchors.margins: 16
            text: "仅从当前工作区移除，不会删除原始 DICOM 文件。"
            color: Theme.text
            font.pixelSize: 13
            wrapMode: Text.WordWrap
        }
    }
}
