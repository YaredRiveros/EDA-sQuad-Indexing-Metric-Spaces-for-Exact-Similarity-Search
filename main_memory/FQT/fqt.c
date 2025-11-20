/*
 * fqt.c - Implementación corregida del FQ-Tree.
 *
 * Correcciones principales aplicadas:
 *  - Abrir ficheros en modo binario ("rb"/"wb").
 *  - Usar sizeof(...) correcto al serializar Obj (queries[i].query).
 *  - Lectura segura de la cadena descr (control EOF / tamaño dinámico).
 *  - Validación de bsize y arity en build().
 *  - Comprobación de retornos de malloc/realloc/fopen.
 *  - Cálculo de step/prev más robusto (prev = min; cast a Tdist).
 *  - Manejo de errores y prints de diagnóstico donde procede.
 *
 * IMPORTANTE: este fichero asume que ../../index.h y ../../bucket.h
 * declaran Obj, Tdist, Index, Tod, y las funciones (openDB, closeDB,
 * createbucket, addbucket, savebucket, loadbucket, freebucket,
 * distance, searchbucket, searchbucketNN, createCelem, addCelem,
 * radCelem, showCelem, freeCelem, printobj, etc.). Si alguno difiere,
 * adapta las llamadas correspondientes.
 */

#include "fqt.h"

void prnstats (Index S); /* prototipo local */

/* compar para qsort: ordena por campo dist */
int compar (const void *a, const void *b)
{
    const Tod *x = (const Tod *) a;
    const Tod *y = (const Tod *) b;
    if (x->dist > y->dist) return 1;
    if (x->dist < y->dist) return -1;
    return 0;
}

/* Prototipo interno */
static fqvpnode buildfqvpt (fqvpt *tree, Tod *od, int nobjs, int depth);

/* Construye recursivamente un nodo (devuelve por valor como en original) */
static fqvpnode buildfqvpt (fqvpt *tree, Tod *od, int nobjs, int depth)
{
    int i, per, pptr;
    Tdist prev, max, min, step;
    fqvpnode node;

    node.hoja = (nobjs <= tree->bsize);

    if (node.hoja) {
        /* Hoja: crear bucket e insertar objetos */
        node.u.hoja.bucket = createbucket();
        if (!node.u.hoja.bucket) {
            fprintf(stderr, "createbucket() failed\n");
            node.u.hoja.size = 0;
            return node;
        }
        node.u.hoja.size = 0;
        for (i = 0; i < nobjs; ++i) {
            node.u.hoja.bucket = addbucket(node.u.hoja.bucket, node.u.hoja.size, od[i].obj);
            node.u.hoja.size++;
        }
    } else {
        /* Interno */
        if (nobjs == 0) {
            node.u.interno.children = NULL;
            return node;
        }

        node.u.interno.children = malloc(tree->arity * sizeof(Tchild));
        if (!node.u.interno.children) {
            perror("malloc children");
            node.u.interno.children = NULL;
            node.hoja = true; /* fallback: marcar hoja vacía */
            node.u.hoja.bucket = NULL;
            node.u.hoja.size = 0;
            return node;
        }

        /* Si no hay pivote en nivel 'depth', añadimos uno */
        if (tree->height <= depth) {
            /* realloc en bloques de 1000 para reducir resonancias */
            int newcap = tree->height + 1000;
            query *tmp = realloc(tree->queries, newcap * sizeof(query));
            if (!tmp) {
                perror("realloc queries");
                /* liberamos children y salimos */
                free(node.u.interno.children);
                node.u.interno.children = NULL;
                node.hoja = true;
                node.u.hoja.bucket = NULL;
                node.u.hoja.size = 0;
                return node;
            }
            tree->queries = tmp;
            /* añadimos el último objeto como pivote (como hacía el original) */
            tree->queries[tree->height++].query = od[--nobjs].obj;
        }

        /* calc distancia desde el pivote de este nivel */
        for (i = 0; i < nobjs; ++i) {
            od[i].dist = distance(tree->queries[depth].query, od[i].obj);
        }

        qsort(od, nobjs, sizeof(Tod), compar);

        min = od[0].dist;
        max = od[nobjs - 1].dist;

        /* prevenir división por cero; arity validada en build() */
        if (tree->arity <= 0) {
            fprintf(stderr, "buildfqvpt: invalid arity %d\n", tree->arity);
            /* fallback: hacer una sola partición */
            step = 0;
        } else {
            /* usar cast para evitar división entera si Tdist es entero */
            step = (Tdist)((double)(max - min) / (double)tree->arity);
        }

        /* repartir intervalos dentro de [min, max] */
        prev = min;
        per = 0;
        pptr = 0;
        for (i = 0; i < tree->arity; ++i) {
            child(&node, i).dist = prev;
            prev += step;
            if (i < tree->arity - 1) {
                /* avanzar per hasta que od[per].dist >= prev
                 * cuidado con per >= nobjs
                 */
                while (per < nobjs && od[per].dist < prev) per++;
            } else {
                per = nobjs;
            }
            /* construir recursivamente el hijo con el subarray od+pptr ... od+per-1 */
            child(&node, i).child = buildfqvpt(tree, od + pptr, per - pptr, depth + 1);
            pptr = per;
        }
    }

    return node;
}

