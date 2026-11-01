/* Clima — app de clima estilo Apple Weather para Fedora (host en C).
 * GTK4 + libadwaita + WebKitGTK 6. La interfaz (HTML/CSS/JS) se reutiliza tal cual;
 * este host obtiene los datos (clima_data), los inyecta y gestiona ubicación,
 * notificaciones de lluvia y refresco. Kotone 2026-07-16. */
#include <adwaita.h>
#include <webkit/webkit.h>
#include <json-glib/json-glib.h>
#include <geoclue.h>
#include <curl/curl.h>
#include <string.h>
#include <locale.h>
#include "clima_data.h"

#ifndef CLIMA_ROOT
#define CLIMA_ROOT "."
#endif
#define REFRESH_MS (15 * 60 * 1000)

typedef struct {
    AdwApplication *app;
    GtkWindow      *win;
    WebKitWebView  *web;
    ClimaLoc       *loc;
    gboolean        busy;
    gboolean        manual;     /* el usuario eligió una ciudad: no re-localizar solo */
    gboolean        located;    /* ya se intentó ubicación automática en esta sesión */
    ClimaLoc       *pending;    /* pedido que llegó con una obtención en curso (se atiende al terminar) */
} App;
static App APP;

/* ------------------------------------------------- inyección a la interfaz */
static void inject (const char *fmt, const char *arg) {
    gchar *js = g_strdup_printf (fmt, arg);
    webkit_web_view_evaluate_javascript (APP.web, js, -1, NULL, NULL, NULL, NULL, NULL);
    g_free (js);
}
/* Cadena C → literal JSON válido para JS (g_strescape pasaba los acentos a octal y salían rotos). */
static gchar *js_string (const char *s) {
    JsonNode *n = json_node_new (JSON_NODE_VALUE);
    json_node_set_string (n, s ? s : "");
    gchar *out = json_to_string (n, FALSE);
    json_node_free (n);
    return out;
}
/* Miembros del puente con valor por defecto (un mensaje incompleto no debe tumbar la app). */
static const char *jstr (JsonObject *o, const char *k) {
    JsonNode *n = json_object_get_member (o, k);
    if (!n || JSON_NODE_TYPE (n) != JSON_NODE_VALUE || json_node_get_value_type (n) != G_TYPE_STRING) return "";
    return json_node_get_string (n);
}
static double jnum (JsonObject *o, const char *k) {
    JsonNode *n = json_object_get_member (o, k);
    return (n && JSON_NODE_TYPE (n) == JSON_NODE_VALUE) ? json_node_get_double (n) : 0;
}

/* Notificaciones (lluvia/suspensión/mal tiempo/pronóstico) ya NO viven en el GUI:
 * se disparan por timers de usuario (clima-notify.sh). Ver clima-check/forecast.timer. */

/* ------------------------------------------------- obtención en 2º plano */
typedef struct { ClimaLoc *loc; gboolean locate; } Job;
typedef struct { gchar *json; gchar *err; ClimaLoc *loc; } Res;

static void do_locate (void);
static void start_fetch (ClimaLoc *loc, gboolean locate);
static gboolean locate_idle (gpointer u) { (void) u; do_locate (); return G_SOURCE_REMOVE; }
static void do_locate_later (void) { g_idle_add (locate_idle, NULL); }

