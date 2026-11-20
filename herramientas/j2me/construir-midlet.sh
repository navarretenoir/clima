#!/usr/bin/env bash
# Construye un MIDlet (Java ME · MIDP 2.0 / CLDC 1.1) para Symbian: ecj (-source 1.3) → ProGuard (-microedition:
# reduce + preverifica) → .jar + .jad. Kotone 2026-10-31.
# Uso: construir-midlet.sh <dir proyecto> <Nombre> <clase MIDlet> <versión> <permisos (doc.; sin firma no se declaran)> [sourcepath extra]
set -euo pipefail
P=$1; NOMBRE=$2; CLASE=$3; VER=$4; PERM=$5; EXTRA=${6:-}
J=$(dirname "$(readlink -f "$0")")
JAVA=${JAVA:-/var/lib/flatpak/app/com.google.AndroidStudio/current/active/files/extra/jbr/bin/java}
LIB="$J/cldcapi11-2.0.4.jar:$J/midpapi20-2.0.4.jar"
ARCH=$(echo "$NOMBRE" | tr -d " ")
cd "$P"; rm -rf build; mkdir -p build/clases build/stubs dist
STUBCP=""
if [ -d stubs ]; then
  "$JAVA" -jar "$J/ecj-4.6.1.jar" -source 1.3 -target 1.1 -nowarn -bootclasspath "$LIB" -d build/stubs $(find stubs -name '*.java')
  STUBCP=":build/stubs"
fi
SP=src; [ -n "$EXTRA" ] && SP="src:$EXTRA"
"$JAVA" -jar "$J/ecj-4.6.1.jar" -source 1.3 -target 1.1 -nowarn -g:none -bootclasspath "$LIB" -cp "build/stubs" \
  -sourcepath "$SP" -d build/clases $(find src -name '*.java')
cat > build/MANIFEST.MF <<M
Manifest-Version: 1.0
MIDlet-Name: $NOMBRE
MIDlet-Vendor: Javiera Navarrete
MIDlet-Version: $VER
MIDlet-Description: $NOMBRE - desarrollado por Javiera Navarrete
MIDlet-Icon: /icono.png
MIDlet-1: $NOMBRE, /icono.png, $CLASE
MicroEdition-Profile: MIDP-2.0
MicroEdition-Configuration: CLDC-1.1
Nokia-MIDlet-Category: Application
Nokia-MIDlet-On-Screen-Keypad: no
M
(cd build/clases && jar_dir=$(pwd); true)
"$JAVA" -jar "$J/proguard-7.6.1/lib/proguard.jar" \
  -injars build/clases -injars res -outjars build/sinmanifiesto.jar -libraryjars "$LIB$STUBCP" \
  -microedition -dontwarn -keep "public class $CLASE" -optimizationpasses 3 -overloadaggressively \
  -repackageclasses '' -allowaccessmodification -dontnote >/dev/null
# jar final con el manifiesto MIDlet primero (Symbian lo exige al inicio)
rm -rf build/jar && mkdir -p build/jar/META-INF && (cd build/jar && unzip -q ../sinmanifiesto.jar && rm -f META-INF/MANIFEST.MF)
cp build/MANIFEST.MF build/jar/META-INF/MANIFEST.MF
(cd build/jar && rm -f "../../dist/$ARCH.jar" && zip -q -X -9 "../../dist/$ARCH.jar" META-INF/MANIFEST.MF && zip -q -X -9 -r "../../dist/$ARCH.jar" . -x META-INF/MANIFEST.MF)
TAM=$(stat -c %s "dist/$ARCH.jar")
{ cat build/MANIFEST.MF | grep -v '^Manifest-Version'; echo "MIDlet-Jar-URL: $ARCH.jar"; echo "MIDlet-Jar-Size: $TAM"; } > "dist/$ARCH.jad"
echo "✔ dist/$ARCH.jar ($TAM bytes) + dist/$ARCH.jad"
