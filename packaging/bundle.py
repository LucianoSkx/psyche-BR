"""Build a relocatable Linux bundle inside the release container."""

from pathlib import Path
import hashlib
import json
import os
import re
import shutil
import subprocess
import sys
import urllib.request
import zipfile

root = Path(sys.argv[1])
def _first_existing(*candidates):
    for candidate in candidates:
        if Path(candidate).exists():
            return candidate
    return candidates[0]

qt = Path(os.environ.get(
    "PSYCHE_QT_DIR",
    _first_existing("/usr/lib/qt6", "/usr/lib/x86_64-linux-gnu/qt6")))
# No Debian os binários ficam em /usr/lib/qt6 e os módulos/plugins no multiarch;
# no Arch tudo mora sob /usr/lib/qt6. Resolve cada um por conta própria.
def _require_dir(label, *candidates):
    for candidate in candidates:
        if Path(candidate).is_dir():
            return Path(candidate)
    raise SystemExit(f"{label} não encontrado (tentado: {', '.join(candidates)})")

qt_qml = Path(os.environ.get(
    "PSYCHE_QML_DIR",
    _require_dir("Diretório de módulos QML", qt / "qml",
                 "/usr/lib/x86_64-linux-gnu/qt6/qml", "/usr/lib/qt6/qml")))
qt_plugins = Path(os.environ.get(
    "PSYCHE_QT_PLUGINS_DIR",
    _require_dir("Diretório de plugins Qt", qt / "plugins",
                 "/usr/lib/x86_64-linux-gnu/qt6/plugins", "/usr/lib/qt6/plugins")))
scanner = os.environ.get(
    "PSYCHE_QMLIMPORTSCANNER",
    _first_existing("/usr/lib/qt6/qmlimportscanner", "/usr/lib/qt6/libexec/qmlimportscanner"))
cmake = Path("CMakeLists.txt").read_text()
match = re.search(r"project\(Psyche VERSION ([0-9.]+)", cmake)
if not match:
    raise SystemExit("Could not read VERSION from CMakeLists.txt")
version = match.group(1)


def copy_files(source: Path, destination: Path, names: list[str]) -> None:
    destination.mkdir(parents=True, exist_ok=True)
    for name in names:
        item = source / name
        if item.is_file():
            shutil.copy2(item, destination / name)


(root / "lib").mkdir(exist_ok=True)

# DepotDownloader é GPL-2.0. O fork patched (niwia/DepotDownloaderModpatched,
# fonte pública) aceita -manifestfile/-depotkeys, que é o que permite baixar
# conteúdo de jogo que a conta anônima não possui. O oficial não aceita essas
# flags, então fica só como reserva para quando o fork não estiver vendorizado.
DD_VERSION = os.environ.get("PSYCHE_DEPOTDOWNLOADER_VERSION", "3.4.0")
DD_URL = (
    "https://github.com/SteamRE/DepotDownloader/releases/download/"
    f"DepotDownloader_{DD_VERSION}/DepotDownloader-framework.zip"
)
dd_dir = root / "depotdownloader"
dd_dir.mkdir(exist_ok=True)
dd_mod = Path("packaging/depotdownloader-mod")
use_mod = os.environ.get("PSYCHE_DEPOTDOWNLOADER", "mod") != "official"
DD_ORIGIN = "patched fork by niwia"
if use_mod and dd_mod.is_dir() and (dd_mod / "DepotDownloader.dll").is_file():
    shutil.copytree(dd_mod, dd_dir, dirs_exist_ok=True)
    (dd_dir / "variant.txt").write_text("mod\n")
else:
    dd_zip = root / "depotdownloader.zip"
    if not dd_zip.exists():
        try:
            with urllib.request.urlopen(DD_URL, timeout=120) as response:
                dd_zip.write_bytes(response.read())
        except Exception as error:  # noqa: BLE001
            raise SystemExit(f"Could not download DepotDownloader: {error}")
    with zipfile.ZipFile(dd_zip) as archive:
        for member in archive.namelist():
            name = Path(member).name
            if member.endswith("/") or not name:
                continue
            if not name.endswith((".dll", ".json")):
                continue
            target = dd_dir / name
            if not target.exists():
                with archive.open(member) as source, target.open("wb") as destination:
                    shutil.copyfileobj(source, destination)
    dd_zip.unlink()
    (dd_dir / "variant.txt").write_text("official\n")
(dd_dir / "DepotDownloader.runtimeconfig.json").write_text(
    """{
  "runtimeOptions": {
    "tfm": "net9.0",
    "rollForward": "LatestMajor",
    "framework": { "name": "Microsoft.NETCore.App", "version": "9.0.0" }
  }
}
"""
)

imports = json.loads(
    subprocess.check_output(
        [
            scanner,
            "-rootPath",
            "qml",
            "-importPath",
            str(qt_qml),
        ]
    )
)
for module in imports:
    if not module.get("path") or not module.get("relativePath"):
        continue
    source = Path(module["path"])
    target = root / "qml" / module["relativePath"]
    target.mkdir(parents=True, exist_ok=True)
    for item in source.iterdir():
        if item.is_file() and item.suffix != ".qmltypes":
            shutil.copy2(item, target / item.name)

for style in ("Fusion", "Material", "Imagine", "Universal"):
    shutil.rmtree(root / "qml" / "QtQuick" / "Controls" / style, ignore_errors=True)
for types in (root / "qml").rglob("*.qmltypes"):
    types.unlink()

