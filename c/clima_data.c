/* clima_data — implementación. Estilo: funciones cortas, un propósito cada una,
 * sin variables globales mutables salvo cachés de sólo-lectura. */
#include "clima_data.h"

#include <curl/curl.h>
#include <json-glib/json-glib.h>
#include <math.h>
#include <string.h>
#include <time.h>

#ifndef CLIMA_ROOT
#define CLIMA_ROOT "."
#endif
#ifndef CLIMA_LIBEXEC
#define CLIMA_LIBEXEC NULL
#endif
#define UA "ClimaKotone/2.0 (Fedora; C)"

const gchar *clima_root (void) {
    const gchar *e = g_getenv ("CLIMA_DIR");
    return e && *e ? e : CLIMA_ROOT;
}
const gchar *clima_libexec (void) {
    static gchar *p = NULL;
    const gchar *e = g_getenv ("CLIMA_LIBEXEC");
    if (e && *e) return e;
    if (!p) p = CLIMA_LIBEXEC ? g_strdup (CLIMA_LIBEXEC) : g_build_filename (clima_root (), "c", NULL);
    return p;
}

/* ---------------------------------------------------------------- HTTP */
typedef struct { char *data; size_t len; } Buf;
static size_t on_write (char *ptr, size_t sz, size_t nm, void *ud) {
    Buf *b = ud; size_t add = sz * nm;
    if (b->len + add > (8u << 20)) return 0;      /* tope 8 MB: una respuesta absurda aborta la descarga */
    char *p = g_realloc (b->data, b->len + add + 1);
    b->data = p; memcpy (p + b->len, ptr, add); b->len += add; p[b->len] = 0;
    return add;
}
/* GET síncrono. Devuelve el cuerpo (g_free) o NULL. */
static gchar *http_get (const gchar *url) {
    CURL *c = curl_easy_init (); if (!c) return NULL;
    Buf b = { g_strdup (""), 0 };
    curl_easy_setopt (c, CURLOPT_URL, url);
    curl_easy_setopt (c, CURLOPT_WRITEFUNCTION, on_write);
    curl_easy_setopt (c, CURLOPT_WRITEDATA, &b);
    curl_easy_setopt (c, CURLOPT_USERAGENT, UA);
    curl_easy_setopt (c, CURLOPT_TIMEOUT, 20L);
    curl_easy_setopt (c, CURLOPT_CONNECTTIMEOUT, 8L);
    curl_easy_setopt (c, CURLOPT_NOSIGNAL, 1L);         /* obligatorio con curl en hilos */
    curl_easy_setopt (c, CURLOPT_ACCEPT_ENCODING, "");  /* gzip/brotli: Open-Meteo responde ~5× más liviano */
    curl_easy_setopt (c, CURLOPT_FOLLOWLOCATION, 1L);
    CURLcode r = curl_easy_perform (c);
    long code = 0; curl_easy_getinfo (c, CURLINFO_RESPONSE_CODE, &code);
    curl_easy_cleanup (c);
    if (r != CURLE_OK || code >= 400) { g_free (b.data); return NULL; }
    return b.data;
}
static JsonNode *http_get_json (const gchar *url) {
    gchar *body = http_get (url); if (!body) return NULL;
    JsonParser *p = json_parser_new ();
    JsonNode *node = NULL;
    if (json_parser_load_from_data (p, body, -1, NULL))
        node = json_node_copy (json_parser_get_root (p));
    g_object_unref (p); g_free (body);
    return node;
}

/* --------------------------------------------------------------- caché */
/* Guarda el último análisis exitoso; si no hay red, se muestra ese dato. */
static gchar *cache_path (void) {
    return g_build_filename (g_get_user_cache_dir (), "clima", "last.json", NULL);
}
static void cache_save (const gchar *json) {
    gchar *p = cache_path (); gchar *dir = g_path_get_dirname (p);
    g_mkdir_with_parents (dir, 0700);
    g_file_set_contents (p, json, -1, NULL);
    g_free (dir); g_free (p);
}
/* Carga el caché y le marca "cached":true (para que la interfaz lo indique). */
gchar *clima_cache_load (void);   /* público, definido tras cache_load_marked */
static gchar *cache_load_marked (void) {
    gchar *p = cache_path (); gchar *body = NULL;
    if (!g_file_get_contents (p, &body, NULL, NULL)) { g_free (p); return NULL; }
    g_free (p);
    JsonParser *jp = json_parser_new (); gchar *out = NULL;
    if (json_parser_load_from_data (jp, body, -1, NULL)) {
        JsonNode *root = json_parser_get_root (jp);
        if (JSON_NODE_HOLDS_OBJECT (root)) {           /* caché corrupto → como si no hubiera */
            json_object_set_boolean_member (json_node_get_object (root), "cached", TRUE);
            out = json_to_string (root, FALSE);
        }
    }
    g_object_unref (jp); g_free (body);
    return out;
}
/* Wrapper público: pinta el último dato al abrir mientras llega el fresco. */
gchar *clima_cache_load (void) { return cache_load_marked (); }

