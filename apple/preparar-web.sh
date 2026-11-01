#!/usr/bin/env bash
# Arma Recursos/web para macOS/iOS: la interfaz compartida ../ui + capa Android (datos en JS) + clima-apple.js (puente
# nativo), igual que el proyecto Android. Se corre antes de abrir el proyecto en Xcode. Desarrollado por Javiera Navarrete.
set -euo pipefail
A=$(cd "$(dirname "$0")" && pwd); W="$A/Recursos/web"
rm -rf "$W"; mkdir -p "$W/data" "$W/fonts"
cp "$A/../ui/"*.js "$A/../ui/"*.css "$W/"
cp "$A/../android/web/clima-android.js" "$A/../android/web/android.css" "$A/web/clima-apple.js" "$W/"
cp -r "$A/../android/web/fonts/." "$W/fonts/" 2>/dev/null || true
cp "$A/../data/comunas_cl.json" "$W/data/"
sed -e 's|<link rel="stylesheet" href="style.css">|<link rel="stylesheet" href="style.css">\n<link rel="stylesheet" href="android.css">|' \
    -e 's|<script src="app.js"></script>|<script src="clima-apple.js"></script>\n  <script src="clima-android.js"></script>\n  <script src="app.js"></script>|' \
    "$A/../ui/index.html" > "$W/index.html"
grep -q clima-apple.js "$W/index.html" && echo "✔ Recursos/web listo"
