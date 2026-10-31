import QtQuick
import QtQuick.Controls.Basic as Basic

Basic.CheckBox {
    id: control
    implicitHeight: 28; spacing: 8; leftPadding: 0; rightPadding: 0
    font.pixelSize: 11
    indicator: Rectangle {
        implicitWidth: 16; implicitHeight: 16
        x: control.leftPadding; y: (control.height - height) / 2
        radius: 3; color: control.checked ? Theme.accent : Theme.wellDark
        border.color: control.activeFocus ? Theme.text : control.checked ? Theme.accent : Theme.line
        Text { anchors.centerIn: parent; text: "✓"; visible: control.checked; font.pixelSize: 12; color: Theme.selectionText }
    }
    contentItem: Text {
        text: control.text; font: control.font
        leftPadding: control.indicator.width + control.spacing
        color: control.enabled ? Theme.text : Theme.muted
        verticalAlignment: Text.AlignVCenter; wrapMode: Text.WordWrap
    }
}
