/*
 * Clima para Windows — nativo Win32 (C89, GDI, WinINet). Un solo código para dos compilaciones:
 *   · Clima.exe    (UNICODE)  Windows 2000, XP, Vista, 7, 8, 8.1, 10 y 11
 *   · Clima9x.exe  (ANSI)     Windows 95 (con IE 4+), 98, 98 SE y Me
 * Datos: Open-Meteo por HTTP (sin TLS: los sistemas antiguos no tienen TLS 1.2 y Open-Meteo atiende sin cifrar;
 * son datos públicos y sólo viaja la coordenada de la comuna elegida).
 * Desarrollado por Javiera Navarrete.
 */
#ifndef WINVER
#define WINVER 0x0400
#endif
#include <windows.h>
#include <wininet.h>
#include <tchar.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <math.h>
#include "json.h"
#include "recursos.h"

#ifndef CLEARTYPE_QUALITY
#define CLEARTYPE_QUALITY 5      /* en 9x/2000 se trata como calidad por defecto */
#endif
#ifndef WM_MOUSEWHEEL
#define WM_MOUSEWHEEL 0x020A
#endif
#ifndef GET_WHEEL_DELTA_WPARAM
#define GET_WHEEL_DELTA_WPARAM(w) ((short)HIWORD(w))
#endif

#define CLASE  TEXT("ClimaJavieraNavarrete")
#define REG    TEXT("Software\\Javiera Navarrete\\Clima")
#define WM_DATOS (WM_APP + 1)
#define ID_BUSCA 10
#define ID_LISTA 11
#define T_ANIM 1
#define T_REFRESCO 2

static const char *API = "http://api.open-meteo.com/v1/forecast?timezone=America%2FSantiago&forecast_days=10&forecast_hours=25"
    "&current=temperature_2m,apparent_temperature,relative_humidity_2m,weather_code,wind_speed_10m,wind_gusts_10m,is_day,surface_pressure"
    "&hourly=temperature_2m,weather_code,precipitation_probability,is_day"
    "&daily=weather_code,temperature_2m_max,temperature_2m_min,sunrise,sunset,precipitation_probability_max,uv_index_max";

/* ------------------------------------------------------------------ estado */
static HWND g_win, g_busca, g_lista;
static HINSTANCE g_inst;
static char g_comuna[96] = "Santiago", g_region[64] = "Metropolitana";
static double g_lat = -33.4489, g_lon = -70.6693;
static JNodo *g_datos;
static BOOL g_cargando, g_viejo;
static char g_error[160];
static int g_scroll, g_scrollMax, g_scrollH, g_scrollHMax, g_yHoras, g_altoHoras;
static int g_pressX, g_pressY, g_baseS, g_baseH, g_arrastra;   /* 0 nada, 1 vertical, 2 horas */
static DWORD g_t0;
static float g_px[90], g_py[90], g_pv[90];
static HFONT f_grande, f_media, f_chica, f_negrita;
static char *g_comunas;            /* recurso comunas.txt (UTF-8, «nombre|región|lat|lon» por línea) */
static int g_ncomunas;
static char **g_lin;               /* punteros a cada línea */
static int g_filtradas[400], g_nfiltradas;

/* ------------------------------------------------------------------ texto UTF-8 → TCHAR (sin CP_UTF8: Win95 no lo trae) */
static int utf8_a_wide(const char *s, WCHAR *o, int max) {
    int n = 0;
    const unsigned char *p = (const unsigned char *)s;
    while (*p && n < max - 1) {
        unsigned cp;
        if (*p < 0x80) cp = *p++;
        else if ((*p & 0xE0) == 0xC0 && p[1]) { cp = (p[0] & 31) << 6 | (p[1] & 63); p += 2; }
        else if ((*p & 0xF0) == 0xE0 && p[1] && p[2]) { cp = (p[0] & 15) << 12 | (p[1] & 63) << 6 | (p[2] & 63); p += 3; }
        else { p++; cp = '?'; }
        o[n++] = (WCHAR)cp;
    }
    o[n] = 0;
    return n;
}
static const TCHAR *T(const char *utf8) {      /* anillo de 8 búferes: usable varias veces en una llamada */
    static TCHAR buf[8][512];
    static int k;
    TCHAR *r = buf[k = (k + 1) & 7];
#ifdef UNICODE
    utf8_a_wide(utf8, r, 512);
#else
    WCHAR w[512];
    int n = utf8_a_wide(utf8, w, 512);
    WideCharToMultiByte(CP_ACP, 0, w, n + 1, r, 512, "?", NULL);
#endif
    return r;
}
/* TCHAR → UTF-8 (para lo que escribe la usuaria en el buscador) */
static void a_utf8(const TCHAR *t, char *o, int max) {
    WCHAR w[256];
    int i, n = 0;
#ifdef UNICODE
    lstrcpynW(w, t, 256);
#else
    MultiByteToWideChar(CP_ACP, 0, t, -1, w, 256);
#endif
    for (i = 0; w[i] && n < max - 4; i++) {
        unsigned c = w[i];
        if (c < 0x80) o[n++] = (char)c;
        else if (c < 0x800) { o[n++] = (char)(0xC0 | c >> 6); o[n++] = (char)(0x80 | (c & 63)); }
        else { o[n++] = (char)(0xE0 | c >> 12); o[n++] = (char)(0x80 | (c >> 6 & 63)); o[n++] = (char)(0x80 | (c & 63)); }
    }
    o[n] = 0;
}
/* minúsculas sin tildes, para buscar comunas */
static void plegar(const char *s, char *o, int max) {
    int n = 0;
    const unsigned char *p = (const unsigned char *)s;
    while (*p && n < max - 1) {
        if (p[0] == 0xC3 && p[1]) {
            unsigned c = p[1] | 0x20;     /* Á→á, etc. */
            char r = c == 0xA1 ? 'a' : c == 0xA9 ? 'e' : c == 0xAD ? 'i' : c == 0xB3 ? 'o' : (c == 0xBA || c == 0xBC) ? 'u' : c == 0xB1 ? 'n' : '?';
            o[n++] = r; p += 2;
        } else { o[n++] = (char)(*p >= 'A' && *p <= 'Z' ? *p + 32 : *p); p++; }
    }
    o[n] = 0;
}

