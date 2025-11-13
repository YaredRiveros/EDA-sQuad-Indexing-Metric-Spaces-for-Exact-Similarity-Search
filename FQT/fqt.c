/**
   This program is free software; you can redistribute it and/or modify
   it under the terms of the GNU General Public License as published by
   the Free Software Foundation; either version 3 of the License, or
   (at your option) any later version.

   This program is distributed in the hope that it will be useful,
   but WITHOUT ANY WARRANTY; without even the implied warranty of
   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
   GNU General Public License for more details.

   You should have received a copy of the GNU General Public License
   along with this program.  If not, see <http://www.gnu.org/licenses/>.
**/

// ============================================================================
// IMPLEMENTACIÓN DE FQT (Fixed Queries Tree)
// ============================================================================
// Paper: "Unlike BKT, FQT utilizes the same pivot at the same level"
// Paper: "FQT is an unbalanced tree"
// 
// DIFERENCIAS CON BKT:
// - BKT: usa pivotes diferentes aleatoriamente en cada sub-árbol
// - FQT: usa el MISMO pivote en todos los nodos del mismo nivel
//
// CARACTERÍSTICAS:
// - Árbol diseñado para funciones de distancia discretas (pero puede extenderse a continuas)
// - Paper: "p_i ∈ P is set as the pivot for the i-th level"
// - Estructura desbalanceada donde diferentes ramas pueden tener alturas distintas
//
// COMPLEJIDADES:
// - Construcción: O(n*l) donde n=objetos, l=altura (número de pivotes)
// - Almacenamiento: O(n*s + n*l)
// - Paper: "With well-chosen pivots, FQT is expected to perform better than BKT"
//
// RELACIÓN CON OTROS ÍNDICES:
// - FHQT: versión balanceada de FQT (todos los objetos en hojas al mismo nivel)
// - FQA: representación como tabla de FHQT (equivalente a LAESA)
// ============================================================================

#include "fqt.h"

void prnstats (Index S);

// Función de comparación para ordenar objetos por distancia al pivote
// Necesaria para particionar objetos en sub-árboles según rangos de distancia
int compar (const void  *a, const void *b)

   {
    Tod *x,*y;
    x = (Tod *) a;
    y = (Tod *) b;
	
    if (x->dist > y->dist) return 1;
     else if (x->dist < y->dist) return -1;
     else return 0;
   }

// Construcción recursiva del FQT
// Paper: "The construction cost of FQT is O(n*l)"
// Paper: "FQT utilizes the same pivot at the same level"
static fqvpnode buildfqvpt (fqvpt *tree, Tod *od, int nobjs, int depth)

   { int i,per,pptr;
     Tdist prev,max,min,step;
     fqvpnode node;

     // Crear nodo hoja si el número de objetos es menor o igual al tamaño de bucket
     // Paper: "FQT is an unbalanced tree" - las hojas pueden estar a diferentes niveles
     node.hoja = (nobjs <= tree->bsize);
     if (node.hoja)
        { node.u.hoja.bucket = createbucket();
	  node.u.hoja.size = 0;
	  for (i=0;i<nobjs;i++) 
	     { node.u.hoja.bucket = addbucket (node.u.hoja.bucket,
					node.u.hoja.size, od[i].obj);
	       node.u.hoja.size++;
	     }
	}
     else
        { if (nobjs == 0)
	     { node.u.interno.children = NULL;
	       return node;
	     }
          node.u.interno.children = malloc (tree->arity * sizeof(Tchild));
          // Paper: "FQT utilizes the same pivot at the same level"
          // Paper: "p_i ∈ P is set as the pivot for the i-th level"
          // Seleccionar pivote para este nivel (depth) - se usa el mismo en todos los nodos de este nivel
          if (tree->height <= depth)  /* add a new query */
             { 
	       if ((tree->height % 1000) == 0)
		  tree->queries = realloc (tree->queries,
                                             (tree->height+1000)*sizeof(query));
	       // El pivote para el nivel depth es un objeto de los disponibles
	       tree->queries[tree->height++].query = od[--nobjs].obj;
             }
	  // Calcular distancias de todos los objetos al pivote del nivel actual
	  // Paper: "It chooses a pivot as the root, and maintains the objects having the distance i to the pivot in its i-th sub-tree"
	  for (i=0;i<nobjs;i++) 
	      od[i].dist = distance (tree->queries[depth].query, od[i].obj);
          qsort (od,nobjs,sizeof(Tod),compar);

          min = od[0].dist; max = od[nobjs-1].dist;

          // Paper: "The continuous distance range can be partitioned into discrete ranges used for indexing"
          // Dividir el rango de distancias [min, max] en 'arity' sub-rangos
          // Cada sub-árbol cubre un rango de distancias [dist, dist_siguiente)
          step = (max-min)/tree->arity;
          prev = 0; per = 0; pptr = 0;
          for (i=0;i<tree->arity;i++)
              { child(&node,i).dist = prev;
                prev += step;
                if (i < tree->arity-1) { while (od[per].dist < prev) per++; }
                else per = nobjs;
                // Construcción recursiva: cada sub-árbol contiene objetos en su rango de distancia
                child(&node,i).child = 
		    buildfqvpt (tree,od+pptr,per-pptr,depth+1);
                pptr = per;
              }
	}
     return node;
   }


