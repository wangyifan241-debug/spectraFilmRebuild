import QtQuick
import QtQuick.Controls
import QtQuick.Layouts
ApplicationWindow {
 visible:true; width:1400; height:900; title:"Phoenix color chain diagnostic — Experimental · scan only"
 color:"#202020"
 ColumnLayout {
  anchors.fill:parent; anchors.margins:12
  Label {text:"视觉参考 / 软件间差异 — 尚无真实胶片验证。原始 v1 与 Optical Print 未改动。";color:"#eac98c"}
  RowLayout {
   ComboBox {id:a;model:resultNames;currentIndex:Math.max(0,resultNames.indexOf("round5_phoenix_i.png"));Layout.fillWidth:true}
   ComboBox {id:b;model:resultNames;currentIndex:Math.max(0,resultNames.indexOf("research_phoenix_i.png"));Layout.fillWidth:true}
   Label {text:"局部放大";color:"white"}
   Slider {id:zoom;from:1;to:4;value:1;Layout.preferredWidth:170}
  }
  RowLayout {
   Layout.fillWidth:true;Layout.fillHeight:true
   Repeater {model:[a.currentIndex,b.currentIndex]
    delegate:Flickable {
     id:viewport
     required property int modelData
     Layout.fillWidth:true;Layout.fillHeight:true;clip:true
     contentWidth:width*zoom.value;contentHeight:height*zoom.value
     Image {width:viewport.width*zoom.value;height:viewport.height*zoom.value;fillMode:Image.PreserveAspectFit;source:modelData>=0?resultUrls[modelData]:"";smooth:zoom.value===1;cache:false}
    }
   }
  }
  Label {text:"同目录 PNG 与 TIFF16 来自同一 sRGB16 像素；recipe/report 保存路径、SHA256、阶段浮点数据。显示器 ICC 未验收。";color:"#bbbbbb";wrapMode:Text.Wrap;Layout.fillWidth:true}
 }
}
