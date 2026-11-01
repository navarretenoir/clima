// suspension_scraper — módulo scraper (C++) para CLIMA.
// Lee el feed oficial del MINEDUC (WordPress RSS), busca la noticia de
// "suspensión de clases" vigente para HOY (hora de Santiago) y verifica si la
// región/comuna de la usuaria está cubierta. Devuelve un veredicto JSON a stdout.
//
// SEGURIDAD (carve-out factual, anti-confabulación): SOLO afirma "suspendido" si
// (a) la nota contiene una fecha (con RANGOS: "21 y 22", "del 21 al 23") que cubre
// el día objetivo y (b) la COMUNA pedida aparece, o la REGIÓN aparece como alcance
// AMPLIO (no como una enumeración de comunas específicas que excluya a la usuaria).
// Siempre entrega la fuente. Si no puede confirmar, suspendido=false. Nunca inventa.
//
// FIX 2026-07-21 (Kotone): el scraper marcaba RM=suspendido por una nota que en
//   realidad solo suspendía Tiltil y San José de Maipo ("…en las comunas de tiltil
//   y san jose de maipo, en la región metropolitana"). Causa: matcheaba el token de
//   región aunque el texto enumerara comunas. Ahora: (1) se botan <script>/<style>
//   (ruido: dropdowns JS, menús con las 16 regiones); (2) matcheo COMUNA-primero;
//   (3) la región solo cuenta si es alcance AMPLIO (guarda antes+después del token,
//   cortando en la 1ª conjunción " y " para no confundir alcances hermanos, p.ej.
//   "…región de Coquimbo y la provincia de Huasco…"); (4) fechas con rangos.
//
// Uso:  suspension-scraper --region "<región>" [--comuna "<comuna>"]
// Compila: g++ -O2 -std=c++17 suspension_scraper.cpp -lcurl -o suspension-scraper
#include <curl/curl.h>
#include <string>
#include <vector>
#include <ctime>
#include <cstdio>
#include <cstring>
#include <cctype>
#include <algorithm>

static const char* FEED = "https://www.mineduc.cl/feed/";

static size_t wr(void* p, size_t s, size_t n, void* u) {
    ((std::string*)u)->append((char*)p, s * n); return s * n;
}
static std::string http_get(const std::string& url) {
    CURL* c = curl_easy_init(); std::string out;
    if (!c) return out;
    curl_easy_setopt(c, CURLOPT_URL, url.c_str());
    curl_easy_setopt(c, CURLOPT_WRITEFUNCTION, wr);
    curl_easy_setopt(c, CURLOPT_WRITEDATA, &out);
    curl_easy_setopt(c, CURLOPT_FOLLOWLOCATION, 1L);
    curl_easy_setopt(c, CURLOPT_TIMEOUT, 8L);
    curl_easy_setopt(c, CURLOPT_CONNECTTIMEOUT, 5L);
    curl_easy_setopt(c, CURLOPT_USERAGENT, "clima-suspension/1.0 (+local)");
    if (curl_easy_perform(c) != CURLE_OK) out.clear();
    curl_easy_cleanup(c);
    return out;
}

// minúsculas + quita tildes/ñ (UTF-8) → texto de cotejo
static std::string norm(const std::string& in) {
    std::string o; o.reserve(in.size());
    for (size_t i = 0; i < in.size(); ) {
        unsigned char ch = in[i];
        if (ch < 0x80) { o += (char)std::tolower(ch); i++; continue; }
        if (i + 1 < in.size()) {
            unsigned char c2 = in[i + 1];
            unsigned int cp = ((ch & 0x1F) << 6) | (c2 & 0x3F);
            switch (cp) {
                case 0xE1: case 0xE0: o += 'a'; i += 2; continue; // á à
                case 0xE9: case 0xE8: o += 'e'; i += 2; continue; // é è
                case 0xED: case 0xEC: o += 'i'; i += 2; continue; // í ì
                case 0xF3: case 0xF2: o += 'o'; i += 2; continue; // ó ò
                case 0xFA: case 0xF9: o += 'u'; i += 2; continue; // ú ù
                case 0xFC: o += 'u'; i += 2; continue;            // ü
                case 0xF1: o += 'n'; i += 2; continue;            // ñ
                case 0xC1: case 0xC0: o += 'a'; i += 2; continue;
                case 0xC9: case 0xC8: o += 'e'; i += 2; continue;
                case 0xCD: o += 'i'; i += 2; continue;
                case 0xD3: o += 'o'; i += 2; continue;
                case 0xDA: o += 'u'; i += 2; continue;
                case 0xD1: o += 'n'; i += 2; continue;
            }
        }
        i++; // otros multibyte: descartar
    }
    return o;
}
// bota bloques <tag>…</tag> (case-insensitive) — para <script>/<style> (ruido).
static std::string remove_tag_blocks(const std::string& h, const std::string& tag) {
    std::string low(h.size(), ' ');
    for (size_t i = 0; i < h.size(); i++) low[i] = (char)std::tolower((unsigned char)h[i]);
    std::string out; out.reserve(h.size());
    std::string open = "<" + tag, close = "</" + tag;
    size_t i = 0;
    while (i < h.size()) {
        size_t a = low.find(open, i);
        if (a == std::string::npos) { out += h.substr(i); break; }
        out += h.substr(i, a - i);
        size_t c = low.find(close, a);
        if (c == std::string::npos) break;            // sin cierre → botar el resto
        size_t gt = low.find('>', c);
        i = (gt == std::string::npos) ? h.size() : gt + 1;
    }
    return out;
}
static std::string strip_tags(const std::string& html) {
    std::string o; bool in = false;
    for (char c : html) { if (c == '<') in = true; else if (c == '>') in = false; else if (!in) o += c; }
    return o;
}
static bool has(const std::string& hay, const std::string& needle) {
    return hay.find(needle) != std::string::npos;
}
static std::string json_escape(const std::string& s) {
    std::string o; for (char c : s) { if (c == '"' || c == '\\') { o += '\\'; o += c; } else if (c == '\n') o += ' '; else o += c; } return o;
}