// Construcción del índice FQT
// Paper: "The construction cost of FQT is O(n*l), where l is the height of FQT"
// Paper: "The height of FQT is set to the number of pivots"
Index build (char *dbname, int n, int *argc, char ***argv)


   { fqvpt *tree;
     int i,k;
     Tod *od;
     int nobjs;
     if (*argc < 2)
        { fprintf (stderr,"Usage: <program> <args> BUCKET-SIZE ARITY\n");
          exit(1);
        }
     tree = malloc (sizeof(fqvpt));
     tree->descr = malloc (strlen(dbname)+1);
     strcpy (tree->descr,dbname);
     
     tree->n = openDB(dbname);
     if (n && (n < tree->n)) tree->n = n;          
     nobjs = tree->n;
     // bsize: tamaño máximo de bucket (determina cuándo crear hojas)
     tree->bsize = atoi((*argv)[0]);
     // arity: número de hijos por nodo (factor de ramificación)
     // Paper: determina en cuántos rangos de distancia se divide cada nivel
     tree->arity = atoi((*argv)[1]);
     tree->queries = NULL;
     // height: número de niveles = número de pivotes usados
     // Paper: "l is the height of FQT"
     tree->height = 0; 
     *argc -= 2; *argv += 2;
     od = malloc (nobjs * sizeof (Tod));
     k=0; i=0;
     while (k < tree->n) od[i++].obj = ++k; 
     tree->node = buildfqvpt (tree,od,nobjs,0);
     free (od);
     prnstats((Index)tree); 
     return (Index)tree;
   }

static void freefqvpt (fqvpnode *node, int arity)

   { if (node->hoja) freebucket (node->u.hoja.bucket,node->u.hoja.size);
     else 
	{ int i;
          if (node->u.interno.children == NULL) return;
	  for (i=0;i<arity;i++)
	      freefqvpt (&child(node,i).child,arity);
          free (node->u.interno.children);
	}
   }

void freeIndex (Index S, bool libobj)

   { fqvpt *tree = (fqvpt*)S;
     free (tree->descr);
     free (tree->queries);
     freefqvpt (&tree->node,tree->arity);
     free (tree);
     if (libobj) closeDB();
   }

// Guardar nodo del FQT recursivamente
// Paper: "The storage cost of FQT is O(n*s + n*l)"
static void savenode (fqvpnode *node, FILE *f, int arity)

   { int i;
     char hoja = node->hoja;
     putc (hoja,f);
     if (node->hoja)
	{ fwrite (&node->u.hoja.size,sizeof(int),1,f);
	  savebucket (node->u.hoja.bucket,node->u.hoja.size,f);
	}
     else
	{ // Guardar rangos de distancia y sub-árboles
	  for (i=0;i<arity;i++)
	      { fwrite (&child(node,i).dist, sizeof(Tdist),1,f);
		savenode(&child(node,i).child,f,arity);
	      }
	}
   }

// Guardar índice FQT completo en disco
// Paper: "The storage cost of FQT is O(n*s + n*l)"
// - O(n*s): almacenar n objetos de tamaño s
// - O(n*l): almacenar estructura del árbol con l niveles
void saveIndex (Index S, char *fname)

   { FILE *f = fopen(fname,"w");
     fqvpt *tree = (fqvpt*)S;
     int i;
     fwrite (tree->descr,strlen(tree->descr)+1,1,f);
     fwrite (&tree->n,sizeof(int),1,f);
     fwrite (&tree->bsize,sizeof(int),1,f);
     fwrite (&tree->arity,sizeof(int),1,f);
     fwrite (&tree->height,sizeof(int),1,f);
     // Paper: "p_i ∈ P is set as the pivot for the i-th level"
     // Guardar los l pivotes (uno por nivel)
     for (i=0;i<tree->height;i++)
	 fwrite(&tree->queries[i].query,sizeof(int),1,f);
     savenode (&tree->node,f, tree->arity);
     fclose (f);
   }

