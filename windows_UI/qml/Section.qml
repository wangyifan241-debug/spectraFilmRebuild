import QtQuick
import QtQuick.Layouts
ColumnLayout {
    id: root
    property string title
    property bool expanded: true
    default property alias content: rows.data
    Layout.fillWidth: true
    spacing: 7
    Rectangle { Layout.fillWidth: true; implicitHeight: 1; color: Theme.line; opacity: 0.65 }
    Item {
        Layout.fillWidth: true; implicitHeight: 25
        activeFocusOnTab: true
        Accessible.role: Accessible.Button
        Accessible.name: root.title
        Accessible.description: root.expanded ? "收起分组" : "展开分组"
        Keys.onSpacePressed: root.expanded = !root.expanded
        Text { x: 13; anchors.verticalCenter: parent.verticalCenter; text: root.expanded ? "▾" : "▸"; color: Theme.muted; font.pixelSize: 12 }
        Text { x: 29; anchors.verticalCenter: parent.verticalCenter; text: root.title; color: Theme.text; font.pixelSize: 12; font.bold: true }
        Rectangle { anchors.fill: parent; color: "transparent"; border.color: Theme.accent; border.width: parent.activeFocus ? 1 : 0; radius: 3 }
        TapHandler { onTapped: root.expanded = !root.expanded }
    }
    ColumnLayout { id: rows; visible: root.expanded; Layout.fillWidth: true; Layout.leftMargin: 14; Layout.rightMargin: 14; Layout.bottomMargin: 10; spacing: 6 }
}
