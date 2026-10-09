# Changelog

## 1.3.0 — 2026-10-09

- Aba Downloads com dois cards: jogo (depots com checkbox, sistema filtrado pelos `oslist`, biblioteca Steam detectada) e Workshop.
- Workshop: cole URLs ou IDs, fila com estado por item, cancelar. Grava o `appworkshop_<appid>.acf` mesclando o item nos que já estavam.
- DepotDownloader patched (fork GPL-2.0 do niwia, vendorizado) no lugar do upstream: é o que aceita `-manifestfile`/`-depotkeys` e baixa conteúdo de jogo comprado.
- Busca reordenada por relevância, com rebaixamento de soundtrack/DLC/demo e selo do ProtonDB.
- Aviso de falta de conexão, botões Apagar e Colar na busca, Apagar na chave Hubcap.

## psyche-BR — interface em pt-BR (fork de ciscosweater/psyche)

- Todos os textos visíveis (QML, status, erros, CLI, instalador do pacote) traduzidos; identificadores, chaves e protocolos intactos.

## 1.2.0 — 2026-09-05

- Stop Import from flickering on resize after Open ZIP.
- Warn when Steam is open during Remove: psyche only edits config.yaml, so restart Steam to refresh the library.
- Say when a Hubcap key is expired, and show quota/expiry after a 401/403/429.

## 1.1.0 — 2026-09-04

- Stop writing depot-only IDs into AdditionalApps. Keyed IDs are added there only when steamcmd app info says they are apps.

## 1.0 — 2026-09-04

- Confirm Hubcap downloads before they use quota, and explain when a key is missing.
- Tabs are now Games, Import, Library, History, Settings, with a next step into Library after apply.
- Drop a ZIP, pick a destination, close while busy, and compact widths behave more clearly.
- Settings shows the version; dialogs and lists match the dark chrome.

## 0.3 — 2026-09-04

First public release.

- Search Hubcap and import packages by AppID (full Lua, base game, DLC, or ZIP) or a local ZIP.
- Merge recognized apps, depots, and keys into SLSsteam `config.yaml` with backups and History restore.
- Library view can remove entries psyche added (or recovered from labelled comments).
- Headless CLI (`--search`, `--appid`, `--apply`, `--restore`, `--health`, `--stats`).
- Bundled Linux x86_64 ZIP with Qt runtime, built on Debian 13.
