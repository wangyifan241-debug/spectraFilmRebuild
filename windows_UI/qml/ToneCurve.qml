import QtQuick
import QtQuick.Controls
import QtQuick.Layouts

ColumnLayout {
    id: editor
    Layout.fillWidth: true
    property int channel: 0
    readonly property var points: preview.postAdjustments.curves[channel]
    readonly property var samples: preview.postAdjustments.curveTables[channel]
    onPointsChanged: plot.requestPaint()
    onSamplesChanged: plot.requestPaint()
    EditorComboBox { Layout.fillWidth: true; model: ["RGB 主曲线", "亮度曲线", "红通道", "绿通道", "蓝通道"]; onActivated: editor.channel = currentIndex }
    Canvas {
        id: plot
        objectName: "toneCurvePlot"
        Layout.fillWidth: true; Layout.preferredHeight: 190
        onPaint: {
            var c = getContext("2d"); c.fillStyle = Theme.well; c.fillRect(0,0,width,height)
            c.strokeStyle = "#4c4c48"; c.lineWidth = 1
            for(var i=1;i<4;++i) { c.beginPath(); c.moveTo(width*i/4,0); c.lineTo(width*i/4,height); c.moveTo(0,height*i/4); c.lineTo(width,height*i/4); c.stroke() }
            c.strokeStyle = Theme.muted; c.beginPath(); c.moveTo(0,height); c.lineTo(width,0); c.stroke()
            c.strokeStyle = Theme.accent; c.lineWidth = 2; c.beginPath()
            for(var j=0;j<editor.samples.length;++j) { var x=j/(editor.samples.length-1)*width, y=(1-editor.samples[j])*height; if(j===0)c.moveTo(x,y);else c.lineTo(x,y) }; c.stroke()
            c.fillStyle = Theme.text
            for(var k=0;k<editor.points.length;++k) { c.beginPath(); c.arc(editor.points[k].x*width,(1-editor.points[k].y)*height,4,0,Math.PI*2);c.fill() }
        }
        MouseArea {
            anchors.fill: parent; acceptedButtons: Qt.LeftButton | Qt.RightButton
            enabled: preview.hasFrame && preview.settingsEditable && !preview.busy
            property int selected: -1
            function copyPoints() { var p=[];for(var i=0;i<editor.points.length;++i)p.push({x:editor.points[i].x,y:editor.points[i].y});return p }
            function commit(p) { preview.setCurve(editor.channel,p) }
            onPressed: function(mouse) {
                var p=copyPoints(); selected=-1
                for(var i=0;i<p.length;++i)if(Math.hypot(p[i].x*width-mouse.x,(1-p[i].y)*height-mouse.y)<10)selected=i
                preview.beginEditGesture("curve"+editor.channel)
                if(mouse.button===Qt.RightButton) {
                    if(selected>0 && selected<p.length-1){p.splice(selected,1);commit(p)}
                    selected=-1;return
                }
                if(selected<0 && p.length<32) {
                    var x=Math.max(.01,Math.min(.99,mouse.x/width)),y=Math.max(0,Math.min(1,1-mouse.y/height))
                    var index=1;while(index<p.length && p[index].x<x)++index
                    if(x-p[index-1].x<.01 || p[index].x-x<.01)return
                    p.splice(index,0,{x:x,y:y});selected=index;commit(p)
                }
            }
            onPositionChanged: function(mouse) {
                if(selected<0 || !(pressedButtons&Qt.LeftButton))return
                var p=copyPoints(),i=selected
                p[i]={x:i===0?0:i===p.length-1?1:Math.max(p[i-1].x+.01,Math.min(p[i+1].x-.01,mouse.x/width)),y:Math.max(0,Math.min(1,1-mouse.y/height))};commit(p)
            }
            onReleased: { selected=-1;preview.endEditGesture() }
            onCanceled: { selected=-1;preview.endEditGesture() }
        }
    }
    Text { Layout.fillWidth: true; text: "单击添加，拖动调整，右键删除中间点"; color: Theme.muted; font.pixelSize: 10; wrapMode: Text.Wrap }
    ActionButton { Layout.fillWidth: true; text: "重置此曲线"; enabled: preview.hasFrame && !preview.busy; onClicked: preview.setCurve(editor.channel,[{x:0,y:0},{x:1,y:1}]) }
}
