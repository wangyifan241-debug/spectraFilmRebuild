import QtQuick

Rectangle {
    id: badge
    property bool cine: false
    property bool customMaterial: false
    property bool selected: false
    objectName: cine ? "cineBadge" : "customMaterialBadge"
    visible: cine || customMaterial
    implicitWidth: cine ? 32 : 40; implicitHeight: 15; radius: 7
    color: selected ? "#51412c" : "transparent"
    border.width: 1; border.color: Theme.accent
    Text { anchors.centerIn: parent; text: badge.cine ? "CINE" : "自定义"; color: Theme.accent; font.pixelSize: 8; font.bold: true; font.letterSpacing: badge.cine ? 0.4 : 0 }
}