/* ------------------------------------------------------------------ registro (preferencias + último pronóstico) */
static void reg_leer(void) {
    HKEY k;
    DWORD t, n;
    char b[64];
    if (RegOpenKeyExA(HKEY_CURRENT_USER, "Software\\Javiera Navarrete\\Clima", 0, KEY_READ, &k) != ERROR_SUCCESS) return;
    n = sizeof g_comuna; RegQueryValueExA(k, "Comuna", NULL, &t, (BYTE *)g_comuna, &n);
    n = sizeof g_region; RegQueryValueExA(k, "Region", NULL, &t, (BYTE *)g_region, &n);
    n = sizeof b; if (RegQueryValueExA(k, "Lat", NULL, &t, (BYTE *)b, &n) == ERROR_SUCCESS) g_lat = atof(b);
    n = sizeof b; if (RegQueryValueExA(k, "Lon", NULL, &t, (BYTE *)b, &n) == ERROR_SUCCESS) g_lon = atof(b);
    n = 0;
    if (RegQueryValueExA(k, "Ultimo", NULL, &t, NULL, &n) == ERROR_SUCCESS && n > 0 && n < 65536) {
        char *j = (char *)malloc(n + 1);
        if (j && RegQueryValueExA(k, "Ultimo", NULL, &t, (BYTE *)j, &n) == ERROR_SUCCESS) {
            j[n] = 0; g_datos = json_leer(j); g_viejo = g_datos != NULL;
        }
        free(j);
    }
    RegCloseKey(k);
}
static void reg_guardar(const char *json) {
    HKEY k;
    DWORD d;
    char b[32];
    if (RegCreateKeyExA(HKEY_CURRENT_USER, "Software\\Javiera Navarrete\\Clima", 0, NULL, 0, KEY_WRITE, NULL, &k, &d) != ERROR_SUCCESS) return;
    RegSetValueExA(k, "Comuna", 0, REG_SZ, (const BYTE *)g_comuna, (DWORD)strlen(g_comuna) + 1);
    RegSetValueExA(k, "Region", 0, REG_SZ, (const BYTE *)g_region, (DWORD)strlen(g_region) + 1);
    sprintf(b, "%.4f", g_lat); RegSetValueExA(k, "Lat", 0, REG_SZ, (const BYTE *)b, (DWORD)strlen(b) + 1);
    sprintf(b, "%.4f", g_lon); RegSetValueExA(k, "Lon", 0, REG_SZ, (const BYTE *)b, (DWORD)strlen(b) + 1);
    if (json && strlen(json) < 15000) RegSetValueExA(k, "Ultimo", 0, REG_BINARY, (const BYTE *)json, (DWORD)strlen(json));
    else if (!json) RegDeleteValueA(k, "Ultimo");
    RegCloseKey(k);
}

/* ------------------------------------------------------------------ red (hilo aparte) */
static DWORD WINAPI hilo_red(LPVOID p) {
    char url[1200], *buf = NULL;
    DWORD cap = 0, n = 0, leidos;
    HINTERNET in, h;
    (void)p;
    sprintf(url, "%s&latitude=%.4f&longitude=%.4f", API, g_lat, g_lon);
    in = InternetOpenA("Clima-Windows/1.0 (Javiera Navarrete)", INTERNET_OPEN_TYPE_PRECONFIG, NULL, NULL, 0);
    h = in ? InternetOpenUrlA(in, url, NULL, 0, INTERNET_FLAG_RELOAD | INTERNET_FLAG_NO_CACHE_WRITE | INTERNET_FLAG_NO_UI, 0) : NULL;
    if (h) {
        for (;;) {
            if (n + 4097 > cap) { char *nb = (char *)realloc(buf, cap = cap ? cap * 2 : 16384); if (!nb) break; buf = nb; }
            if (!InternetReadFile(h, buf + n, 4096, &leidos) || leidos == 0) break;
            n += leidos;
        }
        if (buf) buf[n] = 0;
        InternetCloseHandle(h);
    }
    if (in) InternetCloseHandle(in);
    if (!h || n == 0 || !buf || buf[0] != '{') { free(buf); buf = NULL; }
    PostMessage(g_win, WM_DATOS, 0, (LPARAM)buf);
    return 0;
}
static void actualizar(void) {
    DWORD id;
    HANDLE t;
    if (g_cargando) return;
    g_cargando = TRUE;
    t = CreateThread(NULL, 0, hilo_red, NULL, 0, &id);
    if (t) CloseHandle(t); else g_cargando = FALSE;
    InvalidateRect(g_win, NULL, FALSE);
}