/* ------------------------------------------------------ helpers json-glib */
static double  obj_num (JsonObject *o, const char *k) {
    return (o && json_object_has_member (o, k) &&
            !JSON_NODE_HOLDS_NULL (json_object_get_member (o, k)))
        ? json_object_get_double_member (o, k) : 0;
}
static gboolean obj_has (JsonObject *o, const char *k) {
    return o && json_object_has_member (o, k) &&
           !JSON_NODE_HOLDS_NULL (json_object_get_member (o, k));
}
static const char *obj_str (JsonObject *o, const char *k) {
    return obj_has (o, k) ? json_object_get_string_member (o, k) : "";
}
static double arr_num (JsonObject *o, const char *k, guint i) {
    if (!obj_has (o, k)) return 0;
    JsonArray *a = json_object_get_array_member (o, k);
    if (i >= json_array_get_length (a)) return 0;
    JsonNode *n = json_array_get_element (a, i);
    return JSON_NODE_HOLDS_NULL (n) ? 0 : json_node_get_double (n);
}
static const char *arr_str (JsonObject *o, const char *k, guint i) {
    if (!obj_has (o, k)) return "";
    JsonArray *a = json_object_get_array_member (o, k);
    if (i >= json_array_get_length (a)) return "";
    const char *s = json_node_get_string (json_array_get_element (a, i));
    return s ? s : "";
}
static guint arr_len (JsonObject *o, const char *k) {
    return obj_has (o, k) ? json_array_get_length (json_object_get_array_member (o, k)) : 0;
}

/* --------------------------------------------------------------- comunas */
static JsonArray *comunas_cache = NULL;
static JsonArray *comunas (void) {
    if (comunas_cache) return comunas_cache;
    JsonNode *n = NULL; JsonParser *p = json_parser_new ();
    if (json_parser_load_from_file (p, g_build_filename (clima_root (), "data", "comunas_cl.json", NULL), NULL))
        n = json_node_copy (json_parser_get_root (p));
    g_object_unref (p);
    comunas_cache = n ? json_node_dup_array (n) : json_array_new ();
    if (n) json_node_free (n);
    return comunas_cache;
}
/* normaliza a minúsculas sin tildes para comparar */
static gchar *fold (const char *s) {
    gchar *lo = g_utf8_strdown (s, -1);
    gchar *nf = g_utf8_normalize (lo, -1, G_NORMALIZE_NFD);
    GString *out = g_string_new (NULL);
    for (const char *p = nf; *p; p = g_utf8_next_char (p)) {
        gunichar u = g_utf8_get_char (p);
        if (g_unichar_type (u) != G_UNICODE_NON_SPACING_MARK) g_string_append_unichar (out, u);
    }
    g_free (lo); g_free (nf);
    return g_string_free (out, FALSE);
}
static ClimaLoc *loc_from_obj (JsonObject *o, gboolean is_comuna) {
    ClimaLoc *l = g_new0 (ClimaLoc, 1);
    l->name = g_strdup (obj_str (o, "name"));
    l->region = g_strdup (obj_str (o, "region"));
    l->lat = obj_num (o, "lat"); l->lon = obj_num (o, "lon");
    l->is_comuna = is_comuna;
    l->rm_restr = obj_has (o, "rm_restr") && json_object_get_boolean_member (o, "rm_restr");
    return l;
}

/* --------------------------------------------------------------- búsqueda */
gchar *clima_search_json (const gchar *query, int max) {
    gchar *q = fold (query);
    JsonBuilder *b = json_builder_new ();
    json_builder_begin_array (b);
    int n = 0;
    /* dos pasadas: prefijo primero, luego subcadena */
    for (int pass = 0; pass < 2 && n < max; pass++) {
        JsonArray *cs = comunas ();
        for (guint i = 0; i < json_array_get_length (cs) && n < max; i++) {
            JsonObject *o = json_array_get_object_element (cs, i);
            gchar *nm = fold (obj_str (o, "name"));
            gboolean hit = pass == 0 ? g_str_has_prefix (nm, q) : (strstr (nm, q) != NULL);
            g_free (nm);
            if (!hit) continue;
            json_builder_begin_object (b);
            json_builder_set_member_name (b, "name");   json_builder_add_string_value (b, obj_str (o, "name"));
            json_builder_set_member_name (b, "region"); json_builder_add_string_value (b, obj_str (o, "region"));
            json_builder_set_member_name (b, "lat");    json_builder_add_double_value (b, obj_num (o, "lat"));
            json_builder_set_member_name (b, "lon");    json_builder_add_double_value (b, obj_num (o, "lon"));
            json_builder_end_object (b);
            n++;
        }
    }
    json_builder_end_array (b);
    g_free (q);
    JsonNode *root = json_builder_get_root (b);
    gchar *out = json_to_string (root, FALSE);
    json_node_free (root); g_object_unref (b);
    return out;
}

/* comuna más cercana a unas coordenadas (o NULL si nada a <50 km) */
static JsonObject *nearest_comuna (double lat, double lon) {
    JsonArray *cs = comunas (); JsonObject *best = NULL; double bd = 1e18;
    for (guint i = 0; i < json_array_get_length (cs); i++) {
        JsonObject *o = json_array_get_object_element (cs, i);
        double dla = obj_num (o, "lat") - lat, dlo = obj_num (o, "lon") - lon;
        double d = dla * dla + dlo * dlo;
        if (d < bd) { bd = d; best = o; }
    }
    return (best && bd < 0.20) ? best : NULL;
}