// Cargar nodo del FQT recursivamente desde disco
static void loadnode (fqvpnode *node, FILE *f, int arity)

   { int i;
     char hoja = getc(f);
     node->hoja = hoja;
     if (hoja)
	{ fread (&node->u.hoja.size,sizeof(int),1,f);
	  node->u.hoja.bucket = loadbucket (node->u.hoja.size,f);
	}
     else
	{ node->u.interno.children = malloc(arity*sizeof(Tchild));
	  // Cargar rangos de distancia y sub-árboles recursivamente
	  for (i=0;i<arity;i++)
	      { fread (&child(node,i).dist, sizeof(Tdist),1,f);
		loadnode (&child(node,i).child,f,arity);
	      }
	}
   }

// Cargar índice FQT completo desde disco
// Paper: "FQT utilizes the same pivot at the same level"
Index loadIndex (char *fname)

   { char str[1024]; char *ptr = str;
     FILE *f = fopen(fname,"r");
     fqvpt *tree = malloc (sizeof(fqvpt));
     int i;
     while ((*ptr++ = getc(f)));
     tree->descr = malloc (ptr-str);
     strcpy (tree->descr,str);
     openDB (str);
     fread (&tree->n,sizeof(int),1,f);
     fread (&tree->bsize,sizeof(int),1,f);
     fread (&tree->arity,sizeof(int),1,f);
     fread (&tree->height,sizeof(int),1,f);
     // Paper: "The height of FQT is set to the number of pivots"
     // Cargar los l pivotes (queries) - uno por nivel
     tree->queries = malloc (tree->height * sizeof(query));
     for (i=0;i<tree->height;i++)
         fread(&tree->queries[i].query, sizeof(int),1,f);
     loadnode (&tree->node,f,tree->arity);
     fclose (f);
     return (Index)tree;
   }

// Búsqueda por rango recursiva (MRQ)
// Paper: "In order to compute MRQ(q,r), the nodes in BKT are traversed in depth-first fashion, and Lemma 4.1 is used"
// Paper: "MRQ processing using FQT are the same as when using BKT"
static int _search (fqvpt *tree, fqvpnode *node, Obj obj, Tdist r, int depth,
		  int arity, bool show)

   { int rep = 0;
     if (node->hoja)
        { // Nodo hoja: verificar todos los objetos del bucket
          rep += searchbucket (node->u.hoja.bucket,node->u.hoja.size,obj,r,show);
        }
     else
        { int i;
          Tdist dist;
          if (node->u.interno.children == NULL) return rep;
          // dist: distancia del query al pivote del nivel actual (pre-calculada)
          dist = tree->queries[depth].dist;
          // Paper: "Lemma 4.1 is used" - Poda por desigualdad triangular
          // Si el rango de distancias del sub-árbol [child(i).dist, child(i+1).dist) 
          // intersecta con [dist-r, dist+r], entonces puede contener resultados
          for (i=0;i<arity;i++)
              if (((i==arity-1)||(child(node,i+1).dist > dist-r)) &&
                  (child(node,i).dist <= dist+r))
                 rep += _search(tree,&child(node,i).child,obj,
                                r,depth+1,arity,show);
        }
     return rep;
   }


// Búsqueda por rango (MRQ - Metric Range Query)
// Paper: "MRQ processing using FQT are the same as when using BKT"
// Paper: "nodes in BKT are traversed in depth-first fashion"
int search (Index S, Obj obj, Tdist r, int show)

   { int i;
     int rep = 0;
     fqvpt *tree = (fqvpt*)S;
     // Paper: "FQT utilizes the same pivot at the same level"
     // Pre-calcular distancias del query a todos los pivotes (uno por nivel)
     for (i=0;i<tree->height;i++) 
	 { tree->queries[i].dist = distance (obj,tree->queries[i].query);
	   // Los pivotes también pueden ser parte del resultado
	   if (tree->queries[i].dist <= r)
	      { rep++;
	        if (show) printobj(tree->queries[i].query);
	      }
	}
     // Iniciar búsqueda recursiva en profundidad (depth-first)
     return rep + _search (tree,&tree->node,obj,r,0,tree->arity,show);
   }

