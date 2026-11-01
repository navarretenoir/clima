# Clima

**Aplicación del clima de Chile**: el pronóstico para sus **346 comunas**, con un cielo animado para cada condición, de día y de noche.
Alerta ambiental por MP2,5, restricción vehicular, suspensión de clases según el Mineduc, fase lunar y mareas.
Sin publicidad, sin rastreadores, sin cuentas.

**Desarrollado por Javiera Navarrete** · Licencia [GPL-3.0-or-later](LICENSE)

## Plataformas

| Carpeta | Plataforma | Cómo se compila |
|---|---|---|
| `android/` | Android 5.0 → 16 | Android Studio o `./gradlew assembleRelease` (sin dependencias externas) |
| `c/` + `linux/` | Linux (GTK 4 + WebKitGTK 6) | `make -C c`; paquetes RPM/DEB/AppImage/Flatpak/Snap con `herramientas/linux-pkg/empaquetar.sh . todo` |
| `windows-nativo/` | Windows 2000 → 11 y Windows 95/98/Me | `windows-nativo/construir.sh` (MinGW-w64 + NSIS); proyecto de Visual Studio en `visualstudio/` |
| `apple/` | macOS (Intel y Apple Silicon) e iOS/iPadOS | `apple/preparar-web.sh && xcodegen generate` (también lo compila el flujo de GitHub Actions) |
| `symbian/` | Symbian / Java ME (MIDP 2.0, CLDC 1.1) | `symbian/build.sh` (kit en `herramientas/j2me`) |

`ui/` es la interfaz compartida (HTML/CSS/JS) y `data/` la lista de comunas.

## Fuentes de datos

- Pronóstico, calidad del aire y mareas: [Open-Meteo](https://open-meteo.com) (CC BY 4.0).
- Suspensión de clases: noticias publicadas por el [Mineduc](https://www.mineduc.cl). La app nunca afirma un decreto que no esté publicado.
- Restricción vehicular: [airerm.mma.gob.cl](https://airerm.mma.gob.cl).

Tipografía Inter (SIL Open Font License, ver `android/web/fonts/OFL-Inter.txt`).


---

## English

**Clima** is a weather app for Chile: current conditions, 24-hour and 10-day forecast for all 346 Chilean communes, with an
animated sky for every condition, day and night. It adds Chile-specific information: PM2.5 environmental alerts (official
thresholds), the Santiago driving restriction, school-closure notices as published by the Ministry of Education (it never claims an
unpublished decree), moon phase and tides. No ads, no trackers, no accounts.

Platforms: Android 5.0+, Linux (RPM, DEB, AppImage, Flatpak, Snap), Windows 95/98/Me and 2000 → 11, macOS (Intel and Apple Silicon),
iOS/iPadOS and Symbian / Java ME. The interface is in Spanish (Chile).

**Developed by Javiera Navarrete** · License [GPL-3.0-or-later](LICENSE). Data: Open-Meteo (CC BY 4.0), Mineduc, airerm.mma.gob.cl.