copy_files(qt_plugins / "platforms", root / "plugins" / "platforms", [
    "libqxcb.so",
    "libqwayland.so",
    "libqwayland-generic.so",
])
copy_files(qt_plugins / "imageformats", root / "plugins" / "imageformats", [
    "libqjpeg.so",
    "libqsvg.so",
])
copy_files(qt_plugins / "tls", root / "plugins" / "tls", ["libqopensslbackend.so"])
copy_files(qt_plugins / "iconengines", root / "plugins" / "iconengines", ["libqsvgicon.so"])
copy_files(
    qt_plugins / "wayland-shell-integration",
    root / "plugins" / "wayland-shell-integration",
    ["libxdg-shell.so"],
)
copy_files(
    qt_plugins / "wayland-decoration-client",
    root / "plugins" / "wayland-decoration-client",
    ["libbradient.so"],
)
copy_files(
    qt_plugins / "wayland-graphics-integration-client",
    root / "plugins" / "wayland-graphics-integration-client",
    ["libshm-emulation-server.so"],
)

# Host C runtime and GL dispatcher stay on the system.
excluded = re.compile(
    r"^(ld-linux.*|lib(c|m|dl|pthread|rt|resolv|util|nss_.*)\.so.*"
    r"|lib(GL|EGL|GLX|OpenGL|GLdispatch)\.so.*)$"
)
queue = [p for p in root.rglob("*") if p.is_file() and p.read_bytes()[:4] == b"\x7fELF"]
seen = set()
while queue:
    binary = queue.pop()
    if binary in seen:
        continue
    seen.add(binary)
    output = subprocess.check_output(["ldd", str(binary)], text=True)
    if "not found" in output:
        raise RuntimeError(output)
    for name, path in re.findall(r"^\s*(\S+) => (/\S+)", output, re.M):
        if excluded.match(name):
            continue
        target = root / "lib" / name
        if not target.exists():
            shutil.copy2(path, target)
            queue.append(target)

for binary in list(seen) + list((root / "lib").glob("*")):
    if binary.is_file():
        subprocess.run(["strip", "--strip-unneeded", str(binary)], check=False, capture_output=True)

(root / "fontconfig").mkdir(exist_ok=True)
shutil.copy2("packaging/fonts.conf", root / "fontconfig/fonts.conf")
(root / "bin/qt.conf").write_text(
    "[Paths]\nPrefix=..\nLibraries=lib\nPlugins=plugins\nQmlImports=qml\n"
)
(root / "psyche").write_text(
    """#!/bin/sh
set -eu
bundle_dir=$(CDPATH= cd -- "$(dirname -- "$0")" && pwd)
export LD_LIBRARY_PATH="$bundle_dir/lib${LD_LIBRARY_PATH:+:$LD_LIBRARY_PATH}"
export FONTCONFIG_PATH="$bundle_dir/fontconfig"
export FONTCONFIG_FILE="$bundle_dir/fontconfig/fonts.conf"
export PSYCHE_FONTCONFIG_FILE="$FONTCONFIG_FILE"
export QT_PLUGIN_PATH="$bundle_dir/plugins"
export QML_IMPORT_PATH="$bundle_dir/qml"
export QML2_IMPORT_PATH="$bundle_dir/qml"
exec "$bundle_dir/bin/psyche" "$@"
"""
)
(root / "psyche").chmod(0o755)
shutil.copy2("packaging/install.sh", root / "install.sh")
(root / "install.sh").chmod(0o755)
shutil.copy2("qml/icons/psyche.svg", root / "psyche.svg")
if Path("LICENSE").is_file():
    shutil.copy2("LICENSE", root / "LICENSE")

(root / "licenses").mkdir(exist_ok=True)
shutil.copy2("qml/fonts/OFL.txt", root / "licenses/PixelifySans-OFL.txt")
(root / "licenses" / "THIRD_PARTY.md").write_text(
    f"""This bundle ships Qt 6, libarchive, yaml-cpp, OpenSSL, ICU, and related
shared libraries from the build system. Shared objects in `lib/` can be replaced
with compatible builds. Psyche itself is MIT (`LICENSE`). Pixelify Sans is
SIL OFL (`PixelifySans-OFL.txt`).

`depotdownloader/` holds DepotDownloader {DD_VERSION} ({DD_ORIGIN}), shipped
unmodified. DepotDownloader is GPL-2.0 and runs as a separate process for the
game and Workshop downloads; it is not linked into Psyche, which stays MIT.

The binary bundled here is the patched fork from
https://github.com/niwia/DepotDownloaderModpatched (GPL-2.0), which adds
-manifestfile and -depotkeys. Without those flags DepotDownloader can only fetch
content an anonymous Steam account already has access to. Set
PSYCHE_DEPOTDOWNLOADER=official to fall back to the upstream release.
"""
)
urllib.request.urlretrieve(
    "https://raw.githubusercontent.com/niwia/DepotDownloaderModpatched/main/LICENSE",
    root / "licenses" / "DepotDownloader-GPL-2.0.txt",
)

(root / "INSTALL.md").write_text(
    f"""# psyche {version}

```sh
./psyche
./install.sh   # then ~/.local/share/psyche/bin/psyche
```

Needs this whole directory, not just `bin/psyche`. Linux x86_64, glibc 2.41+.
Software renderer by default; set `QT_QUICK_BACKEND` to override.
"""
)

archive_path = root.parent / (root.name + "-setup.zip")
with zipfile.ZipFile(archive_path, "w", zipfile.ZIP_DEFLATED) as archive:
    for file in sorted(root.rglob("*")):
        if file.is_file():
            archive.write(file, file.relative_to(root))
with zipfile.ZipFile(archive_path) as archive:
    assert archive.testzip() is None
archive_path.with_suffix(".zip.sha256").write_text(
    hashlib.sha256(archive_path.read_bytes()).hexdigest() + "  " + archive_path.name + "\n"
)