ClimaLoc *clima_geocode (const gchar *name) {
    gchar *arr = clima_search_json (name, 1);
    JsonParser *p = json_parser_new (); ClimaLoc *loc = NULL;
    if (json_parser_load_from_data (p, arr, -1, NULL)) {
        JsonArray *a = json_node_get_array (json_parser_get_root (p));
        if (json_array_get_length (a) > 0)
            loc = loc_from_obj (json_array_get_object_element (a, 0), TRUE);
    }
    g_object_unref (p); g_free (arr);
    if (loc) {
        /* recuperar rm_restr desde la comuna original */
        JsonObject *c = nearest_comuna (loc->lat, loc->lon);
        if (c) loc->rm_restr = obj_has (c, "rm_restr") && json_object_get_boolean_member (c, "rm_restr");
        return loc;
    }
    /* online (Open-Meteo geocoding) */
    gchar *esc = g_uri_escape_string (name, NULL, FALSE);
    gchar *url = g_strdup_printf ("https://geocoding-api.open-meteo.com/v1/search?name=%s&count=1&language=es&format=json", esc);
    JsonNode *node = http_get_json (url); g_free (esc); g_free (url);
    if (node) {
        JsonObject *root = json_node_get_object (node);
        if (obj_has (root, "results")) {
            JsonArray *rs = json_object_get_array_member (root, "results");
            if (json_array_get_length (rs) > 0) {
                JsonObject *g = json_array_get_object_element (rs, 0);
                loc = g_new0 (ClimaLoc, 1);
                loc->name = g_strdup (obj_str (g, "name"));
                loc->region = g_strdup (obj_str (g, "admin1"));
                loc->lat = obj_num (g, "latitude"); loc->lon = obj_num (g, "longitude");
            }
        }
        json_node_free (node);
    }
    return loc;
}

ClimaLoc *clima_loc_from_coords (gdouble lat, gdouble lon) {
    JsonObject *c = nearest_comuna (lat, lon);
    ClimaLoc *loc = g_new0 (ClimaLoc, 1);
    loc->name   = g_strdup (c ? obj_str (c, "name") : "Mi ubicación");
    loc->region = g_strdup (c ? obj_str (c, "region") : "");
    loc->lat = lat; loc->lon = lon; loc->is_comuna = c != NULL;
    loc->rm_restr = c && obj_has (c, "rm_restr") && json_object_get_boolean_member (c, "rm_restr");
    return loc;
}

ClimaLoc *clima_current_location (void) {
    /* geolocalización por IP (thread-safe). GeoClue vive en el núcleo Python. */
    JsonNode *node = http_get_json ("http://ip-api.com/json/?fields=city,lat,lon,regionName");
    ClimaLoc *loc = NULL;
    if (node) {
        JsonObject *o = json_node_get_object (node);
        if (obj_has (o, "lat")) {
            double lat = obj_num (o, "lat"), lon = obj_num (o, "lon");
            JsonObject *c = nearest_comuna (lat, lon);
            loc = g_new0 (ClimaLoc, 1);
            loc->name   = g_strdup (c ? obj_str (c, "name") : obj_str (o, "city"));
            loc->region = g_strdup (c ? obj_str (c, "region") : obj_str (o, "regionName"));
            loc->lat = lat; loc->lon = lon; loc->is_comuna = c != NULL;
            loc->rm_restr = c && obj_has (c, "rm_restr") && json_object_get_boolean_member (c, "rm_restr");
        }
        json_node_free (node);
    }
    if (!loc) {  /* Santiago por defecto */
        loc = g_new0 (ClimaLoc, 1);
        loc->name = g_strdup ("Santiago"); loc->region = g_strdup ("Región Metropolitana");
        loc->lat = -33.4489; loc->lon = -70.6693; loc->is_comuna = TRUE; loc->rm_restr = TRUE;
    }
    return loc;
}

void clima_loc_free (ClimaLoc *l) { if (!l) return; g_free (l->name); g_free (l->region); g_free (l); }
ClimaLoc *clima_loc_copy (const ClimaLoc *s) {
    ClimaLoc *l = g_new0 (ClimaLoc, 1);
    *l = *s; l->name = g_strdup (s->name); l->region = g_strdup (s->region); return l;
}

