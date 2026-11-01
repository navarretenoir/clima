#!/usr/bin/env bash
# Construye una app WebView2 (webhost.c + config.h) para Windows: .exe 32 bits (corre en 7/8.1/10/11 de 32 y 64 bits),
# WebView2Loader.dll de Microsoft, instalador NSIS y sumas SHA-256. Uso: construir.sh <dir windows de la app>
# La app trae windows/app.conf con: NOMBRE EXE VERSION DESCRIPCION BIENVENIDA ARCHIVOS_EXTRA (líneas NSIS) y
# opcionalmente la función copiar_extra <dist>. Desarrollado por Javiera Navarrete.
set -euo pipefail
W=$(readlink -f "$1"); K=$(dirname "$(readlink -f "$0")"); WV=$K/../webview2/sdk/build/native
source "$W/app.conf"
VERCOMAS=$(echo "$VERSION.0.0.0" | cut -d. -f1-4 | tr . ,); VERPUNTOS=$(echo "$VERSION.0.0.0" | cut -d. -f1-4)
mkdir -p "$W/build" "$W/dist" "$W/instalador"
sust() { sed -e "s|@NOMBRE@|$NOMBRE|g; s|@EXE@|$EXE|g; s|@VERSION@|$VERSION|g; s|@VERCOMAS@|$VERCOMAS|g; s|@VERPUNTOS@|$VERPUNTOS|g; s|@DESCRIPCION@|$DESCRIPCION|g" "$1"; }
sust "$K/plantilla.rc" > "$W/build/app.rc"; sust "$K/plantilla.manifest" > "$W/build/app.manifest"
cp "$W/src/app.ico" "$W/build/"
{ printf '\xEF\xBB\xBF'; sust "$K/plantilla.nsi" | python3 -c "
import sys; s=sys.stdin.read()
s=s.replace('@BIENVENIDA@', open('$W/instalador/bienvenida.txt',encoding='utf-8').read().strip().replace('\n','\$\\\\r\$\\\\n'))
s=s.replace('@ARCHIVOS_EXTRA@', r'''$ARCHIVOS_EXTRA''')
sys.stdout.write(s)"; } > "$W/instalador/app.nsi"
cp "$WV/x86/WebView2Loader.dll" "$W/dist/"
type copiar_extra >/dev/null 2>&1 && copiar_extra "$W/dist"
distrobox enter empaquetar -- bash -c "
  set -euo pipefail; cd '$W'
  i686-w64-mingw32-windres -Ibuild build/app.rc -O coff -o build/app.o
  i686-w64-mingw32-gcc -Os -std=gnu99 -municode -mwindows -DUNICODE -D_UNICODE -DWINVER=0x0601 -D_WIN32_WINNT=0x0601 \
    -I'$WV/include' -I'$K/include' -Isrc -Wall -Wno-unused-function -Wno-cast-function-type -Wno-incompatible-pointer-types \
    -o 'dist/$EXE.exe' src/webhost.c build/app.o -s -static-libgcc -Wl,--nxcompat -Wl,--dynamicbase \
    -lole32 -loleaut32 -lshell32 -lshlwapi -lwinhttp -luuid -luser32 -lgdi32 -lkernel32
  cd instalador && makensis -V2 app.nsi
  cd ../dist && sha256sum '$EXE.exe' WebView2Loader.dll '$EXE-$VERSION-instalador.exe' > SHA256SUMS.txt
"
ls -la "$W/dist"
