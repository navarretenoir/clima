/* clima_ambiental (C) — datos ambientales de Chile. Incluido por clima_data.c
 * (usa sus helpers http_get_json, obj_num, arr_num, weekday_es, etc.). */

/* --- calidad del aire / episodio (GEC-MMA; MP2,5 24h: 80/110/170) --- */
/* Además deja en keybuf la clave de episodio (la usa la restricción): una sola consulta a la API. */
static void build_air (JsonBuilder *b, const ClimaLoc *loc, char *keybuf, size_t kn) {
    g_strlcpy (keybuf, "normal", kn);
    gchar *url = g_strdup_printf (
        "https://air-quality-api.open-meteo.com/v1/air-quality?latitude=%.4f&longitude=%.4f"
        "&timezone=America/Santiago&current=pm2_5,pm10&hourly=pm2_5&forecast_days=1", loc->lat, loc->lon);
    JsonNode *n = http_get_json (url); g_free (url);
    if (n && !JSON_NODE_HOLDS_OBJECT (n)) { json_node_free (n); n = NULL; }
    if (!n) { json_builder_add_null_value (b); return; }
    JsonObject *o = json_node_get_object (n);
    JsonObject *cur = obj_has (o, "current") ? json_object_get_object_member (o, "current") : NULL;
    double pm25 = obj_num (cur, "pm2_5"), pm10 = obj_num (cur, "pm10");
    JsonObject *hh = obj_has (o, "hourly") ? json_object_get_object_member (o, "hourly") : NULL;
    guint m = arr_len (hh, "pm2_5"); double sum = 0; int c = 0;
    for (guint i = 0; i < m; i++) { double v = arr_num (hh, "pm2_5", i); if (v > 0 || i == 0) { sum += v; c++; } }
    double mean24 = c ? sum / c : pm25;
    const char *lvl = "Bueno", *col = "#34C759", *key = "normal";
    if (mean24 >= 170) { lvl = "Emergencia"; col = "#AF52DE"; key = "emergencia"; }
    else if (mean24 >= 110) { lvl = "Preemergencia"; col = "#FF3B30"; key = "preemergencia"; }
    else if (mean24 >= 80)  { lvl = "Alerta"; col = "#FF9500"; key = "alerta"; }
    else if (mean24 >= 50)  { lvl = "Regular"; col = "#FFD426"; key = "normal"; }
    json_builder_begin_object (b);
      json_builder_set_member_name (b, "pm25");   json_builder_add_double_value (b, round (pm25 * 10) / 10);
      json_builder_set_member_name (b, "pm10");   json_builder_add_double_value (b, round (pm10 * 10) / 10);
      json_builder_set_member_name (b, "mean24"); json_builder_add_double_value (b, round (mean24 * 10) / 10);
      json_builder_set_member_name (b, "level");  json_builder_add_string_value (b, lvl);
      json_builder_set_member_name (b, "color");  json_builder_add_string_value (b, col);
      json_builder_set_member_name (b, "key");    json_builder_add_string_value (b, key);
    json_builder_end_object (b);
    g_strlcpy (keybuf, key, kn);
    json_node_free (n);
}

/* --- fase lunar (cálculo local) --- */
static void build_moon (JsonBuilder *b) {
    const char *names[] = {"Luna nueva","Luna creciente","Cuarto creciente","Gibosa creciente",
        "Luna llena","Gibosa menguante","Cuarto menguante","Luna menguante"};
    const char *emo[] = {"🌑","🌒","🌓","🌔","🌕","🌖","🌗","🌘"};
    time_t now = time (NULL); struct tm g; gmtime_r (&now, &g);
    struct tm ref = { .tm_year = 100, .tm_mon = 0, .tm_mday = 6 };
    double days = difftime (timegm (&g), timegm (&ref)) / 86400.0;
    double syn = 29.53058867;
    double frac = fmod (days, syn) / syn; if (frac < 0) frac += 1;
    int idx = ((int) (frac * 8 + 0.5)) % 8;
    int illum = (int) lround ((1 - cos (2 * M_PI * frac)) / 2 * 100);
    json_builder_begin_object (b);
      json_builder_set_member_name (b, "name");  json_builder_add_string_value (b, names[idx]);
      json_builder_set_member_name (b, "emoji"); json_builder_add_string_value (b, emo[idx]);
      json_builder_set_member_name (b, "illumination"); json_builder_add_int_value (b, illum);
    json_builder_end_object (b);
}

