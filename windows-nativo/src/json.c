/* Lector JSON mínimo en C89 para Clima de Windows. Desarrollado por Javiera Navarrete. */
#include <stdlib.h>
#include <string.h>
#include "json.h"

typedef struct { const char *s; } Lec;

static void ws(Lec *l) { while (*l->s && (unsigned char)*l->s <= ' ') l->s++; }
static JNodo *nuevo(JTipo t) { JNodo *n = (JNodo *)calloc(1, sizeof(JNodo)); if (n) n->tipo = t; return n; }
static JNodo *valor(Lec *l);

static void utf8(char **o, unsigned cp) {
    char *p = *o;
    if (cp < 0x80) *p++ = (char)cp;
    else if (cp < 0x800) { *p++ = (char)(0xC0 | cp >> 6); *p++ = (char)(0x80 | (cp & 63)); }
    else { *p++ = (char)(0xE0 | cp >> 12); *p++ = (char)(0x80 | (cp >> 6 & 63)); *p++ = (char)(0x80 | (cp & 63)); }
    *o = p;
}

static char *texto(Lec *l) {
    const char *ini = ++l->s;
    size_t cap = 0;
    char *r, *o;
    while (*l->s && *l->s != '"') { if (*l->s == '\\' && l->s[1]) l->s++; l->s++; cap++; }
    r = o = (char *)malloc(cap * 3 + 1);
    if (!r) return NULL;
    for (l->s = ini; *l->s && *l->s != '"'; l->s++) {
        if (*l->s != '\\') { *o++ = *l->s; continue; }
        l->s++;
        switch (*l->s) {
            case 'n': *o++ = '\n'; break;
            case 't': *o++ = '\t'; break;
            case 'r': case 'b': case 'f': break;
            case 'u': {
                unsigned cp = 0; int k;
                for (k = 1; k <= 4 && l->s[k]; k++) {
                    char c = l->s[k];
                    cp = cp * 16 + (c >= 'a' ? c - 'a' + 10 : c >= 'A' ? c - 'A' + 10 : c - '0');
                }
                l->s += 4; utf8(&o, cp); break;
            }
            default: *o++ = *l->s;
        }
    }
    *o = 0;
    if (*l->s == '"') l->s++;
    return r;
}

static JNodo *valor(Lec *l) {
    JNodo *n, **ult;
    ws(l);
    switch (*l->s) {
        case '{':
            n = nuevo(JN_OBJ); ult = &n->hijo; l->s++; ws(l);
            if (*l->s == '}') { l->s++; return n; }
            for (;;) {
                char *k; JNodo *v;
                ws(l); if (*l->s != '"') return n;
                k = texto(l); ws(l); if (*l->s == ':') l->s++;
                v = valor(l); if (!v) { free(k); return n; }
                v->clave = k; *ult = v; ult = &v->sig; ws(l);
                if (*l->s == ',') { l->s++; continue; }
                if (*l->s == '}') l->s++;
                return n;
            }
        case '[':
            n = nuevo(JN_LISTA); ult = &n->hijo; l->s++; ws(l);
            if (*l->s == ']') { l->s++; return n; }
            for (;;) {
                JNodo *v = valor(l); if (!v) return n;
                *ult = v; ult = &v->sig; ws(l);
                if (*l->s == ',') { l->s++; continue; }
                if (*l->s == ']') l->s++;
                return n;
            }
        case '"': n = nuevo(JN_TXT); n->txt = texto(l); return n;
        case 't': l->s += 4; n = nuevo(JN_BOOL); n->num = 1; return n;
        case 'f': l->s += 5; return nuevo(JN_BOOL);
        case 'n': l->s += 4; return nuevo(JN_NULO);
        case 0: return NULL;
        default: {
            char *fin;
            n = nuevo(JN_NUM); n->num = strtod(l->s, &fin);
            if (fin == l->s) { free(n); return NULL; }
            l->s = fin; return n;
        }
    }
}

JNodo *json_leer(const char *s) { Lec l; l.s = s; return s ? valor(&l) : NULL; }

void json_libera(JNodo *n) {
    while (n) { JNodo *s = n->sig; json_libera(n->hijo); free(n->txt); free(n->clave); free(n); n = s; }
}

JNodo *json_obj(const JNodo *o, const char *clave) {
    JNodo *h;
    if (!o || o->tipo != JN_OBJ) return NULL;
    for (h = o->hijo; h; h = h->sig) if (h->clave && !strcmp(h->clave, clave)) return h;
    return NULL;
}
JNodo *json_idx(const JNodo *lista, int i) {
    JNodo *h;
    if (!lista || lista->tipo != JN_LISTA) return NULL;
    for (h = lista->hijo; h && i > 0; h = h->sig) i--;
    return h;
}
int json_largo(const JNodo *lista) { int n = 0; JNodo *h; if (!lista) return 0; for (h = lista->hijo; h; h = h->sig) n++; return n; }
double json_num(const JNodo *o, const char *clave, double def) { JNodo *v = json_obj(o, clave); return v && v->tipo == JN_NUM ? v->num : def; }
double json_num_i(const JNodo *l, int i, double def) { JNodo *v = json_idx(l, i); return v && v->tipo == JN_NUM ? v->num : def; }
const char *json_txt(const JNodo *o, const char *clave) { JNodo *v = json_obj(o, clave); return v && v->tipo == JN_TXT ? v->txt : ""; }
const char *json_txt_i(const JNodo *l, int i) { JNodo *v = json_idx(l, i); return v && v->tipo == JN_TXT ? v->txt : ""; }