// Búsqueda de k vecinos más cercanos recursiva (MkNNQ)
// Paper: "In order to compute MkNNQ(q,k), the nodes in BKT are traversed in best-first manner"
// Paper: "in ascending order of their minimum distances to the query object q, and Lemma 4.1 is used"
// Paper: "MkNNQ processing using FQT are the same as when using BKT"
// Paper: "MkNNQ(q,k) follows the second strategy from Section 2.2"
static void _searchNN (fqvpt *tree, fqvpnode *node, Obj obj, Tcelem *res, 
		       int depth)

   { int arity = tree->arity;
     if (node->hoja)
        { // Nodo hoja: buscar k-NN en el bucket
          searchbucketNN (node->u.hoja.bucket,node->u.hoja.size,obj,res);
        }
     else
	{ int i,ci,d;
          Tdist dist;
	  bool ea,eb;
          if (node->u.interno.children == NULL) return;
          // dist: distancia del query al pivote del nivel actual
          dist = tree->queries[depth].dist;
				/* pues los pivotes son del conjunto */
          // Paper: "in ascending order of their minimum distances to the query object q"
          // Encontrar el sub-árbol cuyo rango de distancias contiene dist
          for (ci=0;ci<arity;ci++)
             if (child(node,ci).dist > dist) break;
          ci--; ea = eb = false;
          // Explorar sub-árboles en orden de cercanía (best-first)
          // Comenzar con el sub-árbol más cercano (ci) y expandir hacia ambos lados
          for (d=0;d<=arity;d++)
              { i = ci-d;
                if (i < 0) ea = true;
                if (!ea)
                   { // Paper: "Lemma 4.1 is used to prune unqualified nodes"
                     // Poda: si dist - child(i+1).dist > radCelem(res), no puede contener k-NN
                     if ((i==arity-1) || (radCelem(res) == -1) ||
			 (dist-child(node,i+1).dist <= radCelem(res)))
                       _searchNN (tree,&child(node,i).child,
                                  obj,res,depth+1);
                     else ea = true;
                   }
                if (d==0) continue;
		i = ci+d;
                if (i >= arity) eb = true;
                if (!eb)
                   { // Poda simétrica para el lado derecho
                     if ((radCelem(res) == -1) ||
			 (child(node,i).dist-dist <= radCelem(res)))
                        _searchNN (tree,&child(node,i).child,
                                   obj,res,depth+1);
                     else eb = true;
                   }
              }
	}
   }

// Búsqueda de k vecinos más cercanos (MkNNQ - Metric k-Nearest Neighbor Query)
// Paper: "MkNNQ processing using FQT are the same as when using BKT"
// Paper: "nodes are traversed in best-first manner, i.e., in ascending order of their minimum distances"
Tdist searchNN (Index S, Obj obj, int k, bool show)

   { fqvpt *tree = (fqvpt*)S;
     Tdist mdif;
     int i;
     Tcelem res = createCelem(k);
     // Paper: "FQT utilizes the same pivot at the same level"
     // Pre-calcular distancias a todos los pivotes (uno por nivel)
     for (i=0;i<tree->height;i++) 
	 { tree->queries[i].dist = distance (obj,tree->queries[i].query);
           // Los pivotes también pueden ser candidatos a k-NN
           addCelem (&res,tree->queries[i].query,tree->queries[i].dist);
	 }
     // Iniciar búsqueda recursiva en best-first order
     _searchNN (tree,&tree->node,obj,&res,0);
     if (show) showCelem (&res);
     mdif = radCelem(&res);
     freeCelem (&res);
     return mdif;
   }


static int numnodes (fqvpnode *node, int depth, int arity)

   { int i,ret=1;
     if (node->hoja) return 0;
     if (node->u.interno.children == NULL) return 0;
     for (i=0;i<arity;i++)
	 ret += numnodes(&child(node,i).child,depth-1,arity);
     return ret;
   }

static int numbucks (fqvpnode *node, int depth, int arity)

   { int i,ret=0;
     if (node->hoja) return 1;
     if (node->u.interno.children == NULL) return 0;
     for (i=0;i<arity;i++)
	 ret += numbucks(&child(node,i).child,depth-1,arity);
     return ret;
   }

static int sizebucks (fqvpnode *node, int depth, int arity)

   { int i,ret=0;
     if (node->hoja) return node->u.hoja.size;
     if (node->u.interno.children == NULL) return 0;
     for (i=0;i<arity;i++)
	 ret += sizebucks(&child(node,i).child,depth-1,arity);
     return ret;
   }

// Imprimir estadísticas del FQT construido
// Paper: "FQT is an unbalanced tree"
// Paper: "The construction cost of FQT is O(n*l)"
void prnstats (Index S)

   { fqvpt *tree = (fqvpt*)S;
     int nbucks = numbucks(&tree->node,tree->height,tree->arity);
     int sbucks = sizebucks(&tree->node,tree->height,tree->arity);
     printf ("number of elements: %i\n",sbucks);
     // Paper: "l is the height of FQT" - número de niveles = número de pivotes
     printf ("height: %i\n",tree->height);
     // arity: número de hijos por nodo (factor de ramificación)
     printf ("arity: %i\n",tree->arity);
     printf ("number of nodes: %i\n",
		numnodes(&tree->node,tree->height,tree->arity));
     printf ("number of buckets: %i\n",nbucks);
     printf ("average size of bucket: %0.2f\n", sbucks/(double)nbucks);
   }

