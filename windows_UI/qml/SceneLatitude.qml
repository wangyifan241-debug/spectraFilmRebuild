import QtQuick
import QtQuick.Controls
import QtQuick.Layouts

ColumnLayout {
    id: root
    objectName: "sceneLatitudePanel"
    Layout.fillWidth: true
    spacing: 7
    readonly property var report: preview.sceneReport
    readonly property var scene: report.scene || ({})
    readonly property var medium: report.medium || ({})
    readonly property var fit: report.fit || ({})
    readonly property bool available: report.available === true
    readonly property bool editable: preview.hasFrame && preview.settingsEditable && !preview.busy && !preview.dirty
    function ev(value) { return Number(value).toFixed(2) + " EV" }
    function issueText(issue) {
        const messages = {
            "pull_back_below_minimum": "收回量不足，极端明暗仍落在介质范围外；需大于测量给出的最小值。",
            "pull_back_exceeds_max_lift": "阴影抬起量达到提升上限，请减小。",
            "room_below_minimum": "弯曲空间过小，曲线会过于尖锐。",
            "out_of_range": "求得的曲线超出引擎允许范围。",
            "knees_cross": "高光与阴影曲线交叉，请减小其中一侧。",
            "knee_past_midgrey": "曲线已越过中灰，主体也会改变。"
        }
        return (issue.side === "highlight" ? "高光：" : issue.side === "shadow" ? "阴影：" : "")
            + (messages[issue.code] || issue.message)
    }
    function restoreDraft() {
        if (!high.pressed) high.value = Number(report.highlightPullBack || 0)
        if (!low.pressed) low.value = Number(report.shadowPullBack || 0)
    }
    Component.onCompleted: restoreDraft()
    Connections {
        target: preview
        function onSettingsChanged() { root.restoreDraft() }
        function onStateChanged() { graph.requestPaint() }
    }
    Text {
        Layout.fillWidth: true
        text: root.available ? "实测场景 / 当前胶片与相纸" : String(root.report.reason || "请先测量场景")
        wrapMode: Text.Wrap; color: Theme.muted; font.pixelSize: 11
    }
    Canvas {
        id: graph
        objectName: "sceneLatitudeGraph"
        Layout.fillWidth: true; implicitHeight: 126
        visible: root.available
        onWidthChanged: requestPaint()
        onPaint: {
            const c = getContext("2d")
            c.reset(); c.fillStyle = Theme.wellDark; c.fillRect(0, 0, width, height)
            const hist = root.scene.histogram || ({})
            const bins = hist.fractions || []
            if (!bins.length) return
            const lo = -8, hi = 8, plotH = height-30
            const binLo = Number(hist.lo_ev), binHi = Number(hist.hi_ev)
            function x(ev) { return (ev-lo)/(hi-lo)*width }
            c.save(); c.beginPath(); c.rect(0,0,width,plotH); c.clip()
            c.fillStyle = "#354333"
            c.fillRect(x(root.medium.shadow_ev), 0, x(root.medium.highlight_ev)-x(root.medium.shadow_ev), plotH)
            const placed = hist.placed_fractions || []
            let peak = 0.000001
            for (let i=0; i<bins.length; ++i) {
                const centre = binLo+(i+0.5)/bins.length*(binHi-binLo)
                if (centre>=lo && centre<=hi) peak = Math.max(peak, Number(bins[i]), Number(placed[i] || 0))
            }
            function curve(values, colour) {
                if (!values.length) return
                c.strokeStyle = colour; c.lineWidth = 1.2; c.beginPath()
                for (let i=0; i<values.length; ++i) {
                    const px = x(binLo+(i+0.5)/values.length*(binHi-binLo))
                    const py = plotH-Math.sqrt(Number(values[i])/peak)*(plotH-4)
                    if (i===0) c.moveTo(px,py); else c.lineTo(px,py)
                }
                c.stroke()
            }
            curve(bins, "#a3a3a0"); curve(placed, "#e1a95f")
            c.restore()
            const band = root.report.separation || []
            for (let i=0; i+1<band.length; ++i) {
                const left = Math.max(0,x(Number(band[i].ev))), right = Math.min(width,x(Number(band[i+1].ev)))
                if (right>left) {
                    c.fillStyle = "rgba(250,248,244,"+(0.9*Number(band[i].strength)).toFixed(4)+")"
                    c.fillRect(left,plotH+4,right-left,7)
                }
            }
            c.strokeStyle = "#b7b6b3"; c.lineWidth = 0.75
            for (const edge of [root.medium.shadow_ev,root.medium.highlight_ev]) {
                const xx = x(Number(edge)); c.beginPath(); c.moveTo(xx,0); c.lineTo(xx,plotH+12); c.stroke()
            }
            c.fillStyle = "#b7b6b3"; c.font = "10px sans-serif"
            c.fillText(lo.toFixed(0)+" EV", 3, height-3)
            c.fillText(hi.toFixed(0)+" EV", width-37, height-3)
            c.fillText("0",width/2-3,height-3)
        }
    }
    Text {
        Layout.fillWidth: true; visible: root.available
        text: "介质范围 " + root.ev(root.medium.shadow_ev) + " ～ " + root.ev(root.medium.highlight_ev)
            + "\n场景 P0.1 / P99.9：" + root.ev(root.scene.p0_1) + " / " + root.ev(root.scene.p99_9)
            + "\n灰：原场景 · 金：本次拟合预览 · 绿：介质范围"
            + "\n低于 / 范围内 / 高于：" + (100*Number(root.report.below)).toFixed(1) + "% / "
            + (100*Number(root.report.within)).toFixed(1) + "% / " + (100*Number(root.report.above)).toFixed(1) + "%"
            + "\n下带：实测中性阶调的分离能力（非色彩宽容度）"
        wrapMode: Text.Wrap; color: Theme.muted; font.pixelSize: 10
    }
    RowLayout {
        Layout.fillWidth: true
        ActionButton { objectName: "measureSceneButton"; Layout.fillWidth: true; text: "测量"; enabled: root.editable; onClicked: preview.measureScene() }
        ActionButton { objectName: "autoFitSceneButton"; Layout.fillWidth: true; text: "自动建议并应用"; enabled: root.editable; onClicked: preview.fitScene(0,0,true) }
    }
    Text {
        Layout.fillWidth: true; visible: root.available
        readonly property var suggestion: root.report.suggested || ({})
        text: suggestion.valid ? "自动建议：高光 " + root.ev(suggestion.highlight_pull_back) + " / 阴影 " + root.ev(suggestion.shadow_pull_back)
                              : "当前场景没有可行的自动建议，请手动调整。"
        color: Theme.muted; font.pixelSize: 10; wrapMode: Text.Wrap
    }
    Text { text: "高光收回 " + high.value.toFixed(2) + " 档"; color: Theme.text; font.pixelSize: 11 }
    EditorSlider { id: high; objectName: "sceneHighlightSlider"; Layout.fillWidth: true; from: 0; to: 8; stepSize: 0.05; enabled: root.editable }
    Text { text: "阴影抬起 " + low.value.toFixed(2) + " 档"; color: Theme.text; font.pixelSize: 11 }
    EditorSlider { id: low; objectName: "sceneShadowSlider"; Layout.fillWidth: true; from: 0; to: 8; stepSize: 0.05; enabled: root.editable }
    RowLayout {
        Layout.fillWidth: true
        ActionButton { objectName: "applySceneFitButton"; Layout.fillWidth: true; text: "拟合并应用"; enabled: root.editable; onClicked: preview.fitScene(high.value,low.value,false) }
        ActionButton { objectName: "resetSceneFitButton"; text: "重置"; enabled: root.editable; onClicked: preview.resetScenePlacement() }
    }
    Text {
        Layout.fillWidth: true
        text: root.report.active ? "当前曲线已启用 · 高光 " + root.ev(root.report.highlightPullBack) + " / 阴影 " + root.ev(root.report.shadowPullBack) : "当前场景映射关闭"
        color: Theme.muted; font.pixelSize: 10; wrapMode: Text.Wrap
    }
    Repeater {
        model: root.available ? (root.fit.issues || []) : []
        Text { required property var modelData; Layout.fillWidth: true; text: "拒绝：" + root.issueText(modelData); color: "#efa886"; font.pixelSize: 10; wrapMode: Text.Wrap }
    }
    Repeater {
        model: root.available ? (root.fit.warnings || []) : []
        Text { required property var modelData; Layout.fillWidth: true; text: "提示：" + root.issueText(modelData); color: Theme.accent; font.pixelSize: 10; wrapMode: Text.Wrap }
    }
    Text {
        Layout.fillWidth: true; text: "测量不修改照片。滑块需点击应用；不可行的拟合不会替换现有曲线。映射先于胶片，不修改介质测量数据。"
        color: Theme.muted; font.pixelSize: 10; wrapMode: Text.Wrap
    }
}