/* ------------------------------------------------------------------ WMO */
static void wmo (int code, const char **icon, const char **text, const char **cls) {
    struct { int c; const char *i, *t, *k; } M[] = {
      {0,"☀️","Despejado","clear"},{1,"🌤️","Mayormente despejado","clear"},
      {2,"⛅","Parcialmente nublado","cloud"},{3,"☁️","Nublado","cloud"},
      {45,"🌫️","Niebla","fog"},{48,"🌫️","Niebla con escarcha","fog"},
      {51,"🌦️","Llovizna ligera","rain"},{53,"🌦️","Llovizna","rain"},{55,"🌧️","Llovizna intensa","rain"},
      {56,"🌧️","Llovizna helada","rain"},{57,"🌧️","Llovizna helada","rain"},
      {61,"🌧️","Lluvia ligera","rain"},{63,"🌧️","Lluvia","rain"},{65,"🌧️","Lluvia fuerte","rain"},
      {66,"🌧️","Lluvia helada","rain"},{67,"🌧️","Lluvia helada","rain"},
      {71,"🌨️","Nieve ligera","snow"},{73,"🌨️","Nieve","snow"},{75,"❄️","Nieve fuerte","snow"},{77,"🌨️","Granizo fino","snow"},
      {80,"🌦️","Chubascos ligeros","rain"},{81,"🌧️","Chubascos","rain"},{82,"⛈️","Chubascos violentos","storm"},
      {85,"🌨️","Chubascos de nieve","snow"},{86,"❄️","Chubascos de nieve","snow"},
      {95,"⛈️","Tormenta","storm"},{96,"⛈️","Tormenta con granizo","storm"},{99,"⛈️","Tormenta fuerte","storm"},
    };
    for (size_t i = 0; i < G_N_ELEMENTS (M); i++)
        if (M[i].c == code) { *icon = M[i].i; *text = M[i].t; *cls = M[i].k; return; }
    *icon = "☁️"; *text = "—"; *cls = "cloud";
}
static gboolean is_rain_code (int c) {
    int r[] = {51,53,55,56,57,61,63,65,66,67,80,81,82,95,96,99};
    for (size_t i = 0; i < G_N_ELEMENTS (r); i++) if (r[i] == c) return TRUE;
    return FALSE;
}
static const char *weekday_es (const char *iso) {
    static const char *D[] = {"lunes","martes","miércoles","jueves","viernes","sábado","domingo"};
    struct tm tm = {0}; strptime (iso, "%Y-%m-%d", &tm);
    time_t t = timegm (&tm); struct tm g; gmtime_r (&t, &g);
    int wd = (g.tm_wday + 6) % 7;   /* 0 = lunes */
    return D[wd];
}

/* --------------------------------------------------------------- ambiental */
#include "clima_ambiental_c.inc"   /* aire, luna, mareas, restricción, suspensión (JsonBuilder) */

/* ------------------------------------------------------------- get_weather */
static JsonNode *fetch_weather (const ClimaLoc *loc) {
    gchar *url = g_strdup_printf (
      "https://api.open-meteo.com/v1/forecast?latitude=%.4f&longitude=%.4f&timezone=America/Santiago"
      "&current=temperature_2m,relative_humidity_2m,apparent_temperature,is_day,precipitation,weather_code,"
      "wind_speed_10m,wind_direction_10m,wind_gusts_10m,surface_pressure,pressure_msl,cloud_cover,dew_point_2m"
      "&hourly=temperature_2m,weather_code,precipitation_probability,visibility,is_day"
      "&daily=weather_code,temperature_2m_max,temperature_2m_min,sunrise,sunset,uv_index_max,"
      "precipitation_probability_max,precipitation_sum,wind_speed_10m_max,wind_gusts_10m_max,wind_direction_10m_dominant"
      "&forecast_days=10&wind_speed_unit=kmh", loc->lat, loc->lon);
    JsonNode *n = http_get_json (url); g_free (url);
    return n;
}

/* aviso de lluvia (hoy + 7 días) con resumen inteligente */
static void build_rain (JsonBuilder *b, JsonObject *daily) {
    guint n = arr_len (daily, "time");
    GString *labels = g_string_new (NULL); int cnt = 0; char first[32] = "";
    for (guint i = 0; i < n && i < 8; i++) {
        int code = (int) arr_num (daily, "weather_code", i);
        double prob = arr_num (daily, "precipitation_probability_max", i);
        if (is_rain_code (code) && prob >= 50) {
            const char *iso = arr_str (daily, "time", i);
            char lbl[32];
            if (i == 0) g_strlcpy (lbl, "hoy", sizeof lbl);
            else if (i == 1) g_strlcpy (lbl, "mañana", sizeof lbl);
            else g_snprintf (lbl, sizeof lbl, "el %s", weekday_es (iso));
            if (cnt == 0) g_strlcpy (first, lbl, sizeof first);
            if (cnt) g_string_append (labels, ", ");
            g_string_append (labels, lbl);
            cnt++;
        }
    }
    if (!cnt) { json_builder_add_null_value (b); g_string_free (labels, TRUE); return; }
    gchar *summary;
    if (cnt <= 3) summary = g_strdup_printf ("Se espera lluvia %s.", labels->str);
    else summary = g_strdup_printf ("Semana lluviosa: lluvia %s y %d días más de los próximos 7.", first, cnt - 1);
    json_builder_begin_object (b);
    json_builder_set_member_name (b, "summary"); json_builder_add_string_value (b, summary);
    json_builder_set_member_name (b, "count");   json_builder_add_int_value (b, cnt);
    json_builder_end_object (b);
    g_free (summary); g_string_free (labels, TRUE);
}

/* ---------------------------------------------------------------- contexto
 * Comentario contextual del clima, al estilo de la app de Apple: UNA frase, la
 * más relevante de todas las que podrían decirse ahora mismo. No es sólo lluvia:
 * es una escalera de prioridades — lo que está pasando gana a lo que va a pasar,
 * y lo peligroso gana a lo cosmético. Si nada destaca, se dice algo útil igual.  */

/* Índice de la hora actual dentro del arreglo `hourly.time` (formato ISO local
 * "YYYY-MM-DDTHH:MM"). Devuelve -1 si no calza. */
