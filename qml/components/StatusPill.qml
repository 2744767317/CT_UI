import QtQuick
import GuangSuo.CT

Rectangle {
    id: root
    property string text: ""
    property color tone: Theme.textSecondary
    implicitWidth: label.implicitWidth + 22
    implicitHeight: 28
    radius: 3
    color: Qt.rgba(tone.r, tone.g, tone.b, 0.12)
    border.color: Qt.rgba(tone.r, tone.g, tone.b, 0.42)

    Text {
        id: label
        anchors.centerIn: parent
        text: root.text
        color: root.tone
        font.pixelSize: 13
        font.weight: Font.Medium
    }
}