/* ------------------------------------------------------------------ condición */
static const char *texto_cond(int c) {
    switch (c) {
        case 0: return "Despejado"; case 1: return "Mayormente despejado"; case 2: return "Parcialmente nublado";
        case 3: return "Nublado"; case 45: return "Niebla"; case 48: return "Niebla con escarcha";
        case 51: return "Llovizna débil"; case 53: return "Llovizna"; case 55: return "Llovizna intensa";
        case 56: case 57: return "Llovizna engelante"; case 61: return "Lluvia débil"; case 63: return "Lluvia";
        case 65: return "Lluvia intensa"; case 66: case 67: return "Lluvia engelante"; case 71: return "Nieve débil";
        case 73: return "Nieve"; case 75: return "Nieve intensa"; case 77: return "Granos de nieve";
        case 80: return "Chubascos débiles"; case 81: return "Chubascos"; case 82: return "Chubascos violentos";
        case 85: case 86: return "Chubascos de nieve"; case 95: return "Tormenta eléctrica";
        case 96: case 99: return "Tormenta con granizo"; default: return "—";
    }
}
static int grupo(int c) {   /* 0 despejado 1 nubes 2 niebla 3 lluvia 4 nieve 5 tormenta */
    if (c <= 1) return 0;
    if (c <= 3) return 1;
    if (c <= 48) return 2;
    if ((c >= 71 && c <= 77) || c == 85 || c == 86) return 4;
    if (c >= 95) return 5;
    return 3;
}

/* ------------------------------------------------------------------ dibujo */
static COLORREF mezcla(COLORREF a, COLORREF b, int t) {
    return RGB((GetRValue(a) * (255 - t) + GetRValue(b) * t) / 255, (GetGValue(a) * (255 - t) + GetGValue(b) * t) / 255,
               (GetBValue(a) * (255 - t) + GetBValue(b) * t) / 255);
}
static void rect(HDC dc, int x, int y, int w, int h, COLORREF c) { RECT r; HBRUSH b = CreateSolidBrush(c); SetRect(&r, x, y, x + w, y + h); FillRect(dc, &r, b); DeleteObject(b); }
static void elipse(HDC dc, int x, int y, int w, int h, COLORREF c) {
    HBRUSH b = CreateSolidBrush(c), ob = (HBRUSH)SelectObject(dc, b);
    HPEN p = CreatePen(PS_SOLID, 1, c), op = (HPEN)SelectObject(dc, p);
    Ellipse(dc, x, y, x + w, y + h);
    SelectObject(dc, ob); SelectObject(dc, op); DeleteObject(b); DeleteObject(p);
}
static void linea(HDC dc, int x1, int y1, int x2, int y2, COLORREF c, int g) {
    HPEN p = CreatePen(PS_SOLID, g, c), op = (HPEN)SelectObject(dc, p);
    MoveToEx(dc, x1, y1, NULL); LineTo(dc, x2, y2);
    SelectObject(dc, op); DeleteObject(p);
}
static void marco(HDC dc, int x, int y, int w, int h, COLORREF c) {
    HPEN p = CreatePen(PS_SOLID, 1, c), op = (HPEN)SelectObject(dc, p);
    HBRUSH ob = (HBRUSH)SelectObject(dc, GetStockObject(NULL_BRUSH));
    RoundRect(dc, x, y, x + w, y + h, 18, 18);
    SelectObject(dc, op); SelectObject(dc, ob); DeleteObject(p);
}
static int texto_en(HDC dc, HFONT f, const char *s, int x, int y, COLORREF c, UINT alin) {
    RECT r; SIZE sz; const TCHAR *t = T(s);
    SelectObject(dc, f); SetTextColor(dc, c);
    GetTextExtentPoint32(dc, t, lstrlen(t), &sz);
    if (alin == DT_CENTER) x -= sz.cx / 2; else if (alin == DT_RIGHT) x -= sz.cx;
    SetRect(&r, x, y, x + sz.cx + 2, y + sz.cy);
    DrawText(dc, t, -1, &r, DT_LEFT | DT_NOPREFIX | DT_SINGLELINE);
    return sz.cy;
}

static const COLORREF CIELOS[6][4] = {
    {RGB(0x2E,0x7B,0xD6), RGB(0x8C,0xC6,0xF2), RGB(0x0B,0x15,0x30), RGB(0x2A,0x3E,0x6E)},
    {RGB(0x5D,0x7F,0xA8), RGB(0xA9,0xBE,0xD6), RGB(0x16,0x1E,0x2E), RGB(0x39,0x46,0x5E)},
    {RGB(0x8C,0x97,0xA3), RGB(0xC9,0xCE,0xD4), RGB(0x2A,0x2F,0x37), RGB(0x55,0x5C,0x66)},
    {RGB(0x3D,0x54,0x70), RGB(0x7E,0x93,0xAA), RGB(0x12,0x1A,0x26), RGB(0x2E,0x3B,0x4D)},
    {RGB(0x8F,0xA9,0xC8), RGB(0xE3,0xEB,0xF5), RGB(0x26,0x33,0x4A), RGB(0x56,0x68,0x8A)},
    {RGB(0x2C,0x31,0x40), RGB(0x5B,0x62,0x75), RGB(0x0D,0x0F,0x16), RGB(0x2B,0x2F,0x3D)}};