/* Construye el índice: interfaz principal */
Index build (char *dbname, int n, int *argc, char ***argv)
{
    fqvpt *tree;
    int i, k;
    Tod *od;
    int nobjs;

    if (*argc < 2) {
        fprintf(stderr, "Usage: <program> <args> BUCKET-SIZE ARITY\n");
        exit(1);
    }

    tree = malloc(sizeof(fqvpt));
    if (!tree) {
        perror("malloc tree");
        exit(1);
    }
    memset(tree, 0, sizeof(fqvpt));

    tree->descr = malloc(strlen(dbname) + 1);
    if (!tree->descr) {
        perror("malloc descr");
        free(tree);
        exit(1);
    }
    strcpy(tree->descr, dbname);

    tree->n = openDB(dbname);
    if (tree->n < 0) {
        fprintf(stderr, "openDB failed for %s\n", dbname);
        free(tree->descr);
        free(tree);
        exit(1);
    }
    if (n && (n < tree->n)) tree->n = n;
    nobjs = tree->n;

    /* leer bsize y arity y validarlos */
    tree->bsize = atoi((*argv)[0]);
    tree->arity = atoi((*argv)[1]);
    if (tree->bsize <= 0) {
        fprintf(stderr, "Invalid BUCKET-SIZE: %d\n", tree->bsize);
        free(tree->descr);
        free(tree);
        exit(1);
    }
    if (tree->arity <= 0) {
        fprintf(stderr, "Invalid ARITY: %d\n", tree->arity);
        free(tree->descr);
        free(tree);
        exit(1);
    }

    tree->queries = NULL;
    tree->height = 0;

    *argc -= 2;
    *argv += 2;

    od = malloc(nobjs * sizeof(Tod));
    if (!od) {
        perror("malloc od");
        free(tree->descr);
        free(tree);
        exit(1);
    }

    k = 0;
    i = 0;
    /* llenar objetos (asumiendo que la DB enumera objetos 1..n) */
    while (k < tree->n) {
        od[i++].obj = ++k;
    }

    tree->node = buildfqvpt(tree, od, nobjs, 0);

    free(od);

    prnstats((Index)tree);

    return (Index)tree;
}

/* Liberar recursivamente */
static void freefqvpt (fqvpnode *node, int arity)
{
    if (node->hoja) {
        if (node->u.hoja.bucket) freebucket(node->u.hoja.bucket, node->u.hoja.size);
    } else {
        int i;
        if (node->u.interno.children == NULL) return;
        for (i = 0; i < arity; ++i) {
            freefqvpt(&child(node, i).child, arity);
        }
        free(node->u.interno.children);
    }
}

/* Liberar índice */
void freeIndex (Index S, bool libobj)
{
    fqvpt *tree = (fqvpt*)S;
    if (!tree) return;
    if (tree->descr) free(tree->descr);
    if (tree->queries) free(tree->queries);
    freefqvpt(&tree->node, tree->arity);
    free(tree);
    if (libobj) closeDB();
}

