#!/usr/bin/env bash
# Genera los archivos de proyecto Apple (XcodeGen + Assets + entitlements + CI de GitHub) de una app web de Javiera.
# Uso: apple-plantilla.sh <dir apple> <Nombre> <bundle id> <carpeta fuente> <versión> <usa ubicación 1/0> <descripción>
set -euo pipefail
A=$1; N=$2; BID=$3; SRC=$4; V=$5; UBI=$6; DESC=$7
mkdir -p "$A/Recursos/Assets.xcassets/AppIcon.appiconset" "$A/.github/workflows"
cat > "$A/Recursos/Assets.xcassets/Contents.json" <<J
{ "info" : { "author" : "xcode", "version" : 1 } }
J
cp "$A/Recursos/icono-1024.png" "$A/Recursos/Assets.xcassets/AppIcon.appiconset/icono-1024.png"
cat > "$A/Recursos/Assets.xcassets/AppIcon.appiconset/Contents.json" <<J
{ "images" : [
  { "filename" : "icono-1024.png", "idiom" : "universal", "platform" : "ios", "size" : "1024x1024" },
  { "filename" : "icono-1024.png", "idiom" : "mac", "scale" : "1x", "size" : "512x512" },
  { "filename" : "icono-1024.png", "idiom" : "mac", "scale" : "2x", "size" : "512x512" }
], "info" : { "author" : "xcode", "version" : 1 } }
J
UBI_PLIST=""; UBI_ENT=""
if [ "$UBI" = 1 ]; then
  UBI_PLIST="        NSLocationWhenInUseUsageDescription: \"$N usa tu ubicación aproximada para mostrar los datos de tu comuna o estación más cercana. No se guarda ni se comparte.\""
  UBI_ENT="        com.apple.security.personal-information.location: true"
fi
cat > "$A/project.yml" <<Y
# $N para macOS (Intel + Apple Silicon, binario universal) e iOS/iPadOS — proyecto XcodeGen.
# Generar: brew install xcodegen && ./preparar-web.sh && xcodegen generate && open $N.xcodeproj
# Desarrollado por Javiera Navarrete.
name: $N
options:
  bundleIdPrefix: cl.javieranavarrete
  deploymentTarget: { iOS: "15.0", macOS: "11.0" }
  createIntermediateGroups: true
settings:
  base:
    MARKETING_VERSION: "$V"
    CURRENT_PROJECT_VERSION: "1"
    SWIFT_VERSION: "5.9"
    DEVELOPMENT_TEAM: ""
    CODE_SIGN_STYLE: Automatic
    INFOPLIST_KEY_NSHumanReadableCopyright: "© 2026 Javiera Navarrete. Desarrollado por Javiera Navarrete."
targets:
  $N-macOS:
    type: application
    platform: macOS
    sources: [Comun, $SRC, { path: Recursos/web, type: folder, buildPhase: resources }, Recursos/Assets.xcassets]
    settings:
      base:
        PRODUCT_NAME: $N
        PRODUCT_BUNDLE_IDENTIFIER: $BID
        ARCHS: "\$(ARCHS_STANDARD)"          # arm64 + x86_64: corre nativo en Apple Silicon e Intel
        ONLY_ACTIVE_ARCH: NO
        ENABLE_HARDENED_RUNTIME: YES
        CODE_SIGN_ENTITLEMENTS: Recursos/$N.entitlements
        INFOPLIST_KEY_LSApplicationCategoryType: public.app-category.utilities
    info:
      path: Recursos/Info-macOS.plist
      properties:
        CFBundleDisplayName: $N
        CFBundleShortVersionString: "\$(MARKETING_VERSION)"
        NSHumanReadableCopyright: "© 2026 Javiera Navarrete"
$UBI_PLIST
  $N-iOS:
    type: application
    platform: iOS
    sources: [Comun, $SRC, { path: Recursos/web, type: folder, buildPhase: resources }, Recursos/Assets.xcassets]
    settings:
      base:
        PRODUCT_NAME: $N
        PRODUCT_BUNDLE_IDENTIFIER: $BID
        TARGETED_DEVICE_FAMILY: "1,2"
    info:
      path: Recursos/Info-iOS.plist
      properties:
        CFBundleDisplayName: $N
        CFBundleShortVersionString: "\$(MARKETING_VERSION)"
        UILaunchScreen: {}
        UISupportedInterfaceOrientations: [UIInterfaceOrientationPortrait, UIInterfaceOrientationLandscapeLeft, UIInterfaceOrientationLandscapeRight]
        ITSAppUsesNonExemptEncryption: false
$UBI_PLIST
Y
cat > "$A/Recursos/$N.entitlements" <<E
<?xml version="1.0" encoding="UTF-8"?>
<!DOCTYPE plist PUBLIC "-//Apple//DTD PLIST 1.0//EN" "http://www.apple.com/DTDs/PropertyList-1.0.dtd">
<plist version="1.0"><dict>
  <key>com.apple.security.app-sandbox</key><true/>
  <key>com.apple.security.network.client</key><true/>
$( [ "$UBI" = 1 ] && echo "  <key>com.apple.security.personal-information.location</key><true/>" )
</dict></plist>
E
cat > "$A/.github/workflows/apple.yml" <<G
# Compila $N para macOS (universal) e iOS en un Mac de GitHub (sin firma: para probar; firmar requiere cuenta de Apple).
name: $N Apple
on: [workflow_dispatch, push]
jobs:
  compilar:
    runs-on: macos-14
    steps:
      - uses: actions/checkout@v4
      - run: brew install xcodegen
      - run: ./preparar-web.sh && xcodegen generate
      - run: xcodebuild -project $N.xcodeproj -scheme $N-macOS -configuration Release -destination 'generic/platform=macOS' CODE_SIGNING_ALLOWED=NO build SYMROOT=\$PWD/build
      - run: xcodebuild -project $N.xcodeproj -scheme $N-iOS -configuration Release -destination 'generic/platform=iOS' CODE_SIGNING_ALLOWED=NO build SYMROOT=\$PWD/build
      - run: lipo -info "build/Release/$N.app/Contents/MacOS/$N"
      - uses: actions/upload-artifact@v4
        with: { name: $N-apple, path: "build/Release*/*.app" }
G
cat > "$A/LEEME.txt" <<L
$N para macOS e iOS — desarrollado por Javiera Navarrete.
$DESC

macOS 11+ (binario universal: Apple Silicon e Intel) e iOS/iPadOS 15+. Misma interfaz que Android y Linux, dentro de
WKWebView (Comun/AnfitrionWeb.swift); configuración en $SRC/Config.swift.

Compilar en un Mac:  brew install xcodegen && ./preparar-web.sh && xcodegen generate && open $N.xcodeproj
  · macOS: esquema $N-macOS → Product › Archive (firma «Sign to Run Locally» o tu ID de Apple).
  · iOS: esquema $N-iOS → elige tu iPhone; con un ID de Apple gratis la app dura 7 días (después se reinstala);
    con la cuenta de desarrollador (USD 99/año) dura 1 año y se puede publicar en TestFlight/App Store.
Sin Mac: el flujo .github/workflows/apple.yml compila ambas versiones (sin firmar) en un Mac de GitHub.
L
echo "✔ proyecto Apple de $N en $A"