static int hourly_now (JsonObject *hourly) {
    guint n = arr_len (hourly, "time");
    if (!n) return -1;
    time_t t = time (NULL); struct tm lt; localtime_r (&t, &lt);
    char want[20];
    strftime (want, sizeof want, "%Y-%m-%dT%H:00", &lt);
    for (guint i = 0; i < n; i++) {
        const char *s = arr_str (hourly, "time", i);
        if (s && strncmp (s, want, 13) == 0) return (int) i;   /* hasta la hora */
    }
    return -1;
}
/* "15:00" a partir del ISO de esa hora */
static void hour_label (JsonObject *hourly, guint i, char *out, size_t n) {
    const char *s = arr_str (hourly, "time", i);
    if (s && strlen (s) >= 16) g_snprintf (out, n, "%.5s", s + 11);
    else g_strlcpy (out, "más tarde", n);
}
static gboolean is_storm_code (int c) { return c == 95 || c == 96 || c == 99; }
static gboolean is_snow_code  (int c) { return c == 71 || c == 73 || c == 75 || c == 77 || c == 85 || c == 86; }
static const char *rose (double deg) {
    static const char *R[] = {"del norte","del noreste","del este","del sureste",
                              "del sur","del suroeste","del oeste","del noroeste"};
    int i = (int) lround (fmod (deg + 360.0, 360.0) / 45.0) % 8;
    return R[i];
}

