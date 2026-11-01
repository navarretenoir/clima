#!/usr/bin/env bash
# Empaquetado Linux de las apps GTK4/WebKitGTK de Javiera (Kotone 2026-10-31): RPM, DEB, AppImage, Flatpak y Snap.
# Cada app trae linux/app.conf (variables abajo) y llama:  empaquetar.sh <dir app> [rpm|deb|appimage|flatpak|snap|todo]
#   APP_ID  PAQUETE  NOMBRE  VERSION  RESUMEN  DESCRIPCION  BIN_GUI(nombre en /usr/bin)  [FLATPAK_ID]
#   compilar <prefijo> <dirsalida>   (función: compila con rutas bajo <prefijo> y deja binarios en <dirsalida>)
#   instalar <stage> <prefijo> <binarios>  (función: copia todo al árbol <stage><prefijo>/…)
#   DEPS_RPM  DEPS_DEB  (dependencias de ejecución)
# Rutas estándar: binarios en <prefijo>/bin, datos en <prefijo>/share/<paquete>, módulos en <prefijo>/libexec/<paquete>,
# lanzador en share/applications, ícono en share/icons/hicolor, ficha AppStream en share/metainfo.
set -euo pipefail
APP=$(readlink -f "$1"); QUE=${2:-todo}
KIT=$(dirname "$(readlink -f "$0")")
source "$APP/linux/app.conf"
OUT="$APP/dist/linux"; TMP="$APP/build/linux"
mkdir -p "$OUT" "$TMP"
DEBIAN_BOX=empaquetar; UBUNTU_BOX=empaquetar-ubuntu
ARCH=x86_64

en_caja() {   # en_caja <contenedor> <comando…>: corre en el distrobox con este mismo script y conf
  local caja=$1; shift
  distrobox enter "$caja" -- bash -c "set -euo pipefail; APP='$APP'; source '$APP/linux/app.conf'; $*"
}

arbol() {   # arbol <stage> <prefijo> <binarios>: instalación común + metadatos
  local st=$1 pre=$2 bins=$3
  instalar "$st" "$pre" "$bins"
  install -Dm644 "$APP/linux/$APP_ID.desktop" "$st$pre/share/applications/$APP_ID.desktop"
  install -Dm644 "$APP/linux/$APP_ID.metainfo.xml" "$st$pre/share/metainfo/$APP_ID.metainfo.xml"
  install -Dm644 "$APP/linux/$APP_ID.svg" "$st$pre/share/icons/hicolor/scalable/apps/$APP_ID.svg"
  for t in 48 128 256; do
    mkdir -p "$st$pre/share/icons/hicolor/${t}x$t/apps"
    rsvg-convert -w $t -h $t "$APP/linux/$APP_ID.svg" -o "$st$pre/share/icons/hicolor/${t}x$t/apps/$APP_ID.png"
  done
  install -Dm644 "$APP/linux/LEEME.txt" "$st$pre/share/doc/$PAQUETE/LEEME.txt"
}

