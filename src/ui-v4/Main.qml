import QtQuick
import QtQuick.Controls
import QtQuick.Layouts
import QtQuick.Dialogs
import org.kde.kirigami as Kirigami

Kirigami.ApplicationWindow {
    id: window
    width: 1180
    height: 780
    minimumWidth: 720
    minimumHeight: 520
    title: "SemanticLens for KDE"
    property int selectedIndex: -1
    property bool previewVisible: true
    property bool gridMode: true
    readonly property var filmClient: typeof filmBackend === "undefined" ? null : filmBackend
    property bool filmMode: false
    property var selectedResult: selectedIndex >= 0 && selectedIndex < backend.results.length
                                  ? backend.results[selectedIndex] : ({})
    readonly property int gutter: 24
    readonly property int gap: 16
    readonly property bool narrow: width < 900
    readonly property color subtleColor: Kirigami.Theme.disabledTextColor
    color: Kirigami.Theme.backgroundColor

    function clearSelection() {
        selectedIndex = -1
        backend.closePreview()
    }
    function chooseResult(index) {
        if (index < 0 || index >= backend.results.length) return
        selectedIndex = index
        if (previewVisible) backend.previewResult(backend.results[index])
        if (narrow && previewVisible) previewPanel.forceActiveFocus()
    }
    function setFilter(value) {
        if (backend.kind === value) return
        clearSelection()
        backend.kind = value
    }
    function clearSearch() {
        clearSelection()
        searchField.clear()
        backend.query = ""
        searchField.forceActiveFocus()
    }
    function focusSearch() {
        if (filmMode && filmPage.item) { filmPage.item.focusSearch(); return }
        searchField.forceActiveFocus()
        searchField.selectAll()
    }
    function thumbnailFor(result) {
        const generation = backend.thumbnailRevision
        return generation >= 0 ? backend.thumbnailUrl(result) : ""
    }

    onClosing: backend.closePreview()
    onFilmModeChanged: {
        clearSelection()
        if (filmClient) filmClient.setEnabled(filmMode)
    }
    pageStack.globalToolBar.style: Kirigami.ApplicationHeaderStyle.None
    onNarrowChanged: {
        if (narrow && previewVisible && selectedIndex >= 0) backend.previewResult(selectedResult)
        else if (narrow || !previewVisible) backend.closePreview()
        else if (previewVisible && selectedIndex >= 0) backend.previewResult(selectedResult)
    }

    FolderDialog {
        id: folderPicker
        title: "Choose a folder to search"
        onAccepted: backend.addRoot(selectedFolder)
    }
    Dialog {
        id: exclusionsDialog
        property string rootPath: ""
        title: "Exclude paths from this folder"
        anchors.centerIn: parent
        width: Math.min(window.width - 48, 560)
        standardButtons: Dialog.Ok | Dialog.Cancel
        onAccepted: backend.setExclusions(rootPath, exclusionText.text)
        ColumnLayout {
            width: parent.width
            Label { textFormat: Text.PlainText; text: exclusionsDialog.rootPath; elide: Text.ElideMiddle; Layout.fillWidth: true }
            Label { textFormat: Text.PlainText; text: "One relative file or folder path per line. Existing matches are removed from search."; wrapMode: Text.WordWrap; Layout.fillWidth: true }
            ScrollView {
                Layout.fillWidth: true; Layout.preferredHeight: 180
                TextArea { id: exclusionText; textFormat: TextEdit.PlainText; placeholderText: "Archive\nDownloads/large-export.csv" }
            }
        }
    }
    Dialog {
        id: wipeDialog
        objectName: "wipeConfirmation"
        title: "Remove the search index?"
        anchors.centerIn: parent
        standardButtons: Dialog.Ok | Dialog.Cancel
        onAccepted: { clearSelection(); backend.wipe() }
        Label { textFormat: Text.PlainText; text: "This clears selected folders and their search data. Your original files are kept."; wrapMode: Text.WordWrap; width: Math.min(420, window.width - 96) }
    }
    Dialog {
        id: failuresDialog
        objectName: "failuresDialog"
        title: "Indexing details"
        anchors.centerIn: parent
        width: Math.min(window.width - 48, 720)
        height: Math.min(window.height - 48, 520)
        standardButtons: Dialog.Close
        ColumnLayout {
            anchors.fill: parent
            Label { textFormat: Text.PlainText; text: "Folder indexing warnings and extraction details. Resolve the issue, then refresh the selected folders."; wrapMode: Text.WordWrap; Layout.fillWidth: true }
            ListView {
                Layout.fillWidth: true; Layout.fillHeight: true; clip: true; model: backend.failures; spacing: 14
                delegate: ColumnLayout {
                    required property var modelData
                    width: ListView.view.width
                    Label { textFormat: Text.PlainText; text: modelData.path || "Folder"; wrapMode: Text.WrapAnywhere; Layout.fillWidth: true; font.weight: Font.DemiBold }
                    Label { textFormat: Text.PlainText; text: modelData.error || modelData.warning || "Indexing could not complete."; wrapMode: Text.WordWrap; Layout.fillWidth: true }
                    Kirigami.Separator { Layout.fillWidth: true }
                }
            }
        }
    }

    Dialog {
        id: settingsDialog
        objectName: "settingsDialog"
        title: "Settings"
        anchors.centerIn: parent
        width: Math.min(window.width - 40, 760)
        height: Math.min(window.height - 40, 700)
        standardButtons: Dialog.Close
        ScrollView {
            anchors.fill: parent
            clip: true
            ColumnLayout {
                width: settingsDialog.availableWidth
                spacing: 24
                ColumnLayout {
                    Layout.fillWidth: true; spacing: 6
                    Kirigami.Heading { text: "Local model"; level: 3 }
                    Label {
                        Layout.fillWidth: true; textFormat: Text.PlainText; wrapMode: Text.WordWrap
                        text: backend.modelReady ? "The local embedding model is ready. Queries and indexed content stay on this device." : "SemanticLens uses a local 865 MB model. Downloading is explicit; no account is required."
                    }
                    ProgressBar {
                        Layout.fillWidth: true; visible: backend.downloading
                        from: 0; to: 100; value: backend.downloadPercent
                    }
                    RowLayout {
                        Layout.fillWidth: true
                        Button { objectName: "downloadModelButton"; text: backend.downloading ? "Downloading…" : "Download model"; icon.name: "download"; enabled: backend.modelSetupNeeded && !backend.downloading; onClicked: backend.downloadModel() }
                        Button { text: "Retry connection"; icon.name: "view-refresh"; enabled: !backend.modelReady && !backend.downloading && !backend.busy; onClicked: backend.retryService() }
                        Item { Layout.fillWidth: true }
                    }
                    Label {
                        Layout.fillWidth: true; visible: backend.modelSetupNeeded
                        textFormat: Text.RichText; wrapMode: Text.WordWrap
                        text: 'EmbeddingGemma 2 by Google · <a href="https://ai.google.dev/gemma/apache_2">Apache 2.0</a> · <a href="https://huggingface.co/google/embeddinggemma-2/blob/914f7f89142e33e77833254d9c9b90c3cef7303b/README.md">Model card</a> · <a href="https://ai.google.dev/gemma/prohibited_use_policy">Usage policy</a>'
                        onLinkActivated: link => Qt.openUrlExternally(link)
                    }
                }
                Kirigami.Separator { Layout.fillWidth: true }
                ColumnLayout {
                    Layout.fillWidth: true; spacing: 10
                    RowLayout {
                        Layout.fillWidth: true
                        Kirigami.Heading { text: "Search folders"; level: 3; Layout.fillWidth: true }
                        Button { text: "Add folder"; icon.name: "folder-add"; enabled: backend.connected && backend.modelReady; onClicked: folderPicker.open() }
                    }
                    Label {
                        Layout.fillWidth: true; visible: backend.roots.length === 0
                        textFormat: Text.PlainText; wrapMode: Text.WordWrap
                        text: "Choose only the folders you want to search. Nothing is indexed automatically."
                    }
                    Repeater {
                        model: backend.roots
                        delegate: Rectangle {
                            required property var modelData
                            Layout.fillWidth: true; implicitHeight: rootColumn.implicitHeight + 20
                            radius: 8; color: Kirigami.Theme.alternateBackgroundColor
                            ColumnLayout {
                                id: rootColumn
                                anchors.left: parent.left; anchors.right: parent.right; anchors.top: parent.top
                                anchors.margins: 10; spacing: 8
                                Label { textFormat: Text.PlainText; text: modelData.path || ""; elide: Text.ElideMiddle; Layout.fillWidth: true; font.weight: Font.DemiBold }
                                RowLayout {
                                    Layout.fillWidth: true
                                    Button { text: "Exclusions"; flat: true; onClicked: { exclusionsDialog.rootPath = modelData.path || ""; exclusionText.text = (modelData.exclusions || []).join("\n"); exclusionsDialog.open() } }
                                    ToolButton { icon.name: "list-remove"; text: "Remove folder"; display: AbstractButton.IconOnly; ToolTip.text: "Remove folder"; onClicked: backend.removeRoot(modelData.path) }
                                }
                            }
                        }
                    }
                    RowLayout {
                        Layout.fillWidth: true
                        Button { text: backend.paused ? "Resume indexing" : "Pause indexing"; icon.name: backend.paused ? "media-playback-start" : "media-playback-pause"; enabled: backend.modelReady; onClicked: backend.togglePause() }
                        Button { text: "Refresh folders"; icon.name: "view-refresh"; enabled: backend.modelReady; onClicked: backend.scan() }
                        Button { text: "Indexing details"; visible: backend.failures.length > 0; onClicked: failuresDialog.open() }
                    }
                    Label { objectName: "indexingNotice"; textFormat: Text.PlainText; text: backend.indexingNotice; visible: text.length > 0; wrapMode: Text.WordWrap; Layout.fillWidth: true; color: subtleColor }
                    Label { textFormat: Text.PlainText; text: backend.indexed + " files indexed · " + backend.errors + " issues"; Layout.fillWidth: true; color: subtleColor }
                    ProgressBar { objectName: "indexProgress"; visible: backend.busy && !backend.downloading; Layout.fillWidth: true; indeterminate: backend.total === 0; from: 0; to: Math.max(1, backend.total); value: backend.progress }
                    Button { text: "Clear search data…"; flat: true; enabled: backend.connected; onClicked: wipeDialog.open() }
                }
                Kirigami.Separator { Layout.fillWidth: true }
                Label { textFormat: Text.PlainText; text: backend.message; visible: backend.message.length > 0 && backend.message !== "Ready"; wrapMode: Text.WordWrap; Layout.fillWidth: true }
            }
        }
    }

    header: ToolBar {
        contentHeight: 54
        RowLayout {
            anchors.fill: parent
            anchors.leftMargin: 20; anchors.rightMargin: 12
            spacing: 12
            Kirigami.Icon { source: "search"; implicitWidth: 24; implicitHeight: 24; color: Kirigami.Theme.highlightColor }
            ColumnLayout {
                spacing: 0
                Label { textFormat: Text.PlainText; text: "SemanticLens"; font.weight: Font.DemiBold; font.pixelSize: 16 }
                Label { textFormat: Text.PlainText; text: "Find by meaning"; color: subtleColor; font.pixelSize: 11 }
            }
            Item { Layout.fillWidth: true }
            ToolButton { objectName: "fileModeButton"; text: "Files"; visible: window.filmClient !== null; checkable: true; checked: !window.filmMode; onClicked: window.filmMode = false }
            ToolButton { objectName: "filmModeButton"; text: "Films · pilot"; visible: window.filmClient !== null; checkable: true; checked: window.filmMode; onClicked: window.filmMode = true }
            ToolButton { objectName: "settingsButton"; text: "Settings"; icon.name: "settings-configure"; onClicked: settingsDialog.open() }
        }
    }

    pageStack.initialPage: Kirigami.Page {
        title: ""
        padding: 0
        ColumnLayout {
            visible: !window.filmMode
            anchors.fill: parent
            spacing: 0
            ColumnLayout {
                Layout.fillWidth: true
                Layout.leftMargin: window.gutter
                Layout.rightMargin: window.gutter
                Layout.topMargin: 24
                Layout.bottomMargin: 18
                spacing: 16
                RowLayout {
                    Layout.fillWidth: true
                    Kirigami.Heading { text: "Find what you remember"; level: 1; Layout.fillWidth: true }
                    ToolButton {
                        objectName: "previewToggle"
                        text: window.narrow && window.selectedIndex >= 0 ? (window.previewVisible ? "Back to results" : "Preview selected") : (window.previewVisible ? "Hide preview" : "Show preview")
                        icon.name: window.previewVisible ? "view-right-close" : "view-preview"
                        visible: true
                        enabled: !window.narrow || window.selectedIndex >= 0
                        onClicked: {
                            window.previewVisible = !window.previewVisible
                            if (!window.previewVisible) backend.closePreview()
                            else if (window.selectedIndex >= 0) backend.previewResult(window.selectedResult)
                            if (window.narrow) {
                                if (window.previewVisible) previewPanel.forceActiveFocus()
                                else resultsSurface.focusCurrentResults()
                            }
                        }
                    }
                    ToolButton { objectName: "viewModeToggle"; text: window.gridMode ? "List view" : "Grid view"; icon.name: window.gridMode ? "view-list-details" : "view-grid"; onClicked: window.gridMode = !window.gridMode }
                }
                RowLayout {
                    Layout.fillWidth: true; spacing: 10
                    TextField {
                        id: searchField
                        objectName: "searchField"
                        Layout.fillWidth: true
                        Layout.preferredHeight: 54
                        text: backend.query
                        enabled: backend.modelReady
                        placeholderText: "Describe a document, page or picture…"
                        selectByMouse: true
                        leftPadding: 44
                        font.pixelSize: 16
                        onTextEdited: {
                            window.clearSelection()
                            backend.query = text
                        }
                        Keys.onDownPressed: {
                            if (backend.results.length > 0) {
                                if (window.narrow) {
                                    window.previewVisible = false
                                    backend.closePreview()
                                }
                                window.chooseResult(0)
                                resultsSurface.focusCurrentResults()
                            }
                        }
                        Keys.onEscapePressed: window.clearSearch()
                        background: Rectangle {
                            radius: 12
                            color: Kirigami.Theme.backgroundColor
                            border.width: searchField.activeFocus ? 2 : 1
                            border.color: searchField.activeFocus ? Kirigami.Theme.highlightColor : window.subtleColor
                        }
                        Kirigami.Icon { anchors.left: parent.left; anchors.leftMargin: 14; anchors.verticalCenter: parent.verticalCenter; source: "search"; implicitWidth: 20; implicitHeight: 20; color: subtleColor }
                    }
                    ToolButton { objectName: "clearSearchButton"; icon.name: "edit-clear"; text: "Clear search"; display: AbstractButton.IconOnly; enabled: backend.query.length > 0; ToolTip.text: "Clear search"; onClicked: window.clearSearch() }
                }
                RowLayout {
                    Layout.fillWidth: true; spacing: 8
                    Repeater {
                        model: [ {label: "All", value: ""}, {label: "Documents", value: "text"}, {label: "PDFs", value: "pdf"}, {label: "Images", value: "image"} ]
                        delegate: Button {
                            required property var modelData
                            id: filterButton
                            objectName: modelData.value === "" ? "filterAll" : modelData.value === "text" ? "filterDocuments" : modelData.value === "pdf" ? "filterPdfs" : "filterImages"
                            text: modelData.label
                            property bool filterSelected: backend.kind === modelData.value
                            checkable: false
                            height: 34
                            Layout.minimumHeight: 34
                            Accessible.role: Accessible.RadioButton
                            Accessible.name: modelData.label
                            Accessible.checked: filterSelected
                            onClicked: window.setFilter(modelData.value)
                            background: Rectangle {
                                radius: height / 2
                                color: filterButton.filterSelected ? Kirigami.Theme.highlightColor : Kirigami.Theme.alternateBackgroundColor
                                border.width: filterButton.filterSelected ? 0 : 1
                                border.color: window.subtleColor
                            }
                            contentItem: Label { textFormat: Text.PlainText; text: filterButton.text; horizontalAlignment: Text.AlignHCenter; verticalAlignment: Text.AlignVCenter; color: filterButton.filterSelected ? Kirigami.Theme.highlightedTextColor : Kirigami.Theme.textColor; leftPadding: 14; rightPadding: 14 }
                        }
                    }
                    Item { Layout.fillWidth: true }
                    Label { textFormat: Text.PlainText; text: backend.results.length > 0 ? backend.results.length + " matches" : ""; color: subtleColor }
                }
                Rectangle {
                    objectName: "statusMessage"
                    Layout.fillWidth: true
                    visible: statusLabel.text.length > 0 || backend.errors > 0
                    implicitHeight: statusRow.implicitHeight + 16
                    radius: 6
                    color: Kirigami.Theme.alternateBackgroundColor
                    border.color: backend.modelReady ? Kirigami.Theme.neutralTextColor : Kirigami.Theme.highlightColor
                    RowLayout {
                        id: statusRow
                        anchors.left: parent.left; anchors.right: parent.right; anchors.top: parent.top
                        anchors.margins: 8
                        ColumnLayout {
                            Layout.fillWidth: true
                            spacing: 4
                            Label {
                                id: statusLabel
                                objectName: "mainIndexingNotice"
                                Layout.fillWidth: true
                                visible: text.length > 0
                                textFormat: Text.PlainText
                                text: {
                                    const lines = []
                                    if (backend.message.length > 0 && backend.message !== "Ready") lines.push(backend.message)
                                    if (backend.indexingNotice.length > 0 && backend.indexingNotice !== backend.message) lines.push(backend.indexingNotice)
                                    return lines.join("\n")
                                }
                                wrapMode: Text.WordWrap
                            }
                            Label {
                                objectName: "indexingIssues"
                                Layout.fillWidth: true
                                visible: backend.errors > 0
                                textFormat: Text.PlainText
                                text: backend.errors + (backend.errors === 1 ? " indexing issue" : " indexing issues")
                            }
                        }
                        Button {
                            objectName: "indexingDetailsButton"
                            text: "Details"
                            flat: true
                            visible: backend.failures.length > 0
                            onClicked: failuresDialog.open()
                        }
                    }
                }
                RowLayout {
                    Layout.fillWidth: true
                    visible: backend.busy || backend.downloading
                    Label { textFormat: Text.PlainText; text: backend.downloading ? "Downloading local model" : backend.paused ? "Indexing paused" : "Indexing selected folders"; color: subtleColor; Layout.fillWidth: true }
                    ProgressBar { Layout.preferredWidth: 180; visible: backend.downloading; from: 0; to: 100; value: backend.downloadPercent }
                    ProgressBar { Layout.preferredWidth: 180; visible: backend.busy && !backend.downloading; indeterminate: backend.total === 0; from: 0; to: Math.max(1, backend.total); value: backend.progress }
                    Button { text: "Details"; visible: backend.failures.length > 0; flat: true; onClicked: failuresDialog.open() }
                }
            }

            Kirigami.Separator { Layout.fillWidth: true }

            RowLayout {
                id: workspace
                Layout.fillWidth: true; Layout.fillHeight: true; spacing: 0
                Rectangle {
                    id: resultsSurface
                    objectName: "resultsSurface"
                    Layout.fillWidth: true; Layout.fillHeight: true
                    visible: !window.narrow || !window.previewVisible || window.selectedIndex < 0
                    color: "transparent"
                    focus: true
                    function focusCurrentResults() {
                        if (resultLoader.item) (resultLoader.item as Item).forceActiveFocus()
                    }
                    Keys.onPressed: event => {
                        const count = backend.results.length
                        if (event.key === Qt.Key_Down || event.key === Qt.Key_Right) {
                            if (count > 0) window.chooseResult(Math.min(count - 1, Math.max(0, window.selectedIndex + 1)))
                            event.accepted = true
                        } else if (event.key === Qt.Key_Up || event.key === Qt.Key_Left) {
                            if (count > 0) window.chooseResult(Math.max(0, window.selectedIndex - 1))
                            event.accepted = true
                        } else if (event.key === Qt.Key_Return || event.key === Qt.Key_Enter) {
                            if (window.selectedIndex >= 0) backend.openResult(window.selectedResult)
                            event.accepted = true
                        } else if (event.key === Qt.Key_Escape) {
                            window.clearSearch(); event.accepted = true
                        }
                    }

                    Loader {
                        id: resultLoader
                        anchors.fill: parent
                        anchors.bottomMargin: meaningNote.visible ? meaningNote.implicitHeight + 28 : 0
                        active: backend.results.length > 0
                        sourceComponent: window.gridMode ? gridResults : listResults
                    }

                    ColumnLayout {
                        anchors.centerIn: parent
                        width: Math.min(parent.width - 48, 540)
                        spacing: 14
                        visible: backend.results.length === 0 && backend.query.length === 0
                        Kirigami.Icon { source: "edit-find"; implicitWidth: 44; implicitHeight: 44; color: Kirigami.Theme.highlightColor; Layout.alignment: Qt.AlignHCenter }
                        Kirigami.Heading { text: "Your files, found by memory"; level: 2; Layout.fillWidth: true; horizontalAlignment: Text.AlignHCenter }
                        Label { textFormat: Text.PlainText; text: "Describe a topic, a page detail or a picture. Search runs locally across folders you choose."; wrapMode: Text.WordWrap; horizontalAlignment: Text.AlignHCenter; color: subtleColor; Layout.fillWidth: true }
                        Button { text: "Configure search"; icon.name: "settings-configure"; Layout.alignment: Qt.AlignHCenter; onClicked: settingsDialog.open() }
                    }
                    ColumnLayout {
                        anchors.centerIn: parent
                        width: Math.min(parent.width - 48, 520)
                        spacing: 10
                        visible: backend.results.length === 0 && backend.query.length > 0 && !backend.searching
                        Kirigami.Icon { source: "edit-find"; implicitWidth: 36; implicitHeight: 36; color: subtleColor; Layout.alignment: Qt.AlignHCenter }
                        Kirigami.Heading { text: "No matches yet"; level: 2; Layout.fillWidth: true; horizontalAlignment: Text.AlignHCenter }
                        Label { textFormat: Text.PlainText; text: "Try a different phrase, or check that the folder is selected and indexed."; color: subtleColor; wrapMode: Text.WordWrap; horizontalAlignment: Text.AlignHCenter; Layout.fillWidth: true }
                    }
                    BusyIndicator { anchors.centerIn: parent; running: backend.searching && backend.results.length === 0; visible: running; objectName: "searchBusyIndicator" }
                    Label {
                        id: meaningNote
                        objectName: "meaningMatchNote"
                        anchors.left: parent.left; anchors.right: parent.right; anchors.bottom: parent.bottom
                        anchors.leftMargin: 20; anchors.rightMargin: 20; anchors.bottomMargin: 10
                        visible: backend.results.length > 0
                        textFormat: Text.PlainText
                        text: "Meaning matches are suggestions. Check the source before relying on them."
                        color: window.subtleColor; font.pixelSize: 11; wrapMode: Text.WordWrap
                    }
                }

                Kirigami.Separator { Layout.fillHeight: true; visible: previewPanel.visible && !window.narrow }

                Rectangle {
                    id: previewPanel
                    objectName: "previewPanel"
                    Layout.preferredWidth: window.narrow ? 0 : Math.min(360, Math.max(300, window.width * 0.31))
                    Layout.fillWidth: window.narrow
                    Layout.minimumWidth: visible ? (window.narrow ? 0 : 280) : 0
                    Layout.fillHeight: true
                    visible: window.previewVisible && (!window.narrow || window.selectedIndex >= 0)
                    color: Kirigami.Theme.alternateBackgroundColor
                    focus: window.narrow && window.selectedIndex >= 0
                    Keys.priority: Keys.BeforeItem
                    Keys.onPressed: event => {
                        if (event.key === Qt.Key_Escape) {
                            window.previewVisible = false
                            backend.closePreview()
                            resultsSurface.focusCurrentResults()
                            event.accepted = true
                        } else if ((event.key === Qt.Key_Left || event.key === Qt.Key_Up) && window.selectedIndex > 0) {
                            window.chooseResult(window.selectedIndex - 1); event.accepted = true
                        } else if ((event.key === Qt.Key_Right || event.key === Qt.Key_Down) && window.selectedIndex < backend.results.length - 1) {
                            window.chooseResult(window.selectedIndex + 1); event.accepted = true
                        } else if (event.key === Qt.Key_Return || event.key === Qt.Key_Enter) {
                            backend.openResult(window.selectedResult); event.accepted = true
                        }
                    }
                    ScrollView {
                        id: previewScroll
                        contentWidth: availableWidth
                        objectName: "passageScroll"
                        anchors.fill: parent
                        anchors.margins: 14
                        padding: 0
                        clip: true
                        ColumnLayout {
                            width: previewScroll.availableWidth
                                                        spacing: 9
                        RowLayout {
                            Layout.fillWidth: true
                            Kirigami.Heading { text: "Preview"; level: 3; Layout.fillWidth: true }
                            ToolButton { icon.name: "window-close"; text: "Clear preview"; display: AbstractButton.IconOnly; ToolTip.text: "Clear preview"; onClicked: window.clearSelection() }
                        }
                        Rectangle {
                            objectName: "previewArtwork"
                            Layout.fillWidth: true
                            Layout.preferredHeight: Math.max(92, Math.min(190, previewPanel.height * 0.28))
                            radius: 10; color: Kirigami.Theme.backgroundColor; border.color: window.subtleColor
                            visible: window.selectedIndex < 0 || window.selectedResult.kind === "image" || window.selectedResult.kind === "pdf"
                            Image {
                                objectName: "previewImage"
                                anchors.fill: parent; anchors.margins: 8
                                visible: source.toString().length > 0
                                source: window.selectedIndex < 0 ? "" : backend.previewUrl
                                asynchronous: true; cache: false; fillMode: Image.PreserveAspectFit
                            }
                            ColumnLayout {
                                anchors.centerIn: parent
                                width: parent.width - 28
                                spacing: 8
                                visible: window.selectedIndex < 0 || (backend.previewUrl.toString().length === 0 && backend.previewMessage.length === 0)
                                Kirigami.Icon { source: window.selectedIndex < 0 ? "document-preview" : "image-x-generic"; implicitWidth: 36; implicitHeight: 36; color: subtleColor; Layout.alignment: Qt.AlignHCenter }
                                Label { textFormat: Text.PlainText; text: window.selectedIndex < 0 ? "Select a result to preview" : "Preparing preview…"; color: subtleColor; horizontalAlignment: Text.AlignHCenter; wrapMode: Text.WordWrap; Layout.fillWidth: true }
                            }
                            BusyIndicator { anchors.centerIn: parent; running: window.selectedIndex >= 0 && (backend.previewMessage === "Rendering page…" || backend.previewMessage === "Loading preview…"); visible: running }
                        }
                        Label {
                            objectName: "previewStatus"
                            visible: backend.previewMessage.length > 0 && backend.previewMessage !== "Rendering page…" && backend.previewMessage !== "Loading preview…"
                            textFormat: Text.PlainText; text: backend.previewMessage; wrapMode: Text.WordWrap; color: subtleColor; Layout.fillWidth: true
                        }
                        Label {
                            objectName: "previewTitle"
                            visible: window.selectedIndex >= 0
                            textFormat: Text.PlainText; text: window.selectedResult.title || window.selectedResult.path || ""
                            font.weight: Font.DemiBold; wrapMode: Text.WordWrap; maximumLineCount: 2; elide: Text.ElideRight; Layout.fillWidth: true
                        }
                        RowLayout {
                            objectName: "resultWarning"
                            visible: window.selectedIndex >= 0 && !!window.selectedResult.warning
                            Layout.fillWidth: true; spacing: 8
                            Kirigami.Icon { source: "dialog-warning"; implicitWidth: 16; implicitHeight: 16; color: Kirigami.Theme.neutralTextColor }
                            Label {
                                textFormat: Text.PlainText
                                text: window.selectedIndex >= 0 ? (window.selectedResult.warning || "") : ""
                                wrapMode: Text.WordWrap; color: window.subtleColor; Layout.fillWidth: true
                            }
                        }
                        Label {
                            visible: window.selectedIndex >= 0 && !!window.selectedResult.page
                            textFormat: Text.PlainText; text: window.selectedResult.page > 0 ? "PDF · Page " + window.selectedResult.page : ""
                            color: subtleColor; Layout.fillWidth: true
                        }
                        TextArea {
                            objectName: "passageText"
                            visible: window.selectedIndex >= 0 && window.selectedResult.kind !== "image"
                            textFormat: TextEdit.PlainText
                            text: window.selectedIndex >= 0 ? (window.selectedResult.text || "") : ""
                            readOnly: true; selectByMouse: true; wrapMode: TextEdit.Wrap
                            Layout.fillWidth: true
                            Layout.preferredWidth: previewScroll.availableWidth
                            Layout.preferredHeight: Math.max(80, contentHeight + topPadding + bottomPadding)
                            background: Rectangle { color: "transparent" }
                        }
                        Label {
                            objectName: "previewPath"
                            visible: window.selectedIndex >= 0
                            textFormat: Text.PlainText; text: window.selectedResult.path || ""
                            color: subtleColor; wrapMode: Text.WrapAnywhere; maximumLineCount: 3; elide: Text.ElideMiddle; Layout.fillWidth: true
                        }
                        RowLayout {
                            objectName: "previewActions"
                            Layout.fillWidth: true
                            visible: window.selectedIndex >= 0
                            Button { text: "Open"; icon.name: "document-open"; Layout.fillWidth: true; onClicked: backend.openResult(window.selectedResult) }
                            Button { text: "Show in Dolphin"; icon.name: "folder-open"; Layout.fillWidth: true; onClicked: backend.reveal(window.selectedResult.path || "") }
                        }
                        }
                    }
                }
            }
        }

        Loader {
            id: filmPage
            anchors.fill: parent
            active: window.filmMode
            visible: active
            sourceComponent: Component { FilmPage { client: window.filmClient } }
        }

        Component {
            id: gridResults
            GridView {
                id: gridView
                objectName: "resultGrid"
                anchors.fill: parent
                anchors.margins: 20
                cellWidth: width / Math.max(1, Math.round(width / 240))
                cellHeight: 248
                model: backend.results
                clip: true; focus: true; currentIndex: window.selectedIndex
                keyNavigationEnabled: true; keyNavigationWraps: false
                Keys.priority: Keys.BeforeItem
                Keys.onPressed: event => {
                    if (event.key === Qt.Key_Return || event.key === Qt.Key_Enter) {
                        if (currentIndex >= 0) backend.openResult(backend.results[currentIndex])
                        event.accepted = true
                    } else if (event.key === Qt.Key_Space) {
                        if (currentIndex >= 0) { window.previewVisible = true; window.chooseResult(currentIndex) }
                        event.accepted = true
                    } else if (event.key === Qt.Key_Escape) {
                        if (window.selectedIndex >= 0) window.clearSelection(); else window.clearSearch()
                        event.accepted = true
                    } else if (event.key === Qt.Key_Up || event.key === Qt.Key_Down || event.key === Qt.Key_Left || event.key === Qt.Key_Right) {
                        const columns = Math.max(1, Math.floor(width / cellWidth))
                        const step = event.key === Qt.Key_Up ? -columns : event.key === Qt.Key_Down ? columns : event.key === Qt.Key_Left ? -1 : 1
                        const next = Math.max(0, Math.min(count - 1, currentIndex < 0 ? 0 : currentIndex + step))
                        currentIndex = next; window.chooseResult(next); event.accepted = true
                    }
                }
                onCurrentIndexChanged: if (currentIndex >= 0 && currentIndex !== window.selectedIndex) window.chooseResult(currentIndex)
                delegate: Item {
                    required property var modelData
                    required property int index
                    width: GridView.view.cellWidth - 12; height: GridView.view.cellHeight - 12
                    Rectangle {
                        id: tile
                        anchors.fill: parent
                        radius: 10
                        color: window.selectedIndex === index ? Kirigami.Theme.activeBackgroundColor : Kirigami.Theme.backgroundColor
                        border.width: window.selectedIndex === index ? 2 : 1
                        border.color: window.selectedIndex === index ? Kirigami.Theme.highlightColor : window.subtleColor
                        ColumnLayout {
                            anchors.fill: parent; anchors.margins: 10; spacing: 7
                            Rectangle {
                                Layout.fillWidth: true; Layout.preferredHeight: 126
                                radius: 6; color: Kirigami.Theme.alternateBackgroundColor; clip: true
                                Image {
                                    id: thumb
                                    objectName: "resultThumbnail"
                                    anchors.fill: parent
                                    source: window.thumbnailFor(modelData)
                                    asynchronous: true; cache: true; fillMode: Image.PreserveAspectCrop
                                    visible: modelData.kind !== "text" && status === Image.Ready
                                }
                                Rectangle {
                                    id: documentPaper
                                    anchors.centerIn: parent
                                    width: parent.width * 0.72; height: parent.height * 0.88
                                    radius: 4; color: Kirigami.Theme.backgroundColor; border.color: window.subtleColor
                                    visible: modelData.kind === "text" || (modelData.kind === "pdf" && thumb.status !== Image.Ready)
                                    ColumnLayout {
                                        anchors.fill: parent; anchors.margins: 10; spacing: 5
                                        RowLayout {
                                            Layout.fillWidth: true
                                            Kirigami.Icon { source: modelData.kind === "pdf" ? "application-pdf" : "text-x-generic"; implicitWidth: 13; implicitHeight: 13; color: Kirigami.Theme.highlightColor }
                                            Label { textFormat: Text.PlainText; text: modelData.kind === "pdf" ? "PDF PAGE" : "PASSAGE"; font.pixelSize: 9; font.weight: Font.DemiBold; color: subtleColor; Layout.fillWidth: true }
                                        }
                                        Kirigami.Separator { Layout.fillWidth: true }
                                        Label {
                                            textFormat: Text.PlainText; text: modelData.text || modelData.title || "Document preview"
                                            wrapMode: Text.WordWrap; maximumLineCount: 5; elide: Text.ElideRight
                                            font.pixelSize: 10; Layout.fillWidth: true; Layout.fillHeight: true
                                        }
                                        Rectangle { Layout.fillWidth: true; Layout.preferredHeight: 2; radius: 1; color: Kirigami.Theme.alternateBackgroundColor }
                                    }
                                }
                                Label {
                                    anchors.left: parent.left; anchors.bottom: parent.bottom; anchors.margins: 7
                                    visible: modelData.kind === "pdf" && modelData.page > 0
                                    textFormat: Text.PlainText; text: "Page " + modelData.page
                                    padding: 5; color: Kirigami.Theme.textColor
                                    background: Rectangle { radius: 4; color: Kirigami.Theme.backgroundColor; opacity: 0.92 }
                                }
                                Label {
                                    anchors.right: parent.right; anchors.top: parent.top; anchors.margins: 7
                                    textFormat: Text.PlainText; text: "#" + (index + 1)
                                    font.pixelSize: 10; padding: 4; color: Kirigami.Theme.textColor
                                    background: Rectangle { radius: 4; color: Kirigami.Theme.backgroundColor; opacity: 0.92 }
                                }
                            }
                            Label { textFormat: Text.PlainText; text: modelData.title || modelData.path || "Untitled"; font.weight: Font.DemiBold; elide: Text.ElideRight; Layout.fillWidth: true }
                            Label { textFormat: Text.PlainText; text: modelData.text || (modelData.kind === "image" ? "Image match" : ""); maximumLineCount: 2; wrapMode: Text.WordWrap; elide: Text.ElideRight; color: subtleColor; Layout.fillWidth: true; Layout.fillHeight: true }
                            Label { textFormat: Text.PlainText; text: (modelData.kind === "pdf" && modelData.page > 0 ? "PDF · " : modelData.kind === "image" ? "Image · " : "Document · ") + (modelData.path || ""); elide: Text.ElideMiddle; color: subtleColor; font.pixelSize: 11; Layout.fillWidth: true }
                        }
                    }
                    TapHandler { onTapped: { window.chooseResult(index); gridView.forceActiveFocus() } onDoubleTapped: backend.openResult(modelData) }
                    HoverHandler { cursorShape: Qt.PointingHandCursor }
                    Accessible.role: Accessible.ListItem
                    Accessible.name: (modelData.title || modelData.path || "Result") + (modelData.page > 0 ? ", page " + modelData.page : "")
                }
            }
        }
        Component {
            id: listResults
            ListView {
                objectName: "resultList"
                anchors.fill: parent; anchors.margins: 14
                model: backend.results; clip: true; focus: true; spacing: 6
                currentIndex: window.selectedIndex
                keyNavigationEnabled: true; keyNavigationWraps: false
                Keys.priority: Keys.BeforeItem
                Keys.onPressed: event => {
                    if (event.key === Qt.Key_Return || event.key === Qt.Key_Enter) {
                        if (currentIndex >= 0) backend.openResult(backend.results[currentIndex])
                        event.accepted = true
                    } else if (event.key === Qt.Key_Space) {
                        if (currentIndex >= 0) { window.previewVisible = true; window.chooseResult(currentIndex) }
                        event.accepted = true
                    } else if (event.key === Qt.Key_Escape) {
                        if (window.selectedIndex >= 0) window.clearSelection(); else window.clearSearch()
                        event.accepted = true
                    } else if (event.key === Qt.Key_Up || event.key === Qt.Key_Down) {
                        const step = event.key === Qt.Key_Up ? -1 : 1
                        const next = Math.max(0, Math.min(count - 1, currentIndex < 0 ? 0 : currentIndex + step))
                        currentIndex = next; window.chooseResult(next); event.accepted = true
                    }
                }
                onCurrentIndexChanged: if (currentIndex >= 0 && currentIndex !== window.selectedIndex) window.chooseResult(currentIndex)
                delegate: ItemDelegate {
                    required property var modelData
                    required property int index
                    width: ListView.view.width; height: 104
                    highlighted: window.selectedIndex === index
                    onClicked: window.chooseResult(index)
                    contentItem: RowLayout {
                        spacing: 12
                        Rectangle {
                            Layout.preferredWidth: 100; Layout.preferredHeight: 78; radius: 6; color: Kirigami.Theme.alternateBackgroundColor; clip: true
                            Image { id: listThumb; objectName: "resultThumbnail"; anchors.fill: parent; source: window.thumbnailFor(modelData); asynchronous: true; cache: true; fillMode: Image.PreserveAspectCrop }
                            Kirigami.Icon { anchors.centerIn: parent; source: modelData.kind === "image" ? "image-x-generic" : modelData.kind === "pdf" ? "application-pdf" : "text-x-generic"; implicitWidth: 26; implicitHeight: 26; visible: listThumb.status !== Image.Ready; color: subtleColor }
                        }
                        ColumnLayout {
                            Layout.fillWidth: true; spacing: 4
                            Label { textFormat: Text.PlainText; text: modelData.title || modelData.path || "Untitled"; font.weight: Font.DemiBold; elide: Text.ElideRight; Layout.fillWidth: true }
                            Label { textFormat: Text.PlainText; text: modelData.text || (modelData.kind === "image" ? "Image match" : ""); maximumLineCount: 1; elide: Text.ElideRight; color: subtleColor; Layout.fillWidth: true }
                            Label { textFormat: Text.PlainText; text: (modelData.kind === "pdf" && modelData.page > 0 ? "PDF · Page " + modelData.page + " · " : modelData.kind === "image" ? "Image · " : "Document · ") + (modelData.path || ""); elide: Text.ElideMiddle; color: subtleColor; font.pixelSize: 11; Layout.fillWidth: true }
                        }
                        ToolButton { icon.name: "document-open"; text: "Open file"; display: AbstractButton.IconOnly; ToolTip.text: "Open file"; onClicked: backend.openResult(modelData) }
                        ToolButton { icon.name: "folder-open"; text: "Show in Dolphin"; display: AbstractButton.IconOnly; ToolTip.text: "Show in Dolphin"; onClicked: backend.reveal(modelData.path || "") }
                    }
                    Accessible.name: (modelData.title || modelData.path || "Result") + (modelData.page > 0 ? ", page " + modelData.page : "")
                }
            }
        }
    }

    Connections {
        target: backend
        function onResultsChanged() {
            window.selectedIndex = -1
            backend.closePreview()
        }
        function onChanged() {
            if (searchField.text !== backend.query && !searchField.activeFocus) searchField.text = backend.query
        }
    }
    Shortcut { sequence: "Ctrl+L"; onActivated: window.focusSearch(); context: Qt.ApplicationShortcut }
    Shortcut { sequence: "Ctrl+F"; onActivated: window.focusSearch(); context: Qt.ApplicationShortcut }
}