/* Guarda un nodo de forma recursiva (binario) */
static void savenode (fqvpnode *node, FILE *f, int arity)
{
    int i;
    unsigned char hoja = node->hoja ? 1 : 0;
    if (fwrite(&hoja, sizeof(hoja), 1, f) != 1) {
        perror("savenode: fwrite hoja");
        return;
    }

    if (node->hoja) {
        if (fwrite(&node->u.hoja.size, sizeof(int), 1, f) != 1) {
            perror("savenode: fwrite size");
            return;
        }
        savebucket(node->u.hoja.bucket, node->u.hoja.size, f);
    } else {
        for (i = 0; i < arity; ++i) {
            if (fwrite(&child(node, i).dist, sizeof(Tdist), 1, f) != 1) {
                perror("savenode: fwrite dist");
                return;
            }
            savenode(&child(node, i).child, f, arity);
        }
    }
}

/* Guardar índice en fichero (binario) */
void saveIndex (Index S, char *fname)
{
    FILE *f = fopen(fname, "wb"); /* modo binario */
    if (!f) {
        perror("saveIndex: fopen");
        return;
    }

    fqvpt *tree = (fqvpt*)S;
    int i;

    /* escribir descr incluyendo '\0' */
    if (fwrite(tree->descr, strlen(tree->descr) + 1, 1, f) != 1) {
        perror("saveIndex: fwrite descr");
        fclose(f);
        return;
    }

    if (fwrite(&tree->n, sizeof(int), 1, f) != 1) { perror("fwrite n"); fclose(f); return; }
    if (fwrite(&tree->bsize, sizeof(int), 1, f) != 1) { perror("fwrite bsize"); fclose(f); return; }
    if (fwrite(&tree->arity, sizeof(int), 1, f) != 1) { perror("fwrite arity"); fclose(f); return; }
    if (fwrite(&tree->height, sizeof(int), 1, f) != 1) { perror("fwrite height"); fclose(f); return; }

    /* escribir queries: sólo la parte query.query (Obj), usar sizeof real */
    for (i = 0; i < tree->height; ++i) {
        if (fwrite(&tree->queries[i].query, sizeof(tree->queries[i].query), 1, f) != 1) {
            perror("saveIndex: fwrite query");
            fclose(f);
            return;
        }
    }

    savenode(&tree->node, f, tree->arity);

    fclose(f);
}

/* Carga recursiva de un nodo */
static void loadnode (fqvpnode *node, FILE *f, int arity)
{
    int i;
    int c = getc(f);
    if (c == EOF) {
        fprintf(stderr, "loadnode: unexpected EOF while reading hoja flag\n");
        node->hoja = false;
        node->u.interno.children = NULL;
        return;
    }
    node->hoja = (c != 0);

    if (node->hoja) {
        if (fread(&node->u.hoja.size, sizeof(int), 1, f) != 1) {
            fprintf(stderr, "loadnode: fread size failed\n");
            node->u.hoja.size = 0;
            node->u.hoja.bucket = NULL;
            return;
        }
        node->u.hoja.bucket = loadbucket(node->u.hoja.size, f);
    } else {
        node->u.interno.children = malloc(arity * sizeof(Tchild));
        if (!node->u.interno.children) {
            perror("loadnode: malloc children");
            node->u.interno.children = NULL;
            return;
        }
        for (i = 0; i < arity; ++i) {
            if (fread(&child(node, i).dist, sizeof(Tdist), 1, f) != 1) {
                fprintf(stderr, "loadnode: fread dist failed\n");
                /* intentar continuar pero marcar hijo vacío */
                child(node, i).dist = 0;
                child(node, i).child.u.interno.children = NULL;
                continue;
            }
            loadnode(&child(node, i).child, f, arity);
        }
    }
}