static void nube(HDC dc, int x, int y, int w, COLORREF c) {
    int h = w / 4;
    elipse(dc, x, y + h / 3, w / 2, h, c);
    elipse(dc, x + w / 4, y, w / 2, h + h / 3, c);
    elipse(dc, x + w / 2, y + h / 4, w / 2, h, c);
    rect(dc, x + w / 6, y + h / 2, w * 2 / 3, h * 2 / 3, c);
}
static void estrellas(HDC dc, int w, int h, float t) {
    int i;
    for (i = 0; i < 50; i++) {
        int br = 140 + (int)(100 * sin(t * g_pv[i] * 2 + i));
        rect(dc, (int)(g_px[i] * w), (int)(g_py[i] * h * 0.7f), 2, 2, RGB(br, br, br));
    }
}
static void cielo(HDC dc, int w, int h, int gr, BOOL dia) {
    COLORREF a = CIELOS[gr][dia ? 0 : 2], b = CIELOS[gr][dia ? 1 : 3];
    float t = (GetTickCount() - g_t0) / 1000.0f;
    int i;
    for (i = 0; i < 32; i++) rect(dc, 0, h * i / 32, w, h / 32 + 1, mezcla(a, b, i * 255 / 31));
    if (gr == 5 && (GetTickCount() / 100) % 47 == 0) { rect(dc, 0, 0, w, h, RGB(0xE8, 0xEC, 0xFF)); return; }
    if (gr == 0) {
        if (dia) {
            int r0 = (w < h ? w : h) / 7, cx = w - r0 - 24, cy = r0 + 70, k;
            for (k = 3; k >= 1; k--) { int rr = r0 + k * 10 + (int)(4 * sin(t * 2)); elipse(dc, cx - rr, cy - rr, rr * 2, rr * 2, mezcla(a, RGB(0xFF, 0xE9, 0xA8), 90 / k)); }
            elipse(dc, cx - r0, cy - r0, r0 * 2, r0 * 2, RGB(0xFF, 0xE2, 0x7A));
        } else estrellas(dc, w, h, t);
    } else if (gr == 1 || gr == 2) {
        COLORREF base = gr == 2 ? RGB(0xD8, 0xDC, 0xE2) : (dia ? RGB(0xF2, 0xF5, 0xF9) : RGB(0x5A, 0x64, 0x78));
        if (!dia) estrellas(dc, w, h / 2, t);
        for (i = 0; i < 4; i++) {
            int cw = w / 2 + i * 20, x = (int)fmod(g_px[i] * w + t * (8 + i * 4), (double)(w + cw)) - cw;
            nube(dc, x, 60 + i * h / 9, cw, mezcla(base, a, 60 + i * 30));
        }
        if (gr == 2) for (i = 0; i < 6; i++) { int y = h / 3 + i * h / 10 + (int)(6 * sin(t + i)); linea(dc, 0, y, w, y, mezcla(RGB(0xC9, 0xCE, 0xD4), RGB(255, 255, 255), 40), 2); }
    } else if (gr == 3 || gr == 5) {
        nube(dc, -20, 40, w + 40, gr == 5 ? RGB(0x3A, 0x3F, 0x4E) : RGB(0x6B, 0x7C, 0x91));
        for (i = 0; i < 90; i++) {
            float y = (float)fmod(g_py[i] + t * g_pv[i] * 0.9f, 1.0), x = (float)fmod(g_px[i] + y * 0.08f, 1.0);
            linea(dc, (int)(x * w), (int)(y * h), (int)(x * w) + 2, (int)(y * h) + 10, RGB(0xB8, 0xCC, 0xE4), 1);
        }
    } else if (gr == 4) {
        for (i = 0; i < 90; i++) {
            float y = (float)fmod(g_py[i] + t * g_pv[i] * 0.12f, 1.0);
            int s = 2 + i % 3;
            elipse(dc, (int)((g_px[i] + 0.03f * sin(t + i)) * w), (int)(y * h), s, s, RGB(255, 255, 255));
        }
    }
}
static void icono(HDC dc, int cx, int cy, int s, int c, BOOL dia) {
    int gr = grupo(c), r = s / 3, by;
    COLORREF col;
    if (gr == 0 || c == 1 || c == 2) {
        int sx = cx - (gr == 0 ? 0 : r / 2), sy = cy - (gr == 0 ? 0 : r / 2);
        elipse(dc, sx - r, sy - r, 2 * r, 2 * r, dia ? RGB(0xFF, 0xD5, 0x4A) : RGB(0xE6, 0xE9, 0xF2));
        if (gr == 0) return;
    }
    col = gr == 5 ? RGB(0x8C, 0x93, 0xA6) : gr == 2 ? RGB(0xC9, 0xCE, 0xD4) : RGB(0xEE, 0xF2, 0xF7);
    elipse(dc, cx - s / 2, cy - r / 2, s / 2 + 2, r + 2, col);
    elipse(dc, cx - r, cy - r, r * 2, r * 2 - 2, col);
    elipse(dc, cx, cy - r / 2, s / 2, r + 2, col);
    rect(dc, cx - s / 3, cy, s * 2 / 3, r / 2 + 1, col);
    by = cy + r / 2 + 3;
    if (gr == 3) { int k; for (k = -1; k <= 1; k++) linea(dc, cx + k * r, by, cx + k * r - 2, by + r, RGB(0x7F, 0xC4, 0xFF), 1); }
    else if (gr == 4) { int k; for (k = -1; k <= 1; k++) rect(dc, cx + k * r - 1, by + 1, 3, 3, RGB(255, 255, 255)); }
    else if (gr == 5) { linea(dc, cx, by, cx - 3, by + r / 2 + 2, RGB(0xFF, 0xD5, 0x4A), 2); linea(dc, cx - 3, by + r / 2 + 2, cx + 2, by + r + 3, RGB(0xFF, 0xD5, 0x4A), 2); }
    else if (gr == 2) { linea(dc, cx - s / 2, by, cx + s / 2, by, RGB(0xE0, 0xE4, 0xEA), 1); linea(dc, cx - s / 3, by + 3, cx + s / 3, by + 3, RGB(0xE0, 0xE4, 0xEA), 1); }
}
static const char *DIAS[] = {"Dom", "Lun", "Mar", "Mié", "Jue", "Vie", "Sáb"};
static const char *dia_semana(const char *iso) {
    int y = atoi(iso), m = atoi(iso + 5), d = atoi(iso + 8), k, j, h;
    if (m < 3) { m += 12; y--; }
    k = y % 100; j = y / 100; h = (d + 13 * (m + 1) / 5 + k + k / 4 + j / 4 + 5 * j) % 7;
    return DIAS[(h + 6) % 7];
}
static int red(double d) { return (int)floor(d + 0.5); }