// token canónico de región chilena a partir del texto de región del usuario
static std::string region_token(const std::string& reg_norm) {
    static const std::vector<std::string> toks = {
        "arica", "tarapaca", "antofagasta", "atacama", "coquimbo", "valparaiso",
        "metropolitana", "o'higgins", "ohiggins", "libertador", "maule", "nuble",
        "biobio", "araucania", "los rios", "los lagos", "aysen", "magallanes"
    };
    for (auto& t : toks) if (has(reg_norm, t)) return t;
    return "";
}

// ¿el token de región aparece en `win` como ALCANCE AMPLIO (toda la región)?
// No cuenta si está ligado a una enumeración de comunas/provincia (antes, o después
// hasta la 1ª conjunción " y " — que ya introduce un alcance hermano distinto).
static bool has_comuna_marker(const std::string& s) {
    return s.find("comuna de") != std::string::npos
        || s.find("comunas de") != std::string::npos
        || s.find("provincia de") != std::string::npos;
}
static bool region_wide(const std::string& win, const std::string& rt) {
    if (rt.empty()) return false;
    size_t i = win.find(rt);
    if (i == std::string::npos) return false;
    size_t bs = i > 75 ? i - 75 : 0;
    std::string before = win.substr(bs, i - bs);
    std::string after = win.substr(i, std::min<size_t>(95, win.size() - i));
    size_t cut = after.find(" y ");
    std::string after_head = (cut == std::string::npos) ? after : after.substr(0, cut);
    if (has_comuna_marker(before) || has_comuna_marker(after_head)) return false;
    return true;
}

// posiciones de "de <mes>" cuyo grupo de días (incl. rangos "del A al B") cubre `dd`.
static std::vector<size_t> date_anchors(const std::string& text, int dd, const std::string& mes) {
    std::vector<size_t> res;
    std::string needle = "de " + mes;
    size_t p = 0;
    while ((p = text.find(needle, p)) != std::string::npos) {
        size_t s = p > 40 ? p - 40 : 0;
        std::string pre = text.substr(s, p - s);   // texto justo antes de "de <mes>"
        std::vector<int> nums;
        for (size_t i = 0; i < pre.size(); ) {
            if (isdigit((unsigned char)pre[i])) {
                int v = 0; size_t st = i;
                while (i < pre.size() && isdigit((unsigned char)pre[i]) && (i - st) < 2) { v = v * 10 + (pre[i] - '0'); i++; }
                if (v >= 1 && v <= 31) nums.push_back(v);
            } else i++;
        }
        bool covered = false;
        for (int v : nums) if (v == dd) { covered = true; break; }
        if (!covered && pre.find(" al ") != std::string::npos && nums.size() >= 2) {
            int lo = nums[0], hi = nums[0];
            for (int v : nums) { if (v < lo) lo = v; if (v > hi) hi = v; }
            if (dd >= lo && dd <= hi) covered = true;
        }
        if (covered) res.push_back(p);
        p += needle.size();
    }
    return res;
}