/* Cargar índice desde fichero binario */
Index loadIndex (char *fname)
{
    FILE *f = fopen(fname, "rb");
    if (!f) {
        perror("loadIndex: fopen");
        return NULL;
    }

    fqvpt *tree = malloc(sizeof(fqvpt));
    if (!tree) {
        perror("loadIndex: malloc tree");
        fclose(f);
        return NULL;
    }
    memset(tree, 0, sizeof(fqvpt));

    /* leer descr dinámicamente (hasta '\0') con protección de tamaño */
    size_t cap = 256;
    size_t len = 0;
    char *buf = malloc(cap);
    if (!buf) { perror("malloc buf"); fclose(f); free(tree); return NULL; }

    int ch;
    while ((ch = getc(f)) != EOF) {
        buf[len++] = (char)ch;
        if (buf[len - 1] == '\0') break;
        if (len + 1 >= cap) {
            cap *= 2;
            char *tmp = realloc(buf, cap);
            if (!tmp) { perror("realloc buf"); free(buf); free(tree); fclose(f); return NULL; }
            buf = tmp;
        }
    }
    if (ch == EOF && (len == 0 || buf[len - 1] != '\0')) {
        fprintf(stderr, "loadIndex: unexpected EOF while reading descr\n");
        free(buf);
        free(tree);
        fclose(f);
        return NULL;
    }

    tree->descr = malloc(len);
    if (!tree->descr) { perror("malloc descr2"); free(buf); free(tree); fclose(f); return NULL; }
    memcpy(tree->descr, buf, len);
    free(buf);

    /* leer parámetros */
    if (fread(&tree->n, sizeof(int), 1, f) != 1) { perror("fread n"); freeIndex((Index)tree, false); fclose(f); return NULL; }
    if (fread(&tree->bsize, sizeof(int), 1, f) != 1) { perror("fread bsize"); freeIndex((Index)tree, false); fclose(f); return NULL; }
    if (fread(&tree->arity, sizeof(int), 1, f) != 1) { perror("fread arity"); freeIndex((Index)tree, false); fclose(f); return NULL; }
    if (fread(&tree->height, sizeof(int), 1, f) != 1) { perror("fread height"); freeIndex((Index)tree, false); fclose(f); return NULL; }

    /* validar arity/bsize leídos */
    if (tree->arity <= 0 || tree->bsize <= 0) {
        fprintf(stderr, "loadIndex: invalid arity/bsize read from file\n");
        freeIndex((Index)tree, false);
        fclose(f);
        return NULL;
    }

    /* reservar queries y leer los Obj */
    tree->queries = malloc(tree->height * sizeof(query));
    if (!tree->queries && tree->height > 0) {
        perror("malloc queries loadIndex");
        freeIndex((Index)tree, false);
        fclose(f);
        return NULL;
    }
    for (int i = 0; i < tree->height; ++i) {
        if (fread(&tree->queries[i].query, sizeof(tree->queries[i].query), 1, f) != 1) {
            fprintf(stderr, "loadIndex: fread query failed\n");
            freeIndex((Index)tree, false);
            fclose(f);
            return NULL;
        }
        tree->queries[i].dist = 0; /* se calculará en tiempo de búsqueda */
    }

    loadnode(&tree->node, f, tree->arity);

    fclose(f);

    /* abrir la base de datos (no cargar objetos) */
    if (openDB(tree->descr) < 0) {
        fprintf(stderr, "loadIndex: openDB failed for %s\n", tree->descr);
        /* no liberamos tree aquí — el llamante decide */
    }

    return (Index)tree;
}

/* Búsqueda auxiliar (rango). Implementación consolidada y segura */
static int _search (fqvpt *tree, fqvpnode *node, Obj obj, Tdist r, int depth, int arity, bool show)
{
    int rep = 0;
    if (node->hoja) {
        rep += searchbucket(node->u.hoja.bucket, node->u.hoja.size, obj, r, show);
    } else {
        int i;
        Tdist dist;
        if (node->u.interno.children == NULL) return rep;
        /* calcular distancia del pivote en este nivel (se almacena en tree->queries[depth].dist por caller) */
        dist = tree->queries[depth].dist;
        for (i = 0; i < arity; ++i) {
            Tdist nextDist = (i + 1 < arity) ? child(node, i + 1).dist : (Tdist) (dist + r + 1);
            if (((i == arity - 1) || (nextDist > dist - r)) && (child(node, i).dist <= dist + r)) {
                rep += _search(tree, &child(node, i).child, obj, r, depth + 1, arity, show);
            }
        }
    }
    return rep;
}

/* Interfaz pública de búsqueda (rango) */
int search (Index S, Obj obj, Tdist r, int show)
{
    fqvpt *tree = (fqvpt*)S;
    if (!tree) return 0;

    int rep = 0;
    for (int i = 0; i < tree->height; ++i) {
        tree->queries[i].dist = distance(obj, tree->queries[i].query);
        if (tree->queries[i].dist <= r) {
            rep++;
            if (show) printobj(tree->queries[i].query);
        }
    }
    rep += _search(tree, &tree->node, obj, r, 0, tree->arity, show);
    return rep;
}

