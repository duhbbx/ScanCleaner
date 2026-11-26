import QtQuick
import QtQuick.Controls
Button {
    id: control
    property bool primary: false
    implicitHeight: 36
    implicitWidth: Math.max(76, label.implicitWidth + 26)
    padding: 10
    font.pixelSize: 13
    contentItem: Text {
        id: label
        text: control.text
        font: control.font
        horizontalAlignment: Text.AlignHCenter
        verticalAlignment: Text.AlignVCenter
        color: !control.enabled ? "#9aa5b4" : control.primary ? "white" : "#28384d"
    }
    background: Rectangle {
        radius: 6
        color: !control.enabled ? "#e8edf3" : control.primary ? (control.down ? "#174dae" : control.hovered ? "#275fc2" : "#316dd9") : (control.down ? "#e5ecf5" : control.hovered ? "#f0f4fa" : "white")
        border.width: control.activeFocus ? 2 : 1
        border.color: control.activeFocus ? "#316dd9" : control.primary && control.enabled ? "transparent" : "#d2dae5"
    }
}
