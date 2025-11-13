#include "../../index.h"
#include "../../bucket.h"

// ============================================================================
// ESTRUCTURAS DE DATOS PARA FQT (Fixed Queries Tree)
// ============================================================================
// Paper: "FQT utilizes the same pivot at the same level"
// Paper: "FQT is an unbalanced tree"
// ============================================================================

// Nodo del FQT - puede ser hoja o interno
// Paper: "FQT is an unbalanced tree" - hojas pueden estar a diferentes niveles
typedef struct sfqnode
   { bool hoja;
     union
        { struct
             { void *bucket;  // Almacena objetos en nodos hoja
               int size;
             } hoja;
          struct
             { void *children;  // Array de hijos (Tchild) en nodos internos
             } interno;
        } u;
   } fqvpnode;

// Estructura de un hijo en nodo interno
// Paper: "maintains the objects having the distance i to the pivot in its i-th sub-tree"
// Paper: "each sub-tree covers a range of distance values"
typedef struct
   { Tdist dist;  /* [dist,dist sgte) - rango de distancias que cubre este sub-árbol */
     fqvpnode child;
   } Tchild;

// Estructura de query/pivote
// Paper: "FQT utilizes the same pivot at the same level"
// Paper: "p_i ∈ P is set as the pivot for the i-th level"
typedef struct
   { Obj query;   // El pivote para un nivel específico
     Tdist dist;  // Distancia del query actual a este pivote (calculada durante búsqueda)
   } query;

// Estructura principal del FQT
// Paper: "The construction cost of FQT is O(n*l)"
// Paper: "The storage cost of FQT is O(n*s + n*l)"
typedef struct
   { query *queries;  // Array de pivotes, uno por nivel (tamaño = height)
     int height;      // Paper: "l is the height of FQT" (número de niveles = número de pivotes)
     int bsize;       // Tamaño máximo de bucket (hojas)
     int arity;       // Número de hijos por nodo (factor de ramificación)
      int n;          // Número total de objetos en el índice
     fqvpnode node;   // Nodo raíz del árbol
     char *descr;     // Descripción/nombre de la base de datos
   } fqvpt;

// Macro para acceder al i-ésimo hijo de un nodo interno
#define child(node,i) (((Tchild*)((node)->u.interno.children))[i])

// Estructura temporal para construcción del árbol
// Almacena un objeto y su distancia al pivote del nivel actual
typedef struct
   { Obj obj;
     Tdist dist;
   } Tod;