# ------------------------------------------------------------------ RPM (Fedora/RHEL/openSUSE) — se compila en el anfitrión
hacer_rpm() {
  local R="$TMP/rpm" B="$TMP/rpm-bin" S="$TMP/rpm-stage"
  rm -rf "$R" "$B" "$S"; mkdir -p "$R"/{BUILD,RPMS,SOURCES,SPECS,SRPMS} "$B"
  compilar /usr "$B"
  arbol "$S" /usr "$B"
  local lista; lista=$(cd "$S" && find . -type f -o -type l | sed 's|^\.||' | sort)
  cat > "$R/SPECS/$PAQUETE.spec" <<SPEC
Name:           $PAQUETE
Version:        $VERSION
Release:        1%{?dist}
Summary:        $RESUMEN
License:        GPL-3.0-or-later
URL:            https://github.com/
Vendor:         Javiera Navarrete
Packager:       Javiera Navarrete
Requires:       $DEPS_RPM
AutoReqProv:    yes
%global debug_package %{nil}
%global __os_install_post %{nil}

%description
$DESCRIPCION

Desarrollado por Javiera Navarrete.

%install
cp -a "$S"/. %{buildroot}/

%files
$lista

%changelog
* Sat Oct 31 2026 Javiera Navarrete - $VERSION-1
- Versión empaquetada para Fedora.
SPEC
  rpmbuild --define "_topdir $R" --quiet -bb "$R/SPECS/$PAQUETE.spec"
  cp "$R"/RPMS/*/"$PAQUETE"-*.rpm "$OUT/"
  echo "✔ RPM: $(ls "$OUT"/"$PAQUETE"-*.rpm | tail -1)"
}

# ------------------------------------------------------------------ DEB (Debian 13 / Ubuntu 24.04+) — se compila en Debian trixie
hacer_deb() {
  local S="$TMP/deb-stage" B="$TMP/deb-bin"
  rm -rf "$S" "$B"; mkdir -p "$B"
  en_caja $DEBIAN_BOX "compilar /usr '$B'"
  arbol "$S" /usr "$B"
  mkdir -p "$S/DEBIAN"
  local tam; tam=$(du -sk "$S/usr" | cut -f1)
  cat > "$S/DEBIAN/control" <<CTL
Package: $PAQUETE
Version: $VERSION-1
Section: utils
Priority: optional
Architecture: amd64
Depends: $DEPS_DEB
Installed-Size: $tam
Maintainer: Javiera Navarrete <noreply@kotone.local>
Description: $RESUMEN
 $(echo "$DESCRIPCION" | fold -s -w 78 | sed 's/^/ /;s/^ $/ ./' | sed '1s/^ //')
 .
 Desarrollado por Javiera Navarrete.
CTL
  find "$S" -type d -exec chmod 755 {} +
  en_caja $DEBIAN_BOX "dpkg-deb --root-owner-group -Zxz --build '$S' '$OUT/${PAQUETE}_${VERSION}-1_amd64.deb' >/dev/null"
  echo "✔ DEB: $OUT/${PAQUETE}_${VERSION}-1_amd64.deb"
}

# ------------------------------------------------------------------ AppImage — compilado en Ubuntu 24.04 (glibc 2.39), usa GTK4/WebKitGTK del sistema
hacer_appimage() {
  local A="$TMP/AppDir" B="$TMP/appimage-bin"
  rm -rf "$A" "$B"; mkdir -p "$B"
  en_caja $UBUNTU_BOX "compilar /usr '$B'"
  arbol "$A" /usr "$B"
  cp "$APP/linux/$APP_ID.desktop" "$A/"; cp "$APP/linux/$APP_ID.svg" "$A/"; ln -sf "$APP_ID.svg" "$A/.DirIcon"
  sed -i "s|^Exec=.*|Exec=$BIN_GUI|" "$A/$APP_ID.desktop"
  cat > "$A/AppRun" <<RUN
#!/bin/sh
# $NOMBRE — desarrollado por Javiera Navarrete. Usa GTK 4 y WebKitGTK 6 del sistema (Fedora 39+, Ubuntu 24.04+, Debian 13+).
HERE=\$(dirname "\$(readlink -f "\$0")")
export PATH="\$HERE/usr/bin:\$PATH"
$(appimage_entorno)
exec "\$HERE/usr/bin/$BIN_GUI" "\$@"
RUN
  chmod +x "$A/AppRun"
  local RT="$KIT/runtime-x86_64"
  [ -s "$RT" ] || curl -sfL -m 120 -o "$RT" https://github.com/AppImage/type2-runtime/releases/download/continuous/runtime-x86_64
  rm -f "$TMP/app.squashfs"
  mksquashfs "$A" "$TMP/app.squashfs" -root-owned -noappend -no-xattrs -comp zstd -quiet >/dev/null
  cat "$RT" "$TMP/app.squashfs" > "$OUT/$NOMBRE_ARCHIVO-$VERSION-x86_64.AppImage"
  chmod +x "$OUT/$NOMBRE_ARCHIVO-$VERSION-x86_64.AppImage"
  echo "✔ AppImage: $OUT/$NOMBRE_ARCHIVO-$VERSION-x86_64.AppImage"
}

# ------------------------------------------------------------------ Flatpak — binario de Ubuntu 24.04 sobre org.gnome.Platform 49 (trae WebKitGTK 6)
hacer_flatpak() {
  local F="$TMP/flatpak" B="$TMP/flatpak-bin" REPO="$TMP/flatpak-repo"
  # Flathub exige un ID con dominio propio: FLATPAK_ID (io.github.navarretenoir.*) en app.conf; los demás formatos siguen con APP_ID
  local FID="${FLATPAK_ID:-$APP_ID}"
  rm -rf "$F" "$B" "$REPO"; mkdir -p "$B"
  en_caja $UBUNTU_BOX "compilar /app '$B'"
  flatpak build-init "$F" "$FID" org.gnome.Sdk org.gnome.Platform 49 2>/dev/null \
    || flatpak build-init "$F" "$FID" org.gnome.Platform org.gnome.Platform 49
  arbol "$F/files" "" "$B"
  if [ "$FID" != "$APP_ID" ]; then
    local S="$F/files/share" f
    for f in $(find "$S" -name "$APP_ID.*"); do mv "$f" "$(dirname "$f")/$FID.${f##*/$APP_ID.}"; done
    sed -i "s|^Icon=$APP_ID\$|Icon=$FID|" "$S/applications/$FID.desktop"
    grep -q '^StartupWMClass=' "$S/applications/$FID.desktop" || echo "StartupWMClass=$APP_ID" >> "$S/applications/$FID.desktop"
    sed -i "s|<id>$APP_ID</id>|<id>$FID</id>|; s|>$APP_ID.desktop<|>$FID.desktop<|" "$S/metainfo/$FID.metainfo.xml"
  fi
  # build-finish exige que el lanzador apunte a un comando existente en /app/bin
  flatpak build-finish "$F" --command="$BIN_GUI" --share=network --share=ipc --socket=wayland --socket=fallback-x11 \
    --device=dri $FLATPAK_EXTRA >/dev/null
  flatpak build-export "$REPO" "$F" stable
  flatpak build-bundle "$REPO" "$OUT/$NOMBRE_ARCHIVO-$VERSION.flatpak" "$FID" stable \
    --runtime-repo=https://dl.flathub.org/repo/flathub.flatpakrepo
  echo "✔ Flatpak: $OUT/$NOMBRE_ARCHIVO-$VERSION.flatpak"
}

