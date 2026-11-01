/* clima_cli — el comando de consola `clima`. Reporte ordenado con color y salida --json
 * para que otras apps consuman los datos. Reutiliza clima_data (mismo JSON que la GUI). */
#include "clima_data.h"
#include <json-glib/json-glib.h>
#include <stdio.h>
#include <string.h>
#include <math.h>

/* --- pequeños accesores json-glib --- */
static JsonObject *O  (JsonObject *o, const char *k){ return json_object_get_object_member (o,k); }
static double      N  (JsonObject *o, const char *k){ return o&&json_object_has_member(o,k)&&!JSON_NODE_HOLDS_NULL(json_object_get_member(o,k))?json_object_get_double_member(o,k):0; }
static const char *S  (JsonObject *o, const char *k){ return o&&json_object_has_member(o,k)&&!JSON_NODE_HOLDS_NULL(json_object_get_member(o,k))?json_object_get_string_member(o,k):""; }
static gboolean    HAS(JsonObject *o, const char *k){ return o&&json_object_has_member(o,k)&&!JSON_NODE_HOLDS_NULL(json_object_get_member(o,k)); }
static double AN(JsonObject *o,const char*k,guint i){ if(!HAS(o,k))return 0; JsonArray*a=json_object_get_array_member(o,k); if(i>=json_array_get_length(a))return 0; JsonNode*n=json_array_get_element(a,i); return JSON_NODE_HOLDS_NULL(n)?0:json_node_get_double(n); }
static const char *AS(JsonObject *o,const char*k,guint i){ if(!HAS(o,k))return ""; JsonArray*a=json_object_get_array_member(o,k); if(i>=json_array_get_length(a))return ""; return json_node_get_string(json_array_get_element(a,i)); }
static guint AL(JsonObject *o,const char*k){ return HAS(o,k)?json_array_get_length(json_object_get_array_member(o,k)):0; }

static void hhmm (const char *iso, char *out) { /* "…THH:MM" -> "HH:MM" */
    const char *t = strchr (iso, 'T');
    if (t && strlen (t) >= 6) { out[0]=t[1];out[1]=t[2];out[2]=':';out[3]=t[4];out[4]=t[5];out[5]=0; }
    else g_strlcpy (out, "--:--", 6);
}
static const char *wmo_icon (int code) {
    struct { int c; const char *i; } M[] = {{0,"☀️"},{1,"🌤️"},{2,"⛅"},{3,"☁️"},{45,"🌫️"},{48,"🌫️"},
      {51,"🌦️"},{53,"🌦️"},{55,"🌧️"},{56,"🌧️"},{57,"🌧️"},{61,"🌧️"},{63,"🌧️"},{65,"🌧️"},{66,"🌧️"},{67,"🌧️"},
      {71,"🌨️"},{73,"🌨️"},{75,"❄️"},{77,"🌨️"},{80,"🌦️"},{81,"🌧️"},{82,"⛈️"},{85,"🌨️"},{86,"❄️"},{95,"⛈️"},{96,"⛈️"},{99,"⛈️"}};
    for (size_t i=0;i<G_N_ELEMENTS(M);i++) if(M[i].c==code) return M[i].i;
    return "☁️";
}
static const char *windrose (double d){ const char*R[]={"N","NE","E","SE","S","SO","O","NO"}; return R[((int)round(d/45))%8]; }
static const char *daynm (const char *iso){
    static const char*D[]={"lun","mar","mié","jue","vie","sáb","dom"};
    struct tm tm={0}; strptime(iso,"%Y-%m-%d",&tm); time_t t=timegm(&tm); struct tm g; gmtime_r(&t,&g);
    return D[(g.tm_wday+6)%7];
}