static void pintar(HDC dc, int w, int h) {
    JNodo *cur = json_obj(g_datos, "current"), *dd = json_obj(g_datos, "daily"), *ho = json_obj(g_datos, "hourly");
    int c = (int)json_num(cur, "weather_code", 0), gr = grupo(c), y, i, n, col, alto;
    BOOL dia = !g_datos || json_num(cur, "is_day", 1) > 0;
    char s[160];
    JNodo *mx = json_obj(dd, "temperature_2m_max"), *mn = json_obj(dd, "temperature_2m_min");
    SetBkMode(dc, TRANSPARENT);
    cielo(dc, w, h, gr, dia);
    y = 52 - g_scroll;   /* bajo el buscador */
    y += texto_en(dc, f_media, g_comuna, w / 2, y, RGB(255, 255, 255), DT_CENTER);
    sprintf(s, "%s", g_cargando ? "Actualizando…" : g_error[0] ? g_error : g_viejo ? "Último pronóstico guardado" : g_region);
    y += texto_en(dc, f_chica, s, w / 2, y, RGB(0xDD, 0xE6, 0xF5), DT_CENTER) + 4;
    if (!g_datos) { texto_en(dc, f_media, g_cargando ? "Cargando pronóstico…" : "Sin datos (F5 para reintentar)", w / 2, h / 2, RGB(255, 255, 255), DT_CENTER); return; }
    sprintf(s, "%d°", red(json_num(cur, "temperature_2m", 0)));
    y += texto_en(dc, f_grande, s, w / 2, y, RGB(255, 255, 255), DT_CENTER);
    y += texto_en(dc, f_media, texto_cond(c), w / 2, y, RGB(255, 255, 255), DT_CENTER);
    sprintf(s, "Máx %d°   Mín %d°", red(json_num_i(mx, 0, 0)), red(json_num_i(mn, 0, 0)));
    y += texto_en(dc, f_chica, s, w / 2, y, RGB(255, 255, 255), DT_CENTER) + 12;

    /* 24 horas */
    {
        JNodo *ht = json_obj(ho, "time"), *hT = json_obj(ho, "temperature_2m"), *hC = json_obj(ho, "weather_code"),
              *hP = json_obj(ho, "precipitation_probability"), *hD = json_obj(ho, "is_day");
        HRGN rg;
        int ch = 18;
        col = 58; alto = ch * 3 + 34; g_yHoras = y + g_scroll; g_altoHoras = alto;
        marco(dc, 10, y, w - 20, alto, RGB(255, 255, 255));
        n = json_largo(ht);
        g_scrollHMax = n * col - (w - 20); if (g_scrollHMax < 0) g_scrollHMax = 0;
        rg = CreateRectRgn(11, y + 1, w - 11, y + alto - 1); SelectClipRgn(dc, rg);
        for (i = 0; i < n; i++) {
            int x = 10 + i * col - g_scrollH + col / 2, p;
            const char *hh = json_txt_i(ht, i);
            if (x < -col || x > w + col) continue;
            sprintf(s, "%s", i == 0 ? "Ahora" : (strlen(hh) >= 13 ? hh + 11 : hh)); if (i) s[2] = 0;
            texto_en(dc, f_chica, s, x, y + 6, RGB(255, 255, 255), DT_CENTER);
            icono(dc, x, y + 10 + ch + ch / 2, ch, (int)json_num_i(hC, i, 0), json_num_i(hD, i, 1) > 0);
            sprintf(s, "%d°", red(json_num_i(hT, i, 0)));
            texto_en(dc, f_negrita, s, x, y + 14 + ch * 2, RGB(255, 255, 255), DT_CENTER);
            p = (int)json_num_i(hP, i, 0);
            if (p >= 20) { sprintf(s, "%d%%", p); texto_en(dc, f_chica, s, x, y + alto - ch - 2, RGB(0x9A, 0xD0, 0xFF), DT_CENTER); }
        }
        SelectClipRgn(dc, NULL); DeleteObject(rg);
        y += alto + 12;
    }
    /* detalle */
    {
        int ad = 50, cw = (w - 20) / 2;
        const char *sr = json_txt_i(json_obj(dd, "sunrise"), 0), *ss = json_txt_i(json_obj(dd, "sunset"), 0);
        struct { const char *et; char v[48]; } d[6];
        marco(dc, 10, y, w - 20, ad * 3, RGB(255, 255, 255));
        d[0].et = "Sensación"; sprintf(d[0].v, "%d°", red(json_num(cur, "apparent_temperature", 0)));
        d[1].et = "Humedad"; sprintf(d[1].v, "%d%%", red(json_num(cur, "relative_humidity_2m", 0)));
        d[2].et = "Viento"; sprintf(d[2].v, "%d km/h (ráfagas %d)", red(json_num(cur, "wind_speed_10m", 0)), red(json_num(cur, "wind_gusts_10m", 0)));
        d[3].et = "Índice UV"; sprintf(d[3].v, "%d", red(json_num_i(json_obj(dd, "uv_index_max"), 0, 0)));
        d[4].et = "Sol"; sprintf(d[4].v, "%.5s – %.5s", strlen(sr) >= 16 ? sr + 11 : "—", strlen(ss) >= 16 ? ss + 11 : "—");
        d[5].et = "Presión"; sprintf(d[5].v, "%d hPa", red(json_num(cur, "surface_pressure", 0)));
        for (i = 0; i < 6; i++) {
            int x = 10 + (i % 2) * cw + 12, yy = y + (i / 2) * ad + 6;
            texto_en(dc, f_chica, d[i].et, x, yy, RGB(0xC8, 0xD6, 0xEE), DT_LEFT);
            texto_en(dc, f_negrita, d[i].v, x, yy + 18, RGB(255, 255, 255), DT_LEFT);
        }
        y += ad * 3 + 12;
    }
    /* 10 días */
    {
        JNodo *dt = json_obj(dd, "time"), *dc2 = json_obj(dd, "weather_code"), *dp = json_obj(dd, "precipitation_probability_max");
        int fila = 32, nd = json_largo(dt), bx = w / 2 - 6, bw = w / 2 - 74;
        double tmin = 99, tmax = -99;
        marco(dc, 10, y, w - 20, fila * nd + 10, RGB(255, 255, 255));
        for (i = 0; i < nd; i++) { double a = json_num_i(mn, i, 0), b = json_num_i(mx, i, 0); if (a < tmin) tmin = a; if (b > tmax) tmax = b; }
        if (tmax - tmin < 1) tmax = tmin + 1;
        for (i = 0; i < nd; i++) {
            int yy = y + 5 + i * fila, lo, hi, pr;
            texto_en(dc, f_negrita, i == 0 ? "Hoy" : dia_semana(json_txt_i(dt, i)), 22, yy + 7, RGB(255, 255, 255), DT_LEFT);
            icono(dc, 22 + w / 6 + 10, yy + fila / 2, 18, (int)json_num_i(dc2, i, 0), TRUE);
            pr = (int)json_num_i(dp, i, 0);
            if (pr >= 20) { sprintf(s, "%d%%", pr); texto_en(dc, f_chica, s, 22 + w / 6 + 26, yy + 8, RGB(0x9A, 0xD0, 0xFF), DT_LEFT); }
            lo = (int)((json_num_i(mn, i, 0) - tmin) / (tmax - tmin) * bw);
            hi = (int)((json_num_i(mx, i, 0) - tmin) / (tmax - tmin) * bw);
            rect(dc, bx, yy + fila / 2 - 2, bw, 5, RGB(0x5A, 0x6F, 0x8F));
            rect(dc, bx + lo, yy + fila / 2 - 2, (hi - lo) > 6 ? hi - lo : 6, 5, RGB(0xFF, 0xC2, 0x4B));
            sprintf(s, "%d°", red(json_num_i(mn, i, 0))); texto_en(dc, f_chica, s, bx - 6, yy + 8, RGB(0xC8, 0xD6, 0xEE), DT_RIGHT);
            sprintf(s, "%d°", red(json_num_i(mx, i, 0))); texto_en(dc, f_chica, s, bx + bw + 6, yy + 8, RGB(255, 255, 255), DT_LEFT);
        }
        y += fila * nd + 22;
    }
    texto_en(dc, f_chica, "Datos: Open-Meteo (CC BY 4.0) · Desarrollado por Javiera Navarrete", w / 2, y, RGB(0xDD, 0xE6, 0xF5), DT_CENTER);
    y += 30;
    g_scrollMax = y + g_scroll - h; if (g_scrollMax < 0) g_scrollMax = 0;
}