int main(int argc, char** argv) {
    std::string region, comuna;
    for (int i = 1; i < argc - 1; i++) {
        if (!strcmp(argv[i], "--region")) region = argv[++i];
        else if (!strcmp(argv[i], "--comuna")) comuna = argv[++i];
    }
    curl_global_init(CURL_GLOBAL_DEFAULT);

    std::string rt = region_token(norm(region));
    std::string cn = norm(comuna);

    // fechas objetivo: hoy y los 2 días siguientes (hora de Santiago)
    setenv("TZ", "America/Santiago", 1); tzset();
    static const char* meses[] = {"enero","febrero","marzo","abril","mayo","junio",
        "julio","agosto","septiembre","octubre","noviembre","diciembre"};
    static const char* etiquetas[] = {"hoy","manana","pasado"};
    struct Day { std::string etiqueta, iso, mes; int dd=0; bool susp=false; std::string alcance, fuente, titular; };
    std::vector<Day> days;
    time_t base = time(nullptr);
    for (int k = 0; k < 3; k++) {
        time_t t = base + (time_t)k * 86400; struct tm* lt = localtime(&t);
        Day d; d.etiqueta = etiquetas[k];
        d.dd = lt->tm_mday; d.mes = meses[lt->tm_mon];
        char iso[16]; snprintf(iso, sizeof iso, "%04d-%02d-%02d", lt->tm_year+1900, lt->tm_mon+1, lt->tm_mday);
        d.iso = iso;
        days.push_back(d);
    }

    // recolectar artículos de "suspensión de clases" del feed (una sola descarga c/u)
    struct Art { std::string link, title, text; };
    std::vector<Art> arts;
    bool oficial = false;
    std::string feed = http_get(FEED);
    if (!feed.empty()) {
        oficial = true;
        size_t pos = 0; int fetched = 0;
        while (fetched < 3) {
            size_t a = feed.find("<item", pos); if (a == std::string::npos) break;
            size_t b = feed.find("</item>", a); if (b == std::string::npos) break;
            std::string item = feed.substr(a, b - a); pos = b + 7;
            if (!(has(norm(item), "suspension") && has(norm(item), "clase"))) continue;
            size_t lp = item.find("<link>");
            std::string link = (lp != std::string::npos) ? item.substr(lp+6, item.find("</link>", lp)-(lp+6)) : "";
            size_t tp = item.find("<title>");
            std::string title = (tp != std::string::npos) ? item.substr(tp+7, item.find("</title>", tp)-(tp+7)) : "";
            // bota <script>/<style> (dropdowns JS, menús con las 16 regiones) antes de aplanar
            std::string page = remove_tag_blocks(remove_tag_blocks(http_get(link), "script"), "style");
            std::string text = norm(strip_tags(page));
            arts.push_back({link, title, text}); fetched++;
        }
    }

    // por cada día objetivo: COMUNA-primero; la REGIÓN solo si es alcance amplio.
    // El match de fecha soporta rangos ("21 y 22", "del 21 al 23 de julio").
    for (auto& d : days) {
        for (auto& ar : arts) {
            std::string tnorm = norm(ar.title);
            // camino TÍTULO
            if (!date_anchors(tnorm, d.dd, d.mes).empty()) {
                if (!cn.empty() && cn.size() >= 4 && has(tnorm, cn)) { d.susp = true; d.alcance = "comuna"; }
                else if (region_wide(tnorm, rt))                    { d.susp = true; d.alcance = "region"; }
            }
            // camino CUERPO: ventana alrededor de cada ancla de fecha
            if (!d.susp) {
                for (size_t p : date_anchors(ar.text, d.dd, d.mes)) {
                    size_t s = p > 140 ? p - 140 : 0;
                    std::string win = ar.text.substr(s, 360);
                    if (!cn.empty() && cn.size() >= 4 && has(win, cn)) { d.susp = true; d.alcance = "comuna"; }
                    else if (region_wide(win, rt))                    { d.susp = true; d.alcance = "region"; }
                    if (d.susp) break;
                }
            }
            if (d.susp) { d.fuente = ar.link; d.titular = ar.title; break; }
        }
    }

    printf("{\"oficial\":%s,\"dias\":[", oficial ? "true" : "false");
    for (size_t i = 0; i < days.size(); i++) {
        Day& d = days[i];
        printf("%s{\"etiqueta\":\"%s\",\"fecha\":\"%s\",\"suspendido\":%s,\"alcance\":\"%s\",\"fuente\":\"%s\",\"titular\":\"%s\"}",
            i ? "," : "", d.etiqueta.c_str(), d.iso.c_str(), d.susp ? "true" : "false",
            d.alcance.c_str(), json_escape(d.fuente).c_str(), json_escape(d.titular).c_str());
    }
    printf("]}\n");
    curl_global_cleanup();
    return 0;
}