static void build_context (JsonBuilder *b, JsonObject *w, JsonObject *cur, JsonObject *day) {
    JsonObject *hourly = obj_has (w, "hourly") ? json_object_get_object_member (w, "hourly") : NULL;
    int   now_i   = hourly ? hourly_now (hourly) : -1;
    int   code    = (int) obj_num (cur, "weather_code");
    double precip = obj_num (cur, "precipitation");
    double wind   = obj_num (cur, "wind_speed_10m");
    double gust   = obj_num (cur, "wind_gusts_10m");
    double wdir   = obj_num (cur, "wind_direction_10m");
    double temp   = obj_num (cur, "temperature_2m");
    double feels  = obj_num (cur, "apparent_temperature");
    double cloud  = obj_num (cur, "cloud_cover");
    gboolean is_day = obj_num (cur, "is_day") != 0;
    double tmax   = arr_num (day, "temperature_2m_max", 0);
    double tmin   = arr_num (day, "temperature_2m_min", 0);
    double uv     = arr_num (day, "uv_index_max", 0);
    guint  ndays  = arr_len (day, "time");

    char icon[8] = "🌤️"; char txt[192] = ""; const char *kind = "info";

    /* ---- 1. Está precipitando AHORA. ¿Hasta cuándo? ---------------------- */
    if (!txt[0] && (precip > 0.05 || is_rain_code (code) || is_snow_code (code)) && now_i >= 0) {
        guint n = arr_len (hourly, "time");
        int stop = -1;
        for (guint i = (guint) now_i + 1; i < n && i <= (guint) now_i + 12; i++) {
            int hc = (int) arr_num (hourly, "weather_code", i);
            double hp = arr_num (hourly, "precipitation_probability", i);
            if (!is_rain_code (hc) && !is_snow_code (hc) && hp < 40) { stop = (int) i; break; }
        }
        const char *que = is_snow_code (code) ? "Nieve" : (is_storm_code (code) ? "Tormenta" : "Lluvia");
        if (code >= 51 && code <= 57) que = "Llovizna";
        g_strlcpy (icon, is_snow_code (code) ? "🌨️" : "🌧️", sizeof icon);
        kind = is_snow_code (code) ? "snow" : "rain";
        if (stop < 0)
            g_snprintf (txt, sizeof txt, "%s ahora; sigue durante las próximas horas.", que);
        else {
            int horas = stop - now_i;
            char hl[16]; hour_label (hourly, (guint) stop, hl, sizeof hl);
            if (horas <= 1) g_snprintf (txt, sizeof txt, "%s ahora; para dentro de poco.", que);
            else            g_snprintf (txt, sizeof txt, "%s ahora; amaina hacia las %s.", que, hl);
        }
    }
    /* ---- 2. Precipitación INMINENTE (próximas 6 horas) -------------------
     * Ojo: el `weather_code` horario trae la condición DOMINANTE, así que un
     * día de llovizna sale como "nublado" con 85 % de probabilidad. Por eso la
     * probabilidad dispara por sí sola, igual que hace la app de Apple.        */
    if (!txt[0] && now_i >= 0) {
        guint n = arr_len (hourly, "time");
        int dcode = (int) arr_num (day, "weather_code", 0);
        for (guint i = (guint) now_i + 1; i < n && i <= (guint) now_i + 6; i++) {
            int hc = (int) arr_num (hourly, "weather_code", i);
            double hp = arr_num (hourly, "precipitation_probability", i);
            gboolean por_codigo = (is_rain_code (hc) || is_snow_code (hc)) && hp >= 50;
            gboolean por_prob   = hp >= 70;
            if (!por_codigo && !por_prob) continue;
            int ref = (is_rain_code (hc) || is_snow_code (hc)) ? hc : dcode;
            char hl[16]; hour_label (hourly, i, hl, sizeof hl);
            const char *que = is_snow_code (ref) ? "nieve"
                            : (is_storm_code (ref) ? "tormenta"
                            : ((ref >= 51 && ref <= 57) ? "llovizna" : "lluvia"));
            g_strlcpy (icon, is_storm_code (ref) ? "⛈️" : (is_snow_code (ref) ? "🌨️" : "🌧️"), sizeof icon);
            kind = is_snow_code (ref) ? "snow" : "rain";
            g_snprintf (txt, sizeof txt, "Se espera %s hacia las %s (%d%%).", que, hl, (int) lround (hp));
            break;
        }
    }
    /* ---- 2b. Sin nada inminente, pero el día sigue lluvioso -------------- */
    if (!txt[0]) {
        int dcode = (int) arr_num (day, "weather_code", 0);
        double dprob = arr_num (day, "precipitation_probability_max", 0);
        if ((is_rain_code (dcode) || is_snow_code (dcode)) && dprob >= 60) {
            g_strlcpy (icon, is_snow_code (dcode) ? "🌨️" : "🌧️", sizeof icon);
            kind = is_snow_code (dcode) ? "snow" : "rain";
            g_snprintf (txt, sizeof txt, "%s probable en algún momento de hoy (%d%%).",
                        is_snow_code (dcode) ? "Nieve" : ((dcode >= 51 && dcode <= 57) ? "Llovizna" : "Lluvia"),
                        (int) lround (dprob));
        }
    }
    /* ---- 3. Tormenta en el día ------------------------------------------ */
    if (!txt[0] && is_storm_code ((int) arr_num (day, "weather_code", 0))) {
        g_strlcpy (icon, "⛈️", sizeof icon); kind = "storm";
        g_strlcpy (txt, "Posibles tormentas eléctricas hoy.", sizeof txt);
    }
    /* ---- 4. Ráfagas o viento fuerte ------------------------------------- */
    if (!txt[0] && gust >= 45) {
        g_strlcpy (icon, "💨", sizeof icon); kind = "wind";
        g_snprintf (txt, sizeof txt, "Ráfagas de hasta %d km/h %s.", (int) lround (gust), rose (wdir));
    }
    if (!txt[0] && wind >= 30) {
        g_strlcpy (icon, "💨", sizeof icon); kind = "wind";
        g_snprintf (txt, sizeof txt, "Viento de %d km/h %s.", (int) lround (wind), rose (wdir));
    }
    /* ---- 5. Niebla / visibilidad ---------------------------------------- */
    if (!txt[0] && (code == 45 || code == 48)) {
        g_strlcpy (icon, "🌫️", sizeof icon); kind = "fog";
        g_strlcpy (txt, "Niebla: visibilidad reducida, ojo al manejar.", sizeof txt);
    }
    /* ---- 6. Frío o calor que se siente ---------------------------------- */
    if (!txt[0] && tmax >= 32) {
        g_strlcpy (icon, "🥵", sizeof icon); kind = "heat";
        g_snprintf (txt, sizeof txt, "Calor: hoy llega a %d°. Hidrátate.", (int) lround (tmax));
    }
    if (!txt[0] && tmin <= 1) {
        g_strlcpy (icon, "🥶", sizeof icon); kind = "cold";
        g_snprintf (txt, sizeof txt, "Helada: la mínima baja a %d°.", (int) lround (tmin));
    }
    if (!txt[0] && fabs (feels - temp) >= 4) {
        g_strlcpy (icon, feels < temp ? "🥶" : "🥵", sizeof icon); kind = "feels";
        g_snprintf (txt, sizeof txt, "Se siente como %d°, %s de lo que marca.",
                    (int) lround (feels), feels < temp ? "más frío" : "más caluroso");
    }
    /* ---- 7. UV alto ------------------------------------------------------ */
    if (!txt[0] && is_day && uv >= 8) {
        g_strlcpy (icon, "🧴", sizeof icon); kind = "uv";
        g_snprintf (txt, sizeof txt, "Índice UV muy alto (%d): usa protector.", (int) lround (uv));
    }
    /* ---- 8. Lluvia en los próximos días --------------------------------- */
    if (!txt[0]) {
        for (guint i = 1; i < ndays && i < 8; i++) {
            int dc = (int) arr_num (day, "weather_code", i);
            double dp = arr_num (day, "precipitation_probability_max", i);
            if (is_rain_code (dc) && dp >= 50) {
                const char *iso = arr_str (day, "time", i);
                char lbl[24];
                if (i == 1) g_strlcpy (lbl, "mañana", sizeof lbl);
                else g_snprintf (lbl, sizeof lbl, "el %s", weekday_es (iso));
                g_strlcpy (icon, "💧", sizeof icon); kind = "rain";
                g_snprintf (txt, sizeof txt, "Se espera lluvia %s.", lbl);
                break;
            }
        }
    }
    /* ---- 9. Cambio marcado respecto a mañana ----------------------------- */
    if (!txt[0] && ndays > 1) {
        double t2 = arr_num (day, "temperature_2m_max", 1);
        if (fabs (t2 - tmax) >= 6) {
            g_strlcpy (icon, t2 > tmax ? "📈" : "📉", sizeof icon); kind = "trend";
            g_snprintf (txt, sizeof txt, "Mañana %s: máxima de %d°, %d° %s que hoy.",
                        t2 > tmax ? "sube" : "baja", (int) lround (t2),
                        (int) lround (fabs (t2 - tmax)), t2 > tmax ? "más" : "menos");
        }
    }
    /* ---- 10. Cielo: se despeja o se nubla -------------------------------- */
    if (!txt[0] && now_i >= 0 && hourly) {
        guint n = arr_len (hourly, "time");
        int later = now_i + 4;
        if ((guint) later < n) {
            int hc = (int) arr_num (hourly, "weather_code", (guint) later);
            char hl[16]; hour_label (hourly, (guint) later, hl, sizeof hl);
            if (cloud >= 70 && hc <= 1) {
                g_strlcpy (icon, "🌤️", sizeof icon); kind = "clear";
                g_snprintf (txt, sizeof txt, "Se despeja hacia las %s.", hl);
            } else if (cloud <= 30 && hc >= 3) {
                g_strlcpy (icon, "☁️", sizeof icon); kind = "cloud";
                g_snprintf (txt, sizeof txt, "Se nubla hacia las %s.", hl);
            }
        }
    }
    /* ---- 11. Nada destaca: igual decimos algo con sustancia -------------- */
    if (!txt[0]) {
        g_strlcpy (icon, is_day ? "🌤️" : "🌙", sizeof icon); kind = "steady";
        g_snprintf (txt, sizeof txt, "Sin cambios notorios: máxima de %d° y mínima de %d°.",
                    (int) lround (tmax), (int) lround (tmin));
    }

    json_builder_begin_object (b);
    json_builder_set_member_name (b, "icon"); json_builder_add_string_value (b, icon);
    json_builder_set_member_name (b, "text"); json_builder_add_string_value (b, txt);
    json_builder_set_member_name (b, "kind"); json_builder_add_string_value (b, kind);
    json_builder_end_object (b);
}

