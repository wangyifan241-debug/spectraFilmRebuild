import QtQuick
import QtQuick.Controls
import QtQuick.Layouts
import QtQuick.Dialogs

Dialog {
    id: root
    objectName: "materialManager"
    title: "材料库 · 内置与自定义版本"
    modal: true
    standardButtons: Dialog.Close
    property string selectedId: "open_kodak_trix_7266_033"
    readonly property var rows: preview.materialEntries.filter(function(r) {
        return category.currentIndex === 0 || (category.currentIndex === 1 && !r.custom) || (category.currentIndex === 2 && r.custom && !r.removed) || (category.currentIndex === 3 && r.custom && r.removed)
    })
    readonly property var selected: preview.materialEntries.find(function(r){return r.id === root.selectedId}) || ({})
    function importUrl(url) { preview.importMaterial(url) }
    FileDialog {
        id: importer
        title: "导入完整 Profile 或材料包 pack.json"
        fileMode: FileDialog.OpenFile
        nameFilters: ["材料 JSON (*.json)"]
        onVisibleChanged: preview.setDialogOpen(visible)
        onAccepted: { var file = selectedFile; Qt.callLater(function(){preview.setDialogOpen(false);root.importUrl(file)}) }
    }
    contentItem: ColumnLayout {
        spacing: 10
        RowLayout {
            Layout.fillWidth: true
            EditorComboBox { id: category; objectName: "materialCategory"; model: ["全部材料", "内置材料", "自定义材料", "已移除的历史版本"]; Layout.preferredWidth: 190 }
            ActionButton { objectName: "importMaterialButton"; text: "导入 Profile / 材料包"; enabled: preview.ready && !preview.busy; onClicked: importer.open() }
            ActionButton { text: "重新扫描"; enabled: preview.ready && !preview.busy; onClicked: preview.rescanMaterials() }
        }
        Text { Layout.fillWidth: true; text: "导入生成新版本，不覆盖内置数据。许可为来源声明，非授权认证。"; wrapMode: Text.Wrap; color: Theme.muted; font.pixelSize: 11 }
        RowLayout {
            Layout.fillWidth: true; Layout.fillHeight: true; spacing: 12
            ListView {
                id: materials; objectName: "materialManagerList"
                Layout.preferredWidth: 300; Layout.fillHeight: true; clip: true; model: root.rows
                ScrollBar.vertical: EditorScrollBar {}
                delegate: ItemDelegate {
                    required property var modelData
                    width: materials.width; highlighted: root.selectedId === modelData.id
                    text: (modelData.custom ? "自定义 · " : "内置 · ") + modelData.name + "\n" + modelData.version + (modelData.available ? (modelData.enabled && !modelData.removed ? "" : " · 已禁用/存档") : " · 缺失/损坏")
                    onClicked: root.selectedId = modelData.id
                }
            }
            ScrollView {
                Layout.fillWidth: true; Layout.fillHeight: true; clip: true
                TextArea {
                    objectName: "materialDetails"
                    readOnly: true; selectByMouse: true; wrapMode: TextEdit.Wrap; color: Theme.text
                    text: root.selected.id ? "名称："+root.selected.name+"\nID："+root.selected.id+"\n原材料 ID："+root.selected.logicalId+"\n版本："+root.selected.version+"\n类型："+root.selected.stage+" / "+(root.selected.support || "未声明载体")+"\nSHA-256："+root.selected.sha256+"\n\n来源：\n"+root.selected.source+"\n\n数据质量：\n"+root.selected.quality+"\n\n许可声明：\n"+root.selected.license+(root.selected.error ? "\n\n错误："+root.selected.error : "") : "请选择材料"
                }
            }
        }
        RowLayout {
            Layout.fillWidth: true
            ActionButton { objectName: "useMaterialButton"; text: "用于当前照片"; accented: true; enabled: preview.settingsEditable && root.selected.available === true && root.selected.enabled === true && !root.selected.removed; onClicked: preview.chooseMaterial(root.selectedId) }
            ActionButton { objectName: "toggleMaterialButton"; text: root.selected.enabled ? "禁用新选择" : "重新启用"; enabled: preview.ready && !preview.busy && root.selected.custom === true && root.selected.available; onClicked: preview.setMaterialEnabled(root.selectedId,!root.selected.enabled) }
            ActionButton { objectName: "removeMaterialButton"; text: "从可选列表移除"; enabled: preview.ready && !preview.busy && root.selected.custom === true && !root.selected.removed; onClicked: preview.removeMaterial(root.selectedId) }
        }
        Text { Layout.fillWidth: true; text: "禁用/移除不销毁历史版本。材料包不包含全局光谱表或算法；未知 CDF、显影时间家族和别名依赖会拒绝导入。"; wrapMode: Text.Wrap; color: Theme.muted; font.pixelSize: 11 }
        Text { objectName: "materialOperationStatus"; Layout.fillWidth: true; text: preview.status; wrapMode: Text.Wrap; color: preview.failed ? "#edaa76" : Theme.muted; font.pixelSize: 11 }
    }
}