/* Búsqueda k-NN auxiliar (recursiva) */
static void _searchNN (fqvpt *tree, fqvpnode *node, Obj obj, Tcelem *res, int depth)
{
    int arity = tree->arity;

    if (node->hoja) {
        searchbucketNN(node->u.hoja.bucket, node->u.hoja.size, obj, res);
    } else {
        int ci, i, d;
        Tdist dist;
        bool ea, eb;
        if (node->u.interno.children == NULL) return;
        dist = tree->queries[depth].dist;

        /* encontrar el primer child con child.dist > dist */
        for (ci = 0; ci < arity; ++ci) {
            if (child(node, ci).dist > dist) break;
        }
        ci--;
        ea = eb = false;

        /* explorar en orden creciente de distancia hacia fuera desde ci */
        for (d = 0; d <= arity; ++d) {
            /* lado izquierdo */
            i = ci - d;
            if (i < 0) ea = true;
            if (!ea) {
                if ((i == arity - 1) || (radCelem(res) == -1) || (dist - child(node, i + 1).dist <= radCelem(res))) {
                    _searchNN(tree, &child(node, i).child, obj, res, depth + 1);
                } else ea = true;
            }
            if (d == 0) continue;

            /* lado derecho */
            i = ci + d;
            if (i >= arity) eb = true;
            if (!eb) {
                if ((radCelem(res) == -1) || (child(node, i).dist - dist <= radCelem(res))) {
                    _searchNN(tree, &child(node, i).child, obj, res, depth + 1);
                } else eb = true;
            }
        }
    }
}

/* Interfaz pública k-NN */
Tdist searchNN (Index S, Obj obj, int k, bool show)
{
    fqvpt *tree = (fqvpt*)S;
    if (!tree) return (Tdist)-1;

    Tdist mdif;
    Tcelem res = createCelem(k);

    for (int i = 0; i < tree->height; ++i) {
        tree->queries[i].dist = distance(obj, tree->queries[i].query);
        addCelem(&res, tree->queries[i].query, tree->queries[i].dist);
    }

    _searchNN(tree, &tree->node, obj, &res, 0);

    if (show) showCelem(&res);
    mdif = radCelem(&res);
    freeCelem(&res);
    return mdif;
}

/* Estadísticas / utilidades recursivas */
static int numnodes (fqvpnode *node, int depth, int arity)
{
    int i, ret = 1;
    if (node->hoja) return 0;
    if (node->u.interno.children == NULL) return 0;
    for (i = 0; i < arity; ++i) ret += numnodes(&child(node, i).child, depth - 1, arity);
    return ret;
}

static int numbucks (fqvpnode *node, int depth, int arity)
{
    int i, ret = 0;
    if (node->hoja) return 1;
    if (node->u.interno.children == NULL) return 0;
    for (i = 0; i < arity; ++i) ret += numbucks(&child(node, i).child, depth - 1, arity);
    return ret;
}

static int sizebucks (fqvpnode *node, int depth, int arity)
{
    int i, ret = 0;
    if (node->hoja) return node->u.hoja.size;
    if (node->u.interno.children == NULL) return 0;
    for (i = 0; i < arity; ++i) ret += sizebucks(&child(node, i).child, depth - 1, arity);
    return ret;
}

/* Imprime estadísticas del índice */
void prnstats (Index S)
{
    fqvpt *tree = (fqvpt*)S;
    if (!tree) return;

    int nbucks = numbucks(&tree->node, tree->height, tree->arity);
    int sbucks = sizebucks(&tree->node, tree->height, tree->arity);
    printf("number of elements: %i\n", sbucks);
    printf("height: %i\n", tree->height);
    printf("arity: %i\n", tree->arity);
    printf("number of nodes: %i\n", numnodes(&tree->node, tree->height, tree->arity));
    printf("number of buckets: %i\n", nbucks);
    if (nbucks > 0) printf("average size of bucket: %0.2f\n", sbucks / (double)nbucks);
    else printf("average size of bucket: 0.00\n");
}
