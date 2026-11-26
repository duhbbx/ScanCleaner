import QtQuick
import QtQuick.Controls
import QtQuick.Layouts
import QtQuick.Dialogs

ApplicationWindow {
    id: window
    width: 1380; height: 880
    minimumWidth: 1040; minimumHeight: 700
    visible: true
    title: (cleaner.fileName ? cleaner.fileName + " — " : "") + "净页"
    color: "#f7f9fc"
    font.pixelSize: 13
    property bool quitAfterCancel: false
    property bool controlsReady: false
    Component.onCompleted: { controlsReady = true; applyOptions() }
    property real pageZoom: zoom.currentIndex === 0 ? 1 : [1,1.5,2,3][zoom.currentIndex]
    readonly property string outputSuffix: ["pdf", "png", "jpg"][outputFormat.currentIndex] || "pdf"
    readonly property bool outputIsPdf: outputSuffix === "pdf"
    function applyOptions() { cleaner.setOptions({mode: mode.currentIndex === 0 ? "gray" : "bw", background: Math.round(background.value), darkness: Math.round(darkness.value), sharpness: Math.round(sharpness.value)}) }
    function selectOutput() {
        saveFile.selectedFile = cleaner.suggestedOutput(outputSuffix)
        saveFile.open()
    }
    function exportSelection(url) {
        let first = cleaner.imageInput ? 1 : cleaner.page
        let last = first
        if (outputIsPdf && !cleaner.imageInput) {
            first = allPages.checked ? 1 : firstPage.value
            last = allPages.checked ? cleaner.pageCount : lastPage.value
        }
        cleaner.exportFile(url, first, last, [150, 200, 300, 400][dpi.currentIndex], outputSuffix)
    }
    onClosing: function(event) { if (cleaner.exporting) { event.accepted = false; closePrompt.open() } }

    component Caption: Text { color: "#34465e"; font.pixelSize: 13; font.weight: Font.Medium }
    component Note: Text { color: "#6a7b91"; font.pixelSize: 11; wrapMode: Text.Wrap; Layout.fillWidth: true }
    component Choice: ComboBox {
        implicitHeight: 36
        font.pixelSize: 13
        background: Rectangle { radius: 6; color: "white"; border.color: parent.activeFocus ? "#316dd9" : "#d2dae5" }
        contentItem: Text { text: parent.displayText; leftPadding: 12; rightPadding: 25; verticalAlignment: Text.AlignVCenter; color: "#28384d"; font: parent.font; elide: Text.ElideRight }
        indicator: Canvas {
            width: 10; height: 6; x: parent.width - 21; y: (parent.height - height) / 2
            onPaint: { let p = getContext("2d"); p.clearRect(0,0,width,height); p.strokeStyle = "#657790"; p.lineWidth = 1.6; p.beginPath(); p.moveTo(1,1); p.lineTo(5,5); p.lineTo(9,1); p.stroke() }
        }
    }

    ColumnLayout {
        anchors.fill: parent
        spacing: 0
        Rectangle {
            Layout.fillWidth: true; Layout.preferredHeight: 76; color: "white"
            RowLayout {
                anchors.fill: parent; anchors.leftMargin: 24; anchors.rightMargin: 24; spacing: 18
                Rectangle { width: 35; height: 42; radius: 5; color: "#316dd9"
                    Rectangle { x: 8; y: 11; width: 19; height: 2; color: "white"; radius: 1 }
                    Rectangle { x: 8; y: 19; width: 19; height: 2; color: "white"; radius: 1 }
                    Rectangle { x: 8; y: 27; width: 12; height: 2; color: "white"; radius: 1 }
                }
                ColumnLayout { spacing: 3
                    Text { text: "净页"; color: "#1d304c"; font.pixelSize: 25; font.weight: Font.DemiBold }
                    Text { text: "扫描件去底色"; color: "#6a7b91"; font.pixelSize: 12 }
                }
                Item { Layout.fillWidth: true }
                Text { text: cleaner.fileName || "在本机处理，不上传文件"; color: "#657790"; elide: Text.ElideMiddle; Layout.maximumWidth: 430 }
                ActionButton { text: "打开 PDF 或图片…"; enabled: !cleaner.exporting; onClicked: openFile.open() }
            }
        }
        Rectangle { Layout.fillWidth: true; height: 1; color: "#dce3ed" }
        RowLayout {
            Layout.fillWidth: true; Layout.fillHeight: true; spacing: 0
            Rectangle {
                Layout.preferredWidth: 268; Layout.fillHeight: true; color: "#f7f9fc"
                ScrollView {
                    anchors.fill: parent; anchors.margins: 22; clip: true
                    contentWidth: availableWidth
                    ColumnLayout {
                        width: parent.width; spacing: 11
                        Caption { text: cleaner.imageInput ? "图片处理" : "页面处理"; font.pixelSize: 16 }
                        Choice { id: mode; Layout.fillWidth: true; model: ["柔和去底（推荐）", "纯黑白（适合打印）"]; enabled: !cleaner.exporting; onCurrentIndexChanged: if (window.controlsReady) window.applyOptions() }
                        Note { text: mode.currentIndex === 0 ? "白底，保留字边灰度，减少发粗和锯齿。" : "仅保留黑白两色。照片、细字页面建议用柔和去底。" }
                        RowLayout { Layout.fillWidth: true; Caption { text: "去底色"; Layout.fillWidth: true } Text { text: Math.round(background.value) + "%"; color: "#657790" } }
                        Slider { id: background; Layout.fillWidth: true; from: 0; to: 100; value: 100; stepSize: 1; enabled: !cleaner.exporting; onValueChanged: if (window.controlsReady) window.applyOptions() }
                        RowLayout { Layout.fillWidth: true; Caption { text: "文字深浅"; Layout.fillWidth: true } Text { text: Math.round(darkness.value); color: "#657790" } }
                        Slider { id: darkness; Layout.fillWidth: true; from: -20; to: 20; value: 0; stepSize: 1; enabled: !cleaner.exporting; onValueChanged: if (window.controlsReady) window.applyOptions() }
                        RowLayout { Layout.fillWidth: true; Note { text: "更浅" } Note { text: "更深"; horizontalAlignment: Text.AlignRight } }
                        RowLayout { Layout.fillWidth: true; Caption { text: "轻度锐化"; Layout.fillWidth: true } Text { text: Math.round(sharpness.value); color: "#657790" } }
                        Slider { id: sharpness; Layout.fillWidth: true; from: 0; to: 100; value: 20; stepSize: 1; enabled: !cleaner.exporting; onValueChanged: if (window.controlsReady) window.applyOptions() }
                        ActionButton { text: "恢复默认"; enabled: !cleaner.exporting; onClicked: { mode.currentIndex = 0; background.value = 100; darkness.value = 0; sharpness.value = 20; window.applyOptions() } }
                        Rectangle { Layout.fillWidth: true; height: 1; color: "#dce3ed"; Layout.topMargin: 8; Layout.bottomMargin: 4 }
                        Caption { text: "导出"; font.pixelSize: 16 }
                        Choice {
                            id: outputFormat
                            Layout.fillWidth: true
                            model: ["PDF 文档", "PNG · 无损图片", "JPG · 压缩图片"]
                            currentIndex: cleaner.imageInput ? 1 : 0
                            enabled: !cleaner.exporting
                            Accessible.name: "导出格式"
                        }
                        Caption { visible: dpi.visible; text: cleaner.imageInput ? "PDF 页面 DPI" : "导出分辨率" }
                        Choice {
                            id: dpi
                            Layout.fillWidth: true
                            visible: !cleaner.imageInput || window.outputIsPdf
                            model: ["150 DPI", "200 DPI", "300 DPI", "400 DPI"]
                            currentIndex: 2
                            enabled: !cleaner.exporting
                            Accessible.name: "导出分辨率"
                        }
                        CheckBox {
                            id: allPages
                            visible: !cleaner.imageInput && window.outputIsPdf
                            text: "处理全部页面"; checked: true; enabled: !cleaner.exporting; font.pixelSize: 13
                        }
                        RowLayout {
                            Layout.fillWidth: true
                            visible: allPages.visible && !allPages.checked
                            enabled: !cleaner.exporting && cleaner.pageCount > 0
                            SpinBox { id: firstPage; from: 1; to: Math.max(1,cleaner.pageCount); value: 1; editable: true; Layout.fillWidth: true; implicitWidth: 90; implicitHeight: 34 }
                            Text { text: "至"; color: "#657790" }
                            SpinBox { id: lastPage; from: 1; to: Math.max(1,cleaner.pageCount); value: Math.max(1,cleaner.pageCount); editable: true; Layout.fillWidth: true; implicitWidth: 90; implicitHeight: 34 }
                        }
                        ActionButton { text: "导出 " + window.outputSuffix.toUpperCase() + "…"; primary: true; Layout.fillWidth: true; enabled: cleaner.hasPreview && !cleaner.previewBusy && !cleaner.exporting; onClicked: window.selectOutput() }
                        Note {
                            text: cleaner.imageInput
                                ? (window.outputIsPdf ? "保留原图像素，DPI 决定打印尺寸。" : "按原图像素尺寸导出。")
                                : (window.outputIsPdf ? "保留页面尺寸。输出为图像 PDF，无法直接选中文字。"
                                                       : "导出正在预览的第 " + cleaner.page + " 页。")
                        }
                        ProgressBar { visible: cleaner.exporting; Layout.fillWidth: true; from: 0; to: Math.max(1,cleaner.total); value: cleaner.completed }
                        RowLayout { visible: cleaner.exporting; Layout.fillWidth: true
                            Note { visible: !cleaner.imageInput; text: cleaner.completed + " / " + cleaner.total + " 页" }
                            ActionButton { text: "取消导出"; onClicked: cleaner.cancelExport() }
                        }
                        Item { Layout.preferredHeight: 10 }
                    }
                }
            }
            Rectangle { Layout.fillHeight: true; width: 1; color: "#dce3ed" }
            ColumnLayout {
                Layout.fillWidth: true; Layout.fillHeight: true; spacing: 0
                Rectangle {
                    Layout.fillWidth: true; Layout.preferredHeight: 56; color: "#f7f9fc"
                    RowLayout {
                        anchors.fill: parent; anchors.leftMargin: 16; anchors.rightMargin: 16; spacing: 10
                        ActionButton { text: "上一页"; visible: !cleaner.imageInput; enabled: cleaner.page > 1 && !cleaner.exporting; onClicked: cleaner.setPage(cleaner.page-1) }
                        SpinBox { visible: !cleaner.imageInput; from: 1; to: Math.max(1,cleaner.pageCount); value: cleaner.page; editable: true; enabled: cleaner.pageCount > 0 && !cleaner.exporting; implicitWidth: 96; implicitHeight: 34; onValueModified: cleaner.setPage(value) }
                        Text { visible: !cleaner.imageInput; text: "/ " + cleaner.pageCount; color: "#657790" }
                        ActionButton { text: "下一页"; visible: !cleaner.imageInput; enabled: cleaner.page < cleaner.pageCount && !cleaner.exporting; onClicked: cleaner.setPage(cleaner.page+1) }
                        BusyIndicator { running: cleaner.previewBusy; visible: running; implicitWidth: 24; implicitHeight: 24 }
                        Item { Layout.fillWidth: true }
                        Choice { id: zoom; model: ["适宽", "150%", "200%", "300%"]; implicitWidth: 96 }
                    }
                }
                RowLayout {
                    Layout.fillWidth: true; Layout.fillHeight: true; spacing: 2
                    PageView { Layout.fillWidth: true; Layout.fillHeight: true; heading: "原稿"; pageRatio: cleaner.ratio; zoom: window.pageZoom; loading: cleaner.previewBusy; imageSource: cleaner.hasPreview ? "image://scan/original?v=" + cleaner.revision : "" }
                    PageView { Layout.fillWidth: true; Layout.fillHeight: true; heading: "处理后" + (cleaner.previewBusy ? " · 正在更新" : ""); pageRatio: cleaner.ratio; zoom: window.pageZoom; loading: cleaner.previewBusy; imageSource: cleaner.hasPreview ? "image://scan/cleaned?v=" + cleaner.revision : "" }
                }
            }
        }
        Rectangle { Layout.fillWidth: true; height: 1; color: "#dce3ed" }
        Rectangle { Layout.fillWidth: true; Layout.preferredHeight: 34; color: "white"
            Text { anchors.left: parent.left; anchors.right: parent.right; anchors.margins: 20; anchors.verticalCenter: parent.verticalCenter; text: cleaner.status; color: "#596d86"; font.pixelSize: 12; elide: Text.ElideMiddle }
        }
    }
    DropArea {
        anchors.fill: parent
        enabled: !cleaner.exporting
        onEntered: function(drag) {
            drag.accepted = drag.hasUrls && drag.urls.length === 1
        }
        onDropped: function(drop) {
            if (drop.hasUrls && drop.urls.length === 1) {
                drop.acceptProposedAction()
                cleaner.open(drop.urls[0])
            }
        }
    }
    FileDialog {
        id: openFile
        title: "打开 PDF 或图片"
        nameFilters: ["PDF 和图片 (*.pdf *.png *.jpg *.jpeg *.tif *.tiff *.bmp *.webp)",
                      "PDF 文件 (*.pdf)", "图片 (*.png *.jpg *.jpeg *.tif *.tiff *.bmp *.webp)"]
        onAccepted: cleaner.open(selectedFile)
    }
    FileDialog {
        id: saveFile
        title: "另存处理后的文件"
        fileMode: FileDialog.SaveFile
        nameFilters: [window.outputIsPdf ? "PDF 文件 (*.pdf)" :
                      window.outputSuffix === "png" ? "PNG 图片 (*.png)" : "JPG 图片 (*.jpg *.jpeg)"]
        defaultSuffix: window.outputSuffix
        onAccepted: window.exportSelection(selectedFile)
    }
    Dialog {
        id: errorDialog; property alias message: errorText.text
        title: "请检查"; modal: true; anchors.centerIn: Overlay.overlay; width: 430; standardButtons: Dialog.Ok
        Text { id: errorText; width: parent.width; wrapMode: Text.Wrap; color: "#28384d"; font.pixelSize: 14 }
    }
    Dialog { id: finished; title: "已导出"; modal: true; anchors.centerIn: Overlay.overlay; width: 360
        ColumnLayout { width: parent.width; spacing: 18
            Text { Layout.fillWidth: true; text: cleaner.status; wrapMode: Text.Wrap; color: "#28384d" }
            RowLayout { Item { Layout.fillWidth: true } ActionButton { text: "关闭"; onClicked: finished.close() } ActionButton { text: "打开所在文件夹"; primary: true; onClicked: { cleaner.revealOutput(); finished.close() } } }
        }
    }
    Dialog { id: closePrompt; title: "正在导出"; modal: true; anchors.centerIn: Overlay.overlay; width: 390
        ColumnLayout { width: parent.width; spacing: 18
            Text { text: "是否取消导出并退出？已有文件会保留。"; color: "#28384d"; Layout.fillWidth: true; wrapMode: Text.Wrap }
            RowLayout { Item { Layout.fillWidth: true } ActionButton { text: "继续处理"; onClicked: closePrompt.close() } ActionButton { text: "取消并退出"; onClicked: { window.quitAfterCancel = true; cleaner.cancelExport(); closePrompt.close() } } }
        }
    }
    Connections {
        target: cleaner
        function onError(message) { errorDialog.message = message; errorDialog.open() }
        function onExportFinished(success,cancelled) { if (window.quitAfterCancel) Qt.quit(); else if (success) finished.open() }
    }
    Shortcut { sequences: [StandardKey.Open]; enabled: !cleaner.exporting; onActivated: openFile.open() }
    Shortcut { sequences: [StandardKey.SaveAs]; enabled: cleaner.hasPreview && !cleaner.exporting && !cleaner.previewBusy; onActivated: window.selectOutput() }
}
