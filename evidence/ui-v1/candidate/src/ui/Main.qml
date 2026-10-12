import QtQuick
import QtQuick.Controls
import QtQuick.Layouts
import QtQuick.Dialogs
import org.kde.kirigami as Kirigami

Kirigami.ApplicationWindow {
    id: window
    width: 1060
    height: 740
    minimumWidth: 720
    minimumHeight: 520
    title: "KDE Index"

    FolderDialog {
        id: folderPicker
        title: "Choose a folder to search"
        onAccepted: backend.addRoot(selectedFolder)
    }
    Dialog {
        id: exclusionsDialog
        property string rootPath
        title: "Exclude paths from this folder"
        anchors.centerIn: parent
        width: Math.min(window.width - 64, 560)
        standardButtons: Dialog.Ok | Dialog.Cancel
        onAccepted: backend.setExclusions(rootPath, exclusionText.text)
        ColumnLayout {
            width: parent.width
            Label { text: exclusionsDialog.rootPath; elide: Text.ElideMiddle; Layout.fillWidth: true }
            Label { text: "One relative file or folder path per line. Existing matches are removed from search."; wrapMode: Text.WordWrap; Layout.fillWidth: true }
            ScrollView { Layout.fillWidth: true; Layout.preferredHeight: 160
                TextArea { id: exclusionText; placeholderText: "Archive\nDownloads/large-export.csv" }
            }
        }
    }
    Dialog {
        id: wipeDialog
        title: "Remove the search index?"
        anchors.centerIn: parent
        standardButtons: Dialog.Ok | Dialog.Cancel
        onAccepted: backend.wipe()
        Label { text: "This clears selected folders and their search data. Your original files are kept."; wrapMode: Text.WordWrap; width: 440 }
    }
    Dialog {
        id: failuresDialog
        title: "Files needing attention"
        anchors.centerIn: parent
        width: Math.min(window.width - 64, 720)
        height: Math.min(window.height - 64, 520)
        standardButtons: Dialog.Close
        ColumnLayout {
            anchors.fill: parent
            Label { text: "Up to 20 current extraction errors or warnings. Refresh after fixing a file."; wrapMode: Text.WordWrap; Layout.fillWidth: true }
            ListView { Layout.fillWidth: true; Layout.fillHeight: true; clip: true; model: backend.failures; spacing: 16
                delegate: ColumnLayout {
                    required property var modelData
                    width: ListView.view.width
                    Label { text: modelData.path; wrapMode: Text.WrapAnywhere; Layout.fillWidth: true }
                    Label { text: modelData.error || modelData.warning; wrapMode: Text.WordWrap; Layout.fillWidth: true }
                }
            }
        }
    }
    Dialog {
        id: preview
        objectName: "resultPreview"
        property var selectedResult: ({})
        title: selectedResult.title || "Preview"
        anchors.centerIn: parent
        width: Math.min(window.width - 64, 800)
        height: Math.min(window.height - 64, 600)
        standardButtons: Dialog.Close
        onOpened: backend.previewResult(selectedResult)
        onClosed: backend.closePreview()
        ColumnLayout {
            anchors.fill: parent
            Image { visible: backend.previewUrl.toString() !== ""; source: backend.previewUrl; asynchronous: true; fillMode: Image.PreserveAspectFit; Layout.fillWidth: true; Layout.fillHeight: true }
            Label { visible: backend.previewMessage !== ""; text: backend.previewMessage; wrapMode: Text.WordWrap; Layout.fillWidth: true }
            Label { visible: !!preview.selectedResult.warning; text: preview.selectedResult.warning || ""; wrapMode: Text.WordWrap; Layout.fillWidth: true }
            ScrollView { visible: preview.selectedResult.kind !== "image"; Layout.fillWidth: true; Layout.fillHeight: backend.previewUrl.toString() === ""; Layout.preferredHeight: backend.previewUrl.toString() !== "" ? 100 : -1
                TextArea { text: preview.selectedResult.text || ""; readOnly: true; wrapMode: Text.WordWrap; selectByMouse: true }
            }
            Label { text: (preview.selectedResult.page > 0 ? "Page " + preview.selectedResult.page + " · " : "") + (preview.selectedResult.path || ""); elide: Text.ElideMiddle; Layout.fillWidth: true }
            RowLayout {
                Button { text: "Open file"; onClicked: backend.openResult(preview.selectedResult) }
                Button { text: "Show in Dolphin"; onClicked: backend.reveal(preview.selectedResult.path) }
            }
        }
    }

    pageStack.initialPage: Kirigami.Page {
            title: "Find what you remember"
            padding: Kirigami.Units.largeSpacing
            ColumnLayout {
                anchors.fill: parent
                spacing: Kirigami.Units.largeSpacing
                RowLayout {
                    Layout.fillWidth: true
                    TextField { id: searchField; Layout.fillWidth: true; text: backend.query; placeholderText: "Describe a document, page or picture…"; enabled: backend.modelReady; onTextEdited: backend.query = text; focus: true
                        Keys.onDownPressed: { results.currentIndex = Math.min(results.count - 1, results.currentIndex + 1); results.forceActiveFocus() }
                    }
                    ComboBox { model: ["Everything", "Documents", "PDFs", "Images"]; onActivated: backend.kind = ["", "text", "pdf", "image"][currentIndex] }
                    Button { text: "Choose folder"; icon.name: "folder-open"; enabled: backend.connected && backend.modelReady; onClicked: folderPicker.open() }
                }
                Kirigami.InlineMessage { Layout.fillWidth: true; visible: backend.message !== "Ready" && backend.message !== ""; text: backend.message; type: backend.errors > 0 ? Kirigami.MessageType.Warning : Kirigami.MessageType.Information }
                RowLayout { visible: !backend.modelReady; Layout.fillWidth: true
                    Label { text: "Local search needs an 865 MB model download. No account is required."; wrapMode: Text.WordWrap; Layout.fillWidth: true }
                    Button { text: backend.downloading ? "Downloading…" : "Download model"; enabled: !backend.downloading; onClicked: backend.downloadModel() }
                }
                Label { visible: !backend.modelReady; Layout.fillWidth: true; wrapMode: Text.WordWrap; textFormat: Text.RichText; text: 'EmbeddingGemma 2 by Google · <a href="https://ai.google.dev/gemma/apache_2">Apache 2.0</a> · <a href="https://huggingface.co/google/embeddinggemma-2/blob/914f7f89142e33e77833254d9c9b90c3cef7303b/README.md">Model card</a> · <a href="https://ai.google.dev/gemma/prohibited_use_policy">Usage policy</a>'; onLinkActivated: link => Qt.openUrlExternally(link) }
                ProgressBar { visible: backend.downloading || backend.busy; Layout.fillWidth: true; indeterminate: backend.busy && backend.total === 0; from: 0; to: backend.downloading ? 100 : Math.max(1, backend.total); value: backend.downloading ? backend.downloadPercent : backend.progress }

                SplitView {
                    Layout.fillWidth: true; Layout.fillHeight: true
                    orientation: Qt.Horizontal
                    Pane {
                        SplitView.preferredWidth: 260; SplitView.minimumWidth: 220
                        ColumnLayout {
                            anchors.fill: parent
                            Kirigami.Heading { text: "Selected folders"; level: 3 }
                            Label { visible: backend.roots.length === 0; text: "Choose folders you want to search. Nothing is indexed automatically."; wrapMode: Text.WordWrap; Layout.fillWidth: true }
                            ListView { Layout.fillWidth: true; Layout.fillHeight: true; clip: true; model: backend.roots; spacing: 12
                                delegate: ColumnLayout {
                                    required property var modelData
                                    width: ListView.view.width
                                    Label { text: modelData.path; elide: Text.ElideMiddle; Layout.fillWidth: true; ToolTip.text: modelData.path; ToolTip.visible: hovered; property bool hovered: pathHover.hovered; HoverHandler { id: pathHover } }
                                    RowLayout {
                                        Button { text: "Exclude…"; flat: true; onClicked: { exclusionsDialog.rootPath = modelData.path; exclusionText.text = (modelData.exclusions || []).join("\n"); exclusionsDialog.open() } }
                                        ToolButton { icon.name: "list-remove"; text: "Remove folder"; display: AbstractButton.IconOnly; ToolTip.text: text; ToolTip.visible: hovered; onClicked: backend.removeRoot(modelData.path) }
                                    }
                                }
                            }
                            Label { text: backend.indexed + " indexed · " + backend.errors + " errors"; Layout.fillWidth: true }
                            Button { text: "View errors and warnings…"; visible: backend.failures.length > 0; flat: true; onClicked: failuresDialog.open() }
                            RowLayout {
                                Button { text: backend.paused ? "Resume" : "Pause"; enabled: backend.modelReady; onClicked: backend.togglePause() }
                                Button { text: "Refresh"; enabled: backend.modelReady; onClicked: backend.scan() }
                            }
                            Button { text: "Clear search data…"; flat: true; enabled: backend.connected; onClicked: wipeDialog.open() }
                        }
                    }
                    Pane {
                        SplitView.fillWidth: true
                        ColumnLayout {
                            anchors.fill: parent
                            RowLayout { Layout.fillWidth: true
                                Kirigami.Heading { text: "Results"; level: 3; Layout.fillWidth: true }
                                Label { text: backend.results.length > 0 ? backend.results.length + " matches" : "" }
                            }
                            ListView {
                                id: results
                                Layout.fillWidth: true; Layout.fillHeight: true
                                model: backend.results; clip: true; spacing: 4; currentIndex: -1
                                Keys.onReturnPressed: if (currentIndex >= 0) backend.openResult(backend.results[currentIndex])
                                Keys.onSpacePressed: if (currentIndex >= 0) { preview.selectedResult = backend.results[currentIndex]; preview.open() }
                                delegate: ItemDelegate {
                                    required property var modelData
                                    required property int index
                                    width: ListView.view.width; highlighted: ListView.isCurrentItem
                                    onClicked: { results.currentIndex = index; preview.selectedResult = modelData; preview.open() }
                                    contentItem: RowLayout {
                                        Kirigami.Icon { source: modelData.kind === "image" ? "image-x-generic" : modelData.kind === "pdf" ? "application-pdf" : "text-x-generic"; Layout.preferredWidth: 32; Layout.preferredHeight: 32 }
                                        ColumnLayout { Layout.fillWidth: true
                                            Label { text: modelData.title; font.bold: true; elide: Text.ElideRight; Layout.fillWidth: true }
                                            Label { text: modelData.text || "Image match"; maximumLineCount: 2; wrapMode: Text.WordWrap; elide: Text.ElideRight; Layout.fillWidth: true }
                                            Label { text: (modelData.page > 0 ? "Page " + modelData.page + " · " : "") + modelData.path; opacity: 0.65; elide: Text.ElideMiddle; Layout.fillWidth: true }
                                        }
                                        ToolButton { icon.name: "document-open"; text: "Open file"; display: AbstractButton.IconOnly; ToolTip.text: text; ToolTip.visible: hovered; onClicked: backend.openResult(modelData) }
                                        ToolButton { icon.name: "folder-open"; text: "Show in Dolphin"; display: AbstractButton.IconOnly; ToolTip.text: text; ToolTip.visible: hovered; onClicked: backend.reveal(modelData.path) }
                                    }
                                }
                                Kirigami.PlaceholderMessage { anchors.centerIn: parent; width: parent.width - 32; visible: results.count === 0; text: backend.query.length > 0 ? "No results yet" : "Search by words, meaning or visual content"; explanation: backend.query.length > 0 ? "Try different words, or check that your folder is indexed." : "Describe what you remember. Exact names and identifiers work too." }
                            }
                            Label { text: "Meaning matches are suggestions. Check the source before relying on them."; opacity: 0.65; wrapMode: Text.WordWrap; Layout.fillWidth: true; visible: backend.results.length > 0 }
                        }
                    }
                }
            }
    }
}
