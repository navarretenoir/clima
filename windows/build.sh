#!/usr/bin/env bash
# build.sh — arma el port a Windows x64 de Clima (Electron, reutiliza ui/ tal cual).
# El backend (src/data.js + src/main.js + src/preload.js) ya está verificado 1:1 contra
# c/clima_data.c + clima_ambiental_c.inc + los scrapers Mineduc/airerm; este script solo
# refresca app/ui + app/data desde el proyecto y repackagea con @electron/packager.
set -euo pipefail
RAIZ="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
PROYECTO="$(dirname "$RAIZ")"

mkdir -p "$RAIZ/app/ui" "$RAIZ/app/data"
cp "$PROYECTO"/ui/*.html "$PROYECTO"/ui/*.css "$PROYECTO"/ui/*.js "$RAIZ/app/ui/"
cp "$PROYECTO"/data/comunas_cl.json "$PROYECTO"/data/cities_cl.json "$RAIZ/app/data/"

cd "$RAIZ"
rm -rf dist
npx -y @electron/packager . Clima --platform=win32 --arch=x64 \
  --electron-version=44.5.1 --out=dist --overwrite --icon=app/clima.ico

echo "Listo: $RAIZ/dist/Clima-win32-x64/Clima.exe"
echo "Para distribuir: cd dist && zip -rq ~/Descargas/Clima-Windows-x64.zip Clima-win32-x64"