/* ------------------------------------------------------------------ comunas */
static void cargar_comunas(void) {
    HRSRC r = FindResource(g_inst, MAKEINTRESOURCE(IDR_COMUNAS), RT_RCDATA);
    HGLOBAL g = r ? LoadResource(g_inst, r) : NULL;
    DWORD n = r ? SizeofResource(g_inst, r) : 0;
    char *p;
    int k = 0;
    if (!g) return;
    g_comunas = (char *)malloc(n + 1);
    memcpy(g_comunas, LockResource(g), n); g_comunas[n] = 0;
    for (p = g_comunas; *p; p++) if (*p == '\n') g_ncomunas++;
    g_lin = (char **)malloc(sizeof(char *) * (g_ncomunas + 1));
    for (p = g_comunas; *p && k < g_ncomunas;) { g_lin[k++] = p; p = strchr(p, '\n'); if (!p) break; *p++ = 0; }
    g_ncomunas = k;
}
static void campo(const char *l, int i, char *o, int max) {   /* i-ésimo campo de «a|b|c|d» */
    int n = 0;
    while (i > 0 && *l) { if (*l++ == '|') i--; }
    while (*l && *l != '|' && n < max - 1) o[n++] = *l++;
    o[n] = 0;
}
static void filtrar(void) {
    TCHAR t[128];
    char q[256], nq[256], nom[128], nn[128], reg[96], fila[256];
    int i;
    GetWindowText(g_busca, t, 128);
    a_utf8(t, q, sizeof q); plegar(q, nq, sizeof nq);
    SendMessage(g_lista, LB_RESETCONTENT, 0, 0);
    g_nfiltradas = 0;
    if (!nq[0]) { ShowWindow(g_lista, SW_HIDE); return; }
    for (i = 0; i < g_ncomunas && g_nfiltradas < 400; i++) {
        campo(g_lin[i], 0, nom, sizeof nom); plegar(nom, nn, sizeof nn);
        if (!strstr(nn, nq)) continue;
        campo(g_lin[i], 1, reg, sizeof reg);
        sprintf(fila, "%s · %s", nom, reg);
        SendMessage(g_lista, LB_ADDSTRING, 0, (LPARAM)T(fila));
        g_filtradas[g_nfiltradas++] = i;
    }
    ShowWindow(g_lista, g_nfiltradas ? SW_SHOW : SW_HIDE);
    if (g_nfiltradas) SendMessage(g_lista, LB_SETCURSEL, 0, 0);
}
static void elegir(int k) {
    char b[32];
    if (k < 0 || k >= g_nfiltradas) return;
    campo(g_lin[g_filtradas[k]], 0, g_comuna, sizeof g_comuna);
    campo(g_lin[g_filtradas[k]], 1, g_region, sizeof g_region);
    campo(g_lin[g_filtradas[k]], 2, b, sizeof b); g_lat = atof(b);
    campo(g_lin[g_filtradas[k]], 3, b, sizeof b); g_lon = atof(b);
    reg_guardar(NULL);
    json_libera(g_datos); g_datos = NULL; g_viejo = FALSE; g_scroll = g_scrollH = 0;
    SetWindowText(g_busca, TEXT(""));
    ShowWindow(g_lista, SW_HIDE);
    SetFocus(g_win);
    actualizar();
}