gchar *clima_analyze_json (const ClimaLoc *loc, gchar **error_out) {
    if (error_out) *error_out = NULL;
    JsonNode *wnode = fetch_weather (loc);
    if (wnode && !JSON_NODE_HOLDS_OBJECT (wnode)) { json_node_free (wnode); wnode = NULL; }
    if (!wnode) {
        /* sin internet: mostrar el último dato guardado */
        gchar *cached = cache_load_marked ();
        if (cached) return cached;
        if (error_out) *error_out = g_strdup ("sin conexión y sin datos en caché");
        return NULL;
    }
    JsonObject *w = json_node_get_object (wnode);
    JsonObject *cur = json_object_get_object_member (w, "current");
    JsonObject *day = json_object_get_object_member (w, "daily");

    int code = (int) obj_num (cur, "weather_code");
    const char *icon, *text, *cls; wmo (code, &icon, &text, &cls);
    gboolean is_day = obj_num (cur, "is_day") != 0;
    int temp = (int) lround (obj_num (cur, "temperature_2m"));
    int feels = (int) lround (obj_num (cur, "apparent_temperature"));

    JsonBuilder *b = json_builder_new ();
    json_builder_begin_object (b);

    json_builder_set_member_name (b, "location");
    json_builder_begin_object (b);
      json_builder_set_member_name (b, "name");   json_builder_add_string_value (b, loc->name);
      json_builder_set_member_name (b, "region"); json_builder_add_string_value (b, loc->region);
      json_builder_set_member_name (b, "lat");    json_builder_add_double_value (b, loc->lat);
      json_builder_set_member_name (b, "lon");    json_builder_add_double_value (b, loc->lon);
      json_builder_set_member_name (b, "comuna"); json_builder_add_boolean_value (b, loc->is_comuna);
      json_builder_set_member_name (b, "rm_restr");json_builder_add_boolean_value (b, loc->rm_restr);
    json_builder_end_object (b);

    json_builder_set_member_name (b, "current");
    json_builder_begin_object (b);
      json_builder_set_member_name (b, "temp");   json_builder_add_int_value (b, temp);
      json_builder_set_member_name (b, "feels");  json_builder_add_int_value (b, feels);
      json_builder_set_member_name (b, "icon");   json_builder_add_string_value (b, icon);
      json_builder_set_member_name (b, "text");   json_builder_add_string_value (b, text);
      json_builder_set_member_name (b, "class");  json_builder_add_string_value (b, cls);
      json_builder_set_member_name (b, "is_day"); json_builder_add_boolean_value (b, is_day);
      json_builder_set_member_name (b, "humidity");json_builder_add_int_value (b, (int) obj_num (cur, "relative_humidity_2m"));
      json_builder_set_member_name (b, "wind");   json_builder_add_int_value (b, (int) lround (obj_num (cur, "wind_speed_10m")));
      json_builder_set_member_name (b, "gust");   json_builder_add_int_value (b, (int) lround (obj_num (cur, "wind_gusts_10m")));
      json_builder_set_member_name (b, "wind_dir");json_builder_add_double_value (b, obj_num (cur, "wind_direction_10m"));
      json_builder_set_member_name (b, "pressure");json_builder_add_int_value (b, (int) lround (obj_has (cur,"pressure_msl") ? obj_num (cur,"pressure_msl") : obj_num (cur,"surface_pressure")));
      json_builder_set_member_name (b, "cloud");  json_builder_add_int_value (b, (int) obj_num (cur, "cloud_cover"));
      json_builder_set_member_name (b, "precip"); json_builder_add_double_value (b, obj_num (cur, "precipitation"));
      json_builder_set_member_name (b, "dew");
      if (obj_has (cur, "dew_point_2m")) json_builder_add_int_value (b, (int) lround (obj_num (cur, "dew_point_2m")));
      else json_builder_add_null_value (b);
    json_builder_end_object (b);

    json_builder_set_member_name (b, "today");
    json_builder_begin_object (b);
      json_builder_set_member_name (b, "max"); json_builder_add_int_value (b, (int) lround (arr_num (day, "temperature_2m_max", 0)));
      json_builder_set_member_name (b, "min"); json_builder_add_int_value (b, (int) lround (arr_num (day, "temperature_2m_min", 0)));
      json_builder_set_member_name (b, "uv");  json_builder_add_double_value (b, arr_num (day, "uv_index_max", 0));
      json_builder_set_member_name (b, "sunrise"); json_builder_add_string_value (b, arr_str (day, "sunrise", 0));
      json_builder_set_member_name (b, "sunset");  json_builder_add_string_value (b, arr_str (day, "sunset", 0));
    json_builder_end_object (b);

    /* `rain` se mantiene tal cual (lo consumen el CLI y clima-notify.sh);
     * `context` es el comentario contextual nuevo que pinta la interfaz. */
    json_builder_set_member_name (b, "rain"); build_rain (b, day);
    json_builder_set_member_name (b, "context"); build_context (b, w, cur, day);

    json_builder_set_member_name (b, "ambiental"); build_ambiental (b, loc, day);

    /* Momento real de la consulta. Viaja dentro del JSON, así que también queda
       grabado en el caché: la interfaz muestra la hora del DATO, no la hora en que
       se dibujó la pantalla (que siempre parecía recién actualizada, aun offline). */
    json_builder_set_member_name (b, "fetched"); json_builder_add_int_value (b, (gint64) time (NULL));

    json_builder_set_member_name (b, "raw"); json_builder_add_value (b, json_node_copy (wnode));

    json_builder_end_object (b);
    JsonNode *root = json_builder_get_root (b);
    gchar *out = json_to_string (root, FALSE);
    json_node_free (root); g_object_unref (b); json_node_free (wnode);
    cache_save (out);   /* guarda para uso sin conexión */
    return out;
}