/* --- mareas (Open-Meteo Marine; puerto de referencia más cercano) --- */
static void build_tides (JsonBuilder *b, const ClimaLoc *loc) {
    struct { const char *name; double lat, lon; } P[] = {
      {"Arica",-18.48,-70.32},{"Iquique",-20.21,-70.15},{"Antofagasta",-23.65,-70.40},
      {"Caldera",-27.07,-70.82},{"Coquimbo",-29.95,-71.34},{"Valparaíso",-33.03,-71.63},
      {"San Antonio",-33.59,-71.61},{"Constitución",-35.33,-72.41},{"Talcahuano",-36.72,-73.11},
      {"Lebu",-37.61,-73.65},{"Corral",-39.87,-73.43},{"Puerto Montt",-41.47,-72.94},
      {"Castro",-42.48,-73.76},{"Puerto Chacabuco",-45.46,-72.82},{"Punta Arenas",-53.16,-70.91},
    };
    int best = 0; double bd = 1e18;
    for (size_t i = 0; i < G_N_ELEMENTS (P); i++) {
        double dla = P[i].lat - loc->lat, dlo = (P[i].lon - loc->lon) * cos (loc->lat * M_PI / 180);
        double d = dla * dla + dlo * dlo; if (d < bd) { bd = d; best = i; }
    }
    double dist_km = sqrt (bd) * 111.0;
    gchar *url = g_strdup_printf (
        "https://marine-api.open-meteo.com/v1/marine?latitude=%.3f&longitude=%.3f"
        "&timezone=America/Santiago&hourly=sea_level_height_msl&forecast_days=2", P[best].lat, P[best].lon);
    JsonNode *n = http_get_json (url); g_free (url);
    if (n && !JSON_NODE_HOLDS_OBJECT (n)) { json_node_free (n); n = NULL; }
    if (!n) { json_builder_add_null_value (b); return; }
    JsonObject *no = json_node_get_object (n);
    JsonObject *hh = obj_has (no, "hourly") ? json_object_get_object_member (no, "hourly") : NULL;
    guint m = arr_len (hh, "sea_level_height_msl");
    if (m < 6) { json_builder_add_null_value (b); json_node_free (n); return; }
    /* extremos: pleamar (máximo local) y bajamar (mínimo local) futuros */
    char hi_t[24] = "", lo_t[24] = ""; double hi_v = 0, lo_v = 0; gboolean got_hi = 0, got_lo = 0;
    for (guint i = 1; i + 1 < m; i++) {
        double y0 = arr_num (hh, "sea_level_height_msl", i - 1);
        double y1 = arr_num (hh, "sea_level_height_msl", i);
        double y2 = arr_num (hh, "sea_level_height_msl", i + 1);
        if (!got_hi && y1 >= y0 && y1 >= y2) { g_strlcpy (hi_t, arr_str (hh, "time", i), sizeof hi_t); hi_v = y1; got_hi = 1; }
        else if (!got_lo && y1 <= y0 && y1 <= y2) { g_strlcpy (lo_t, arr_str (hh, "time", i), sizeof lo_t); lo_v = y1; got_lo = 1; }
        if (got_hi && got_lo) break;
    }
    json_builder_begin_object (b);
      json_builder_set_member_name (b, "port");    json_builder_add_string_value (b, P[best].name);
      json_builder_set_member_name (b, "coastal"); json_builder_add_boolean_value (b, dist_km < 15);
      json_builder_set_member_name (b, "dist_km"); json_builder_add_int_value (b, (int) lround (dist_km));
      json_builder_set_member_name (b, "next_high");
      if (got_hi) { json_builder_begin_array (b); json_builder_add_string_value (b, hi_t); json_builder_add_double_value (b, round (hi_v*100)/100); json_builder_end_array (b); }
      else json_builder_add_null_value (b);
      json_builder_set_member_name (b, "next_low");
      if (got_lo) { json_builder_begin_array (b); json_builder_add_string_value (b, lo_t); json_builder_add_double_value (b, round (lo_v*100)/100); json_builder_end_array (b); }
      else json_builder_add_null_value (b);
    json_builder_end_object (b);
    json_node_free (n);
}