static void bar (double lo, double hi, double gmin, double gmax, char *out) {
    int W=18; double span = gmax-gmin; if(span<1)span=1;
    int a=(int)((lo-gmin)/span*W), b=(int)((hi-gmin)/span*W);
    if(a<0)a=0;
    if(a>W)a=W;
    if(b<=a)b=a+1;
    if(b>W)b=W;
    int p=0;
    for(int i=0;i<a;i++) out[p++]=' ';
    for(int i=a;i<b;i++){ memcpy(out+p,"━",3); p+=3; }   /* U+2501 = 3 bytes UTF-8 */
    for(int i=b;i<W;i++) out[p++]=' ';
    out[p]=0;
}

/* imprime el reporte a partir del JSON de análisis */
static void print_report (JsonObject *root) {
    JsonObject *loc=O(root,"location"), *cur=O(root,"current"), *tod=O(root,"today");
    JsonObject *raw=O(root,"raw"), *day=O(raw,"daily"), *hr=O(raw,"hourly");
    JsonObject *amb=HAS(root,"ambiental")?O(root,"ambiental"):NULL;

    const char *region=S(loc,"region");
    printf("\n  \033[1m%s  %s%s%s\033[0m\n", S(cur,"icon"), S(loc,"name"),
           *region?", ":"", region);
    printf("  \033[1m\033[38;5;123m%d°C\033[0m  %s   ·   máx %d°  mín %d°\n",
           (int)N(cur,"temp"), S(cur,"text"), (int)N(tod,"max"), (int)N(tod,"min"));
    printf("  Sensación %d°   ·   humedad %d%%   ·   viento %d km/h %s\n",
           (int)N(cur,"feels"), (int)N(cur,"humidity"), (int)N(cur,"wind"), windrose(N(cur,"wind_dir")));
    char sr[6], ss[6]; hhmm(S(tod,"sunrise"),sr); hhmm(S(tod,"sunset"),ss);
    printf("  Amanecer %s   ·   Atardecer %s   ·   UV %g   ·   presión %d hPa\n",
           sr, ss, N(tod,"uv"), (int)N(cur,"pressure"));
    { time_t tt=time(NULL); struct tm lt; localtime_r(&tt,&lt);
      double pp_hoy = HAS(day,"precipitation_sum") ? AN(day,"precipitation_sum",0) : 0;
      double vis = HAS(hr,"visibility") ? AN(hr,"visibility",lt.tm_hour)/1000.0 : 0;
      printf("  Precipitación hoy %.1f mm   ·   punto de rocío %d°   ·   visibilidad %.1f km\n",
             pp_hoy, (int)N(cur,"dew"), vis); }
    if (HAS(root,"rain"))
        printf("  \033[38;5;39m💧 %s\033[0m\n", S(O(root,"rain"),"summary"));

    /* próximas horas */
    printf("\n  \033[2mPRÓXIMAS HORAS\033[0m\n  ");
    guint start=0; time_t now=time(NULL);
    for (guint i=0;i<AL(hr,"time");i++){ struct tm tm={0}; strptime(AS(hr,"time",i),"%Y-%m-%dT%H:%M",&tm);
        if (timegm(&tm) >= now-3600) { start=i; break; } }
    for (guint i=start;i<start+12 && i<AL(hr,"time");i++){
        const char *iso=AS(hr,"time",i); char hh[3]={iso[11],iso[12],0};
        printf("%sh %s%3d°   ", hh, wmo_icon((int)AN(hr,"weather_code",i)), (int)round(AN(hr,"temperature_2m",i)));
    }
    printf("\n");

    /* 10 días */
    printf("\n  \033[2mPRONÓSTICO 10 DÍAS\033[0m\n");
    double gmin=1e9,gmax=-1e9;
    for(guint i=0;i<AL(day,"time");i++){ double lo=AN(day,"temperature_2m_min",i),hi=AN(day,"temperature_2m_max",i);
        if(lo<gmin)gmin=lo;
        if(hi>gmax)gmax=hi; }
    for(guint i=0;i<AL(day,"time");i++){
        const char *iso=AS(day,"time",i);
        char nm[8]; if(i==0) g_strlcpy(nm,"Hoy",sizeof nm); else { g_strlcpy(nm,daynm(iso),sizeof nm); nm[0]=g_ascii_toupper(nm[0]); }
        int pp=(int)AN(day,"precipitation_probability_max",i);
        int lo=(int)round(AN(day,"temperature_2m_min",i)), hi=(int)round(AN(day,"temperature_2m_max",i));
        char b[80]; bar(lo,hi,gmin,gmax,b);
        char pps[24]; if(pp>=20) g_snprintf(pps,sizeof pps,"\033[38;5;39m%3d%%\033[0m",pp); else g_strlcpy(pps,"    ",sizeof pps);
        printf("  %-4s %s  %s  %3d° \033[38;5;240m%s\033[0m %3d°\n", nm, wmo_icon((int)AN(day,"weather_code",i)), pps, lo, b, hi);
    }

    /* ambiental */
    if (amb) {
        printf("\n  \033[2mAMBIENTAL\033[0m\n");
        if (HAS(amb,"air")) { JsonObject *a=O(amb,"air");
            printf("  Calidad del aire:  %s  (MP2,5 %g µg/m³ prom. 24h · actual %g)\n",
                   S(a,"level"), N(a,"mean24"), N(a,"pm25")); }
        if (HAS(amb,"restriccion")) { JsonObject *r=O(amb,"restriccion");
            if (json_object_get_boolean_member(r,"aplica"))
                printf("  Restricción vehic.: dígitos \033[1m%s\033[0m (sello verde) · episodio %s\n", S(r,"digitos"), S(r,"level"));
            else printf("  Restricción vehic.: %s\n", S(r,"motivo")); }
        if (HAS(amb,"suspension")) { JsonObject *s=O(amb,"suspension");
            const char *nivel = S(s,"nivel");
            gboolean of = HAS(s,"oficial") && json_object_get_boolean_member(s,"oficial");
            gboolean es_susp = strstr(nivel,"suspendidas") != NULL;
            const char *nivel_txt = strcmp(nivel,"Sin aviso")==0 ? "Sin aviso de suspensión" : nivel;
            printf("  Suspensión clases:  %s · %s\n", nivel_txt,
                   (of && es_susp) ? "Confirmado por Mineduc" : "Estimación por pronóstico, sin confirmación de Mineduc"); }
        if (HAS(amb,"moon")) { JsonObject *m=O(amb,"moon");
            printf("  Luna:  %s %s (%d%% iluminada)\n", S(m,"emoji"), S(m,"name"), (int)N(m,"illumination")); }
        if (HAS(amb,"tides")) { JsonObject *t=O(amb,"tides");
            char lbl[96]; if (json_object_get_boolean_member(t,"coastal")) g_strlcpy(lbl,S(t,"port"),sizeof lbl);
            else g_snprintf(lbl,sizeof lbl,"%s (costa más cercana, %d km)", S(t,"port"), (int)N(t,"dist_km"));
            char ht[6]="--:--", lt[6]="--:--"; double hv=0,lv=0;
            if (HAS(t,"next_high")){ JsonArray*a=json_object_get_array_member(t,"next_high"); hhmm(json_array_get_string_element(a,0),ht); hv=json_array_get_double_element(a,1); }
            if (HAS(t,"next_low")){ JsonArray*a=json_object_get_array_member(t,"next_low"); hhmm(json_array_get_string_element(a,0),lt); lv=json_array_get_double_element(a,1); }
            printf("  Marea en %s:  ▲ pleamar %s (%.2f m) · ▼ bajamar %s (%.2f m)\n", lbl, ht, hv, lt, lv); }
    }
    char nowh[6]; time_t tt=time(NULL); struct tm lt; localtime_r(&tt,&lt); g_snprintf(nowh,sizeof nowh,"%02d:%02d",lt.tm_hour,lt.tm_min);
    printf("\n  \033[2mDatos: Open-Meteo · aire/mareas Open-Meteo · episodios GEC-MMA · restricción MTT · %s\033[0m\n\n", nowh);
}

