import QtQuick
import QtQuick.Controls
import GuangSuo.CT

Button {
    id: control
    property bool primary: false
    property bool active: false
    implicitHeight: primary ? Theme.primaryHeight : Theme.controlHeight
    implicitWidth: Math.max(primary ? 148 : 88, contentItem.implicitWidth + 24)
    font.pixelSize: 15
    font.weight: primary ? Font.DemiBold : Font.Medium

    contentItem: Text {
        text: control.text
        color: control.enabled ? ((control.primary || control.active) ? "#16191B" : Theme.text) : Theme.textMuted
        font: control.font
        horizontalAlignment: Text.AlignHCenter
        verticalAlignment: Text.AlignVCenter
        elide: Text.ElideRight
    }

    background: Rectangle {
        radius: Theme.radius
        color: !control.enabled ? Theme.control
             : (control.primary || control.active) ? (control.hovered ? Theme.accentHover : Theme.accent)
             : control.pressed ? "#364047" : control.hovered ? "#30393E" : Theme.control
        border.color: (control.primary || control.active) && control.enabled ? Theme.accent : Theme.border
    }
}