static gboolean on_fetched (gpointer data) {
    Res *r = data;
    APP.busy = FALSE;
    if (r->json) {
        if (APP.loc) clima_loc_free (APP.loc);
        APP.loc = r->loc; r->loc = NULL;
        inject ("window.onClimaData(%s)", r->json);
        if (!APP.located && !APP.manual) { APP.located = TRUE; do_locate_later (); }
        /* Notificaciones movidas FUERA de la app a timers de usuario livianos
         * (clima-notify.sh + clima-check/forecast.timer): suspensión de clases,
         * alerta de mal tiempo y pronóstico de mañana 22:00. No se notifica desde el GUI. */
    } else {
        gchar *arg = js_string (r->err ? r->err : "error");
        inject ("window.onError(%s)", arg);
        g_free (arg);
    }
    if (r->loc) clima_loc_free (r->loc);
    g_free (r->json); g_free (r->err); g_free (r);
    /* Una ciudad elegida mientras se obtenía otra cosa: se atiende ahora (antes se perdía). */
    if (APP.pending) { ClimaLoc *p = APP.pending; APP.pending = NULL; start_fetch (p, FALSE); }
    return G_SOURCE_REMOVE;
}
static gpointer fetch_thread (gpointer data) {
    Job *job = data;
    ClimaLoc *loc = job->locate ? clima_current_location () : job->loc;
    Res *r = g_new0 (Res, 1);
    r->loc = loc;
    if (loc) r->json = clima_analyze_json (loc, &r->err);
    else r->err = g_strdup ("sin ubicación");
    g_free (job);
    g_idle_add (on_fetched, r);
    return NULL;
}
static void start_fetch (ClimaLoc *loc, gboolean locate) {
    if (APP.busy) {
        /* Sólo se guarda un pedido explícito con ciudad; los refrescos automáticos sobran. */
        if (loc) { if (APP.pending) clima_loc_free (APP.pending); APP.pending = loc; }
        return;
    }
    APP.busy = TRUE;
    Job *job = g_new0 (Job, 1); job->loc = loc; job->locate = locate;
    g_thread_unref (g_thread_new ("clima-fetch", fetch_thread, job));
}

/* --------------------------------- ubicación vía GeoClue (servicio de GNOME) */
static GCancellable *geo_cancel = NULL;
static guint         geo_tid = 0;
static gboolean      geo_pending = FALSE;   /* evita doble resolución (GeoClue vs timeout) */
static void geo_done (GObject *src, GAsyncResult *res, gpointer u) {
    (void) src; (void) u;
    GError *err = NULL;
    GClueSimple *simple = gclue_simple_new_finish (res, &err);
    if (!geo_pending) {                     /* ya resolvió el timeout → descartar (sin fugas) */
        g_clear_object (&simple); g_clear_error (&err);
        return;
    }
    geo_pending = FALSE;
    if (geo_tid) { g_source_remove (geo_tid); geo_tid = 0; }
    if (simple) {
        GClueLocation *loc = gclue_simple_get_location (simple);
        if (loc) {
            ClimaLoc *cl = clima_loc_from_coords (gclue_location_get_latitude (loc),
                                                  gclue_location_get_longitude (loc));
            g_object_unref (simple);
            start_fetch (cl, FALSE);
            return;
        }
        g_object_unref (simple);
    }
    if (err) g_error_free (err);
    start_fetch (NULL, TRUE);                /* GeoClue no dio ubicación → IP */
}
static gboolean geo_timeout (gpointer u) {
    (void) u; geo_tid = 0;
    if (!geo_pending) return G_SOURCE_REMOVE;
    geo_pending = FALSE;                      /* GeoClue tardó demasiado → IP directo */
    if (geo_cancel) g_cancellable_cancel (geo_cancel);
    start_fetch (NULL, TRUE);
    return G_SOURCE_REMOVE;
}
static void do_locate (void) {
    if (geo_pending) return;
    if (APP.busy) { g_timeout_add_seconds (2, locate_idle, NULL); return; }
    g_clear_object (&geo_cancel);
    geo_cancel = g_cancellable_new ();
    geo_pending = TRUE;
    geo_tid = g_timeout_add_seconds (6, geo_timeout, NULL);
    gclue_simple_new ("io.github.navarretenoir.Clima", GCLUE_ACCURACY_LEVEL_CITY, geo_cancel, geo_done, NULL);
}

/* recuadro «Metro»: estado oficial en un hilo (no bloquea la ventana) → window.onMetro(json) */
static gboolean metro_listo (gpointer d) { inject ("window.onMetro&&window.onMetro(%s)", d); g_free (d); return G_SOURCE_REMOVE; }
static gpointer metro_hilo (gpointer u) { (void) u; g_idle_add (metro_listo, clima_metro_json ()); return NULL; }