static void clima_cli_usage (void) {
    printf(
      "\n  \033[1mclima\033[0m %s — el tiempo en Chile, por comuna.\n\n"
      "  clima                    tu ubicación actual (GeoClue/IP)\n"
      "  clima <comuna>           una comuna o ciudad (ej: clima Valdivia)\n"
      "  clima --here             forzar la ubicación actual\n"
      "  clima --json [<comuna>]  salida JSON completa\n"
      "  clima --list <texto>     listar comunas/ciudades que coincidan\n"
      "  clima --version | -v     versión\n"
      "  clima --help    | -h     esta ayuda\n\n", CLIMA_VERSION);
}

/* entrada de consola: devuelve el código de salida del proceso */
int clima_cli_run (int argc, char **argv) {
    const char *city = NULL; gboolean want_json=FALSE, here=FALSE; const char *list=NULL;
    GString *cityparts = g_string_new (NULL);
    for (int i=1;i<argc;i++){
        if (!strcmp(argv[i],"--json")) want_json=TRUE;
        else if (!strcmp(argv[i],"--here")) here=TRUE;
        else if (!strcmp(argv[i],"--list") && i+1<argc) list=argv[++i];
        else if (!strcmp(argv[i],"--gui")) {}
        else if (!strcmp(argv[i],"--version") || !strcmp(argv[i],"-v")) {
            printf("clima %s\n", CLIMA_VERSION); g_string_free(cityparts,TRUE); return 0;
        }
        else if (!strcmp(argv[i],"--help") || !strcmp(argv[i],"-h")) {
            clima_cli_usage (); g_string_free(cityparts,TRUE); return 0;
        }
        /* Una opción desconocida NO es el nombre de una comuna: si cae al else de abajo
           se geocodifica y termina en "No encontré esa ubicación", que confunde. */
        else if (argv[i][0]=='-' && argv[i][1]) {
            fprintf(stderr,"clima: opción desconocida: %s\n", argv[i]);
            clima_cli_usage (); g_string_free(cityparts,TRUE); return 2;
        }
        else { if(cityparts->len) g_string_append_c(cityparts,' '); g_string_append(cityparts,argv[i]); }
    }
    if (cityparts->len) city = cityparts->str;

    if (list) {
        gchar *j = clima_search_json (list, 20);
        JsonParser *p=json_parser_new();
        if (json_parser_load_from_data(p,j,-1,NULL)){ JsonArray*a=json_node_get_array(json_parser_get_root(p));
            for(guint i=0;i<json_array_get_length(a);i++){ JsonObject*o=json_array_get_object_element(a,i);
                printf("%-24s %s\n", S(o,"name"), S(o,"region")); } }
        g_object_unref(p); g_free(j); g_string_free(cityparts,TRUE); return 0;
    }

    ClimaLoc *loc = (city && !here) ? clima_geocode (city) : clima_current_location ();
    if (!loc) { fprintf(stderr,"No encontré esa ubicación.\n"); g_string_free(cityparts,TRUE); return 2; }

    gchar *err=NULL; gchar *json = clima_analyze_json (loc, &err);
    int rc = 0;
    if (!json) { fprintf(stderr,"No pude obtener el clima (%s).\n", err?err:"?"); rc=1; }
    else if (want_json) { printf("%s\n", json); }
    else {
        JsonParser *p=json_parser_new();
        if (json_parser_load_from_data(p,json,-1,NULL)) print_report(json_node_get_object(json_parser_get_root(p)));
        g_object_unref(p);
    }
    clima_loc_free(loc); g_free(json); g_free(err); g_string_free(cityparts,TRUE);
    return rc;
}