/* --- restricción vehicular (MTT; zona RM por comuna) --- */
static void build_restriccion (JsonBuilder *b, const ClimaLoc *loc, const char *key) {
    time_t now = time (NULL); struct tm lt; localtime_r (&now, &lt);
    int wd = (lt.tm_wday + 6) % 7;   /* 0 = lunes */
    gboolean season = lt.tm_mon + 1 >= 5 && lt.tm_mon + 1 <= 8;   /* 1-may → 31-ago */
    const char *dig[] = {"8 y 9","0 y 1","2 y 3","4 y 5","6 y 7"};
    json_builder_begin_object (b);
    if (!loc->rm_restr) {
        json_builder_set_member_name (b, "aplica"); json_builder_add_boolean_value (b, FALSE);
        json_builder_set_member_name (b, "motivo"); json_builder_add_string_value (b,
            "La restricción por dígitos rige en la Provincia de Santiago, San Bernardo y Puente Alto.");
    } else if (!season) {
        json_builder_set_member_name (b, "aplica"); json_builder_add_boolean_value (b, FALSE);
        json_builder_set_member_name (b, "motivo"); json_builder_add_string_value (b,
            "Fuera de temporada (rige del 1 de mayo al 31 de agosto).");
    } else if (wd >= 5) {
        json_builder_set_member_name (b, "aplica"); json_builder_add_boolean_value (b, FALSE);
        json_builder_set_member_name (b, "motivo"); json_builder_add_string_value (b,
            "Fin de semana: sin restricción (rige lunes a viernes).");
    } else {
        const char *lvl = !strcmp (key, "alerta") ? "Alerta" : !strcmp (key, "preemergencia") ? "Preemergencia"
                        : !strcmp (key, "emergencia") ? "Emergencia" : "Normal";
        /* override con episodio OFICIAL de airerm (tabla DATADA de HOY, ventana
         * acotada). Solo sube el nivel a un episodio declarado; los dígitos siguen
         * del calendario. En día normal (oficial=false) manda el cálculo por aire. */
        gboolean oficial = FALSE; char *epi = NULL; gchar *out = NULL;
        if (g_spawn_command_line_sync (g_build_filename (clima_libexec (), "restriccion-scraper", NULL), &out, NULL, NULL, NULL) && out) {
            JsonParser *p = json_parser_new ();
            if (json_parser_load_from_data (p, out, -1, NULL)) {
                JsonObject *ro = json_node_get_object (json_parser_get_root (p));
                if (json_object_get_boolean_member_with_default (ro, "oficial", FALSE)) {
                    const char *e = json_object_get_string_member_with_default (ro, "episodio", "Normal");
                    if (!strcmp (e, "Preemergencia") || !strcmp (e, "Emergencia")) { oficial = TRUE; epi = g_strdup (e); }
                }
            }
            g_object_unref (p);
        }
        g_free (out);
        json_builder_set_member_name (b, "aplica");  json_builder_add_boolean_value (b, TRUE);
        json_builder_set_member_name (b, "level");   json_builder_add_string_value (b, oficial && epi ? epi : lvl);
        json_builder_set_member_name (b, "digitos"); json_builder_add_string_value (b, dig[wd]);
        json_builder_set_member_name (b, "oficial"); json_builder_add_boolean_value (b, oficial);
        g_free (epi);
    }
    json_builder_end_object (b);
}

/* --- suspensión de clases: dato OFICIAL de Mineduc (scraper C++) con fallback a
 *     estimación por pronóstico. Solo afirma "suspendidas" si la fuente lo dice. --- */
