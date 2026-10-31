import QtQuick
import QtQuick.Controls.Basic as Basic

Basic.Slider {
    id: control
    implicitHeight: 26
    leftPadding: 7; rightPadding: 7
    hoverEnabled: true
    background: Rectangle {
        x: control.leftPadding; y: (control.height - height) / 2
        width: control.availableWidth; height: 3; radius: 1.5
        color: Theme.line
        Rectangle { width: control.visualPosition * parent.width; height: parent.height; radius: 1.5; color: control.enabled ? Theme.accent : Theme.muted }
    }
    handle: Rectangle {
        x: control.leftPadding + control.visualPosition * (control.availableWidth - width)
        y: (control.height - height) / 2
        implicitWidth: 12; implicitHeight: 12; radius: 6
        color: !control.enabled ? "#737570" : control.pressed ? Theme.accent : Theme.text
        border.width: control.activeFocus ? 2 : 1
        border.color: control.activeFocus ? Theme.accent : Theme.card
    }
}
