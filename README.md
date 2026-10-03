# psyche-BR

App Linux (C++17, Qt 6/QML) que busca na Hubcap, importa pacotes Lua/ZIP e mescla no `config.yaml` do SLSsteam. Interface em pt-BR.

Fork do [ciscosweater/psyche](https://github.com/ciscosweater/psyche). Só a interface foi traduzida; o funcionamento é o do upstream.

Não instala jogos, plugins da Steam nem executa Lua. Lua é só lido. Edições no YAML preservam comentários e gravam backup antes.

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

CMake ≥ 3.21, C++17, Qt ≥ 6.5 (Quick, Quick Controls 2, Concurrent, Network), libarchive, yaml-cpp.

```sh
cmake -S . -B build
cmake --build build -j
./build/psyche
```

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

MIT. O ZIP leva Qt e outras bibliotecas de runtime (veja `licenses/THIRD_PARTY.md` no pacote). Pixelify Sans é SIL OFL (`qml/fonts/OFL.txt`).

Nota do autor original (tradução): usei IA ao escrever isso. O código pode estar errado de jeitos que parecem certos — bugs, casos de borda ruins, sobras. Leia antes de confiar, principalmente em torno de `config.yaml` e chaves de API. MIT, sem garantia.