static void build_suspension (JsonBuilder *b, const ClimaLoc *loc, JsonObject *daily) {
    int peor = 0; char cuando[16] = ""; char motivo[64] = "";
    const char *lbl[] = {"hoy","mañana","pasado mañana"};
    for (guint i = 0; i < 3 && i < arr_len (daily, "time"); i++) {
        int code = (int) arr_num (daily, "weather_code", i);
        double gust = arr_num (daily, "wind_gusts_10m_max", i);
        double rain = arr_num (daily, "precipitation_sum", i);
        gboolean snow = (code>=71 && code<=77) || code==85 || code==86;
        gboolean storm = code>=95;
        int r = 0;
        if (gust >= 90 || rain >= 60 || storm || (snow && rain >= 20)) r = 2;
        else if (gust >= 65 || rain >= 30 || snow) r = 1;
        if (r > peor) {
            peor = r; g_strlcpy (cuando, lbl[i], sizeof cuando);
            if (storm) g_strlcpy (motivo, "tormenta eléctrica prevista", sizeof motivo);
            else if (snow) g_snprintf (motivo, sizeof motivo, "nieve (viento %d km/h)", (int) lround (gust));
            else g_snprintf (motivo, sizeof motivo, "viento %d km/h y %d mm de lluvia", (int) lround (gust), (int) lround (rain));
        }
    }
    const char *niveles[] = {"Sin aviso","Posible suspensión por temporal","Suspensión probable por temporal"};

    /* --- consulta OFICIAL a Mineduc (módulo scraper C++) --- */
    gboolean oficial = FALSE; char *fuente = NULL, *dia_susp = NULL;
    {
        gchar *reg = g_shell_quote (loc && loc->region ? loc->region : "");
        gchar *com = g_shell_quote (loc && loc->name ? loc->name : "");
        gchar *cmd = g_strdup_printf ("%s/suspension-scraper --region %s --comuna %s", clima_libexec (), reg, com);
        gchar *out = NULL;
        if (g_spawn_command_line_sync (cmd, &out, NULL, NULL, NULL) && out) {
            JsonParser *p = json_parser_new ();
            if (json_parser_load_from_data (p, out, -1, NULL)) {
                JsonObject *ro = json_node_get_object (json_parser_get_root (p));
                oficial = json_object_get_boolean_member_with_default (ro, "oficial", FALSE);
                if (json_object_has_member (ro, "dias")) {
                    JsonArray *dias = json_object_get_array_member (ro, "dias");
                    for (guint i = 0; dias && i < json_array_get_length (dias); i++) {
                        JsonObject *d = json_array_get_object_element (dias, i);
                        if (json_object_get_boolean_member_with_default (d, "suspendido", FALSE) && !dia_susp) {
                            dia_susp = g_strdup (json_object_get_string_member_with_default (d, "etiqueta", ""));
                            const char *fu = json_object_get_string_member_with_default (d, "fuente", "");
                            if (*fu) fuente = g_strdup (fu);
                        }
                    }
                }
            }
            g_object_unref (p);
        }
        g_free (out); g_free (cmd); g_free (reg); g_free (com);
    }

    json_builder_begin_object (b);
    if (dia_susp) {
        const char *ct = !strcmp (dia_susp,"hoy") ? "hoy"
                       : !strcmp (dia_susp,"manana") ? "mañana" : "pasado mañana";
        char niv[48]; g_snprintf (niv, sizeof niv, "Clases suspendidas %s", ct);
        json_builder_set_member_name (b, "nivel");  json_builder_add_string_value (b, niv);
        json_builder_set_member_name (b, "riesgo"); json_builder_add_int_value (b, 2);
        json_builder_set_member_name (b, "cuando"); json_builder_add_string_value (b, ct);
        json_builder_set_member_name (b, "motivo"); json_builder_add_string_value (b, "informado por el Ministerio de Educación");
        json_builder_set_member_name (b, "oficial"); json_builder_add_boolean_value (b, TRUE);
        json_builder_set_member_name (b, "fuente"); json_builder_add_string_value (b, fuente ? fuente : "https://www.mineduc.cl/");
    } else {
        json_builder_set_member_name (b, "nivel");  json_builder_add_string_value (b, niveles[peor]);
        json_builder_set_member_name (b, "riesgo"); json_builder_add_int_value (b, peor);
        json_builder_set_member_name (b, "cuando");
        if (peor) json_builder_add_string_value (b, cuando); else json_builder_add_null_value (b);
        json_builder_set_member_name (b, "motivo"); json_builder_add_string_value (b, peor ? motivo : "sin eventos severos previstos");
        json_builder_set_member_name (b, "oficial"); json_builder_add_boolean_value (b, oficial);
    }
    json_builder_end_object (b);
    g_free (fuente); g_free (dia_susp);
}

/* --- ensambla el objeto "ambiental" --- */
static void build_ambiental (JsonBuilder *b, const ClimaLoc *loc, JsonObject *daily) {
    char key[24];
    json_builder_begin_object (b);
      json_builder_set_member_name (b, "air");         build_air (b, loc, key, sizeof key);
      json_builder_set_member_name (b, "moon");        build_moon (b);
      json_builder_set_member_name (b, "tides");       build_tides (b, loc);
      json_builder_set_member_name (b, "restriccion"); build_restriccion (b, loc, key);
      json_builder_set_member_name (b, "suspension");  build_suspension (b, loc, daily);
    json_builder_end_object (b);
}
