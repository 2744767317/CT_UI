import QtQuick
import QtQuick.Controls
import QtQuick.Layouts
import GuangSuo.CT

Dialog {
    id: root
    property int selectedIndex: -1
    property string modalityFilter: "ALL"
    property string searchText: ""
    modal: true
    focus: true
    closePolicy: Popup.CloseOnEscape
    width: Math.min(920, parent ? parent.width - 96 : 920)
    height: Math.min(720, parent ? parent.height - 96 : 720)
    anchors.centerIn: parent
    padding: 0

    function matches(choice) {
        const modalityMatches = modalityFilter === "ALL"
                                || (modalityFilter === "XRAY" && choice.modality !== "CT")
                                || choice.modality === modalityFilter
        const query = searchText.trim().toLowerCase()
        if (query.length === 0)
            return modalityMatches
        return modalityMatches
                && (String(choice.patientName).toLowerCase().includes(query)
                    || String(choice.patientId).toLowerCase().includes(query)
                    || String(choice.description).toLowerCase().includes(query))
    }

    function selectFirstVisible() {
        for (let i = 0; i < medicalData.seriesChoices.length; ++i) {
            const choice = medicalData.seriesChoices[i]
            if (matches(choice)) {
                selectedIndex = choice.index
                return
            }
        }
        selectedIndex = -1
    }

    function visibleCount() {
        let count = 0
        for (let i = 0; i < medicalData.seriesChoices.length; ++i) {
            if (matches(medicalData.seriesChoices[i]))
                ++count
        }
        return count
    }

    onOpened: {
        if (selectedIndex < 0 || selectedIndex >= medicalData.seriesChoices.length)
            selectFirstVisible()
    }
    onModalityFilterChanged: selectFirstVisible()

    background: Rectangle {
        color: Theme.panel
        border.color: Theme.borderStrong
        radius: Theme.radius
    }

    contentItem: ColumnLayout {
        spacing: 0

        Rectangle {
            Layout.fillWidth: true
            Layout.preferredHeight: 72
            color: Theme.panelRaised
            border.color: Theme.border
            Column {
                anchors.left: parent.left
                anchors.leftMargin: 20
                anchors.verticalCenter: parent.verticalCenter
                spacing: 3
                Text { text: "选择患者与影像序列"; color: Theme.text; font.pixelSize: 20; font.weight: Font.DemiBold }
                Text { text: "同一目录可包含多个患者、CT 重建序列和 X 线投影"; color: Theme.textSecondary; font.pixelSize: 13 }
            }
        }

        Rectangle {
            Layout.fillWidth: true
            Layout.preferredHeight: 38
            color: Theme.app
            RowLayout {
                anchors.fill: parent
                anchors.leftMargin: 16
                anchors.rightMargin: 16
                spacing: 12
                Text { text: "患者"; color: Theme.textMuted; font.pixelSize: 12; Layout.preferredWidth: 190 }
                Text { text: "模态 / 序列"; color: Theme.textMuted; font.pixelSize: 12; Layout.fillWidth: true }
                Text { text: "矩阵"; color: Theme.textMuted; font.pixelSize: 12; Layout.preferredWidth: 150 }
                Text { text: "实例"; color: Theme.textMuted; font.pixelSize: 12; Layout.preferredWidth: 70; horizontalAlignment: Text.AlignRight }
            }
        }

        Rectangle {
            Layout.fillWidth: true
            Layout.preferredHeight: 54
            color: Theme.panel
            border.color: Theme.border
            RowLayout {
                anchors.fill: parent
                anchors.leftMargin: 12
                anchors.rightMargin: 12
                spacing: 6
                Repeater {
                    model: [["全部", "ALL"], ["CT", "CT"], ["X 线", "XRAY"]]
                    delegate: ActionButton {
                        required property var modelData
                        text: modelData[0]
                        checkable: true
                        checked: root.modalityFilter === modelData[1]
                        active: checked
                        onClicked: root.modalityFilter = modelData[1]
                    }
                }
                Item { Layout.fillWidth: true }
                TextField {
                    Layout.preferredWidth: 280
                    placeholderText: "搜索患者姓名、ID 或序列"
                    onTextChanged: {
                        root.searchText = text
                        root.selectFirstVisible()
                    }
                }
            }
        }

        ListView {
            id: seriesList
            Layout.fillWidth: true
            Layout.fillHeight: true
            clip: true
            model: medicalData.seriesChoices
            currentIndex: root.selectedIndex
            spacing: 2

            delegate: Rectangle {
                required property int index
                required property var modelData
                property bool matchesFilter: root.matches(modelData)
                width: ListView.view.width
                height: matchesFilter ? 70 : 0
                visible: matchesFilter
                color: index === root.selectedIndex ? Theme.control : "transparent"
                border.color: index === root.selectedIndex ? Theme.accent : Theme.border

                RowLayout {
                    anchors.fill: parent
                    anchors.leftMargin: 16
                    anchors.rightMargin: 16
                    spacing: 12

                    ColumnLayout {
                        Layout.preferredWidth: 190
                        spacing: 2
                        Text {
                            Layout.fillWidth: true
                            text: modelData.patientName.length > 0 ? modelData.patientName : "未提供姓名"
                            color: Theme.text
                            font.pixelSize: 14
                            font.weight: Font.DemiBold
                            elide: Text.ElideRight
                        }
                        Text { text: modelData.patientId; color: Theme.textSecondary; font.pixelSize: 12 }
                    }
                    ColumnLayout {
                        Layout.fillWidth: true
                        spacing: 2
                        Text {
                            text: modelData.modality + "  " + modelData.description
                            color: modelData.modality === "CT" ? Theme.volume : Theme.accent
                            font.pixelSize: 14
                            font.weight: Font.DemiBold
                            Layout.fillWidth: true
                            elide: Text.ElideRight
                        }
                        Text {
                            text: modelData.sourceDirectory
                            color: Theme.textMuted
                            font.pixelSize: 11
                            Layout.fillWidth: true
                            elide: Text.ElideMiddle
                        }
                    }
                    Text { text: modelData.dimensions; color: Theme.textSecondary; font.pixelSize: 13; Layout.preferredWidth: 150 }
                    Text {
                        text: modelData.instanceCount
                        color: Theme.text
                        font.pixelSize: 13
                        Layout.preferredWidth: 70
                        horizontalAlignment: Text.AlignRight
                    }
                }

                MouseArea {
                    anchors.fill: parent
                    onClicked: root.selectedIndex = index
                    onDoubleClicked: {
                        root.selectedIndex = index
                        root.close()
                        medicalData.selectSeries(index)
                    }
                }
            }

            ScrollBar.vertical: ScrollBar {}
        }

        Rectangle { Layout.fillWidth: true; Layout.preferredHeight: 1; color: Theme.border }
        RowLayout {
            Layout.fillWidth: true
            Layout.margins: 16
            Text {
                Layout.fillWidth: true
                text: root.visibleCount() + " / " + medicalData.seriesChoices.length + " 个可加载影像对象"
                color: Theme.textSecondary
                font.pixelSize: 13
            }
            ActionButton { text: "取消"; onClicked: root.close() }
            ActionButton {
                text: "载入所选影像"
                primary: true
                enabled: root.selectedIndex >= 0
                onClicked: {
                    const index = root.selectedIndex
                    root.close()
                    medicalData.selectSeries(index)
                }
            }
        }
    }
}
