# Cambios / Changelog

## 1.3.0 (2026-11-08)
- Accesibilidad: el zoom de la interfaz ya no está bloqueado; el buscador tiene nombre accesible; los días del pronóstico y las sugerencias de ciudad se pueden usar con teclado y lector de pantalla (Enter/Espacio, flechas, Escape para cerrar el detalle); el botón de ubicación y el buscador miden 44 px; foco visible.
- Error de red: la pantalla de error ahora trae un botón «Reintentar».
- Sin conexión: si se busca otra ciudad y no hay red, ya no se muestra el clima guardado de la ciudad anterior; se avisa que no hay datos guardados de ese lugar. Con la misma ciudad se sigue mostrando el dato guardado, marcado como tal.
- Aire, mareas y fuentes oficiales (Mineduc, airerm) tienen un tope de 9 s: si una no responde, el pronóstico aparece igual con esa tarjeta sin dato, en vez de quedar cargando.
- Visibilidad: si el pronóstico no trae el dato, la tarjeta dice «Sin dato» (antes decía «Reducida»).
- Android: si el sistema cierra el proceso de la vista web por falta de memoria, la pantalla se recrea en vez de cerrar la app.
- Pruebas: `node tests/clima-android.test.js` (caché por lugar, topes de tiempo, accesibilidad del HTML); se ejecutan en CI.
- Versiones unificadas en 1.3.0 (Android versionCode 4, escritorio, Windows, Apple y paquetes de distribuciones).

## Primera versión pública / First public release
- Android 5.0+, Linux (RPM, DEB, AppImage, Flatpak, Snap), Windows, macOS/iOS. Fuentes y paquetes en las *releases*.