# ------------------------------------------------------------------ Snap (core24 + gnome-46-2404) — sin snapcraft: se arma a mano
hacer_snap() {
  local P="$TMP/snap" B="$TMP/snap-bin"
  rm -rf "$P" "$B"; mkdir -p "$B" "$P/meta/gui" "$P/gnome-platform"
  en_caja $UBUNTU_BOX "compilar /snap/$PAQUETE/current/usr '$B'"
  arbol "$P" /usr "$B"
  cp "$APP/linux/$APP_ID.svg" "$P/meta/gui/icon.svg"
  sed "s|^Exec=.*|Exec=$PAQUETE|; s|^Icon=.*|Icon=\${SNAP}/meta/gui/icon.svg|" "$APP/linux/$APP_ID.desktop" > "$P/meta/gui/$PAQUETE.desktop"
  cat > "$P/lanzar" <<'L'
#!/bin/bash
# Entorno de la plataforma GNOME 46 (snap de contenido gnome-46-2404) para GTK 4 / WebKitGTK 6.
G="$SNAP/gnome-platform"; T=x86_64-linux-gnu
export LD_LIBRARY_PATH="$SNAP/usr/lib/$T:$G/usr/lib/$T:$G/usr/lib:${LD_LIBRARY_PATH:-}"
export XDG_DATA_DIRS="$SNAP/usr/share:$G/usr/share:/usr/share:${XDG_DATA_DIRS:-}"
export GDK_PIXBUF_MODULE_FILE="$G/usr/lib/$T/gdk-pixbuf-2.0/2.10.0/loaders.cache"
export GIO_MODULE_DIR="$G/usr/lib/$T/gio/modules"
export GSETTINGS_SCHEMA_DIR="$G/usr/share/glib-2.0/schemas"
export WEBKIT_EXEC_PATH="$G/usr/lib/$T/webkitgtk-6.0"
export WEBKIT_DISABLE_SANDBOX_THIS_IS_DANGEROUS=1   # el confinamiento del snap ya aísla el proceso
export XDG_CACHE_HOME="$SNAP_USER_COMMON/.cache" XDG_CONFIG_HOME="$SNAP_USER_DATA/.config"
L
  echo "$(appimage_entorno | sed 's/\$HERE/$SNAP/g')" >> "$P/lanzar"
  echo "exec \"\$SNAP/usr/bin/$BIN_GUI\" \"\$@\"" >> "$P/lanzar"
  chmod +x "$P/lanzar"
  local REPO_URL; REPO_URL=$(git -C "$APP" remote get-url origin 2>/dev/null | sed 's/\.git$//')
  cat > "$P/meta/snap.yaml" <<Y
name: $PAQUETE
version: '$VERSION'
summary: '$RESUMEN'
title: $NOMBRE
license: GPL-3.0-or-later
website: $REPO_URL
source-code: $REPO_URL
issues: $REPO_URL/issues
contact: $REPO_URL/issues
description: |
  $DESCRIPCION
  Desarrollado por Javiera Navarrete.
base: core24
grade: stable
confinement: strict
architectures: [amd64]
plugs:
  gnome-46-2404:
    interface: content
    target: \$SNAP/gnome-platform
    default-provider: gnome-46-2404
  gtk-3-themes:
    interface: content
    target: \$SNAP/data-dir/themes
    default-provider: gtk-common-themes
apps:
  $PAQUETE:
    command: lanzar
    plugs: [network, wayland, x11, desktop, desktop-legacy, opengl, gsettings$SNAP_EXTRA]
Y
  mkdir -p "$P/data-dir/themes"; chmod -R u+rwX,go+rX "$P"   # la Snap Store exige archivos legibles por todos
  rm -f "$OUT/${PAQUETE}_${VERSION}_amd64.snap"
  mksquashfs "$P" "$OUT/${PAQUETE}_${VERSION}_amd64.snap" -noappend -comp xz -all-root -no-xattrs -no-fragments -quiet >/dev/null
  echo "✔ Snap: $OUT/${PAQUETE}_${VERSION}_amd64.snap (instalar: sudo snap install --dangerous …)"
}

case "$QUE" in
  rpm) hacer_rpm ;; deb) hacer_deb ;; appimage) hacer_appimage ;; flatpak) hacer_flatpak ;; snap) hacer_snap ;;
  todo) hacer_rpm; hacer_deb; hacer_appimage; hacer_flatpak; hacer_snap
        (cd "$OUT" && sha256sum -- * | grep -v SHA256SUMS > SHA256SUMS); echo "✔ sumas: $OUT/SHA256SUMS" ;;
  *) echo "uso: $0 <dir app> [rpm|deb|appimage|flatpak|snap|todo]"; exit 2 ;;
esac
