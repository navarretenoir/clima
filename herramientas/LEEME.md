# Kits de compilación

Scripts propios (desarrollados por Javiera Navarrete). Las piezas de terceros no van en el repositorio; se bajan aparte:

| Pieza | Dónde va | De dónde |
|---|---|---|
| SDK de WebView2 (Microsoft.Web.WebView2 1.0.2903.40) | `herramientas/webview2/sdk/` | NuGet: `microsoft.web.webview2` (descomprimir el .nupkg) |
| Runtime AppImage type2 | `herramientas/linux-pkg/runtime-x86_64` | se baja solo desde github.com/AppImage/type2-runtime |
| CLDC 1.1 / MIDP 2.0 (MicroEmulator 2.0.4) | `herramientas/j2me/cldcapi11-2.0.4.jar`, `midpapi20-2.0.4.jar` | Maven Central: `org.microemu` |
| Compilador ecj 4.6.1 | `herramientas/j2me/ecj-4.6.1.jar` | Maven Central: `org.eclipse.jdt.core.compiler:ecj:4.6.1` |
| ProGuard 7.6.1 | `herramientas/j2me/proguard-7.6.1/` | github.com/Guardsquare/proguard/releases |
| Bouncy Castle 1.60 (fuentes j2me, sólo para Metro en Symbian) | `herramientas/j2me/bc-j2me-src/` | bouncycastle.org (fuentes `bcprov-jdk15on-160` + `j2me`), adaptadas a CLDC con `sin-genericos.py` |

Linux usa además distrobox (Debian trixie y Ubuntu 24.04) para DEB, AppImage, Flatpak y Snap; Windows usa MinGW-w64 + NSIS.
