import QtQuick
import QtQuick.Controls.Basic as Basic

Basic.ScrollBar {
    id: control
    policy: Basic.ScrollBar.AsNeeded
    minimumSize: 0.12
    implicitWidth: 7
    contentItem: Rectangle { implicitWidth: 5; implicitHeight: 24; radius: 2.5; color: control.pressed ? Theme.accent : "#737770"; opacity: control.size < 1 ? 0.85 : 0 }
    background: Rectangle { color: Theme.wellDark; opacity: control.size < 1 ? 0.3 : 0; radius: 3 }
}