/* --------------------------------------------------------- puente JS→C */
static void on_bridge (WebKitUserContentManager *ucm, JSCValue *value, gpointer u) {
    (void) ucm; (void) u;
    gchar *msg = jsc_value_to_string (value);
    JsonParser *jp = json_parser_new ();
    if (msg && json_parser_load_from_data (jp, msg, -1, NULL) &&
        JSON_NODE_HOLDS_OBJECT (json_parser_get_root (jp))) {
        JsonObject *o = json_node_get_object (json_parser_get_root (jp));
        const char *action = jstr (o, "action");
        if (!strcmp (action, "search")) {
            gchar *res = clima_search_json (jstr (o, "q"), 8);
            inject ("window.onSearchResults(%s)", res);
            g_free (res);
        } else if (!strcmp (action, "city") && *jstr (o, "name")) {
            ClimaLoc *l = g_new0 (ClimaLoc, 1);
            l->name = g_strdup (jstr (o, "name"));
            l->region = g_strdup (jstr (o, "region"));
            l->lat = jnum (o, "lat");
            l->lon = jnum (o, "lon");
            l->is_comuna = TRUE;
            ClimaLoc *near = clima_geocode (l->name);       /* recupera rm_restr */
            if (near) { l->rm_restr = near->rm_restr; clima_loc_free (near); }
            APP.manual = TRUE;
            start_fetch (l, FALSE);
        } else if (!strcmp (action, "locate")) {
            APP.manual = FALSE;
            do_locate ();                       /* GeoClue (GNOME) → IP */
        } else if (!strcmp (action, "metro")) {
            g_thread_unref (g_thread_new ("clima-metro", metro_hilo, NULL));
        } else if (!strcmp (action, "abrirMetro")) {
            g_spawn_command_line_async ("metro-chile", NULL);
        } else if (!strcmp (action, "refresh")) {
            if (APP.loc) start_fetch (clima_loc_copy (APP.loc), FALSE);
            else do_locate ();
        }
    }
    g_object_unref (jp); g_free (msg);
}

/* ----------------------------------------------------------- ciclo de vida */
static void on_load (WebKitWebView *web, WebKitLoadEvent ev, gpointer u) {
    (void) web; (void) u;
    if (ev != WEBKIT_LOAD_FINISHED) return;
    /* CACHE: pinta de inmediato el último dato guardado (cached:true) para que la
     * app abra al tiro; luego se refresca desde las fuentes y reemplaza. */
    gchar *cached = clima_cache_load ();
    if (cached) {
        inject ("window.onClimaData(%s)", cached);
        /* REFRESCO AL ABRIR: reusar la ubicación que viene en el caché y traer datos
         * frescos YA, sin esperar a GeoClue (que en este host tarda o no responde, y
         * era la razón de que el dato no se actualizara tras el inicio). */
        if (!APP.loc) {
            JsonParser *jp = json_parser_new ();
            JsonNode *rn = json_parser_load_from_data (jp, cached, -1, NULL) ? json_parser_get_root (jp) : NULL;
            JsonObject *root = rn && JSON_NODE_HOLDS_OBJECT (rn) ? json_node_get_object (rn) : NULL;
            JsonNode *ln = root ? json_object_get_member (root, "location") : NULL;
            if (ln && JSON_NODE_HOLDS_OBJECT (ln)) {
                JsonObject *lo = json_node_get_object (ln);
                if (*jstr (lo, "name")) {
                    ClimaLoc *l = g_new0 (ClimaLoc, 1);
                    l->name   = g_strdup (jstr (lo, "name"));
                    l->region = g_strdup (jstr (lo, "region"));
                    l->lat = jnum (lo, "lat");
                    l->lon = jnum (lo, "lon");
                    l->is_comuna = json_object_get_boolean_member_with_default (lo, "comuna", FALSE);
                    l->rm_restr  = json_object_get_boolean_member_with_default (lo, "rm_restr", FALSE);
                    APP.loc = l;
                    start_fetch (clima_loc_copy (APP.loc), FALSE);
                }
            }
            g_object_unref (jp);
        }
        g_free (cached);
    }
    if (!APP.loc && !APP.busy)
        do_locate ();       /* primer arranque sin caché: ubicación vía GeoClue (GNOME) */
}
static gboolean on_refresh_timer (gpointer u) {
    (void) u;
    if (!APP.manual) do_locate ();                       /* me moví? → GeoClue/IP */
    else if (APP.loc) start_fetch (clima_loc_copy (APP.loc), FALSE);
    return G_SOURCE_CONTINUE;
}
static gboolean on_key (GtkEventControllerKey *k, guint keyval, guint code, GdkModifierType st, gpointer u) {
    (void) k; (void) code; (void) st; (void) u;
    if (keyval == GDK_KEY_F11) {
        if (gtk_window_is_fullscreen (APP.win)) gtk_window_unfullscreen (APP.win);
        else gtk_window_fullscreen (APP.win);
        return TRUE;
    }
    if (keyval == GDK_KEY_Escape && gtk_window_is_fullscreen (APP.win)) {
        gtk_window_unfullscreen (APP.win); return TRUE;
    }
    return FALSE;
}

