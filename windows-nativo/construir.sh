#!/usr/bin/env bash
# Construye Clima para Windows: Clima.exe (2000→11), Clima9x.exe (95/98/Me) y el instalador NSIS, con sumas SHA-256.
# Corre en el contenedor «empaquetar» (MinGW-w64 + NSIS). Desarrollado por Javiera Navarrete.
set -euo pipefail
cd "$(dirname "$(readlink -f "$0")")"
distrobox enter empaquetar -- bash -c '
  set -euo pipefail
  make -s all
  mkdir -p instalador/ansi
  for f in ACUERDO.txt LEEME.txt; do iconv -f UTF-8 -t CP1252 instalador/$f | sed "s/\$/\r/" > instalador/ansi/$f; done
  { printf "\xEF\xBB\xBF"; cat instalador/ACUERDO.txt; } > instalador/ansi/ACUERDO-licencia.txt
  printf "\xEF\xBB\xBF" > instalador/ansi/clima.nsi; cat instalador/clima.nsi >> instalador/ansi/clima.nsi
  cp instalador/ansi/clima.nsi instalador/clima-bom.nsi
  (cd instalador && makensis -V2 clima-bom.nsi && rm -f clima-bom.nsi)
  (cd dist && sha256sum Clima.exe Clima9x.exe Clima-*-instalador.exe > SHA256SUMS.txt)
'
ls -la dist
