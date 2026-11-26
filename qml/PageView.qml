import QtQuick
import QtQuick.Controls
import QtQuick.Layouts
Rectangle {
    id: root
    property string heading: ""
    property string imageSource: ""
    property real pageRatio: 0.707
    property real zoom: 1
    property bool loading: false
    color: "#e2e8f0"
    clip: true
    ColumnLayout {
        anchors.fill: parent
        spacing: 0
        Rectangle {
            Layout.fillWidth: true
            Layout.preferredHeight: 38
            color: "#f5f7fa"
            Text { anchors.left: parent.left; anchors.leftMargin: 16; anchors.verticalCenter: parent.verticalCenter; text: root.heading; color: "#45566e"; font.pixelSize: 12 }
        }
        Flickable {
            id: viewport
            Layout.fillWidth: true
            Layout.fillHeight: true
            clip: true
            boundsBehavior: Flickable.StopAtBounds
            contentWidth: Math.max(width, page.width + 28)
            contentHeight: Math.max(height, page.height + 28)
            Image {
                id: page
                x: (viewport.contentWidth - width) / 2
                y: (viewport.contentHeight - height) / 2
                width: Math.max(1, viewport.width - 28) * root.zoom
                height: width / root.pageRatio
                source: root.imageSource
                visible: source.toString() !== ""
                asynchronous: true
                cache: false
                smooth: true
                fillMode: Image.PreserveAspectFit
                Rectangle { anchors.fill: parent; z: -1; color: "white"; border.color: "#c5ceda" }
            }
            ScrollBar.vertical: ScrollBar { }
            ScrollBar.horizontal: ScrollBar { }
        }
    }
    Text {
        anchors.centerIn: parent
        visible: root.imageSource === ""
        text: root.loading ? "正在准备预览…" : "打开或拖入 PDF 或图片"
        color: "#6b7d94"
        font.pixelSize: 14
    }
}
