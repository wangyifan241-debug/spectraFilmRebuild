import QtQuick
import QtQuick.Controls.Basic as Basic

Basic.ComboBox {
    id: control
    implicitHeight: 30
    leftPadding: 10; rightPadding: 27
    hoverEnabled: true
    font.pixelSize: 11
    background: Rectangle {
        radius: 5; color: control.down ? "#464945" : Theme.well
        border.width: 1; border.color: control.activeFocus ? Theme.accent : control.hovered ? "#777b74" : Theme.line
    }
    contentItem: Text {
        text: control.displayText; font: control.font
        color: control.enabled ? Theme.text : Theme.muted
        verticalAlignment: Text.AlignVCenter; elide: Text.ElideRight
    }
    indicator: Text {
        x: control.width - width - 10; y: (control.height - height) / 2
        text: "⌄"; font.pixelSize: 15; color: control.enabled ? Theme.muted : Theme.line
    }
    popup: Basic.Popup {
        y: control.height + 4; width: control.width
        implicitHeight: Math.min(contentItem.implicitHeight + 10, 330)
        padding: 5
        background: Rectangle { radius: 6; color: Theme.card; border.color: Theme.line }
        contentItem: ListView {
            implicitHeight: contentHeight; clip: true
            model: control.popup.visible ? control.delegateModel : null
            currentIndex: control.highlightedIndex
            Basic.ScrollIndicator.vertical: Basic.ScrollIndicator {}
        }
    }
    delegate: Basic.ItemDelegate {
        width: control.width - 10; implicitHeight: 30
        text: control.textAt(index); font: control.font
        contentItem: Text { text: parent.text; font: control.font; color: highlighted ? Theme.selectionText : Theme.text; elide: Text.ElideRight; verticalAlignment: Text.AlignVCenter }
        background: Rectangle { radius: 4; color: highlighted ? Theme.selection : "transparent" }
        highlighted: control.highlightedIndex === index
    }
}
