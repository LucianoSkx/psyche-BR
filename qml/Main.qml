import QtQuick
import QtQuick.Controls
import QtQuick.Layouts
import QtQuick.Dialogs

ApplicationWindow {
    id: window
    objectName: "psycheWindow"
    width: 960; height: 720
    minimumWidth: 620; minimumHeight: 580
    visible: true; title: "psyche"
    readonly property bool dark: true
    readonly property color canvas: "#111112"
    readonly property color surface: "#19191b"
    readonly property color raised: "#232326"
    readonly property color ink: "#e0e0e4"
    readonly property color muted: "#b0b0b8"
    readonly property color border: "#45454d"
    readonly property color accent: "#c4c4ca"
    readonly property int tabGames: 0
    readonly property int tabImport: 1
    readonly property int tabDownloads: 2
    readonly property int tabLibrary: 3
    readonly property int tabHistory: 4
    readonly property int tabSettings: 5
    color: canvas
    font.family: "Sans Serif"; font.pixelSize: 14
    palette.window: canvas; palette.base: surface; palette.text: ink
    palette.windowText: ink; palette.button: raised; palette.buttonText: ink
    palette.highlight: "#66666e"; palette.highlightedText: "#ffffff"
    palette.placeholderText: muted; palette.mid: border; palette.light: border; palette.dark: canvas
    property string removingId: ""
    property string removingName: ""
    property bool hydrated: false
    property bool searched: false
    property bool needsKey: false
    property bool dropHover: false
    property string notice: ""
    property string lastSearch: preferences.lastQuery
    property string pendingAppId: ""
    property string pendingName: ""
    property int restoreIndex: -1
    property string restoreDestination: ""
    FontLoader { id: pixel; objectName: "pixelFont"; source: "fonts/PixelifySans.ttf" }
    Component.onCompleted: { width = preferences.windowWidth; height = preferences.windowHeight; hydrated = true }
    onClosing: function(close) {
        if (backend.busy) {
            close.accepted = false
            window.notice = "Aguarde a tarefa atual terminar."
            return
        }
        preferences.saveWindow(width, height)
    }
    function downloadOSIndex() {
        const values = backend.platforms.length > 0 ? backend.platforms : ["linux", "windows", "mac"]
        return Math.max(0, values.indexOf(preferences.downloadOS))
    }
    function formatOS(value) {
        return value === "linux" ? "Linux" : value === "windows" ? "Windows" : "macOS"
    }
    function contentLabel() {
        const i = ["full", "basegame", "dlc", "zip"].indexOf(preferences.downloadContent)
        return ["Jogo + DLCs", "Só o jogo base", "Só DLCs", "ZIP completo"][Math.max(0, i)]
    }
    function promptKey() {
        window.needsKey = true
        tabs.currentIndex = window.tabSettings
    }
    function requestFetch(appId, name) {
        if (!preferences.hasApiKey) { promptKey(); return }
        window.pendingAppId = appId
        window.pendingName = name
        fetchDialog.open()
    }
    function runSearch(offset) {
        if (!preferences.hasApiKey) { promptKey(); return }
        window.needsKey = false
        if (offset === 0) lastSearch = query.text.trim()
        if (/^[0-9]+$/.test(lastSearch)) { requestFetch(lastSearch, ""); return }
        searched = true
        backend.search(lastSearch, offset)
    }
    function acceptZip(url) {
        const path = String(url)
        if (!path.toLowerCase().endsWith(".zip")) {
            window.notice = "Solte um único arquivo ZIP."
            return false
        }
        window.notice = ""
        backend.inspect(url)
        return true
    }
    function pathIndex(items, path) {
        for (let i = 0; i < items.length; ++i) if (items[i].path === path) return i
        return -1
    }
    function formatWhen(value) {
        const date = new Date(value)
        return isNaN(date.getTime()) ? "" : Qt.locale().toString(date, Locale.ShortFormat)
    }
    Connections { target: preferences; function onChanged() { if (tabs.currentIndex === window.tabLibrary) backend.refreshLibrary() } }
    Connections { target: backend; function onPackageLoaded() { tabs.currentIndex = window.tabImport } }
    Connections {
        target: backend
        function onChanged() { if (!backend.busy && window.notice === "Aguarde a tarefa atual terminar.") window.notice = "" }
    }

    component PixelText: Label { textFormat: Text.PlainText; font.family: pixel.name; font.pixelSize: 26; color: window.ink; wrapMode: Text.WordWrap }
    component Hint: Label { textFormat: Text.PlainText; color: window.muted; wrapMode: Text.WordWrap; Layout.fillWidth: true; font.pixelSize: 12 }
    component Action: Button {
        id: action
        property bool primary: false
        implicitHeight: 34; leftPadding: 12; rightPadding: 12
        hoverEnabled: true
        contentItem: Text {
            text: action.text; font: action.font
            horizontalAlignment: Text.AlignHCenter; verticalAlignment: Text.AlignVCenter
            color: !action.enabled ? "#85858c" : action.primary ? "#171719" : window.ink
            elide: Text.ElideRight
        }
        background: Rectangle {
            radius: 4
            color: !action.enabled ? "#242427" : action.primary ? (action.hovered ? "#e1e1e5" : window.accent) : action.hovered || action.down || action.checked ? "#35353d" : action.flat ? "transparent" : window.raised
            border.width: action.activeFocus ? 2 : 1
            border.color: action.activeFocus ? window.ink : action.primary && action.enabled ? window.accent : window.border
        }
    }
    component Input: TextField {
        implicitHeight: 36; leftPadding: 10; rightPadding: 10
        color: window.ink; placeholderTextColor: window.muted
        selectByMouse: true
        background: Rectangle { color: window.surface; radius: 4; border.width: parent.activeFocus ? 2 : 1; border.color: parent.activeFocus ? window.accent : window.border }
    }
    component PathField: Input { readOnly: true; Layout.fillWidth: true; font.pixelSize: 12; Accessible.name: "Caminho" }
    component Card: Pane {
        padding: 12
        contentHeight: contentItem.children.length > 0 ? contentItem.children[0].implicitHeight : 0
        background: Rectangle { color: window.surface; radius: 6; border.color: window.border }
    }
    component ChoiceBox: ComboBox {
        id: box
        enabled: !backend.busy; implicitHeight: 40
        contentItem: Text {
            text: box.displayText; color: box.enabled ? window.ink : window.muted
            verticalAlignment: Text.AlignVCenter; elide: Text.ElideRight
            leftPadding: 12; rightPadding: 28; font.pixelSize: 13
        }
        background: Rectangle { color: window.raised; radius: 4; border.color: box.activeFocus ? window.ink : window.border }
        indicator: Text { text: "⌄"; color: window.ink; font.pixelSize: 20; x: box.width - width - 12; anchors.verticalCenter: parent.verticalCenter }
        delegate: ItemDelegate {
            required property int index
            required property var modelData
            width: box.width; highlighted: box.highlightedIndex === index
            contentItem: Text {
                text: box.textRole && modelData && typeof modelData === "object" ? (modelData[box.textRole] || "") : String(modelData)
                color: window.ink; elide: Text.ElideRight; leftPadding: 8; font.pixelSize: 13
            }
            background: Rectangle { color: highlighted ? window.surface : window.raised }
        }
        popup: Popup {
            y: box.height + 2; width: box.width; padding: 1
            implicitHeight: Math.min(240, contentItem.implicitHeight + 2)
            contentItem: ListView {
                clip: true; implicitHeight: contentHeight
                model: box.popup.visible ? box.delegateModel : null
                currentIndex: box.highlightedIndex
                ScrollBar.vertical: ScrollBar {}
            }
            background: Rectangle { color: window.raised; radius: 4; border.color: window.border }
        }
    }
    component ContentChoice: ChoiceBox {
        model: ["Jogo + DLCs", "Só o jogo base", "Só DLCs", "ZIP completo"]
        currentIndex: ["full", "basegame", "dlc", "zip"].indexOf(preferences.downloadContent)
        onActivated: preferences.setDownloadContent(["full", "basegame", "dlc", "zip"][currentIndex])
        Accessible.name: "Conteúdo para baixar da Hubcap"
        ToolTip.visible: hovered; ToolTip.text: "Cada download usa a cota diária da Hubcap."
    }
    component Tick: CheckBox {
        id: tick
        indicator: Rectangle {
            implicitWidth: 18; implicitHeight: 18
            x: tick.leftPadding; y: parent.height / 2 - height / 2
            radius: 3; color: window.surface
            border.color: tick.activeFocus ? window.ink : window.border
            Rectangle {
                anchors.centerIn: parent; width: 10; height: 10; radius: 2
                color: window.accent; visible: tick.checked
            }
        }
        contentItem: Text {
            text: tick.text; color: window.ink; font.pixelSize: 13
            leftPadding: tick.indicator.width + 8; verticalAlignment: Text.AlignVCenter
        }
    }
    component NavTab: TabButton {
        id: nav
        implicitHeight: 38
        contentItem: Text {
            text: nav.text; font.family: pixel.name
            font.pixelSize: window.width < 720 ? 15 : 18
            color: nav.checked ? window.ink : window.muted
            horizontalAlignment: Text.AlignHCenter; verticalAlignment: Text.AlignVCenter
            elide: Text.ElideRight
        }
        background: Rectangle {
            color: nav.hovered || nav.checked ? window.surface : "transparent"
            border.color: nav.activeFocus ? window.muted : "transparent"
            Rectangle { anchors.bottom: parent.bottom; width: parent.width; height: 2; color: window.accent; visible: nav.checked }
        }
    }
    component Sheet: Dialog {
        id: sheet
        modal: true; parent: Overlay.overlay; anchors.centerIn: parent; focus: true
        width: Math.min(window.width - 32, 480)
        closePolicy: Popup.CloseOnEscape | Popup.CloseOnPressOutside
        palette.window: window.surface; palette.windowText: window.ink
        background: Rectangle { color: window.surface; radius: 6; border.color: window.border }
        header: Label {
            text: sheet.title; font.family: pixel.name; font.pixelSize: 20; color: window.ink
            padding: 16; bottomPadding: 8; wrapMode: Text.WordWrap
        }
        padding: 16
    }
    header: Pane {
        padding: 0; background: Rectangle { color: window.canvas }
        ColumnLayout {
            anchors.fill: parent; spacing: 12
            RowLayout {
                Layout.fillWidth: true; Layout.topMargin: 16; Layout.leftMargin: 16; Layout.rightMargin: 16
                PixelText { text: "psyche_"; font.pixelSize: 30; Accessible.name: "psyche" }
                Item { Layout.fillWidth: true }
                Action {
                    text: preferences.hasApiKey ? "Hubcap" : "+ Conectar Hubcap"
                    flat: true; onClicked: tabs.currentIndex = window.tabSettings
                    Accessible.name: "Configurar conexão Hubcap"
                }
            }
            TabBar {
                id: tabs; objectName: "mainTabs"
                Layout.fillWidth: true; Layout.leftMargin: 16; Layout.rightMargin: 16
                Component.onCompleted: currentIndex = preferences.lastTab
                onCurrentIndexChanged: if (window.hydrated) preferences.saveNavigation(currentIndex, window.lastSearch)
                background: Item {}
                NavTab { text: "Jogos"; objectName: "searchTab" }
                NavTab { text: "Importar"; objectName: "importTab" }
                NavTab { text: "Downloads"; objectName: "downloadsTab" }
                NavTab { text: "Biblioteca"; objectName: "libraryTab" }
                NavTab { text: "Histórico"; objectName: "historyTab" }
                NavTab { text: "Configurações"; objectName: "settingsTab" }
            }
        }
    }
    StackLayout {
        anchors.fill: parent; currentIndex: tabs.currentIndex
        Pane {
            padding: 16; background: Item {}
            ColumnLayout {
                anchors.fill: parent; spacing: 10
                RowLayout {
                    Layout.fillWidth: true; spacing: 10
                    Input {
                        id: query; objectName: "searchQuery"; Layout.fillWidth: true
                        Component.onCompleted: text = preferences.lastQuery
                        placeholderText: "Nome do jogo ou AppID"
                        enabled: !backend.busy; Accessible.name: "Nome do jogo ou AppID"
                        onAccepted: if (text.trim()) window.runSearch(0)
                    }
                    Action {
                        text: "Apagar"; enabled: !backend.busy && query.text.length > 0; Accessible.name: "Apagar texto da busca"
                        ToolTip.visible: hovered; ToolTip.text: "Limpar a caixa de busca"
                        onClicked: { query.text = ""; query.forceActiveFocus() }
                    }
                    Action {
                        text: "Colar"; enabled: !backend.busy; Accessible.name: "Colar texto na busca"
                        ToolTip.visible: hovered; ToolTip.text: "Colar da área de transferência"
                        onClicked: { query.forceActiveFocus(); query.paste() }
                    }
                    Action { text: "Buscar"; primary: true; enabled: !backend.busy && query.text.trim().length > 0; onClicked: window.runSearch(0) }
                }
                RowLayout {
                    Layout.fillWidth: true
                    ContentChoice { Layout.preferredWidth: 180 }
                    Hint {
                        text: preferences.hasApiKey ? "Usa cota da Hubcap" : "Conecte a Hubcap em Configurações para buscar."
                        horizontalAlignment: Text.AlignRight
                    }
                }
                Label {
                    visible: backend.networkIssue.length > 0; text: backend.networkIssue
                    color: "#e06c75"; font.pixelSize: 12; Layout.fillWidth: true
                }
                ListView {
                    id: results; objectName: "searchResults"
                    Layout.fillWidth: true; Layout.fillHeight: true
                    clip: true; spacing: 10; model: backend.games
                    ScrollBar.vertical: ScrollBar {}
                    delegate: ItemDelegate {
                        id: result
                        required property var modelData
                        width: results.width; height: window.width < 820 ? 94 : 116
                        enabled: !backend.busy; padding: 12
                        background: Rectangle { radius: 5; color: result.hovered ? window.raised : window.surface; border.color: result.activeFocus ? window.ink : window.border }
                        contentItem: RowLayout {
                            spacing: 10
                            GameCover {
                                appId: modelData.appId; title: modelData.name; active: tabs.currentIndex === window.tabGames
                                dark: true; accent: window.accent
                                Layout.preferredWidth: window.width < 820 ? 120 : 180
                                Layout.preferredHeight: window.width < 820 ? 56 : 84
                            }
                            ColumnLayout {
                                Layout.fillWidth: true; spacing: 6
                                Label { textFormat: Text.PlainText; text: modelData.name; font.pixelSize: 16; color: window.ink; elide: Text.ElideRight; Layout.fillWidth: true }
                                Hint { text: "#" + modelData.appId + (modelData.proton ? " • ProtonDB " + modelData.proton.charAt(0).toUpperCase() + modelData.proton.slice(1) : "") }
                            }
                            Label { textFormat: Text.PlainText; text: "→"; font.pixelSize: 22; color: window.muted }
                        }
                        onClicked: window.requestFetch(modelData.appId, modelData.name)
                        Accessible.name: "Baixar " + modelData.name + ", AppID " + modelData.appId + ", usa cota da Hubcap"
                    }
                    Column {
                        anchors.centerIn: parent; width: parent.width - 32; spacing: 10
                        visible: results.count === 0 && !backend.busy && results.height > 90
                        PixelText { text: window.searched ? "Nada aqui." : "Qual jogo?"; font.pixelSize: 30; anchors.horizontalCenter: parent.horizontalCenter }
                        Label { textFormat: Text.PlainText; text: window.searched ? "Tente outro nome." : "Busque acima ou abra um ZIP."; color: window.muted; anchors.horizontalCenter: parent.horizontalCenter }
                    }
                    BusyIndicator { anchors.centerIn: parent; running: backend.activity === "search"; visible: running; palette.dark: window.accent }
                }
                RowLayout {
                    visible: backend.games.length > 0 || backend.hasMore || backend.searchOffset > 0
                    Layout.fillWidth: true
                    Action { text: "Anterior"; Accessible.name: "Página anterior"; enabled: !backend.busy && backend.searchOffset > 0; onClicked: window.runSearch(Math.max(0, backend.searchOffset - 100)) }
                    Hint { text: "Página " + (Math.floor(backend.searchOffset / 100) + 1); horizontalAlignment: Text.AlignHCenter }
                    Action { text: "Próxima"; Accessible.name: "Próxima página"; enabled: !backend.busy && backend.hasMore; onClicked: window.runSearch(backend.searchOffset + 100) }
                }
            }
        }
        Pane {
            padding: 16; background: Item {}
            ColumnLayout {
                anchors.fill: parent; spacing: 10
                RowLayout {
                    Layout.fillWidth: true
                    PixelText { text: backend.ready ? "Pronto para adicionar." : "Importar ZIP"; Layout.fillWidth: true }
                    Action { text: "Abrir ZIP"; enabled: !backend.busy; onClicked: zipDialog.open() }
                }
                ScrollView {
                    id: importScroll; objectName: "importPage"
                    Layout.fillWidth: true; Layout.fillHeight: true; clip: true; contentWidth: width
                    ScrollBar.horizontal: ScrollBar { policy: ScrollBar.AlwaysOff }
                    ColumnLayout {
                        id: importContent
                        width: importScroll.width; spacing: 10
                        Card {
                            objectName: "zipCard"
                            visible: !backend.ready
                            Layout.fillWidth: true; contentHeight: 164
                            background: Rectangle {
                                color: window.surface; radius: 6
                                border.color: window.dropHover ? window.ink : window.border
                                border.width: window.dropHover ? 2 : 1
                            }
                            Column {
                                width: parent.width; anchors.verticalCenter: parent.verticalCenter; spacing: 12
                                PixelText { text: "+ ZIP"; font.pixelSize: 32; anchors.horizontalCenter: parent.horizontalCenter }
                                Hint { width: parent.width; text: backend.activity === "import" ? "Preparando…" : "Solte o arquivo aqui."; horizontalAlignment: Text.AlignHCenter }
                                BusyIndicator { running: backend.activity === "import"; visible: running; anchors.horizontalCenter: parent.horizontalCenter; palette.dark: window.accent }
                            }
                            DropArea {
                                anchors.fill: parent
                                onEntered: function(drag) { window.dropHover = !backend.busy; drag.accepted = !backend.busy }
                                onExited: window.dropHover = false
                                onDropped: function(drop) {
                                    window.dropHover = false
                                    if (backend.busy) return
                                    if (!drop.hasUrls || drop.urls.length !== 1) {
                                        window.notice = "Solte um único arquivo ZIP."
                                        return
                                    }
                                    if (window.acceptZip(drop.urls[0]))
                                        drop.acceptProposedAction()
                                }
                            }
                        }
                        Card {
                            objectName: "readyCard"
                            visible: backend.ready; Layout.fillWidth: true
                            ColumnLayout {
                                width: parent.width; spacing: 10
                                RowLayout {
                                    Layout.fillWidth: true; spacing: 10
                                    GameCover {
                                        visible: backend.appId.length > 0; active: visible && tabs.currentIndex === window.tabImport
                                        appId: backend.appId; title: backend.source; dark: true; accent: window.accent
                                        Layout.preferredWidth: 140
                                        Layout.preferredHeight: 65
                                    }
                                    ColumnLayout {
                                        Layout.fillWidth: true; spacing: 10
                                        Label { textFormat: Text.PlainText; text: backend.source; font.pixelSize: 16; color: window.ink; wrapMode: Text.WrapAnywhere; Layout.fillWidth: true }
                                        Hint { text: backend.counts.apps + " apps  ·  " + backend.counts.depots + " depots  ·  " + backend.counts.keys + " chaves" }
                                    }
                                }
                                Hint { visible: backend.ready; text: "Download na aba Downloads." }
                                Input { Layout.fillWidth: true; text: backend.gameName; placeholderText: "Nome do jogo nos comentários"; Accessible.name: "Nome do jogo nos comentários YAML"; enabled: !backend.busy && !backend.applied; onTextEdited: backend.gameName = text }
                                Action { id: details; objectName: "entriesToggle"; text: checked ? "− Ocultar entradas" : "+ Ver entradas"; checkable: true; flat: true; Accessible.name: "Mostrar entradas do pacote" }
                                ScrollView {
                                    visible: details.checked; Layout.fillWidth: true; Layout.preferredHeight: Math.min(140, entriesText.implicitHeight)
                                    TextArea { id: entriesText; text: backend.preview; padding: 8; background: Rectangle { color: window.canvas; radius: 4 } readOnly: true; wrapMode: TextEdit.Wrap; selectByMouse: true; font.family: "monospace"; font.pixelSize: 12; color: window.ink; Accessible.name: "Entradas do pacote" }
                                }
                            }
                        }
                    }
                }
                RowLayout {
                    Layout.fillWidth: true
                    ColumnLayout {
                        Layout.fillWidth: true; spacing: 4
                        Label { textFormat: Text.PlainText; text: "config.yaml"; color: window.ink; font.pixelSize: 13 }
                        Label { textFormat: Text.PlainText; text: preferences.destination || "Escolha o diretório SLSsteam"; color: window.muted; font.pixelSize: 12; elide: Text.ElideLeft; Layout.fillWidth: true }
                    }
                    Action { text: "Trocar"; flat: true; enabled: !backend.busy; onClicked: destinationDialog.open() }
                }
                RowLayout {
                    Layout.fillWidth: true; spacing: 12
                    Hint { visible: backend.ready && !preferences.destinationValid; text: "Escolha um diretório SLSsteam." }
                    Hint { visible: backend.applied; text: "Backup salvo no Histórico." }
                    Action {
                        visible: backend.applied
                        text: "Abrir Biblioteca"
                        onClicked: tabs.currentIndex = window.tabLibrary
                    }
                    Action {
                        objectName: "applyButton"
                        text: backend.applied ? "Adicionado ✓" : backend.activity === "apply" ? "Adicionando…" : "Adicionar ao config.yaml"
                        primary: true
                        enabled: backend.ready && preferences.destinationValid && !backend.busy && !backend.applied
                        onClicked: backend.apply()
                    }
                }
            }
        }

        Pane {
            padding: 16; background: Item {}
            ColumnLayout {
                anchors.fill: parent; spacing: 12
                PixelText { text: "Downloads" }
                RowLayout {
                    Layout.fillWidth: true; Layout.fillHeight: true; spacing: 12
                Card {
                    Layout.fillWidth: true; Layout.fillHeight: true
                    ColumnLayout {
                        width: parent.width; spacing: 12
                        RowLayout {
                            Layout.fillWidth: true; spacing: 10; enabled: backend.ready
                            GameCover {
                                visible: backend.ready && backend.appId.length > 0
                                appId: backend.appId; title: backend.source; active: tabs.currentIndex === window.tabDownloads
                                dark: true; accent: window.accent
                                Layout.preferredWidth: window.width < 820 ? 120 : 180
                                Layout.preferredHeight: window.width < 820 ? 56 : 84
                            }
                            ColumnLayout {
                                Layout.fillWidth: true; spacing: 6
                                Label {
                                    textFormat: Text.PlainText; color: window.ink; font.pixelSize: 16
                                    text: backend.ready ? backend.source : "Nenhum pacote pronto"
                                    elide: Text.ElideRight; Layout.fillWidth: true
                                }
                                Hint { text: backend.ready ? "#" + backend.appId + (backend.proton.length > 0 ? " • ProtonDB " + backend.proton.charAt(0).toUpperCase() + backend.proton.slice(1) : "") : "Importe um pacote na aba Importar ou abra um ZIP." }
                            }
                        }
                        ScrollView {
                            visible: backend.depotDetails.length > 0; Layout.fillWidth: true; Layout.preferredHeight: backend.depotDetails.length > 0 ? 280 : 0
                            ListView {
                                id: depotView; model: backend.depotDetails; clip: true; spacing: 4; width: parent.width
                                delegate: RowLayout {
                                    width: depotView.width
                                    property string depot: modelData.depot
                                    property bool selected: sel.checked
                                    Tick { id: sel; checked: true; text: modelData.name && modelData.name.length > 0 ? modelData.name + " (" + modelData.depot + ")" : modelData.depot; Accessible.name: "Baixar depot " + modelData.depot }
                                    Item { Layout.preferredWidth: 8 }
                                    Label {
                                        textFormat: Text.PlainText
                                        text: modelData.oslist && modelData.oslist.length > 0 ? modelData.oslist.map(window.formatOS).join(", ") : "—"
                                        color: window.muted; font.pixelSize: 11; Layout.fillWidth: true; elide: Text.ElideRight
                                    }
                                }
                            }
                        }
                        RowLayout {
                            Layout.fillWidth: true; spacing: 10
                            Label { textFormat: Text.PlainText; text: "Sistema"; color: window.ink; font.pixelSize: 13 }
                            ChoiceBox {

                                id: osBox; Layout.preferredWidth: 160
                                model: backend.platforms.length > 0 ? backend.platforms.map(window.formatOS) : ["Linux", "Windows", "macOS"]
                                currentIndex: window.downloadOSIndex()
                                onActivated: {
                                    const values = backend.platforms.length > 0 ? backend.platforms : ["linux", "windows", "mac"]
                                    preferences.setDownloadOS(values[currentIndex])
                                }
                                Accessible.name: "Sistema do download"
                            }
                        }
                        RowLayout {
                            Layout.fillWidth: true; spacing: 10
                            Label { textFormat: Text.PlainText; text: "Biblioteca"; color: window.ink; font.pixelSize: 13 }
                            Label {
                                textFormat: Text.PlainText
                                text: preferences.downloadLibrary || (preferences.libraries.length > 0 ? preferences.libraries[0].path : "Nenhuma biblioteca Steam detectada")
                                color: window.muted; font.pixelSize: 12; elide: Text.ElideLeft; Layout.fillWidth: true
                            }
                            Action { text: "Escolher…"; flat: true; enabled: !backend.busy; onClicked: libraryDialog.open() }
                        }
                        RowLayout {
                            Layout.fillWidth: true; spacing: 10
                            Action {
                                objectName: "downloadButton"
                                text: backend.downloading ? "Baixando… " + backend.downloadPercent + "%" : "Baixar jogo"
                                primary: true
                                enabled: backend.ready && !backend.downloading && !backend.busy
                                onClicked: {
                                    var selected = []
                                    for (var i = 0; i < depotView.count; i++) {
                                        var it = depotView.itemAt(i)
                                        if (it && it.selected)
                                            selected.push(it.depot)
                                    }
                                    backend.downloadGame(selected)
                                }
                            }
                            Action { text: "Cancelar"; flat: true; visible: backend.downloading; onClicked: backend.cancelDownload() }
                            Hint { visible: backend.downloadStatus.length > 0; text: backend.downloadStatus; horizontalAlignment: Text.AlignRight }
                        }
                        ProgressBar {
                            visible: backend.downloading; Layout.fillWidth: true
                            from: 0; to: 100; value: backend.downloadPercent
                        }
                        Label {
                            visible: backend.eosWarning
                            textFormat: Text.PlainText
                            text: "Atenção: jogo usa Epic Online Services (EOSSDK). Pode precisar do proxy EOS do ACCELA/ASSella."
                            color: "#e5c07b"; font.pixelSize: 12; wrapMode: Text.WordWrap; Layout.fillWidth: true
                        }
                    }
                }
                Card {
                    objectName: "workshopCard"
                    Layout.fillWidth: true; Layout.fillHeight: true
                    ColumnLayout {
                        width: parent.width; spacing: 12
                        Label {
                            textFormat: Text.PlainText; color: window.ink; font.pixelSize: 16
                            text: "Workshop"
                        }
                        Hint {
                            text: "Cole URLs ou IDs de mods do Steam Workshop (um por linha ou separados por espaço)."
                        }
                        TextArea {
                            id: workshopInput
                            objectName: "workshopInput"
                            Layout.fillWidth: true
                            Layout.preferredHeight: 90
                            wrapMode: TextEdit.Wrap
                            selectByMouse: true
                            font.family: "monospace"; font.pixelSize: 12
                            color: window.ink
                            placeholderText: "https://steamcommunity.com/sharedfiles/filedetails/?id=123456789"
                            background: Rectangle { color: window.canvas; radius: 4; border.color: window.border }
                            text: backend.workshopInput
                            onTextEdited: backend.workshopInput = text
                            enabled: !backend.workshopBusy
                            Accessible.name: "IDs de mods do Workshop"
                        }
                        Hint {
                            visible: backend.workshopIds.length > 0
                            text: backend.workshopIds.length + (backend.workshopIds.length === 1 ? " item detectado" : " itens detectados")
                        }
                        ListView {
                            id: workshopView; objectName: "workshopList"
                            model: backend.workshopQueue
                            clip: true; spacing: 4
                            Layout.fillWidth: true
                            Layout.preferredHeight: Math.min(backend.workshopQueue.length * 28 + 4, 200)
                            delegate: RowLayout {
                                width: workshopView.width
                                Label {
                                    textFormat: Text.PlainText; color: window.ink; font.pixelSize: 12
                                    font.family: "monospace"
                                    text: modelData.id
                                    Layout.preferredWidth: 130
                                    elide: Text.ElideMiddle
                                }
                                Label {
                                    textFormat: Text.PlainText
                                    font.pixelSize: 11
                                    Layout.fillWidth: true
                                    elide: Text.ElideRight
                                    color: modelData.state === "erro" ? "#e06c75" : modelData.state === "ok" ? "#8fbf7f" : window.muted
                                    text: modelData.state === "erro" ? modelData.error : modelData.state === "ok" ? "Baixado" : "Pendente"
                                }
                            }
                        }
                        RowLayout {
                            Layout.fillWidth: true; spacing: 10
                            Action {
                                objectName: "workshopDownloadButton"
                                text: backend.workshopBusy ? "Baixando… " + backend.workshopPercent + "%" : "Baixar todos"
                                primary: true
                                enabled: backend.workshopIds.length > 0 && !backend.workshopBusy && !backend.busy && !backend.downloading
                                onClicked: backend.downloadWorkshop()
                            }
                            Action { text: "Limpar"; flat: true; enabled: !backend.workshopBusy; onClicked: backend.clearWorkshop() }
                            Hint {
                                visible: backend.workshopStatus.length > 0
                                text: backend.workshopStatus
                                horizontalAlignment: Text.AlignRight
                            }
                        }
                        ProgressBar {
                            visible: backend.workshopBusy; Layout.fillWidth: true
                            from: 0; to: 100; value: backend.workshopPercent
                        }
                        Label {
                            textFormat: Text.PlainText
                            text: "Os mods vão para steamapps/workshop/content na biblioteca escolhida."
                            color: window.muted; font.pixelSize: 11; wrapMode: Text.WordWrap
                            Layout.fillWidth: true
                        }
                    }
                }
            }
        }
        }
        Pane {
            padding: 16; background: Item {}
            ColumnLayout {
                anchors.fill: parent; spacing: 10
                RowLayout {
                    PixelText { text: "Biblioteca"; Layout.fillWidth: true }
                    Action { text: "Refresh"; enabled: !backend.busy; onClicked: backend.refreshLibrary() }
                }
                Hint { text: backend.libraryError || "Games in config.yaml" }
                ListView {
                    id: installedList; objectName: "installedList"
                    Layout.fillWidth: true; Layout.fillHeight: true; clip: true; spacing: 8
                    model: backend.installed; ScrollBar.vertical: ScrollBar {}
                    delegate: Card {
                        required property var modelData
                        width: installedList.width
                        Accessible.name: modelData.name + ", " + modelData.apps + " apps"
                        RowLayout {
                            width: parent.width; spacing: 10
                            GameCover { appId: modelData.appId; title: modelData.name; active: tabs.currentIndex === window.tabLibrary; dark: true; accent: window.accent; Layout.preferredWidth: 100; Layout.preferredHeight: 47 }
                            ColumnLayout {
                                Layout.fillWidth: true; spacing: 4
                                Label { textFormat: Text.PlainText; text: modelData.name; color: window.ink; elide: Text.ElideRight; Layout.fillWidth: true }
                                Hint { text: modelData.apps + " apps · " + modelData.depots + " depots · " + modelData.keys + " chaves" }
                            }
                            Action { text: "Remover"; enabled: !backend.busy; Accessible.name: "Remover " + modelData.name; onClicked: { window.removingId = modelData.id; window.removingName = modelData.name; preferences.refreshSteamRunning(); removeDialog.open() } }
                        }
                    }
                    PixelText { anchors.centerIn: parent; visible: installedList.count === 0 && !backend.libraryError; text: "Nenhum jogo configurado."; font.pixelSize: 26 }
                }
            }
        }
        Pane {
            padding: 16; background: Item {}
            ColumnLayout {
                anchors.fill: parent; spacing: 12
                PixelText { text: "Histórico" }
                ListView {
                    id: historyList; objectName: "historyList"
                    Layout.fillWidth: true; Layout.fillHeight: true; clip: true; spacing: 10; model: preferences.history
                    ScrollBar.vertical: ScrollBar {}
                    delegate: Card {
                        required property var modelData
                        required property int index
                        width: historyList.width
                        Accessible.name: modelData.source + (modelData.restored ? ", restaurado" : ", adicionado")
                        ColumnLayout {
                            width: parent.width; spacing: 14
                            RowLayout {
                                Layout.fillWidth: true; spacing: 14
                                GameCover {
                                    visible: !!modelData.appId; active: visible && tabs.currentIndex === window.tabHistory
                                    appId: modelData.appId || ""; title: modelData.source; dark: true; accent: window.accent
                                    Layout.preferredWidth: 100; Layout.preferredHeight: 47
                                }
                                ColumnLayout {
                                    Layout.fillWidth: true; spacing: 6
                                    Label { textFormat: Text.PlainText; text: modelData.source; color: window.ink; elide: Text.ElideRight; Layout.fillWidth: true }
                                    Hint { text: window.formatWhen(modelData.date) + (modelData.restored ? "  ·  Restaurado" : "  ·  Adicionado") }
                                }
                            }
                            RowLayout {
                                Action { text: "Restaurar"; enabled: !backend.busy && !modelData.restored; Accessible.name: "Restaurar " + modelData.source; onClicked: { window.restoreIndex = index; window.restoreDestination = modelData.destination; restoreDialog.open() } }
                                Action { text: "Abrir backup"; flat: true; Accessible.name: "Abrir backup de " + modelData.source; onClicked: backend.openFolder(modelData.backup) }
                            }
                        }
                    }
                    PixelText { anchors.centerIn: parent; visible: historyList.count === 0; text: "Nada adicionado ainda."; font.pixelSize: 26 }
                }
            }
        }
        ScrollView {
            id: settingsScroll; objectName: "settingsPage"
            clip: true; contentWidth: width
            ScrollBar.horizontal: ScrollBar { policy: ScrollBar.AlwaysOff }
            ColumnLayout {
                width: settingsScroll.width; spacing: 10
                PixelText { text: "Configurações"; Layout.margins: 16; Layout.bottomMargin: 0 }
                Hint { text: "psyche " + (Qt.application.version || ""); Layout.leftMargin: 16; Layout.rightMargin: 16; Layout.topMargin: -4 }
                Hint {
                    visible: window.needsKey && !preferences.hasApiKey
                    text: "A busca precisa de uma chave Hubcap."
                    Layout.leftMargin: 16; Layout.rightMargin: 16
                }
                Card {
                    Layout.fillWidth: true; Layout.leftMargin: 16; Layout.rightMargin: 16
                    ColumnLayout {
                        width: parent.width; spacing: 12
                        Label { textFormat: Text.PlainText; text: "Chave Hubcap"; color: window.ink }
                        Hint { visible: preferences.importedKey && !preferences.environmentKey; text: "Chave do ACCELA." }
                        Hint { visible: preferences.environmentKey; text: "Chave de ambiente ativa." }
                        RowLayout {
                            Input {
                                id: apiKey; objectName: "apiKeyInput"; Layout.fillWidth: true
                                Component.onCompleted: text = preferences.apiKey
                                echoMode: reveal.checked ? TextInput.Normal : TextInput.Password
                                placeholderText: "Cole sua chave"; enabled: !backend.busy; Accessible.name: "Chave Hubcap"
                            }
                            Action { id: reveal; text: checked ? "Ocultar" : "Mostrar"; checkable: true; Accessible.name: "Mostrar chave Hubcap" }
                            Action {
                                text: "Apagar"; enabled: !backend.busy && (apiKey.text.length > 0 || preferences.hasApiKey); Accessible.name: "Apagar chave Hubcap"
                                onClicked: {
                                    apiKey.text = ""
                                    remember.checked = false
                                    preferences.savePreferences("", false, preferences.theme)
                                    savedLabel.text = "Chave apagada."
                                    apiKey.forceActiveFocus()
                                }
                            }
                        }
                        Tick { id: remember; text: "Lembrar neste computador"; checked: preferences.rememberKey; enabled: !backend.busy; Accessible.name: "Lembrar chave Hubcap neste computador" }
                        Hint { visible: remember.checked; text: "Salvo localmente, sem criptografia." }
                        RowLayout {
                            Action {
                                text: "Salvar"; primary: true; enabled: !backend.busy
                                onClicked: {
                                    if (preferences.savePreferences(apiKey.text, remember.checked, preferences.theme)) {
                                        savedLabel.text = "Salvo."
                                        if (preferences.hasApiKey) window.needsKey = false
                                    }
                                }
                            }
                            Label { textFormat: Text.PlainText; id: savedLabel; color: window.muted }
                        }
                    }
                }
                Card {
                    Layout.fillWidth: true; Layout.leftMargin: 16; Layout.rightMargin: 16
                    ColumnLayout {
                        width: parent.width; spacing: 8
                        RowLayout {
                            Label { textFormat: Text.PlainText; text: "Hubcap"; color: window.ink; Layout.fillWidth: true }
                            Action { text: backend.checkingHubcap ? "Verificando…" : "Verificar conexão"; enabled: !backend.checkingHubcap; onClicked: backend.checkHubcap() }
                        }
                        Hint { visible: !!backend.hubcapInfo.health; text: "Serviço: " + (backend.hubcapInfo.health || "") }
                        Hint { visible: !!backend.hubcapInfo.healthError; text: backend.hubcapInfo.healthError || "" }
                        Hint { visible: !!backend.hubcapInfo.accountError; text: backend.hubcapInfo.accountError || "" }
                        Hint {
                            property var account: backend.hubcapInfo.account || ({})
                            visible: !!backend.hubcapInfo.account
                            text: (account.username || "Conta") + " · Hoje: " + (account.daily_usage ?? "—") + " / " + (account.daily_limit ?? "—")
                                + (account.can_make_requests === false ? " · Bloqueada" : "")
                        }
                        Hint {
                            property var account: backend.hubcapInfo.account || ({})
                            visible: !!account.api_key_expires_at
                            text: "Expira: " + (account.api_key_expires_at || "")
                        }
                    }
                }
                Card {
                    Layout.fillWidth: true; Layout.leftMargin: 16; Layout.rightMargin: 16
                    ColumnLayout {
                        width: parent.width; spacing: 12
                        Label { textFormat: Text.PlainText; text: "Diretório SLSsteam"; color: window.ink }
                        PathField { text: preferences.destination || "Não encontrado"; Accessible.name: "Diretório de configuração SLSsteam" }
                        ChoiceBox {
                            visible: preferences.destinations.length > 1; Layout.fillWidth: true
                            model: preferences.destinations; textRole: "path"
                            currentIndex: window.pathIndex(preferences.destinations, preferences.destination)
                            displayText: currentIndex < 0 ? "Escolha uma instalação…" : currentText
                            onActivated: preferences.chooseDestination(preferences.folderUrl(preferences.destinations[currentIndex].path))
                            Accessible.name: "Instalações SLSsteam detectadas"
                        }
                        RowLayout {
                            Action { text: "Trocar diretório"; enabled: !backend.busy; onClicked: destinationDialog.open() }
                            Action { text: "Detectar"; enabled: !backend.busy; onClicked: preferences.detectPaths() }
                            Label { textFormat: Text.PlainText; text: preferences.destinationValid ? "Pronto" : "Selecione um diretório"; color: window.muted; Layout.fillWidth: true; elide: Text.ElideRight }
                        }
                    }
                }
                Item { Layout.preferredHeight: 16 }
            }
        }
    }
    footer: Pane {
        padding: 12; leftPadding: 24; rightPadding: 24
        background: Rectangle { color: window.canvas; Rectangle { width: parent.width; height: 1; color: window.border } }
        ColumnLayout {
            anchors.fill: parent; spacing: 8
            ProgressBar {
                Layout.fillWidth: true; visible: backend.busy; indeterminate: true
                background: Rectangle { implicitHeight: 4; color: window.raised; radius: 2 }
                contentItem: Item {
                    implicitHeight: 4
                    Rectangle { width: parent.width * 0.4; height: parent.height; radius: 2; color: window.accent }
                }
            }
            Label { textFormat: Text.PlainText;
                text: window.notice !== "" ? window.notice : preferences.settingsError ? preferences.message : backend.status
                color: window.notice !== "" || preferences.settingsError || backend.statusKind === "error" ? "#e2aaaa" : window.muted
                wrapMode: Text.WrapAnywhere; Layout.fillWidth: true; font.pixelSize: 12
                Accessible.role: Accessible.StaticText
                Accessible.name: text
            }
        }
    }
    Sheet {
        id: fetchDialog
        title: "Baixar da Hubcap?"
        contentItem: Label {
            textFormat: Text.PlainText; color: window.ink; wrapMode: Text.WordWrap
            Accessible.name: "Confirmar download da Hubcap"
            text: "Isso usa a cota diária da Hubcap.\n\nBaixar " + window.contentLabel() + " para " + (window.pendingName || ("AppID " + window.pendingAppId)) + "?"
        }
        footer: DialogButtonBox {
            Action { text: "Cancelar"; DialogButtonBox.buttonRole: DialogButtonBox.RejectRole }
            Action { text: "Baixar"; primary: true; DialogButtonBox.buttonRole: DialogButtonBox.AcceptRole }
        }
        onAccepted: backend.fetch(window.pendingAppId, window.pendingName)
    }
    Sheet {
        id: removeDialog
        title: "Remover " + window.removingName + "?"
        contentItem: Label {
            textFormat: Text.PlainText
            text: "Remove AppIDs, depots e chaves do config.yaml. Não desinstala o jogo da Steam. Entradas compartilhadas ficam. Um backup é salvo."
                  + (preferences.steamRunning
                         ? "\n\nSteam is open. Restart Steam after removing or the library can stay out of date."
                         : "")
            color: window.ink; wrapMode: Text.WordWrap
            Accessible.name: text
        }
        footer: DialogButtonBox {
            Action { text: "Cancelar"; DialogButtonBox.buttonRole: DialogButtonBox.RejectRole }
            Action { text: "Remover"; DialogButtonBox.buttonRole: DialogButtonBox.AcceptRole }
        }
        onAccepted: backend.removeGame(window.removingId)
    }
    Connections { target: tabs; function onCurrentIndexChanged() { if (tabs.currentIndex === window.tabLibrary) backend.refreshLibrary() } }
    Sheet {
        id: restoreDialog
        title: "Restaurar backup?"
        contentItem: Label {
            textFormat: Text.PlainText
            text: "Isso também desfaz alterações feitas após esta importação.\n\n" + window.restoreDestination
            wrapMode: Text.WrapAnywhere; color: window.ink
            Accessible.name: "Confirmar restauração do backup"
        }
        footer: DialogButtonBox {
            Action { text: "Cancelar"; DialogButtonBox.buttonRole: DialogButtonBox.RejectRole }
            Action { text: "Restaurar"; primary: true; DialogButtonBox.buttonRole: DialogButtonBox.AcceptRole }
        }
        onAccepted: backend.restore(window.restoreIndex)
    }
    FileDialog { id: zipDialog; title: "Abrir ZIP"; currentFolder: preferences.importDirectory; nameFilters: ["Pacotes ZIP (*.zip)"]; onAccepted: backend.inspect(selectedFile) }
    FolderDialog { id: destinationDialog; title: "Diretório SLSsteam"; currentFolder: preferences.folderUrl(preferences.destination); onAccepted: preferences.chooseDestination(selectedFolder) }
    FolderDialog { id: downloadLibraryDialog; title: "Biblioteca Steam para downloads"; currentFolder: preferences.folderUrl(preferences.downloadLibrary || preferences.steamDirectory); onAccepted: preferences.chooseDownloadLibrary(selectedFolder) }
    Dialog {
        id: libraryDialog
        modal: true; anchors.centerIn: parent; width: Math.min(window.width - 48, 440)
        palette.window: window.surface; palette.windowText: window.ink
        title: "Biblioteca Steam"
        closePolicy: Popup.CloseOnEscape | Popup.CloseOnPressOutside
        contentItem: ColumnLayout {
            spacing: 6
            Repeater {
                model: preferences.libraries
                ItemDelegate {
                    required property var modelData
                    width: parent.width; height: 44
                    contentItem: ColumnLayout {
                        spacing: 2
                        Label { textFormat: Text.PlainText; text: modelData.label; color: window.ink; font.pixelSize: 13 }
                        Label {
                            visible: modelData.path !== ""; textFormat: Text.PlainText
                            text: modelData.path; color: window.muted; font.pixelSize: 11; elide: Text.ElideLeft
                        }
                    }
                    background: Rectangle {
                        radius: 4
                        color: modelData.path === preferences.downloadLibrary || (modelData.path === "" && preferences.downloadLibrary === "") ? window.raised : "transparent"
                    }
                    onClicked: {
                        preferences.chooseDownloadLibrary(modelData.path === "" ? "" : preferences.folderUrl(modelData.path))
                        libraryDialog.close()
                    }
                }
            }
            Action { text: "Outro diretório…"; flat: true; onClicked: { libraryDialog.close(); downloadLibraryDialog.open() } }
        }
    }
}