static void activate (GtkApplication *app, gpointer u) {
    (void) u;
    /* GTK activa el locale del sistema (es_CL usa coma decimal); forzamos punto para que
     * printf("%.4f") arme URLs válidas (latitude=-33.45, no -33,45). El display va por JS. */
    setlocale (LC_NUMERIC, "C");
    AdwApplicationWindow *win = ADW_APPLICATION_WINDOW (adw_application_window_new (app));
    APP.win = GTK_WINDOW (win);
    gtk_window_set_title (APP.win, "Clima");
    gtk_window_set_default_size (APP.win, 430, 880);

    APP.web = WEBKIT_WEB_VIEW (webkit_web_view_new ());
    WebKitUserContentManager *ucm = webkit_web_view_get_user_content_manager (APP.web);
    webkit_user_content_manager_register_script_message_handler (ucm, "bridge", NULL);
    g_signal_connect (ucm, "script-message-received::bridge", G_CALLBACK (on_bridge), NULL);
    g_signal_connect (APP.web, "load-changed", G_CALLBACK (on_load), NULL);

    /* Rendimiento: aceleración por hardware SIEMPRE (compositing en GPU) — clave para que
     * el canvas de SkyFX fluya en este equipo. La GPU la elige el lanzador (DRI_PRIME).
     * 2026-10-06: con WebKit 2.54 ALWAYS dejaba el WebProcess en 1,5 GB y colgado (vista congelada,
     sin animaciones) → por defecto NEVER; CLIMA_HWACCEL=always lo reactiva. */
    {
        WebKitSettings *st = webkit_web_view_get_settings (APP.web);
        const char *hw = g_getenv ("CLIMA_HWACCEL");
        webkit_settings_set_hardware_acceleration_policy (st,
            (hw && g_strcmp0 (hw, "always") == 0) ? WEBKIT_HARDWARE_ACCELERATION_POLICY_ALWAYS
                                                 : WEBKIT_HARDWARE_ACCELERATION_POLICY_NEVER);
    }

    AdwHeaderBar *header = ADW_HEADER_BAR (adw_header_bar_new ());
    gtk_widget_add_css_class (GTK_WIDGET (header), "flat");
    adw_header_bar_set_title_widget (header, adw_window_title_new ("Clima", ""));
    AdwToolbarView *tv = ADW_TOOLBAR_VIEW (adw_toolbar_view_new ());
    adw_toolbar_view_add_top_bar (tv, GTK_WIDGET (header));
    adw_toolbar_view_set_content (tv, GTK_WIDGET (APP.web));
    adw_toolbar_view_set_top_bar_style (tv, ADW_TOOLBAR_FLAT);
    adw_application_window_set_content (win, GTK_WIDGET (tv));

    GtkEventController *key = gtk_event_controller_key_new ();
    g_signal_connect (key, "key-pressed", G_CALLBACK (on_key), NULL);
    gtk_widget_add_controller (GTK_WIDGET (win), key);

    gchar *idx = g_build_filename (clima_root (), "ui", "index.html", NULL);
    gchar *uri = g_filename_to_uri (idx, NULL, NULL);
    webkit_web_view_load_uri (APP.web, uri);
    g_free (uri); g_free (idx);

    g_timeout_add (REFRESH_MS, on_refresh_timer, NULL);
    gtk_window_present (APP.win);
}

int main (int argc, char **argv) {
    curl_global_init (CURL_GLOBAL_DEFAULT);   /* obligatorio antes de usar libcurl en hilos */
    /* Sin "--gui" → comando de consola (reporte / --json / --list). Con "--gui" → app gráfica. */
    gboolean gui = FALSE;
    for (int i = 1; i < argc; i++) if (!strcmp (argv[i], "--gui")) gui = TRUE;
    if (!gui) return clima_cli_run (argc, argv);

    APP.app = adw_application_new ("io.github.navarretenoir.Clima", G_APPLICATION_DEFAULT_FLAGS);
    g_signal_connect (APP.app, "activate", G_CALLBACK (activate), NULL);
    int status = g_application_run (G_APPLICATION (APP.app), 1, argv);  /* no pasar --gui al parser */
    g_object_unref (APP.app);
    return status;
}
