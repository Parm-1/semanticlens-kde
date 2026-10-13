import QtQuick
import QtQuick.Controls
import QtQuick.Layouts
import QtQuick.Dialogs
import org.kde.kirigami as Kirigami

Item {
    id: page
    objectName: "filmPage"
    required property var client
    property int selectedIndex: -1
    readonly property var selectedResult: selectedIndex >= 0 && selectedIndex < client.results.length ? client.results[selectedIndex] : ({})
    function focusSearch() { filmSearch.forceActiveFocus(); filmSearch.selectAll() }
    function choose(index) {
        selectedIndex = index
        filmResults.currentIndex = index
        client.clearSelection()
    }
    function revealSelected() {
        if (selectedResult.evidence && selectedResult.evidence.length)
            client.revealEvidence(selectedResult.evidence[0].reference)
    }
    function coverageText(sources) {
        if (!sources || !sources.length) return "Coverage unreported"
        return sources.map(function(source) {
            var coverage = source.coverage || {}
            return source.source_id + ": " + (coverage.kind || "unreported") +
                (coverage.unit_ids && coverage.unit_ids.length ? " · " + coverage.unit_ids.join(", ") : "")
        }).join("\n")
    }
    function matchLabel(result) {
        if (result.source_exact) return "Source quote match"
        if (result.identity_match) return "Title match"
        if ((result.lanes || []).some(function(lane) { return lane.lane === "analysis_lexical" })) return "Interpretation text match"
        return "Ranked suggestion"
    }
    function locatorText(evidence) {
        var unit = evidence.unit_id || (evidence.reference && evidence.reference.unit_id) || ""
        if (evidence.page) return "PDF page " + evidence.page + (unit ? " · " + unit : "")
        if (evidence.cue_start_ms !== undefined && evidence.cue_start_ms !== null) {
            var seconds = Math.floor(evidence.cue_start_ms / 1000)
            var milliseconds = evidence.cue_start_ms - seconds * 1000
            return "Subtitle offset " + seconds + "." + ("00" + milliseconds).slice(-3) + " s" + (unit ? " · " + unit : "") + " (playback alignment unverified)"
        }
        return unit
    }
    function jobSummary() {
        var active = 0, failed = 0
        for (var i=0; i<client.jobs.length; i++) {
            var state = client.jobs[i].state
            if (state === "pending" || state === "running" || state === "paused") active++
            if (state === "failed" || state === "interrupted") failed++
        }
        return (active ? active + " active imports" : "No active imports") +
            (failed ? " · " + failed + " failed or interrupted" : "") +
            (client.warnings.length ? " · " + client.warnings.length + " warnings" : "")
    }
    Connections { target: page.client; function onResultsChanged() { page.selectedIndex = -1 } }
    FolderDialog { id: collectionPicker; title: "Choose a film collection folder"; onAccepted: client.addCollection(selectedFolder) }
    FileDialog { id: importPicker; property bool analysis: false; title: analysis ? "Import a film analysis sidecar" : "Import a film source manifest"; nameFilters: ["JSON sidecar (*.json)"]; onAccepted: client.importFile(selectedFile, analysis) }
    Dialog {
        id: removeDialog; title: "Remove this film collection?"; anchors.centerIn: parent; standardButtons: Dialog.Ok | Dialog.Cancel
        onAccepted: client.removeCollection()
        Label { textFormat: Text.PlainText; text: "Removes this collection's indexed sources and analyses. Original files are kept."; wrapMode: Text.WordWrap; width: 360 }
    }
    Dialog {
        id: exclusionsDialog; title: "Film collection exclusions"; anchors.centerIn: parent; standardButtons: Dialog.Ok | Dialog.Cancel
        onAccepted: client.setExclusions(exclusions.text)
        TextArea { id: exclusions; textFormat: TextEdit.PlainText; placeholderText: "One relative prefix per line"; width: 420; height: 160 }
    }
    Dialog {
        id: statusDialog; objectName: "filmStatusDialog"; title: "Film status and imports"; anchors.centerIn: parent; modal: true; standardButtons: Dialog.Close
        width: Math.min(680, page.width-32); height: Math.min(460, page.height-40)
        ScrollView {
            anchors.fill: parent; clip: true; contentWidth: availableWidth
            ColumnLayout {
                width: parent.width; spacing: 12
                Label { textFormat: Text.PlainText; text: client.message; wrapMode: Text.WrapAnywhere; Layout.fillWidth: true }
                Repeater { model: client.warnings; delegate: Label { required property var modelData; textFormat:Text.PlainText;text:typeof modelData==="string"?modelData:modelData.error||modelData.warning||"Collection warning";wrapMode:Text.WordWrap;Layout.fillWidth:true } }
                Repeater { model: client.jobs; delegate: RowLayout { required property var modelData; Layout.fillWidth:true; Label {textFormat:Text.PlainText;text:modelData.kind + " · " + modelData.state + " · " + modelData.stage + (modelData.error ? " — " + modelData.error : "");wrapMode:Text.WordWrap;Layout.fillWidth:true} ToolButton {text:"Cancel";visible:modelData.state==="pending"||modelData.state==="running"||modelData.state==="paused";onClicked:client.cancelJob(modelData.job_id)} } }
            }
        }
    }
    ColumnLayout {
        anchors.fill: parent; anchors.margins: page.height<650 ? 12 : 24; spacing: page.height<650 ? 6 : 12
        RowLayout {
            Layout.fillWidth: true
            Label { textFormat: Text.PlainText; text: "Films · offline pilot"; font.pixelSize: 20; font.weight: Font.DemiBold }
            Item { Layout.fillWidth: true }
            ToolButton {
                objectName: "filmCollectionMenu"; text: "Collections"; icon.name: "folder"; enabled: client.available
                onClicked: collectionMenu.open()
                Menu {
                    id: collectionMenu
                    MenuItem { objectName: "filmAddCollection"; text: "Add collection…"; onTriggered: collectionPicker.open() }
                    MenuSeparator {}
                    MenuItem { text: "Import sources…"; enabled: client.collectionId.length>0; onTriggered: {importPicker.analysis=false; importPicker.open()} }
                    MenuItem { text: "Import analysis…"; enabled: client.collectionId.length>0; onTriggered: {importPicker.analysis=true; importPicker.open()} }
                    MenuItem { text: "Exclusions…"; enabled: client.collectionId.length>0; onTriggered: {for(var i=0;i<client.collections.length;i++)if(client.collections[i].collection_id===client.collectionId)exclusions.text=(client.collections[i].exclusions || []).join("\n");exclusionsDialog.open()} }
                    MenuItem { text: "Remove collection…"; enabled: client.collectionId.length>0; onTriggered: removeDialog.open() }
                }
            }
            ToolButton { text: "Refresh"; icon.name: "view-refresh"; enabled: client.available; onClicked: client.reconcile() }
        }
        Label { visible: page.height>=650; textFormat: Text.PlainText; text: "Search supplied scripts and subtitles, with optional imported interpretations. Everything runs locally."; wrapMode: Text.WordWrap; Layout.fillWidth: true; color: Kirigami.Theme.disabledTextColor }
        RowLayout {
            Layout.fillWidth: true
            ComboBox {
                id: collectionSelector; objectName: "filmCollectionSelector"; Layout.fillWidth: true
                model: { var rows=[{path: "All film collections", collection_id: "", exclusions: []}]; for(var i=0;i<client.collections.length;i++)rows.push(client.collections[i]); return rows }
                textRole: "path"; valueRole: "collection_id"
                delegate: ItemDelegate { required property var modelData; width: collectionSelector.width; contentItem: Label { textFormat: Text.PlainText; text: modelData.path; elide: Text.ElideMiddle } }
                currentIndex: { for (var i=0;i<model.length;i++) if (model[i].collection_id===client.collectionId) return i; return 0 }
                onActivated: {client.selectCollection(currentValue); exclusions.text=(model[currentIndex].exclusions || []).join("\n")}
            }
            CheckBox {
                id: priorOptIn
                objectName: "filmPriorOptIn"; visible: client.mode==="hybrid"; enabled: client.available
                checked: client.includeUnverifiedPrior; onToggled: client.includeUnverifiedPrior=checked
                text: page.height<650 ? "Explore unverified priors" : "Explore unverified prior interpretations (no source grounding)"
                contentItem: Label { text: priorOptIn.text; textFormat: Text.PlainText; wrapMode: Text.WordWrap; leftPadding: priorOptIn.indicator.width+priorOptIn.spacing; verticalAlignment: Text.AlignVCenter }
                ToolTip.visible: hovered
                ToolTip.text: "Explicitly include imported interpretations with no source grounding. They cannot open source evidence."
                Layout.preferredWidth: page.height<650 ? 230 : 450
                Layout.maximumWidth: page.width*0.6
            }
        }
        RowLayout {
            Layout.fillWidth: true
            TextField {
                id: filmSearch; objectName: "filmSearchField"; Layout.fillWidth: true; implicitHeight: 48; enabled: client.available; text: client.query
                placeholderText: "Describe a theme, relationship or remembered line…"; selectByMouse: true; onTextEdited: client.query=text
                Keys.onDownPressed: {if(client.results.length){page.choose(0);filmResults.forceActiveFocus()}}
            }
            ComboBox { objectName: "filmModeSelector"; model: ["Hybrid", "Source only"]; currentIndex: client.mode==="source" ? 1 : 0; onActivated: client.mode=currentIndex === 0 ? "hybrid" : "source" }
        }
        Label { objectName: "filmStatusMessage"; textFormat: Text.PlainText; text: client.message; wrapMode: Text.WordWrap; maximumLineCount: page.height<650 ? 2 : 3; elide: Text.ElideRight; Layout.fillWidth: true }
        Label { textFormat: Text.PlainText; text: "Text hidden until revealed; ranking may use spoilers. Results are suggestions."; wrapMode: Text.WordWrap; Layout.fillWidth: true; color: Kirigami.Theme.disabledTextColor }
        RowLayout {
            Layout.fillWidth: true; Layout.fillHeight: true; spacing: 20
            ListView {
                id: filmResults; objectName: "filmResults"; model: client.results; clip: true; spacing: 8; Layout.fillWidth: true; Layout.fillHeight: true
                Keys.onDownPressed: page.choose(Math.min(page.selectedIndex+1,count-1))
                Keys.onUpPressed: page.choose(Math.max(page.selectedIndex-1,0))
                Keys.onSpacePressed: function(event) { page.revealSelected(); event.accepted = true }
                delegate: ItemDelegate {
                    required property var modelData; required property int index
                    width: ListView.view.width; highlighted: page.selectedIndex===index; onClicked: page.choose(index)
                    Keys.priority: Keys.BeforeItem
                    Keys.onSpacePressed: function(event) {
                        if (page.selectedIndex !== index) page.choose(index)
                        page.revealSelected()
                        event.accepted = true
                    }
                    contentItem: ColumnLayout {
                        Label { textFormat: Text.PlainText; text: modelData.display_title || modelData.film_id; font.weight: Font.DemiBold; wrapMode: Text.WordWrap; Layout.fillWidth: true }
                        Label { textFormat: Text.PlainText; text: modelData.edition_id; color: Kirigami.Theme.disabledTextColor; elide: Text.ElideMiddle; Layout.fillWidth: true }
                        Label { objectName: "filmMatchLabel"; textFormat: Text.PlainText; text: page.matchLabel(modelData) }
                        Label { textFormat: Text.PlainText; text: (modelData.lanes || []).map(function(lane){return lane.lane.replace(/_/g," ")}).join(" · "); wrapMode: Text.WordWrap; Layout.fillWidth: true; color: Kirigami.Theme.disabledTextColor }
                    }
                }
                Label { visible: filmResults.count===0; anchors.centerIn: parent; width: parent.width; horizontalAlignment: Text.AlignHCenter; textFormat: Text.PlainText; text: client.searching ? "Searching films…" : "Import source manifests to begin. Analyses are optional."; wrapMode: Text.WordWrap }
            }
            ScrollView {
                objectName: "filmEvidencePanel"; Layout.preferredWidth: Math.max(280,page.width*0.4); Layout.maximumWidth: page.width*0.55; Layout.fillHeight: true; clip: true; contentWidth: availableWidth
                ColumnLayout {
                    width: parent.width; spacing: 12
                    Label { textFormat: Text.PlainText; text: page.selectedResult.display_title || "Source evidence"; font.pixelSize: 18; font.weight: Font.DemiBold; wrapMode: Text.WordWrap; Layout.fillWidth: true }
                    Repeater {
                        model: page.selectedResult.evidence || []
                        delegate: Button {
                            id: evidenceButton
                            objectName: "filmEvidenceReveal"
                            required property var modelData; Layout.fillWidth: true
                            text: (modelData.reference.reference_kind === "analysis_only" ? "Reveal unverified interpretation" : "Reveal " + (modelData.analysis_id || modelData.reference.analysis_id ? "interpretation and source" : "source excerpt")) + "\n" + (modelData.reference.analysis_id || modelData.reference.source_id || "")
                            contentItem: Label { text: evidenceButton.text; textFormat: Text.PlainText; wrapMode: Text.WrapAnywhere; horizontalAlignment: Text.AlignHCenter; verticalAlignment: Text.AlignVCenter }
                            onClicked: client.revealEvidence(modelData.reference)
                        }
                    }
                    Label { objectName: "filmEvidenceLocator"; textFormat: Text.PlainText; text: client.evidence.label || client.evidence.relative_path || "Select a film, then reveal its evidence or declared interpretation."; wrapMode: Text.WrapAnywhere; Layout.fillWidth: true }
                    Label { textFormat: Text.PlainText; visible: !!client.evidence.source_kind; text: (client.evidence.source_kind || "") + " · " + (client.evidence.edition_relation || "unreported edition relationship").replace(/_/g," "); wrapMode: Text.WordWrap; Layout.fillWidth: true; color: Kirigami.Theme.disabledTextColor }
                    Label { objectName: "filmEvidenceCoverage"; textFormat: Text.PlainText; visible: !!client.evidence.coverage; text: page.coverageText(client.evidence.coverage); wrapMode: Text.WordWrap; Layout.fillWidth: true; color: Kirigami.Theme.disabledTextColor }
                    Label { objectName: "filmAnalysisIdentity"; textFormat: Text.PlainText; visible: !!client.evidence.analysis_text; text: "Analysis: " + (client.evidence.reference && client.evidence.reference.analysis_id || "") + "\nClaim: " + (client.evidence.reference && client.evidence.reference.claim_id || ""); wrapMode: Text.WrapAnywhere; Layout.fillWidth: true; color: Kirigami.Theme.disabledTextColor }
                    Label { objectName: "filmGenerationMetadata"; textFormat: Text.PlainText; visible: !!client.evidence.analysis_text; text: "Declared basis: " + (client.evidence.basis || "unreported").replace(/_/g," ") + (client.evidence.facet ? " · " + client.evidence.facet.replace(/_/g," ") : "") + (client.evidence.scope ? " · " + client.evidence.scope : "") + (client.evidence.generation && client.evidence.generation.model_id ? "\nDeclared model: " + client.evidence.generation.model_id + (client.evidence.generation.model_revision ? " @ " + client.evidence.generation.model_revision : "") : ""); wrapMode: Text.WrapAnywhere; Layout.fillWidth: true; color: Kirigami.Theme.disabledTextColor }
                    Label { objectName: "filmEvidenceUnit"; textFormat: Text.PlainText; text: page.locatorText(client.evidence); wrapMode: Text.WordWrap; Layout.fillWidth: true; color: Kirigami.Theme.disabledTextColor }
                    TextArea { objectName: "filmSourceExcerpt"; textFormat: TextEdit.PlainText; visible: text.length>0; text: client.evidence.source_excerpt || client.evidence.unit_text || ""; readOnly: true; selectByMouse: true; wrapMode: TextEdit.Wrap; Layout.fillWidth: true; background: Rectangle {color: Kirigami.Theme.alternateBackgroundColor;radius:6} }
                    Label { textFormat: Text.PlainText; visible: !!client.evidence.analysis_text; text: client.evidence.origin==="authored_fixture" ? "Declared origin: authored fixture · interpretation unreviewed" : client.evidence.origin==="human_curated" ? "Declared origin: human-curated · interpretation unreviewed" : "Declared origin: imported AI · not independently reviewed"; wrapMode: Text.WordWrap; Layout.fillWidth: true }
                    TextArea { objectName: "filmAnalysisText"; textFormat: TextEdit.PlainText; visible: text.length>0; text: client.evidence.analysis_text || ""; readOnly: true; selectByMouse: true; wrapMode: TextEdit.Wrap; Layout.fillWidth: true }
                    Label { textFormat: Text.PlainText; text: (client.evidence.limitations || []).join("\n"); visible: text.length>0; wrapMode: Text.WordWrap; Layout.fillWidth: true; color: Kirigami.Theme.disabledTextColor }
                    Label { objectName: "filmUnobservable"; textFormat: Text.PlainText; text: (client.evidence.not_observable || []).length ? "Not established from these sources: " + client.evidence.not_observable.join(", ") : ""; visible: text.length>0; wrapMode: Text.WordWrap; Layout.fillWidth: true; color: Kirigami.Theme.disabledTextColor }
                    Button { objectName: "filmOpenSource"; text: "Open verified source copy"; enabled: client.evidence.reference_kind === "source" && !!client.evidence.source_excerpt; Layout.fillWidth: true; onClicked: client.navigate("open") }
                    Button { objectName: "filmRevealSource"; text: "Show verified copy in Dolphin"; enabled: client.evidence.reference_kind === "source" && !!client.evidence.source_excerpt; Layout.fillWidth: true; onClicked: client.navigate("reveal") }
                }
            }
        }
        RowLayout {
            Layout.fillWidth: true
            Label { objectName: "filmJobSummary"; textFormat: Text.PlainText; text: page.jobSummary(); wrapMode: Text.WordWrap; Layout.fillWidth: true; color: client.warnings.length ? Kirigami.Theme.negativeTextColor : Kirigami.Theme.disabledTextColor }
            ToolButton { objectName: "filmStatusDetails"; text: "Status and imports"; icon.name: "dialog-information"; onClicked: statusDialog.open() }
        }
    }
}
