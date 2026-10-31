import QtQuick
import QtQuick.Controls
import QtQuick.Layouts
import QtQuick.Dialogs
import SpektraLab.Native 1.0

ApplicationWindow {
    id: window
    objectName: "editorWindow"
    width: 1540; height: 980
    minimumWidth: 1100; minimumHeight: 740
    visible: true
    color: Theme.card
    title: (preview.hasFrame ? preview.fileName + " — " : "") + "SpektraLab"
    font.family: "Segoe UI"
    font.pixelSize: 12
    palette.window: Theme.card
    palette.windowText: Theme.text
    palette.text: Theme.text
    palette.buttonText: Theme.text
    palette.base: Theme.well
    palette.button: Theme.well
    palette.highlight: Theme.accent
    property bool closePending: false
    property bool cropMode: false
    property bool postMode: false
    property bool compareMode: false
    property bool originalMode: false
    property rect draftCrop: Qt.rect(0, 0, 1, 1)
    property int exportFormat: 0
    property int exportQuality: 95
    property int exportLongEdge: 0
    readonly property bool canEditCrop: preview.hasFrame && !preview.busy && !preview.batchExporting && !preview.filmFrame.pairActive
    readonly property bool canBrowse: preview.ready && !preview.busy && !preview.batchExporting && !cropMode
    // Text controls keep their own undo stack while they own keyboard focus.
    readonly property bool editingText: activeFocusItem !== null && typeof activeFocusItem.text === "string" && typeof activeFocusItem.undo === "function"
    onCropModeChanged: updateDialogState()

    function updateDialogState() {
        preview.setDialogOpen(cropMode || openDialog.visible || saveDialog.visible || batchDialog.visible || exportPopup.visible)
    }
    function startCrop() {
        if (!canEditCrop) return
        draftCrop = preview.cropRect
        cropMode = true
        Qt.callLater(function() { canvas.resetView(true) })
    }
    function finishCrop() {
        var next = draftCrop
        var commit = cropMode && canEditCrop
        cropMode = false
        if (commit) preview.setCropRect(next)
        Qt.callLater(function() { canvas.resetView(true) })
    }
    function cancelCrop() { cropMode = false; canvas.resetView(true) }
    function filmGroup(positive, monochrome) {
        var rows = []
        for (var i = 0; i < preview.films.length; ++i)
            if ((monochrome || preview.films[i].positive === positive) && preview.films[i].monochrome === monochrome)
                rows.push({ "name": preview.films[i].name, "catalogIndex": i, "enabled": preview.films[i].enabled, "cine": preview.films[i].cine, "custom": preview.films[i].custom })
        return rows
    }
    function openExport() { if (preview.hasFrame && !preview.busy && !cropMode) exportPopup.open() }
    function showExportOptions() { openExport() }
    function showSceneFit() {
        postMode = false
        Qt.callLater(function() {
            parameterScroll.contentItem.contentY = Math.max(0, sceneFitSection.y - 8)
        })
    }
    function showFilmFrame() {
        postMode = false
        Qt.callLater(function() { parameterScroll.contentItem.contentY = Math.max(0, filmFrameSection.y - 8) })
    }
    function formatFilter() {
        if (exportFormat === 3) return ["JPEG 图像 (*.jpg *.jpeg)"]
        if (exportFormat === 1 || exportFormat === 2) return ["PNG 图像 (*.png)"]
        return ["16 位 TIFF (*.tif *.tiff)"]
    }
    onClosing: function(event) {
        if (preview.batchExporting) preview.cancelExport()
        if (preview.busy) { closePending = true; event.accepted = false }
    }
    Connections {
        target: preview
        function onStateChanged() { if (window.closePending && !preview.busy) window.close() }
        function onSourceOpened() { window.cropMode = false; canvas.resetView(true) }
        function onCatalogChanged() { Qt.callLater(function() { filmQuickSelect.currentIndex = preview.filmIndex }) }
        function onSettingsChanged() { Qt.callLater(function() { filmQuickSelect.currentIndex = preview.filmIndex }) }
    }
    MaterialManager { id: materialManager; anchors.centerIn: parent; width: Math.min(920,window.width-40); height: Math.min(760,window.height-40) }
    FileDialog {
        id: openDialog
        title: "导入一张或多张 RAW 照片"
        fileMode: FileDialog.OpenFiles
        nameFilters: ["RAW 与双拼项目 (*.arw *.nef *.nrw *.dng *.cr2 *.cr3 *.raf *.rw2 *.orf *.pef *.srw *.spektrapair)", "所有文件 (*)"]
        onVisibleChanged: window.updateDialogState()
        onAccepted: preview.openFiles(selectedFiles)
    }
    FileDialog {
        id: saveDialog
        title: "导出当前画面 · 不覆盖现有文件"
        fileMode: FileDialog.SaveFile
        nameFilters: window.formatFilter()
        defaultSuffix: window.exportFormat === 3 ? "jpg" : window.exportFormat === 0 ? "tif" : "png"
        currentFolder: preview.sourceFolder
        onVisibleChanged: window.updateDialogState()
        onAccepted: preview.exportImage(selectedFile, window.exportFormat, window.exportQuality, window.exportLongEdge)
    }
    FolderDialog {
        id: batchDialog
        title: "选择批量导出文件夹 · 跳过已有文件"
        currentFolder: preview.sourceFolder
        onVisibleChanged: window.updateDialogState()
        onAccepted: preview.exportBatch(selectedFolder, window.exportFormat, window.exportQuality, window.exportLongEdge)
    }
    Shortcut { sequences: [StandardKey.Open]; enabled: window.canBrowse; onActivated: openDialog.open() }
    Shortcut { sequence: "Ctrl+E"; enabled: preview.hasFrame && !preview.busy && !window.cropMode; onActivated: window.openExport() }
    Shortcut { sequence: "Ctrl+0"; enabled: preview.hasFrame; onActivated: canvas.resetView(true) }
    Shortcut { sequence: "Ctrl+1"; enabled: preview.hasFrame && !window.cropMode; onActivated: canvas.resetView(false) }
    Shortcut { sequence: "Ctrl+Z"; enabled: preview.settingsEditable && preview.canUndo && !window.cropMode && !window.editingText; onActivated: preview.undo() }
    Shortcut { sequences: ["Ctrl+Shift+Z", "Ctrl+Y"]; enabled: preview.settingsEditable && preview.canRedo && !window.cropMode && !window.editingText; onActivated: preview.redo() }
    Shortcut { sequence: "Escape"; enabled: window.cropMode; onActivated: window.cancelCrop() }
    Shortcut { sequence: "Return"; enabled: window.cropMode; onActivated: window.finishCrop() }

    component Note: Text {
        Layout.fillWidth: true
        color: Theme.muted
        font.pixelSize: 10
        wrapMode: Text.WordWrap
        lineHeight: 1.35
    }
    component Adjustment: ColumnLayout {
        id: adjustment
        property string label
        property string setting
        property real from: 0
        property real to: 2
        property real step: 0.05
        property int decimals: 2
        property string suffix: ""
        property real fallback: 0
        property bool available: true
        property bool post: false
        readonly property var values: post ? preview.postAdjustments : preview.adjustments
        readonly property real current: values[setting] === undefined ? fallback : Number(values[setting])
        Layout.fillWidth: true; spacing: 2
        RowLayout {
            Layout.fillWidth: true
            Text { text: adjustment.label; color: adjustment.available ? Theme.text : Theme.muted; font.pixelSize: 11 }
            Item { Layout.fillWidth: true }
            Text { text: (adjustment.post && adjustmentSlider.pressed ? adjustmentSlider.from + adjustmentSlider.position * (adjustmentSlider.to - adjustmentSlider.from) : adjustment.current).toFixed(adjustment.decimals) + adjustment.suffix; color: Theme.muted; font.pixelSize: 10 }
        }
        EditorSlider {
            id: adjustmentSlider
            objectName: (adjustment.post ? "postAdjustmentSlider_" : "adjustmentSlider_") + adjustment.setting
            Layout.fillWidth: true; implicitHeight: 24
            live: !adjustment.post
            from: adjustment.from; to: adjustment.to; stepSize: adjustment.step
            value: adjustment.current
            enabled: preview.settingsEditable && adjustment.available && (!adjustment.post || (preview.hasFrame && !preview.busy))
            onPressedChanged: {
                if (pressed) preview.beginEditGesture(adjustment.setting)
                else {
                    if (adjustment.post) preview.setPostAdjustment(adjustment.setting, Math.max(from, Math.min(to, Math.round((from + position * (to - from)) / stepSize) * stepSize)))
                    preview.endEditGesture()
                }
            }
            onMoved: { if (adjustment.post) { if (!pressed) preview.setPostAdjustment(adjustment.setting, value) } else preview.setAdjustment(adjustment.setting, value) }
            ToolTip.visible: hovered && adjustment.post
            ToolTip.text: "拖动查看数值，松开后应用完整精度显影；方向键可微调"
        }
    }
    component Effect: ColumnLayout {
        id: effect
        property string label
        property string activeKey
        property string amountKey
        property real maximum: 2
        Layout.fillWidth: true; spacing: 1
        EditorCheckBox {
            text: effect.label; font.pixelSize: 11
            palette.windowText: Theme.text
            checked: preview.adjustments[effect.activeKey] === undefined ? true : preview.adjustments[effect.activeKey]
            enabled: preview.settingsEditable
            onToggled: preview.setAdjustment(effect.activeKey, checked)
        }
        Adjustment { label: "强度"; setting: effect.amountKey; fallback: 1; from: 0; to: effect.maximum; available: preview.adjustments[effect.activeKey] !== false }
    }
    component FilmGroup: ColumnLayout {
        id: group
        property bool positive: false
        property bool monochrome: false
        Layout.fillWidth: true; spacing: 3
        Text { text: group.monochrome ? "黑白 · Black & White" : group.positive ? "反转片 · Positive" : "负片 · Negative"; color: Theme.muted; font.pixelSize: 10; font.bold: true }
        ListView {
            id: list
            objectName: group.monochrome ? "monochromeFilmList" : group.positive ? "positiveFilmList" : "filmList"
            Layout.fillWidth: true; Layout.preferredHeight: Math.min(contentHeight, group.positive ? 120 : group.monochrome ? 150 : 240)
            model: window.filmGroup(group.positive, group.monochrome); clip: true
            currentIndex: {
                for (var i = 0; i < model.length; ++i)
                    if (model[i].catalogIndex === preview.filmIndex) return i
                return -1
            }
            onCurrentIndexChanged: { if (currentIndex >= 0) positionViewAtIndex(currentIndex, ListView.Contain) }
            onCountChanged: Qt.callLater(function() { if (list.currentIndex >= 0) list.positionViewAtIndex(list.currentIndex, ListView.Contain) })
            ScrollBar.vertical: EditorScrollBar {}
            delegate: Rectangle {
                required property var modelData
                width: list.width - 10; height: Math.max(29, filmLabel.implicitHeight + 8); radius: 5
                color: modelData.catalogIndex === preview.filmIndex ? Theme.selection : filmMouse.containsMouse ? "#41413e" : "transparent"
                Text {
                    id: filmLabel
                    anchors.left: parent.left; anchors.right: filmBadge.visible ? filmBadge.left : parent.right; anchors.verticalCenter: parent.verticalCenter; anchors.leftMargin: 8; anchors.rightMargin: 6
                    verticalAlignment: Text.AlignVCenter; text: modelData.name
                    color: modelData.catalogIndex === preview.filmIndex ? Theme.selectionText : preview.settingsEditable ? Theme.text : Theme.muted
                    font.pixelSize: 11; wrapMode: Text.WordWrap
                }
                StockBadge { id: filmBadge; anchors.right: parent.right; anchors.rightMargin: 5; anchors.verticalCenter: parent.verticalCenter; cine: modelData.cine; customMaterial: modelData.custom; selected: modelData.catalogIndex === preview.filmIndex }
                MouseArea { id: filmMouse; anchors.fill: parent; hoverEnabled: true; enabled: preview.settingsEditable && modelData.enabled; onClicked: preview.filmIndex = modelData.catalogIndex }
            }
        }
    }

    RowLayout {
        anchors.fill: parent; spacing: 1
        Rectangle {
            Layout.preferredWidth: Theme.leftWidth; Layout.fillHeight: true; color: Theme.card
            ColumnLayout {
                anchors.fill: parent; spacing: 0
                Text { text: "胶片与相纸"; color: Theme.text; font.pixelSize: 14; font.bold: true; Layout.leftMargin: 14; Layout.preferredHeight: Theme.topHeight; verticalAlignment: Text.AlignVCenter }
                ScrollView {
                    Layout.fillWidth: true; Layout.fillHeight: true; clip: true; contentWidth: availableWidth
                    ColumnLayout {
                        width: parent.width; spacing: 0
                        Section {
                            title: "导航图"
                            Rectangle {
                                Layout.fillWidth: true; Layout.preferredHeight: 132; color: Theme.wellDark
                                FrameCanvas { anchors.fill: parent; anchors.margins: 6; image: preview.previewImage; enabled: false }
                                Text { anchors.centerIn: parent; visible: !preview.hasFrame; text: "导入照片开始"; color: Theme.muted; font.pixelSize: 11 }
                            }
                            RowLayout {
                                ActionButton { text: "适合"; implicitHeight: 23; enabled: preview.hasFrame; onClicked: canvas.resetView(true) }
                                Item { Layout.fillWidth: true }
                                Text { text: preview.hasFrame ? canvas.scalePercent.toFixed(1) + "%" : ""; color: Theme.muted; font.pixelSize: 10 }
                            }
                        }
                        Section {
                            title: "胶片"
                            EditorComboBox {
                                id: filmQuickSelect
                                objectName: "filmQuickSelect"; Layout.fillWidth: true
                                model: preview.films; textRole: "name"; currentIndex: preview.filmIndex
                                displayText: (preview.films[preview.filmIndex] || {}).name || "选择胶片"
                                enabled: preview.settingsEditable
                                delegate: ItemDelegate {
                                    required property var modelData
                                    required property int index
                                    width: ListView.view.width; implicitHeight: 31; enabled: modelData.enabled
                                    highlighted: filmQuickSelect.highlightedIndex === index
                                    contentItem: Text { text: modelData.name; color: parent.highlighted ? Theme.selectionText : parent.enabled ? Theme.text : Theme.muted; font.pixelSize: 11; elide: Text.ElideRight; verticalAlignment: Text.AlignVCenter }
                                    background: Rectangle { radius: 4; color: parent.highlighted ? Theme.selection : parent.hovered ? Theme.well : "transparent" }
                                }
                                onActivated: preview.filmIndex = currentIndex
                                ToolTip.visible: hovered; ToolTip.text: "快速选择全部胶片，包括滚动列表之外的型号"
                            }
                            FilmGroup { positive: true }
                            FilmGroup { positive: false }
                            FilmGroup { monochrome: true }
                        }
                        Section {
                            title: "印相材料 · 相纸 / 印片"
                            Note { visible: preview.filmIsPositive; text: "反转片直接扫描，不使用相纸。"; color: Theme.accent }
                            Note { objectName: "printMaterialPolicyNote"; visible: preview.filmIsMonochrome && !preview.filmIsPositive; text: (preview.papers[preview.paperIndex] || {}).unfiltered2302 ? "黑白电影印片 · 不使用 Multigrade 滤片" : (preview.papers[preview.paperIndex] || {}).multigrade ? "银盐相纸 · Multigrade 滤镜 2" : "自定义黑白材料 · 沿用兼容滤片，需独立标定"; color: Theme.accent }
                            ListView {
                                id: papers; objectName: "paperList"
                                Layout.fillWidth: true; Layout.preferredHeight: 218; clip: true
                                model: preview.papers; currentIndex: preview.paperIndex
                                onCurrentIndexChanged: positionViewAtIndex(currentIndex, ListView.Contain)
                                onCountChanged: Qt.callLater(function() { papers.positionViewAtIndex(preview.paperIndex, ListView.Contain) })
                                ScrollBar.vertical: EditorScrollBar {}
                                delegate: Rectangle {
                                    required property var modelData
                                    required property int index
                                    readonly property bool compatible: !preview.filmIsPositive && modelData.monochrome === preview.filmIsMonochrome
                                    width: papers.width - 10; height: Math.max(29, paperLabel.implicitHeight + 8); radius: 5
                                    color: !preview.filmIsPositive && index === preview.paperIndex ? Theme.selection : paperMouse.containsMouse && paperMouse.enabled ? "#41413e" : "transparent"
                                    Text {
                                        id: paperLabel
                                        anchors.left: parent.left; anchors.right: paperBadge.visible ? paperBadge.left : parent.right; anchors.verticalCenter: parent.verticalCenter; anchors.leftMargin: 8; anchors.rightMargin: 6; verticalAlignment: Text.AlignVCenter; text: modelData.name
                                        color: !preview.filmIsPositive && index === preview.paperIndex ? Theme.selectionText : preview.settingsEditable && parent.compatible ? Theme.text : Theme.muted
                                        font.pixelSize: 11; wrapMode: Text.WordWrap
                                    }
                                    StockBadge { id: paperBadge; anchors.right: parent.right; anchors.rightMargin: 5; anchors.verticalCenter: parent.verticalCenter; cine: modelData.cine; customMaterial: modelData.custom; selected: !preview.filmIsPositive && index === preview.paperIndex }
                                    MouseArea { id: paperMouse; anchors.fill: parent; hoverEnabled: true; enabled: preview.settingsEditable && parent.compatible; onClicked: preview.paperIndex = index }
                                }
                            }
                        }
                        Section {
                            title: "裁切"
                            RowLayout {
                                Layout.fillWidth: true
                                EditorComboBox {
                                    id: cropAspect; Layout.fillWidth: true
                                    model: ["自由比例", "1:1", "3:2", "4:3", "16:9"]; enabled: window.canEditCrop
                                    currentIndex: {
                                        var c = preview.cropRect
                                        if (!preview.hasFrame || (c.x === 0 && c.y === 0 && c.width === 1 && c.height === 1)) return 0
                                        var ratio = c.width * preview.metadata.sourceWidth / (c.height * preview.metadata.sourceHeight)
                                        if (preview.quarterTurns % 2 !== 0) ratio = 1 / ratio
                                        var values = [0, 1, 1.5, 4 / 3, 16 / 9]
                                        for (var i = 1; i < values.length; ++i)
                                            if (Math.abs(ratio - values[i]) < 0.002) return i
                                        return 0
                                    }
                                    onActivated: { window.cropMode = false; preview.setCropAspect([0, 1, 1.5, 4 / 3, 16 / 9][currentIndex]); canvas.resetView(true) }
                                }
                                ActionButton { text: "重置"; enabled: window.canEditCrop; onClicked: { window.cropMode = false; preview.resetCrop(); canvas.resetView(true) } }
                            }
                            ActionButton { Layout.fillWidth: true; text: window.cropMode ? "完成裁切" : "在画面中拖选裁切"; accented: window.cropMode; enabled: window.canEditCrop; onClicked: window.cropMode ? window.finishCrop() : window.startCrop() }
                            Note { text: window.cropMode ? "在未旋转原图上拖出矩形；完成后恢复方向与拉直。Enter 确认，Esc 取消。" : "预览与导出使用同一裁切与方向；原始 RAW 不变。" }
                            RowLayout {
                                Layout.fillWidth: true
                                ActionButton { objectName: "rotateLeftButton"; Layout.fillWidth: true; text: "左转 90°"; enabled: window.canEditCrop && !window.cropMode; onClicked: { preview.rotateCounterClockwise(); canvas.resetView(true) } }
                                ActionButton { objectName: "rotateRightButton"; Layout.fillWidth: true; text: "右转 90°"; enabled: window.canEditCrop && !window.cropMode; onClicked: { preview.rotateClockwise(); canvas.resetView(true) } }
                            }
                            RowLayout {
                                Layout.fillWidth: true
                                ActionButton { objectName: "flipHorizontalButton"; Layout.fillWidth: true; text: "水平翻转"; accented: preview.flipHorizontal; enabled: window.canEditCrop && !window.cropMode; onClicked: preview.setFlipHorizontal(!preview.flipHorizontal) }
                                ActionButton { objectName: "flipVerticalButton"; Layout.fillWidth: true; text: "垂直翻转"; accented: preview.flipVertical; enabled: window.canEditCrop && !window.cropMode; onClicked: preview.setFlipVertical(!preview.flipVertical) }
                            }
                            RowLayout {
                                Layout.fillWidth: true
                                Text { text: "拉直"; color: Theme.text; font.pixelSize: 11 }
                                Item { Layout.fillWidth: true }
                                Text { text: (straightenSlider.pressed ? straightenSlider.from + straightenSlider.position * (straightenSlider.to - straightenSlider.from) : preview.straightenDegrees).toFixed(1) + "°"; color: Theme.muted; font.pixelSize: 10 }
                            }
                            EditorSlider {
                                id: straightenSlider; objectName: "straightenSlider"
                                Layout.fillWidth: true; implicitHeight: 24
                                from: -45; to: 45; stepSize: 0.1; live: false
                                value: preview.straightenDegrees
                                enabled: window.canEditCrop && !window.cropMode
                                onPressedChanged: {
                                    if (pressed) preview.beginEditGesture("straightenDegrees")
                                    else {
                                        preview.setStraightenDegrees(Math.round((from + position * (to - from)) * 10) / 10)
                                        preview.endEditGesture()
                                    }
                                }
                                onMoved: { if (!pressed) preview.setStraightenDegrees(value) }
                            }
                            Note { text: "拉直在松开滑块后应用。边缘自动收紧以避免空白。" }
                            ActionButton { objectName: "resetGeometryButton"; Layout.fillWidth: true; text: "重置裁切与方向"; enabled: window.canEditCrop && !window.cropMode; onClicked: { preview.resetGeometry(); canvas.resetView(true) } }
                        }
                        Section {
                            title: "放大机 · 相纸亮度"
                            RowLayout {
                                Layout.fillWidth: true
                                Text { text: "亮度"; color: Theme.text; font.pixelSize: 11 }
                                Item { Layout.fillWidth: true }
                                Text { text: preview.filmIsPositive ? "不适用" : (preview.exposureEv >= 0 ? "+" : "") + preview.exposureEv.toFixed(1) + " EV"; color: Theme.muted; font.pixelSize: 10 }
                            }
                            EditorSlider {
                                objectName: "exposureSlider"; Layout.fillWidth: true; from: -2; to: 2; stepSize: 0.1; implicitHeight: 24
                                value: preview.exposureEv; enabled: preview.settingsEditable && !preview.filmIsPositive
                                onPressedChanged: { if (pressed) preview.beginEditGesture("exposureEv"); else preview.endEditGesture() }
                                onMoved: preview.exposureEv = value
                            }
                            Adjustment { label: "黄色滤镜"; setting: "yFilterShift"; from: -1; to: 1; step: 0.01; decimals: 2; available: !preview.filmIsPositive && !preview.filmIsMonochrome }
                            Adjustment { label: "洋红滤镜"; setting: "mFilterShift"; from: -1; to: 1; step: 0.01; decimals: 2; available: !preview.filmIsPositive && !preview.filmIsMonochrome }
                        }
                    }
                }
            }
        }
        Rectangle {
            Layout.fillWidth: true; Layout.fillHeight: true; color: Theme.ground
            ColumnLayout {
                anchors.fill: parent; spacing: 0
                Rectangle {
                    Layout.fillWidth: true; implicitHeight: Theme.topHeight; color: Theme.card
                    RowLayout {
                        anchors.fill: parent; anchors.leftMargin: 8; anchors.rightMargin: 8; spacing: 6
                        ActionButton { text: "导入 RAW"; enabled: window.canBrowse; onClicked: openDialog.open(); implicitHeight: 27 }
                        ActionButton { objectName: "openMaterialManagerButton"; text: "材料库"; enabled: window.canBrowse; onClicked: materialManager.open(); implicitHeight: 27 }
                        ActionButton { text: "导出"; enabled: preview.hasFrame && !preview.busy && !window.cropMode; onClicked: window.openExport(); implicitHeight: 27 }
                        ActionButton { text: "裁切"; accented: window.cropMode; enabled: window.canEditCrop; onClicked: window.cropMode ? window.finishCrop() : window.startCrop(); implicitHeight: 27 }
                        ActionButton { objectName: "undoButton"; text: "撤销"; enabled: preview.settingsEditable && preview.canUndo && !window.cropMode; onClicked: preview.undo(); implicitHeight: 27; ToolTip.visible: hovered; ToolTip.text: "撤销当前照片的上一步调整 · Ctrl+Z" }
                        ActionButton { objectName: "redoButton"; text: "重做"; enabled: preview.settingsEditable && preview.canRedo && !window.cropMode; onClicked: preview.redo(); implicitHeight: 27; ToolTip.visible: hovered; ToolTip.text: "重做当前照片的调整 · Ctrl+Shift+Z / Ctrl+Y" }
                        Item { Layout.fillWidth: true }
                        Text { text: preview.hasFrame ? canvas.scalePercent.toFixed(1) + "%" : ""; color: Theme.muted; font.pixelSize: 10; horizontalAlignment: Text.AlignRight }
                        ActionButton { text: "适合"; accented: canvas.fit; enabled: preview.hasFrame; onClicked: canvas.resetView(true); implicitHeight: 27 }
                        ActionButton { text: "100%"; accented: !canvas.fit; enabled: preview.hasFrame && !window.cropMode; onClicked: canvas.resetView(false); implicitHeight: 27 }
                        ActionButton { text: "原图"; accented: window.originalMode; enabled: preview.hasFrame && !window.cropMode; onClicked: { window.originalMode = !window.originalMode;canvas.forceActiveFocus() } implicitHeight: 27 }
                        ActionButton { text: "分屏"; accented: window.compareMode; enabled: preview.hasFrame && !window.cropMode; onClicked: { window.compareMode = !window.compareMode;canvas.forceActiveFocus() } implicitHeight: 27 }
                    }
                }
                Rectangle {
                    objectName: "operationErrorBanner"
                    visible: preview.failed
                    Layout.fillWidth: true; implicitHeight: errorText.implicitHeight + 20
                    color: "#573b33"
                    Text {
                        id: errorText; anchors.fill: parent; anchors.margins: 10
                        text: "操作未完成，保留上一张成功画面：" + preview.status
                        color: "#ffd0c0"; font.pixelSize: 12; wrapMode: Text.Wrap
                    }
                }
                Item {
                    Layout.fillWidth: true; Layout.fillHeight: true
                    FrameCanvas {
                        id: canvas; objectName: "frameCanvas"; anchors.fill: parent; anchors.margins: 18
                        image: window.cropMode ? preview.uncroppedImage : preview.previewImage
                        beforeImage: (window.compareMode || window.originalMode || heldOriginal) && !window.cropMode ? preview.originalImage : preview.previewImage
                        compare: window.compareMode && !window.cropMode
                        showOriginal: (window.originalMode || heldOriginal) && !window.cropMode
                        property bool heldOriginal: false
                        enabled: !window.cropMode; focus: true
                        Keys.onPressed: function(event) { if(event.key===Qt.Key_Space && !event.isAutoRepeat) { heldOriginal=true;event.accepted=true } }
                        Keys.onReleased: function(event) { if(event.key===Qt.Key_Space) { heldOriginal=false;event.accepted=true } }
                        onActiveFocusChanged: if(!activeFocus)heldOriginal=false
                    }
                    Rectangle {
                        visible: canvas.compare && !canvas.showOriginal && preview.hasFrame
                        x: canvas.x + canvas.imageRect.x + canvas.imageRect.width * canvas.splitPosition
                        y: canvas.y + canvas.imageRect.y; width: 1; height: canvas.imageRect.height
                        color: Theme.text
                        MouseArea {
                            anchors.fill: parent; anchors.leftMargin: -7; anchors.rightMargin: -7
                            cursorShape: Qt.SplitHCursor
                            onPositionChanged: function(mouse) { if(pressed) { var p=mapToItem(canvas,mouse.x,mouse.y);canvas.splitPosition=(p.x-canvas.imageRect.x)/canvas.imageRect.width } }
                        }
                    }
                    Item {
                        id: cropOverlay
                        objectName: "cropOverlay"
                        anchors.fill: canvas
                        visible: window.cropMode
                        readonly property rect imageBox: canvas.imageRect
                        readonly property rect selectionBox: Qt.rect(imageBox.x + window.draftCrop.x * imageBox.width, imageBox.y + window.draftCrop.y * imageBox.height, window.draftCrop.width * imageBox.width, window.draftCrop.height * imageBox.height)
                        Rectangle { x: cropOverlay.imageBox.x; y: cropOverlay.imageBox.y; width: cropOverlay.imageBox.width; height: Math.max(0, cropOverlay.selectionBox.y - y); color: "#99000000" }
                        Rectangle { x: cropOverlay.imageBox.x; y: cropOverlay.selectionBox.y + cropOverlay.selectionBox.height; width: cropOverlay.imageBox.width; height: Math.max(0, cropOverlay.imageBox.y + cropOverlay.imageBox.height - y); color: "#99000000" }
                        Rectangle { x: cropOverlay.imageBox.x; y: cropOverlay.selectionBox.y; width: Math.max(0, cropOverlay.selectionBox.x - x); height: cropOverlay.selectionBox.height; color: "#99000000" }
                        Rectangle { x: cropOverlay.selectionBox.x + cropOverlay.selectionBox.width; y: cropOverlay.selectionBox.y; width: Math.max(0, cropOverlay.imageBox.x + cropOverlay.imageBox.width - x); height: cropOverlay.selectionBox.height; color: "#99000000" }
                        Rectangle {
                            x: cropOverlay.selectionBox.x; y: cropOverlay.selectionBox.y; width: cropOverlay.selectionBox.width; height: cropOverlay.selectionBox.height
                            color: "transparent"; border.color: Theme.accent; border.width: 1
                            Rectangle { x: parent.width / 3; width: 1; height: parent.height; color: "#70ffffff" }
                            Rectangle { x: parent.width * 2 / 3; width: 1; height: parent.height; color: "#70ffffff" }
                            Rectangle { y: parent.height / 3; height: 1; width: parent.width; color: "#70ffffff" }
                            Rectangle { y: parent.height * 2 / 3; height: 1; width: parent.width; color: "#70ffffff" }
                        }
                        MouseArea {
                            anchors.fill: parent; enabled: window.canEditCrop; cursorShape: Qt.CrossCursor
                            property point origin
                            property bool drawing: false
                            function normalized(x, y) {
                                return Qt.point(Math.max(0, Math.min(1, (x - cropOverlay.imageBox.x) / cropOverlay.imageBox.width)), Math.max(0, Math.min(1, (y - cropOverlay.imageBox.y) / cropOverlay.imageBox.height)))
                            }
                            function updateCrop(x, y) {
                                var p = normalized(x, y)
                                window.draftCrop = Qt.rect(Math.min(origin.x, p.x), Math.min(origin.y, p.y), Math.abs(p.x - origin.x), Math.abs(p.y - origin.y))
                            }
                            onPressed: function(mouse) {
                                var b = cropOverlay.imageBox
                                if (mouse.x < b.x || mouse.y < b.y || mouse.x > b.x + b.width || mouse.y > b.y + b.height) { mouse.accepted = false; return }
                                origin = normalized(mouse.x, mouse.y); drawing = true
                            }
                            onPositionChanged: function(mouse) { if (drawing) updateCrop(mouse.x, mouse.y) }
                            onReleased: function(mouse) {
                                if (!drawing) return
                                updateCrop(mouse.x, mouse.y); drawing = false
                                if (window.draftCrop.width > 0.003 && window.draftCrop.height > 0.003) window.finishCrop()
                                else window.draftCrop = preview.cropRect
                            }
                            onCanceled: { drawing = false; window.draftCrop = preview.cropRect }
                        }
                    }
                    Column {
                        anchors.centerIn: parent; spacing: 16; visible: !preview.hasFrame
                        Text { anchors.horizontalCenter: parent.horizontalCenter; text: "SpektraLab"; color: "#e5e2dc"; font.pixelSize: 32; font.letterSpacing: 1 }
                        Text { anchors.horizontalCenter: parent.horizontalCenter; text: "导入 RAW，逐张选择胶片与相纸"; color: "#d0cdc6"; font.pixelSize: 13 }
                        Text { anchors.horizontalCenter: parent.horizontalCenter; text: "支持一次拖入多张照片"; color: "#c0bdb7"; font.pixelSize: 11 }
                    }
                    DropArea { anchors.fill: parent; enabled: window.canBrowse; onDropped: function(drop) { if (drop.hasUrls) preview.openFiles(drop.urls) } }
                    Rectangle {
                        anchors.right: parent.right; anchors.top: parent.top; anchors.margins: 26
                        width: window.cropMode ? 78 : 48; height: 20; radius: 2; color: "#b32d2d2c"; visible: preview.hasFrame
                        Text { anchors.centerIn: parent; text: window.cropMode ? "裁切原图" : "PREVIEW"; color: Theme.text; font.pixelSize: 9; font.letterSpacing: 1 }
                    }
                    BusyIndicator { anchors.centerIn: parent; running: preview.busy; visible: running; width: 44; height: 44 }
                }
                Rectangle {
                    Layout.fillWidth: true; implicitHeight: 28; color: Theme.card
                    Text { anchors.fill: parent; anchors.leftMargin: 12; anchors.rightMargin: 12; verticalAlignment: Text.AlignVCenter; text: preview.hasFrame ? preview.metadata.width + " × " + preview.metadata.height + "   ·   SDR sRGB / 8 位预览   ·   " + (preview.dirty ? "更新预览中…" : "选择与调整自动应用") : "3400 像素编辑预览"; color: preview.dirty ? Theme.accent : Theme.muted; font.pixelSize: 10; elide: Text.ElideRight }
                }
                Rectangle {
                    Layout.fillWidth: true; implicitHeight: Theme.stripHeight; color: Theme.card
                    Rectangle { width: parent.width; height: 1; color: Theme.line; opacity: 0.5 }
                    ColumnLayout {
                        anchors.fill: parent; anchors.margins: 8; spacing: 6
                        RowLayout {
                            Layout.fillWidth: true; spacing: 6
                            Text { text: preview.libraryItems.length + " 张照片 · 已选 " + preview.selectedCount; color: Theme.muted; font.pixelSize: 10 }
                            Item { Layout.fillWidth: true }
                            ActionButton { objectName: "createPairButton"; text: "半格双拼"; implicitHeight: 23; font.pixelSize: 10; enabled: window.canBrowse && preview.hasFrame && preview.selectedCount === 2; onClicked: preview.createSelectedPair(); ToolTip.visible: hovered; ToolTip.text: "选两张 RAW 创建独立项目，原片与各自裁切保留。" }
                            ActionButton { text: "全选"; implicitHeight: 23; font.pixelSize: 10; enabled: window.canBrowse && preview.libraryItems.length > 0; onClicked: preview.selectAllPhotos(true) }
                            ActionButton { text: "清除选择"; implicitHeight: 23; font.pixelSize: 10; enabled: window.canBrowse && preview.selectedCount > 0; onClicked: preview.selectAllPhotos(false) }
                            ActionButton { text: "同步设置"; implicitHeight: 23; font.pixelSize: 10; enabled: window.canBrowse && preview.hasFrame && preview.selectedCount > 0; onClicked: preview.syncSelectedSettings(false); ToolTip.visible: hovered; ToolTip.text: "将当前照片的胶片、相纸、解码和效果同步到所选照片，不同步裁切。" }
                        }
                        ListView {
                            id: filmstrip; objectName: "photoStrip"
                            Layout.fillWidth: true; Layout.fillHeight: true
                            orientation: ListView.Horizontal; spacing: 8; clip: true
                            model: preview.libraryItems; currentIndex: preview.activeIndex
                            onCurrentIndexChanged: positionViewAtIndex(currentIndex, ListView.Contain)
                            ScrollBar.horizontal: ScrollBar {}
                            delegate: Rectangle {
                                required property var modelData
                                required property int index
                                width: 132; height: filmstrip.height - 3
                                color: Theme.wellDark; border.color: modelData.active ? Theme.accent : Theme.line; border.width: modelData.active ? 2 : 0
                                Text { anchors.centerIn: parent; anchors.verticalCenterOffset: -8; text: modelData.error ? "无法读取" : "点击预览"; color: modelData.error ? "#ffb39f" : Theme.muted; font.pixelSize: 10 }
                                FrameCanvas { anchors.fill: parent; anchors.margins: 5; anchors.bottomMargin: 22; image: modelData.thumbnail; enabled: false }
                                Text { anchors.left: parent.left; anchors.right: parent.right; anchors.bottom: parent.bottom; anchors.margins: 6; text: modelData.name; color: Theme.text; font.pixelSize: 10; elide: Text.ElideMiddle }
                                MouseArea {
                                    anchors.fill: parent; enabled: window.canBrowse; hoverEnabled: true
                                    onClicked: { window.cropMode = false; preview.selectImage(index) }
                                    ToolTip.visible: containsMouse && modelData.error.length > 0; ToolTip.text: modelData.error
                                }
                                EditorCheckBox { anchors.top: parent.top; anchors.right: parent.right; padding: 0; scale: 0.8; checked: modelData.selected; enabled: window.canBrowse; onToggled: preview.setItemSelected(index, checked) }
                            }
                        }
                    }
                }
            }
        }
        Rectangle {
            Layout.preferredWidth: Theme.rightWidth; Layout.fillHeight: true; color: Theme.card
            ColumnLayout {
                anchors.fill: parent; spacing: 0
                RowLayout {
                    Layout.fillWidth: true; Layout.preferredHeight: Theme.topHeight; Layout.leftMargin: 10; Layout.rightMargin: 10
                    Text { text: "参数"; color: Theme.text; font.pixelSize: 13; font.bold: true }
                    Item { Layout.fillWidth: true }
                    ActionButton { text: "显影前"; accented: !window.postMode; onClicked: window.postMode = false }
                    ActionButton { objectName: "postDevelopTab"; text: "显影后"; accented: window.postMode; onClicked: window.postMode = true }
                }
                ScrollView {
                    id: parameterScroll
                    Layout.fillWidth: true; Layout.fillHeight: true; clip: true; contentWidth: availableWidth
                    ColumnLayout {
                        width: parent.width; spacing: 0
                        Section {
                            title: "直方图 · 当前画面"
                            Canvas {
                                id: histogramPlot; objectName: "displayHistogram"
                                Layout.fillWidth: true; Layout.preferredHeight: 96
                                readonly property var histogramData: preview.histogram
                                onHistogramDataChanged: requestPaint()
                                onWidthChanged: requestPaint()
                                onHeightChanged: requestPaint()
                                onPaint: {
                                    var ctx = getContext("2d")
                                    ctx.clearRect(0, 0, width, height)
                                    ctx.fillStyle = Theme.wellDark
                                    ctx.fillRect(0, 0, width, height)
                                    ctx.strokeStyle = "#494946"; ctx.lineWidth = 1
                                    for (var grid = 1; grid < 4; ++grid) {
                                        ctx.beginPath(); ctx.moveTo(width * grid / 4, 0)
                                        ctx.lineTo(width * grid / 4, height); ctx.stroke()
                                    }
                                    if (!histogramData || !histogramData.samples) return
                                    var colors = ["#ee7777", "#88cc88", "#77aaff", "#c9c9c2"]
                                    var maximum = 1
                                    for (var c = 0; c < 4; ++c)
                                        for (var b = 0; b < 256; ++b)
                                            maximum = Math.max(maximum, Number(histogramData.channels[c][b]))
                                    // One common scale keeps channel heights comparable.
                                    for (var channel = 3; channel >= 0; --channel) {
                                        ctx.beginPath(); ctx.moveTo(0, height)
                                        for (var bin = 0; bin < 256; ++bin)
                                            ctx.lineTo(bin * (width - 1) / 255,
                                                height - 2 - Number(histogramData.channels[channel][bin]) / maximum * (height - 5))
                                        ctx.lineTo(width, height); ctx.closePath()
                                        ctx.globalAlpha = channel === 3 ? 0.2 : 0.12
                                        ctx.fillStyle = colors[channel]; ctx.fill()
                                        ctx.globalAlpha = channel === 3 ? 0.5 : 0.9
                                        ctx.strokeStyle = colors[channel]; ctx.stroke()
                                    }
                                    ctx.globalAlpha = 1
                                }
                            }
                            Note { text: preview.hasFrame ? "ProPhoto 调色 → sRGB · RGB / 亮度 · 裁切后\n" + canvas.colourStatus : "导入照片后显示" }
                        }
                        Section {
                            visible: window.postMode
                            title: "显影后 · 基础调整"
                            EditorCheckBox {
                                text: "启用显影后调整"; palette.windowText: Theme.text
                                checked: preview.postAdjustments.enabled !== false
                                enabled: preview.hasFrame && preview.settingsEditable && !preview.busy
                                onToggled: preview.setPostAdjustment("enabled", checked)
                            }
                            Note { text: "工作色域：" + preview.postAdjustments.workingSpace + "。调色后转换至 sRGB 显示/输出，不改变胶片材料或重新生成颗粒。画布按住空格查看原图。" }
                            Adjustment { post: true; label: "色温"; setting: "temperature"; from: -100; to: 100; step: 1; decimals: 0; available: preview.postAdjustments.enabled !== false }
                            Adjustment { post: true; label: "色调"; setting: "tint"; from: -100; to: 100; step: 1; decimals: 0; available: preview.postAdjustments.enabled !== false }
                            Adjustment { post: true; label: "曝光"; setting: "exposure"; from: -3; to: 3; step: 0.1; decimals: 1; suffix: " EV"; available: preview.postAdjustments.enabled !== false }
                            Adjustment { post: true; label: "对比度"; setting: "contrast"; from: -50; to: 50; step: 1; decimals: 0; available: preview.postAdjustments.enabled !== false }
                            Adjustment { post: true; label: "亮度"; setting: "brightness"; from: -50; to: 50; step: 1; decimals: 0; available: preview.postAdjustments.enabled !== false }
                            Adjustment { post: true; label: "饱和度"; setting: "saturation"; from: -100; to: 100; step: 1; decimals: 0; available: preview.postAdjustments.enabled !== false }
                            Adjustment { post: true; label: "高光"; setting: "highlights"; from: -100; to: 100; step: 1; decimals: 0; available: preview.postAdjustments.enabled !== false }
                            Adjustment { post: true; label: "阴影"; setting: "shadows"; from: -100; to: 100; step: 1; decimals: 0; available: preview.postAdjustments.enabled !== false }
                            Adjustment { post: true; label: "黑场"; setting: "blackPoint"; from: 0; to: 50; step: 1; decimals: 0; available: preview.postAdjustments.enabled !== false }
                            Adjustment { post: true; label: "白场"; setting: "whitePoint"; from: 0; to: 50; step: 1; decimals: 0; available: preview.postAdjustments.enabled !== false }
                            ActionButton { Layout.fillWidth: true; text: "重置显影后调整"; enabled: preview.hasFrame && preview.settingsEditable && !preview.busy; onClicked: preview.resetPostAdjustments() }
                        }
                        Section { visible: window.postMode; title: "曲线"; ToneCurve {} }
                        Section {
                            visible: window.postMode; title: "色彩平衡 · 主控 / 分区"
                            Repeater {
                                model: ["整体", "阴影", "中间调", "高光"]
                                delegate: ColumnLayout {
                                    required property int index
                                    required property string modelData
                                    Layout.fillWidth: true
                                    Text { text: modelData; color: Theme.text; font.pixelSize: 11 }
                                    Adjustment { post: true; label: "色相"; setting: "zone"+index+"Hue"; from: 0; to: 360; step: 1; decimals: 0; suffix: "°" }
                                    Adjustment { post: true; label: "色彩强度"; setting: "zone"+index+"Saturation"; from: 0; to: 1; step: .01 }
                                    Adjustment { post: true; label: "明度"; setting: "zone"+index+"Luminance"; from: -1; to: 1; step: .01 }
                                }
                            }
                        }
                        Section {
                            visible: window.postMode; title: "暗角"
                            Adjustment { post: true; label: "数量"; setting: "vignetteAmount"; from: -100; to: 100; step: 1; decimals: 0 }
                            Adjustment { post: true; label: "中点"; setting: "vignetteMidpoint"; from: 0; to: 100; fallback: 50; step: 1; decimals: 0 }
                        }
                        Section {
                            visible: !window.postMode
                            title: "输入 / 相机"
                            Adjustment { label: "胶片曝光"; setting: "filmExposureEv"; from: -4; to: 4; step: 0.1; decimals: 1; suffix: " EV" }
                            Note { text: "在胶片曝光前调整场景亮度。每张照片的效果与裁切自动保存，下次导入时恢复。" }
                        }
                        Section {
                            visible: !window.postMode
                            title: "场景拟合"
                            id: sceneFitSection
                            SceneLatitude {}
                        }
                        Section {
                            visible: !window.postMode
                            title: "胶片画幅与效果"
                            RowLayout {
                                Text { text: "画幅长边"; color: Theme.text; font.pixelSize: 11 }
                                EditorComboBox {
                                    Layout.fillWidth: true
                                    model: ["18 mm", "24 mm", "35 mm（默认）", "36 mm（135）", "56 mm", "70 mm", "90 mm"]
                                    readonly property var lengths: [18, 24, 35, 36, 56, 70, 90]
                                    readonly property real actualLength: Number(preview.adjustments.filmFormatMm === undefined ? 35 : preview.adjustments.filmFormatMm)
                                    currentIndex: lengths.indexOf(actualLength)
                                    displayText: currentIndex < 0 ? actualLength.toFixed(1) + " mm（自定）" : currentText
                                    enabled: preview.settingsEditable
                                    onActivated: preview.setAdjustment("filmFormatMm", lengths[currentIndex])
                                }
                            }
                            Note { text: "物理画幅决定颗粒与光晕尺度；裁切不改变画幅。" }
                            Effect { label: "颗粒"; activeKey: "grainActive"; amountKey: "grainAmount" }
                            Effect { label: "光晕"; activeKey: "halationActive"; amountKey: "halationAmount"; maximum: 4 }
                            Effect { label: "眩光"; activeKey: "glareActive"; amountKey: "glareAmount"; maximum: 30 }
                        }
                        Section {
                            visible: !window.postMode
                            id: filmFrameSection
                            title: "胶片边框 · 日期"
                            FilmFrame { }
                        }
                        Section {
                            visible: !window.postMode
                            title: "RAW 解码"
                            EditorComboBox { id: decode; Layout.fillWidth: true; model: ["兼容模式（默认）", "高光扩展（实验）"]; currentIndex: preview.decodeMode; enabled: preview.settingsEditable && !preview.filmFrame.pairActive; onActivated: preview.decodeMode = currentIndex }
                            Note { text: preview.decodeMode ? "保留去马赛克后的部分高光及负值；仍使用整数去马赛克。" : "与已验证的原生 RAW 流程保持一致。" }
                        }
                        Section {
                            title: "照片信息"
                            Note { text: preview.hasFrame ? preview.metadata.camera : "尚未打开照片"; color: Theme.text }
                            Note { text: preview.hasFrame ? "原图 " + preview.metadata.sourceWidth + " × " + preview.metadata.sourceHeight + " 像素\n编辑预览 " + preview.metadata.previewWidth + " × " + preview.metadata.previewHeight + "\n" + (preview.metadata.mode === "headroom" ? "高光扩展解码" : "兼容解码") : "" }
                            Note { text: preview.hasFrame ? "本次渲染 " + (preview.metadata.renderMs / 1000).toFixed(2) + " 秒 · " + (preview.metadata.cached ? "复用负片" : "完整渲染") : "" }
                            ActionButton { Layout.fillWidth: true; text: "重置当前照片设置"; enabled: preview.settingsEditable; onClicked: preview.resetSettings() }
                        }
                        Section {
                            title: preview.batchExporting ? "批量导出" : preview.failed ? "无法完成操作" : "状态"
                            Note { text: window.closePending ? "等待当前操作完成后关闭…" : preview.status; color: preview.failed ? "#ffb39f" : Theme.muted }
                            Note { objectName: "savedStatusNote"; text: preview.savedStatus; visible: text.length > 0 }
                            Note { objectName: "persistenceWarningNote"; text: preview.persistenceWarning; visible: text.length > 0; color: "#ffb39f" }
                            ProgressBar { Layout.fillWidth: true; visible: preview.batchExporting; from: 0; to: Math.max(1, preview.batchTotal); value: preview.batchProgress }
                            ActionButton { Layout.fillWidth: true; text: "当前照片完成后停止"; visible: preview.batchExporting; onClicked: preview.cancelExport() }
                            ActionButton { Layout.fillWidth: true; text: "重新尝试预览"; visible: preview.failed && preview.hasFrame && !preview.batchExporting; enabled: !preview.busy; onClicked: preview.apply() }
                        }
                    }
                }
            }
        }
    }
    Popup {
        id: exportPopup
        objectName: "exportPopup"
        anchors.centerIn: parent
        width: 470; padding: 22
        modal: true; focus: true
        closePolicy: Popup.CloseOnEscape | Popup.CloseOnPressOutside
        onVisibleChanged: window.updateDialogState()
        background: Rectangle { color: Theme.card; border.color: Theme.line; radius: 7 }
        contentItem: ColumnLayout {
            spacing: 14
            Text { text: "导出照片"; color: Theme.text; font.pixelSize: 20; font.bold: true }
            Note { text: "当前画面或所选照片 · sRGB\n裁切与效果一起写入文件。" }
            Rectangle { Layout.fillWidth: true; implicitHeight: 1; color: Theme.line; opacity: 0.5 }
            RowLayout {
                Text { text: "格式"; color: Theme.text; Layout.preferredWidth: 76 }
                EditorComboBox { Layout.fillWidth: true; model: ["TIFF · 16 位", "PNG · 8 位", "PNG · 16 位", "JPEG · 8 位"]; currentIndex: window.exportFormat; onActivated: window.exportFormat = currentIndex }
            }
            RowLayout {
                visible: window.exportFormat === 3
                Text { text: "JPEG 质量"; color: Theme.text; Layout.preferredWidth: 76 }
                EditorSlider { Layout.fillWidth: true; from: 1; to: 100; stepSize: 1; value: window.exportQuality; onMoved: window.exportQuality = value }
                Text { text: window.exportQuality; color: Theme.muted; Layout.preferredWidth: 28 }
            }
            RowLayout {
                Text { text: "输出尺寸"; color: Theme.text; Layout.preferredWidth: 76 }
                EditorComboBox {
                    Layout.fillWidth: true; model: ["原始裁切尺寸", "长边 3840 像素", "长边 2560 像素", "长边 1920 像素", "长边 1080 像素"]
                    readonly property var sizes: [0, 3840, 2560, 1920, 1080]
                    currentIndex: Math.max(0, sizes.indexOf(window.exportLongEdge))
                    onActivated: window.exportLongEdge = sizes[currentIndex]
                }
            }
            Note { text: "保持长宽比例，不放大图像。PNG 无损保存，JPEG 为有损压缩。" }
            Rectangle { Layout.fillWidth: true; implicitHeight: 1; color: Theme.line; opacity: 0.5 }
            ActionButton { Layout.fillWidth: true; accented: true; text: "导出当前已显示画面…"; enabled: preview.hasFrame && !preview.busy; onClicked: { saveDialog.open(); exportPopup.close() } }
            ActionButton { Layout.fillWidth: true; text: "导出所选 " + preview.selectedCount + " 张到文件夹…"; enabled: preview.selectedCount > 0 && !preview.busy; onClicked: { batchDialog.open(); exportPopup.close() } }
            Note { text: "当前画面导出不覆盖已有文件。批量导出逐张处理，自动跳过同名文件；未访问的照片也按各自设置渲染。" }
            ActionButton { Layout.alignment: Qt.AlignRight; text: "取消"; onClicked: exportPopup.close() }
        }
    }
}