/* Estado del Metro de Santiago (API oficial de metro.cl) para el recuadro «Metro» — el mismo que muestra Android
 * (allí lo entrega la app Metro). Devuelve {"ok":bool,"problemas":[{"linea":"L2","texto":"…"}]}. Kotone 2026-10-31. */
gchar *clima_metro_json (void) {
    JsonNode *n = http_get_json ("https://www.metro.cl/api/estadoRedDetalle.php");
    JsonBuilder *b = json_builder_new ();
    json_builder_begin_object (b);
    json_builder_set_member_name (b, "ok"); json_builder_add_boolean_value (b, n && JSON_NODE_HOLDS_OBJECT (n));
    json_builder_set_member_name (b, "problemas"); json_builder_begin_array (b);
    if (n && JSON_NODE_HOLDS_OBJECT (n)) {
        JsonObject *o = json_node_get_object (n);
        GList *ks = json_object_get_members (o);
        for (GList *k = ks; k; k = k->next) {
            JsonNode *ln = json_object_get_member (o, k->data);
            if (!ln || !JSON_NODE_HOLDS_OBJECT (ln)) continue;
            JsonObject *L = json_node_get_object (ln);
            gchar *lin = g_ascii_strup ((const char *) k->data + 1, -1), *etq = g_strconcat ("L", lin, NULL);
            if (g_strcmp0 (json_object_get_string_member_with_default (L, "estado", "1"), "1")) {
                json_builder_begin_object (b);
                json_builder_set_member_name (b, "linea"); json_builder_add_string_value (b, etq);
                json_builder_set_member_name (b, "texto"); json_builder_add_string_value (b, json_object_get_string_member_with_default (L, "mensaje_app", "Con problemas"));
                json_builder_end_object (b);
            }
            JsonArray *es = json_object_has_member (L, "estaciones") ? json_object_get_array_member (L, "estaciones") : NULL;
            for (guint i = 0; es && i < json_array_get_length (es); i++) {
                JsonObject *e = json_array_get_object_element (es, i);
                if (!e || !g_strcmp0 (json_object_get_string_member_with_default (e, "estado", "1"), "1")) continue;
                gchar *t = g_strdup_printf ("%s: %s", json_object_get_string_member_with_default (e, "nombre", ""),
                                            json_object_get_string_member_with_default (e, "descripcion_app", "Con problemas"));
                json_builder_begin_object (b);
                json_builder_set_member_name (b, "linea"); json_builder_add_string_value (b, etq);
                json_builder_set_member_name (b, "texto"); json_builder_add_string_value (b, t);
                json_builder_end_object (b);
                g_free (t);
            }
            g_free (lin); g_free (etq);
        }
        g_list_free (ks);
    }
    json_builder_end_array (b); json_builder_end_object (b);
    JsonGenerator *g = json_generator_new ();
    JsonNode *root = json_builder_get_root (b);
    json_generator_set_root (g, root);
    gchar *out = json_generator_to_data (g, NULL);
    json_node_free (root); g_object_unref (g); g_object_unref (b);
    if (n) json_node_free (n);
    return out;
}

