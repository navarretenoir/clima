/* Lector JSON mínimo en C89 (sin dependencias) para Clima de Windows. Desarrollado por Javiera Navarrete. */
#ifndef CLIMA_JSON_H
#define CLIMA_JSON_H

typedef enum { JN_NULO, JN_BOOL, JN_NUM, JN_TXT, JN_LISTA, JN_OBJ } JTipo;

typedef struct JNodo {
    JTipo tipo;
    double num;
    char *txt;                  /* UTF-8 (JN_TXT) */
    char *clave;                /* si es miembro de un objeto */
    struct JNodo *hijo, *sig;   /* lista/objeto: primer hijo; hermanos */
} JNodo;

JNodo *json_leer(const char *s);
void json_libera(JNodo *n);
JNodo *json_obj(const JNodo *o, const char *clave);
JNodo *json_idx(const JNodo *lista, int i);
int json_largo(const JNodo *lista);
double json_num(const JNodo *o, const char *clave, double def);
double json_num_i(const JNodo *lista, int i, double def);
const char *json_txt(const JNodo *o, const char *clave);
const char *json_txt_i(const JNodo *lista, int i);

#endif
