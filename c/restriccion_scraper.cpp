// restriccion_scraper — módulo scraper (C++) para CLIMA.
// Lee la tabla OFICIAL y DATADA de restricción de la portada de airerm.mma.gob.cl
// (WordPress, server-rendered) y confirma si HOY hay episodio ambiental declarado
// (preemergencia/emergencia). Devuelve veredicto JSON a stdout.
//
// SEGURIDAD (carve-out): solo afirma episodio si (a) la tabla es de HOY (match de
// fecha "DD de MES, AAAA") y (b) la palabra del episodio aparece DENTRO de la
// ventana de la tabla (no en la leyenda de medidas). Si no, oficial=false y la app
// usa su calendario determinista.
//
// Compila: g++ -O2 -std=c++17 restriccion_scraper.cpp -lcurl -o restriccion-scraper
#include <curl/curl.h>
#include <string>
#include <cstdio>
#include <cctype>
#include <ctime>

static const char* FUENTE = "https://airerm.mma.gob.cl/";

static size_t wr(void* p, size_t s, size_t n, void* u) {
    ((std::string*)u)->append((char*)p, s * n); return s * n;
}
static std::string http_get(const std::string& url) {
    CURL* c = curl_easy_init(); std::string out; if (!c) return out;
    curl_easy_setopt(c, CURLOPT_URL, url.c_str());
    curl_easy_setopt(c, CURLOPT_WRITEFUNCTION, wr);
    curl_easy_setopt(c, CURLOPT_WRITEDATA, &out);
    curl_easy_setopt(c, CURLOPT_FOLLOWLOCATION, 1L);
    curl_easy_setopt(c, CURLOPT_TIMEOUT, 6L);
    curl_easy_setopt(c, CURLOPT_CONNECTTIMEOUT, 4L);
    curl_easy_setopt(c, CURLOPT_USERAGENT, "clima-restriccion/1.0");
    if (curl_easy_perform(c) != CURLE_OK) out.clear();
    curl_easy_cleanup(c);
    return out;
}
// quita tags + minúsculas + colapsa espacios
static std::string strip_lower(const std::string& html) {
    std::string o; bool in = false; char prev = 0;
    for (char c : html) {
        if (c == '<') in = true;
        else if (c == '>') in = false;
        else if (!in) {
            char lc = (char)std::tolower((unsigned char)c);
            if (std::isspace((unsigned char)lc)) { if (prev != ' ') { o += ' '; prev = ' '; } }
            else { o += lc; prev = lc; }
        }
    }
    return o;
}
static bool has(const std::string& h, const std::string& n) { return h.find(n) != std::string::npos; }

int main() {
    curl_global_init(CURL_GLOBAL_DEFAULT);
    std::string t = strip_lower(http_get(FUENTE));

    // fecha de HOY (Santiago) como "DD de MES"
    setenv("TZ", "America/Santiago", 1); tzset();
    time_t now = time(nullptr); struct tm* lt = localtime(&now);
    static const char* meses[] = {"enero","febrero","marzo","abril","mayo","junio",
        "julio","agosto","septiembre","octubre","noviembre","diciembre"};
    char hoy[40]; snprintf(hoy, sizeof hoy, "%d de %s", lt->tm_mday, meses[lt->tm_mon]);
    char iso[16]; snprintf(iso, sizeof iso, "%04d-%02d-%02d", lt->tm_year+1900, lt->tm_mon+1, lt->tm_mday);

    bool oficial = false; const char* episodio = "Normal";
    // FIX 2026-10-31 (Kotone): la portada dice «Restricción» CON tilde (o &oacute;) y strip_lower no quita
    // tildes → el ancla sin tilde nunca calzaba y siempre caía al calendario. Se aceptan las 3 grafías.
    size_t p = std::string::npos;
    for (const char* a : {"restricci\xc3\xb3n vehicular para hoy", "restricci&oacute;n vehicular para hoy",
                          "restriccion vehicular para hoy"})
        if ((p = t.find(a)) != std::string::npos) break;
    if (p != std::string::npos) {
        std::string win = t.substr(p, 1000);          // solo la tabla, no la leyenda
        if (has(win, hoy)) {                           // la tabla es de HOY
            oficial = true;
            if (has(win, "emergencia ambiental")) episodio = "Emergencia";
            else if (has(win, "preemergencia")) episodio = "Preemergencia";
        }
    }
    printf("{\"oficial\":%s,\"fecha\":\"%s\",\"episodio\":\"%s\"}\n",
        oficial ? "true" : "false", iso, episodio);
    curl_global_cleanup();
    return 0;
}
