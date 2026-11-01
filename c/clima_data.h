/* clima_data — capa de datos de Clima en C.
 * Obtiene clima (Open-Meteo), ubicación, comunas de Chile y datos ambientales,
 * y arma el JSON de análisis que consume la interfaz (idéntico al del núcleo Python).
 * Sin estado global observable: cada función es autocontenida y fácil de leer. */
#ifndef CLIMA_DATA_H
#define CLIMA_DATA_H

#include <glib.h>

/* Una ubicación: comuna/ciudad de Chile o coordenada libre. */
typedef struct {
    gchar   *name;
    gchar   *region;
    gdouble  lat, lon;
    gboolean is_comuna;
    gboolean rm_restr;   /* dentro de la zona de restricción vehicular (RM) */
} ClimaLoc;

void       clima_loc_free (ClimaLoc *loc);
ClimaLoc  *clima_loc_copy (const ClimaLoc *loc);

/* Búsqueda instantánea de comunas/ciudades (comunas primero). Devuelve JSON array
 * [{name,region,lat,lon}], listo para inyectar en el buscador. */
gchar     *clima_search_json (const gchar *query, int max);

/* Geocodifica un nombre a una ubicación (comunas/ciudades locales, luego online). */
ClimaLoc  *clima_geocode (const gchar *name);

/* Construye una ubicación desde coordenadas, anclada a la comuna más cercana
 * (para la ubicación entregada por GeoClue). */
ClimaLoc  *clima_loc_from_coords (gdouble lat, gdouble lon);

/* Reporte de consola (comando `clima`). Definido en clima_cli.c. */
int        clima_cli_run (int argc, char **argv);
/* Rutas de instalación: $CLIMA_DIR (AppImage/Flatpak) o la raíz compilada; módulos en $CLIMA_LIBEXEC o <raíz>/c */
const gchar *clima_root (void);
const gchar *clima_libexec (void);

/* Ubicación actual real (IP → comuna más cercana → Santiago). */
ClimaLoc  *clima_current_location (void);

/* Descarga el clima + ambiental y devuelve el JSON de análisis serializado
 * (location, current, today, rain, ambiental, raw). g_free() al terminar.
 * *error_out queda con un mensaje si falla (o NULL). */
gchar     *clima_analyze_json (const ClimaLoc *loc, gchar **error_out);

/* Devuelve el último JSON cacheado (con "cached":true) o NULL si no hay. Para
 * pintar de inmediato al abrir mientras se refresca desde las fuentes. */
gchar     *clima_cache_load (void);

gchar *clima_metro_json (void);   /* estado del Metro (metro.cl) para el recuadro «Metro» */

#endif
