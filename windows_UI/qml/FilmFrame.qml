import QtQuick
import QtQuick.Controls
import QtQuick.Layouts

ColumnLayout {
    id: root
    objectName: "filmFramePanel"
    Layout.fillWidth: true
    spacing: 7
    readonly property var config: preview.filmFrame
    readonly property bool editable: preview.hasFrame && preview.settingsEditable
    readonly property bool dateAvailable: !config.active || ["135_xpan","120_6x12","120_6x17"].indexOf(config.format) < 0
    component Caption: Text { color: Theme.muted; font.pixelSize: 10; Layout.fillWidth: true; wrapMode: Text.Wrap }
    component Choice: RowLayout {
        required property string label
        required property string setting
        required property var values
        required property var labels
        Layout.fillWidth: true
        Text { text: label; color: Theme.text; font.pixelSize: 11; Layout.preferredWidth: 58 }
        EditorComboBox {
            Layout.fillWidth: true
            model: labels
            currentIndex: values.indexOf(root.config[setting])
            enabled: root.editable && !(setting === "format" && root.config.pairActive)
            onActivated: preview.setFilmFrame(setting, values[currentIndex])
        }
    }
    component Amount: ColumnLayout {
        required property string label
        required property string setting
        property real low: 0
        property real high: 4
        Layout.fillWidth: true
        RowLayout {
            Layout.fillWidth: true
            Text { text: label; color: Theme.text; font.pixelSize: 11; Layout.fillWidth: true }
            Text { text: Number(root.config[setting]).toFixed(2); color: Theme.muted; font.pixelSize: 10 }
        }
        EditorSlider {
            Layout.fillWidth: true; from: low; to: high; value: Number(root.config[setting]); enabled: root.editable
            onPressedChanged: pressed ? preview.beginEditGesture("filmFrame." + setting) : preview.endEditGesture()
            onMoved: preview.setFilmFrame(setting,value)
        }
    }
    ColumnLayout {
        visible: root.config.pairActive; Layout.fillWidth: true
        Text { text: "半格双拼 · 18×24 mm × 2"; color: Theme.text; font.pixelSize: 12 }
        EditorCheckBox { objectName: "pairTurnedToggle"; text: "上下排列"; checked: root.config.pairTurned; enabled: root.editable; onToggled: preview.setFilmFrame("pairTurned",checked) }
        Amount { label: root.config.active ? "间隔 1 mm（胶片边框固定）" : "间隔 / mm"; setting: "pairSpacing"; low: 0.5; high: 2; enabled: !root.config.active }
        Repeater {
            model: 2
            delegate: ColumnLayout {
                required property int index
                readonly property string prefix: "pair" + index
                Layout.fillWidth: true
                Text { text: index === 0 ? "第一格（左 / 上）" : "第二格（右 / 下）"; color: Theme.text; font.pixelSize: 11 }
                Amount { label: "独立曝光 / EV"; setting: prefix+"Exposure"; low: -8; high: 8 }
                Amount { label: "放大"; setting: prefix+"Scale"; low: 1; high: 4 }
                Amount { label: "水平位置"; setting: prefix+"X"; low: -1; high: 1 }
                Amount { label: "垂直位置"; setting: prefix+"Y"; low: -1; high: 1 }
                ActionButton { text: "本格旋转 90°"; enabled: root.editable; onClicked: preview.setFilmFrame(prefix+"Turns",(root.config[prefix+"Turns"]+1)%4) }
            }
        }
        Caption { text: "两格独立测光，间隔是未曝光胶片。沿用创建时各自的裁切与解码；胶片、相纸、整体调色和日期共用。项目引用原片，不移动原片。" }
    }
    EditorCheckBox {
        objectName: "filmEdgeToggle"
        text: "胶片边框"; checked: root.config.active; enabled: root.editable
        onToggled: preview.setFilmFrame("active",checked)
    }
    ColumnLayout {
        visible: root.config.active
        Layout.fillWidth: true
        Choice { label: "画幅"; setting: "format"; values: ["135","135_half","120_645","120_6x6","120_6x7","120_6x8","120_6x9","135_xpan","120_6x12","120_6x17"]; labels: ["135 / 35 mm","135 单半格","120 / 645","120 / 6×6","120 / 6×7","120 / 6×8","120 / 6×9","XPan","120 / 6×12","120 / 6×17"] }
        Choice { label: "扫描"; setting: "mode"; values: ["strip","filed"]; labels: ["完整胶片条","锉边片夹"] }
        Choice { label: "片窗"; setting: "gate"; values: ["auto","square","rounded","eared","shouldered","kicked"]; labels: ["相机默认","方角","圆角","耳形","肩形","外扩角"] }
        Choice { label: "齿孔底"; setting: "holes"; values: ["white","black"]; labels: ["透光 / 白","遮光 / 黑"] }
        Choice { label: "片夹"; setting: "carrier"; values: ["black","open","none"]; labels: ["黑色片夹","开放透光","无片夹"] }
        RowLayout {
            Text { text: "帧号"; color: Theme.text; font.pixelSize: 11; Layout.fillWidth: true }
            SpinBox { objectName: "filmFrameNumber"; from: 0; to: 99; value: root.config.frameNumber; enabled: root.editable; onValueModified: preview.setFilmFrame("frameNumber",value) }
        }
        Caption { text: "帧号 0 按种子自动编号。画面按当前裁切居中裁入所选片窗，不拉伸；边框参与曝光、颗粒与光晕。" }
        Amount { label: "边缘雾化"; setting: "fog" }
        Amount { label: "漏光"; setting: "leaks" }
        RowLayout {
            ActionButton { Layout.fillWidth: true; text: "重新过片"; enabled: root.editable; onClicked: preview.setFilmFrame("frameSeed",(root.config.frameSeed+1)%2147483647) }
            ActionButton { Layout.fillWidth: true; text: "更换相机片窗"; enabled: root.editable; onClicked: preview.setFilmFrame("cameraSeed",(root.config.cameraSeed+1)%2147483647) }
        }
        Caption { text: "完整胶片画布暂不支持分条计算；原尺寸导出比普通照片占用更多显存。" }
    }
    EditorCheckBox {
        objectName: "dateBackToggle"
        text: root.dateAvailable ? "日期后背" : "日期后背（全景不适用）"; checked: root.config.dateActive; enabled: root.editable && root.dateAvailable
        onToggled: preview.setFilmFrame("dateActive",checked)
    }
    ColumnLayout {
        Layout.fillWidth: true; visible: root.config.dateActive
        TextField {
            objectName: "dateBackText"
            Layout.fillWidth: true; text: root.config.dateText; maximumLength: 64; enabled: root.editable
            placeholderText: "例如 26 10 30"
            validator: RegularExpressionValidator { regularExpression: /^[0-9 .:/'-]*$/ }
            onEditingFinished: preview.setFilmFrame("dateText",text)
        }
        Caption { text: "默认填入今天，可自行改为拍摄日期；尚未自动读取 EXIF 日期。" }
        Choice { label: "字形"; setting: "dateStyle"; values: ["lcd","dots","data"]; labels: ["七段 LCD","点阵","数据后背"] }
        Choice { label: "位置"; setting: "dateCorner"; values: ["br","bl","tr","tl"]; labels: ["右下","左下","右上","左上"] }
        Amount { label: "曝光 / EV"; setting: "dateExposure"; low: -2; high: 8 }
        Amount { label: "字号 / mm"; setting: "dateSize"; low: 0.4; high: 3 }
        Caption { text: "日期曝光在胶片上，不是导出水印。全景画幅暂不提供日期后背。" }
    }
}
