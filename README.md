# psyche-BR

App Linux (C++17, Qt 6/QML) que busca na Hubcap, importa pacotes Lua/ZIP, mescla no `config.yaml` do SLSsteam e baixa jogos e mods do Workshop. Interface em pt-BR.

Fork do [ciscosweater/psyche](https://github.com/ciscosweater/psyche). Interface traduzida; a busca foi reordenada por relevância e os downloads são novidade do fork.

Não instala plugins da Steam nem executa Lua. Lua é só lido. Edições no YAML preservam comentários e gravam backup antes.

Renderizador padrão é software (`QT_QUICK_BACKEND`).

## Instalar

Pegue um `*-setup.zip` nos [Releases](https://github.com/LucianoSkx/psyche-BR/releases), extraia e rode:

```sh
./install.sh
~/.local/share/psyche/bin/psyche
```

Ou rode `./psyche` do diretório extraído. Mantenha o runtime Qt ao lado do binário.

Linux x86_64, glibc 2.41+ (Debian 13 / similar).

## Compilar

CMake ≥ 3.21, C++17, Qt ≥ 6.5 (Quick, Quick Controls 2, Concurrent, Network), libarchive, yaml-cpp. Para os downloads, runtime .NET 9.

```sh
cmake -S . -B build
cmake --build build -j
./build/psyche
```

## Downloads

A aba Downloads tem dois cards lado a lado.

**Jogo.** Depots com checkbox, sistema (Linux/Windows/macOS, filtrado pelos `oslist` reais do jogo) e biblioteca Steam detectada. O pacote vem pronto da aba Importar. Os manifestos são buscados na Hubcap e ficam em cache; o destino é `steamapps/common/<nome>` na biblioteca escolhida.

**Workshop.** Cole URLs ou IDs de mods (um por linha, espaço ou vírgula; duplicatas saem), veja a fila e baixe todos. Os mods vão para `steamapps/workshop/content/<appid>/<id>`, e o app grava o `appworkshop_<appid>.acf` da biblioteca mesclando o item novo nos que já estavam, para a Steam listar como instalado.

O AppID de cada item vem da Hubcap; quando ela não tem o item no catálogo, sai da página pública da Steam.

Requisitos: runtime **.NET 9** (procure em `~/.dotnet/dotnet`, `/usr/share/dotnet/dotnet` ou no `PATH`) e o binário do DepotDownloader, que já vem no pacote instalado.

Ambos os fluxos usam um fork patched do DepotDownloader (`niwia/DepotDownloaderModpatched`, GPL-2.0), que aceita `-manifestfile` e `-depotkeys`. Sem essas flags o DepotDownloader oficial só alcança conteúdo que a conta anônima da Steam já acessa, e devolve `not available from this account` para jogo comprado. Para builds com o upstream, use `PSYCHE_DEPOTDOWNLOADER=official` no `packaging/bundle.py`.

Cancelar interrompe o processo em andamento. O ACF só é gravado quando existe biblioteca Steam escolhida — sem ela a Steam não lê o arquivo.

## CLI

Qualquer argumento abre a CLI (sem tela). Nada é gravado sem `--apply`.

```sh
./build/psyche --search "Nome do jogo"
./build/psyche --appid 620
./build/psyche --zip pacote.zip --apply --destination /caminho/para/SLSsteam
./build/psyche --help
```

Busca e downloads por AppID usam cota da Hubcap. ZIPs locais, não.

## Hubcap

Defina a chave em Configurações ou exporte `PSYCHE_HUBCAP_API_KEY`.

Se nenhum estiver definido, o psyche lê `morrenus_api_key` do ASSella em `$XDG_CONFIG_HOME/Tachibana Labs/ACCELA.conf` (só leitura).

A chave salva vai sem criptografia para `~/.local/share/psyche/settings.json` (arquivo `0600`, diretório `0700`). Marque **Lembrar neste computador** só se quiser isso.

## Licença

MIT. O ZIP leva Qt e outras bibliotecas de runtime, e o fork patched do DepotDownloader (GPL-2.0, fonte em [niwia/DepotDownloaderModpatched](https://github.com/niwia/DepotDownloaderModpatched)), que roda como processo separado e não é linkado no psyche — veja `licenses/THIRD_PARTY.md` no pacote. Pixelify Sans é SIL OFL (`qml/fonts/OFL.txt`).

Nota do autor original (tradução): usei IA ao escrever isso. O código pode estar errado de jeitos que parecem certos — bugs, casos de borda ruins, sobras. Leia antes de confiar, principalmente em torno de `config.yaml` e chaves de API. MIT, sem garantia.