/* buscador: Enter elige, flechas mueven la lista, Esc cierra */
static WNDPROC g_editOrig;
static LRESULT CALLBACK proc_busca(HWND h, UINT m, WPARAM wp, LPARAM lp) {
    if (m == WM_KEYDOWN) {
        int s = (int)SendMessage(g_lista, LB_GETCURSEL, 0, 0);
        if (wp == VK_RETURN) { elegir(s < 0 ? 0 : s); return 0; }
        if (wp == VK_DOWN) { SendMessage(g_lista, LB_SETCURSEL, s + 1 < g_nfiltradas ? s + 1 : s, 0); return 0; }
        if (wp == VK_UP) { SendMessage(g_lista, LB_SETCURSEL, s > 0 ? s - 1 : 0, 0); return 0; }
        if (wp == VK_ESCAPE) { SetWindowText(h, TEXT("")); ShowWindow(g_lista, SW_HIDE); SetFocus(g_win); return 0; }
    }
    if (m == WM_CHAR && (wp == VK_RETURN || wp == VK_ESCAPE)) return 0;
    return CallWindowProc(g_editOrig, h, m, wp, lp);
}

static int limitar(int v, int max) { return v < 0 ? 0 : v > max ? max : v; }

static void acerca(void) {
    MessageBox(g_win, T("Clima 1.0 para Windows\n\nDesarrollado por Javiera Navarrete.\n\n"
        "Pronóstico de Open-Meteo (CC BY 4.0) para las 346 comunas de Chile.\n"
        "Sin publicidad ni rastreo: sólo se consulta el pronóstico de la comuna elegida.\n\n"
        "F5 actualizar · rueda o arrastre para desplazar · Esc cierra el buscador"), TEXT("Acerca de Clima"), MB_OK | MB_ICONINFORMATION);
}

/* ------------------------------------------------------------------ ventana */
static LRESULT CALLBACK proc(HWND h, UINT m, WPARAM wp, LPARAM lp) {
    switch (m) {
    case WM_CREATE: {
        HFONT fe;
        g_busca = CreateWindowEx(WS_EX_CLIENTEDGE, TEXT("EDIT"), TEXT(""), WS_CHILD | WS_VISIBLE | ES_AUTOHSCROLL,
                                 10, 10, 300, 26, h, (HMENU)ID_BUSCA, g_inst, NULL);
        g_lista = CreateWindowEx(WS_EX_CLIENTEDGE, TEXT("LISTBOX"), NULL, WS_CHILD | WS_VSCROLL | LBS_NOTIFY | LBS_NOINTEGRALHEIGHT,
                                 10, 38, 300, 260, h, (HMENU)ID_LISTA, g_inst, NULL);
        fe = CreateFont(-15, 0, 0, 0, FW_NORMAL, 0, 0, 0, DEFAULT_CHARSET, 0, 0, CLEARTYPE_QUALITY, 0, TEXT("Tahoma"));
        SendMessage(g_busca, WM_SETFONT, (WPARAM)fe, TRUE); SendMessage(g_lista, WM_SETFONT, (WPARAM)fe, TRUE);
        g_editOrig = (WNDPROC)SetWindowLongPtr(g_busca, GWLP_WNDPROC, (LONG_PTR)proc_busca);
        SetTimer(h, T_ANIM, 66, NULL);
        SetTimer(h, T_REFRESCO, 15 * 60 * 1000, NULL);
        return 0;
    }
    case WM_SIZE: {
        int w = LOWORD(lp);
        MoveWindow(g_busca, 10, 10, w - 20 - 34, 26, TRUE);
        MoveWindow(g_lista, 10, 38, w - 20 - 34, 260, TRUE);
        InvalidateRect(h, NULL, FALSE);
        return 0;
    }
    case WM_TIMER:
        if (wp == T_ANIM && !IsIconic(h)) {
            RECT r; GetClientRect(h, &r); r.top = 40; InvalidateRect(h, &r, FALSE);
        } else if (wp == T_REFRESCO) actualizar();
        return 0;
    case WM_COMMAND:
        if (LOWORD(wp) == ID_BUSCA && HIWORD(wp) == EN_CHANGE) filtrar();
        else if (LOWORD(wp) == ID_LISTA && HIWORD(wp) == LBN_DBLCLK) elegir((int)SendMessage(g_lista, LB_GETCURSEL, 0, 0));
        return 0;
    case WM_DATOS: {
        char *j = (char *)lp;
        g_cargando = FALSE;
        if (j) {
            JNodo *n = json_leer(j);
            if (n && json_obj(n, "current")) { json_libera(g_datos); g_datos = n; g_viejo = FALSE; g_error[0] = 0; reg_guardar(j); }
            else { json_libera(n); strcpy(g_error, "Respuesta inesperada de Open-Meteo"); }
            free(j);
        } else strcpy(g_error, "Sin conexión: mostrando lo último guardado");
        InvalidateRect(h, NULL, FALSE);
        return 0;
    }
    case WM_ERASEBKGND: return 1;
    case WM_PAINT: {
        PAINTSTRUCT ps; RECT r; HDC dc = BeginPaint(h, &ps), mem; HBITMAP bm, ob; int w, hh;
        GetClientRect(h, &r); w = r.right; hh = r.bottom;
        mem = CreateCompatibleDC(dc); bm = CreateCompatibleBitmap(dc, w, hh); ob = (HBITMAP)SelectObject(mem, bm);
        pintar(mem, w, hh);
        /* botón «i» (acerca de) junto al buscador */
        elipse(mem, w - 38, 10, 26, 26, RGB(255, 255, 255));
        texto_en(mem, f_negrita, "i", w - 25, 13, RGB(0x2E, 0x5B, 0x96), DT_CENTER);
        BitBlt(dc, 0, 0, w, hh, mem, 0, 0, SRCCOPY);
        SelectObject(mem, ob); DeleteObject(bm); DeleteDC(mem);
        EndPaint(h, &ps);
        return 0;
    }
    case WM_LBUTTONDOWN: {
        RECT r; int x = (short)LOWORD(lp), y = (short)HIWORD(lp);
        GetClientRect(h, &r);
        if (x > r.right - 40 && y < 40) { acerca(); return 0; }
        ShowWindow(g_lista, SW_HIDE); SetFocus(h);
        g_pressX = x; g_pressY = y; g_baseS = g_scroll; g_baseH = g_scrollH;
        g_arrastra = (y + g_scroll >= g_yHoras && y + g_scroll <= g_yHoras + g_altoHoras) ? 2 : 1;
        SetCapture(h);
        return 0;
    }
    case WM_MOUSEMOVE:
        if (g_arrastra && (wp & MK_LBUTTON)) {
            int x = (short)LOWORD(lp), y = (short)HIWORD(lp);
            if (g_arrastra == 2 && abs(x - g_pressX) > abs(y - g_pressY)) g_scrollH = limitar(g_baseH - (x - g_pressX), g_scrollHMax);
            else g_scroll = limitar(g_baseS - (y - g_pressY), g_scrollMax);
            if (g_scroll == 0 && y - g_pressY > 140 && !g_cargando) actualizar();   /* tirar hacia abajo para actualizar */
            InvalidateRect(h, NULL, FALSE);
        }
        return 0;
    case WM_LBUTTONUP: g_arrastra = 0; ReleaseCapture(); return 0;
    case WM_MOUSEWHEEL:
        g_scroll = limitar(g_scroll - GET_WHEEL_DELTA_WPARAM(wp) / 2, g_scrollMax);
        InvalidateRect(h, NULL, FALSE);
        return 0;
    case WM_KEYDOWN:
        if (wp == VK_F5) actualizar();
        else if (wp == VK_DOWN || wp == VK_NEXT) g_scroll = limitar(g_scroll + (wp == VK_NEXT ? 300 : 40), g_scrollMax);
        else if (wp == VK_UP || wp == VK_PRIOR) g_scroll = limitar(g_scroll - (wp == VK_PRIOR ? 300 : 40), g_scrollMax);
        else if (wp == VK_RIGHT) g_scrollH = limitar(g_scrollH + 60, g_scrollHMax);
        else if (wp == VK_LEFT) g_scrollH = limitar(g_scrollH - 60, g_scrollHMax);
        else if (wp == VK_F1) acerca();
        InvalidateRect(h, NULL, FALSE);
        return 0;
    case WM_CHAR:
        if (wp >= 32) { SetFocus(g_busca); SendMessage(g_busca, WM_CHAR, wp, lp); }   /* escribir = buscar comuna */
        return 0;
    case WM_GETMINMAXINFO: ((MINMAXINFO *)lp)->ptMinTrackSize.x = 360; ((MINMAXINFO *)lp)->ptMinTrackSize.y = 480; return 0;
    case WM_DESTROY: PostQuitMessage(0); return 0;
    }
    return DefWindowProc(h, m, wp, lp);
}

