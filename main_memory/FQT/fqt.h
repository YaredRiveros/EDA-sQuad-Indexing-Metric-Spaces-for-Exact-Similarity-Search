#ifndef FQT_H
#define FQT_H

/* Encabezado corregido para FQ-Tree (FQVP tree).
 * Mantiene las mismas dependencias originales. Asegúrate que
 * ../../index.h y ../../bucket.h declaren Obj, Tdist, Index, etc.
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdbool.h>
#include <errno.h>

#include "../../index.h"
#include "../../bucket.h"

/* Nodo del árbol FQ (hoja o interno). Se conserva la unión original. */
typedef struct sfqnode {
    bool hoja;
    union {
        struct {
            void *bucket;    /* bucket creado por bucket.h */
            int size;        /* número de objetos en el bucket */
        } hoja;
        struct {
            void *children;  /* puntero a array de Tchild (se castea) */
        } interno;
    } u;
} fqvpnode;

/* Un hijo contiene el límite (dist) y el nodo hijo. 
 * Nota: el diseño original tenía fqvpnode por valor dentro de Tchild.
 * Lo dejamos por compatibilidad (evita cambiar API externa).
 */
typedef struct {
    Tdist dist;     /* [dist, dist siguiente) */
    fqvpnode child; /* subárbol */
} Tchild;

/* Query del nivel: almacena un objeto pivote y su distancia calculada. */
typedef struct {
    Obj query;
    Tdist dist;
} query;

/* Estructura principal del índice FQ */
typedef struct {
    query *queries;     /* array dinámico de queries (pivotes por nivel) */
    int height;         /* número de pivotes (niveles) */
    int bsize;          /* tamaño de bucket */
    int arity;          /* aridad del árbol */
    int n;              /* número de objetos en la BD */
    fqvpnode node;      /* nodo raíz (por valor, como en el original) */
    char *descr;        /* descripción / nombre BD */
} fqvpt;

#define child(node,i) (((Tchild*)((node)->u.interno.children))[i])

/* Entrada auxiliar para ordenamiento (objeto + distancia) */
typedef struct {
    Obj obj;
    Tdist dist;
} Tod;

#endif /* FQT_H */