int WINAPI WinMain(HINSTANCE inst, HINSTANCE prev, LPSTR cmd, int show) {
    WNDCLASS wc;
    MSG msg;
    int i;
    (void)prev; (void)cmd;
    g_inst = inst; g_t0 = GetTickCount();
    srand(g_t0);
    for (i = 0; i < 90; i++) { g_px[i] = rand() / (float)RAND_MAX; g_py[i] = rand() / (float)RAND_MAX; g_pv[i] = 0.5f + rand() / (float)RAND_MAX; }
    f_grande = CreateFont(-84, 0, 0, 0, FW_LIGHT, 0, 0, 0, DEFAULT_CHARSET, 0, 0, CLEARTYPE_QUALITY, 0, TEXT("Segoe UI Light"));
    f_media = CreateFont(-22, 0, 0, 0, FW_NORMAL, 0, 0, 0, DEFAULT_CHARSET, 0, 0, CLEARTYPE_QUALITY, 0, TEXT("Tahoma"));
    f_chica = CreateFont(-14, 0, 0, 0, FW_NORMAL, 0, 0, 0, DEFAULT_CHARSET, 0, 0, CLEARTYPE_QUALITY, 0, TEXT("Tahoma"));
    f_negrita = CreateFont(-14, 0, 0, 0, FW_BOLD, 0, 0, 0, DEFAULT_CHARSET, 0, 0, CLEARTYPE_QUALITY, 0, TEXT("Tahoma"));
    cargar_comunas();
    reg_leer();
    ZeroMemory(&wc, sizeof wc);
    wc.lpfnWndProc = proc; wc.hInstance = inst; wc.lpszClassName = CLASE;
    wc.hIcon = LoadIcon(inst, MAKEINTRESOURCE(IDI_CLIMA)); wc.hCursor = LoadCursor(NULL, IDC_ARROW);
    wc.style = CS_HREDRAW | CS_VREDRAW;
    if (!RegisterClass(&wc)) return 1;
    g_win = CreateWindow(CLASE, TEXT("Clima"), WS_OVERLAPPEDWINDOW, CW_USEDEFAULT, CW_USEDEFAULT, 440, 780, NULL, NULL, inst, NULL);
    if (!g_win) return 1;
    ShowWindow(g_win, show); UpdateWindow(g_win);
    actualizar();
    while (GetMessage(&msg, NULL, 0, 0) > 0) { TranslateMessage(&msg); DispatchMessage(&msg); }
    return (int)msg.wParam;
}
