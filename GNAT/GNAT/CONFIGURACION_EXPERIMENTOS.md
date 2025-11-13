# Configuración de GNAT para Experimentos# Configuración de GNAT para Experimentos



## Descripción## Descripción General



**GNAT** es un índice **HÍBRIDO** que combina particionamiento compacto con técnicas pivot-based.**GNAT (Geometric Near-neighbor Access Tree)** es un índice **HÍBRIDO** que combina:

OJO: la estrategia para los híbridos usada por el autor para una comparación justa es setear m (arity) = 5.

- **Compact-Partitioning:** Usa m centros por nodo para particionar el espacio

---- **Pivot-Based:** Usa esos centros como pivotes para poda con desigualdad triangular

- **MBBs:** Almacena rangos min/max de distancias para mejor poda

## Compilación

**Paper Reference:** Section 5.1 - The GHT Family

```bash

cd GNAT/GNAT**Clasificación del paper:**

make> "pivot-based indexes (i.e., LAESA, EPT*, BKT, FQT, and MVPT), and **the hybrid index GNAT**"

```

---

---

## Uso con Makefile (Automático)

## Ver Configuración

El Makefile permite control directo de la altura y opcionalmente del arity (m).

```bash

make info---

```

## Compilación

Muestra la altura, arity (m) y total de pivotes calculado como: **M × (M^HEIGHT - 1)/(M-1)**

```bash

---cd GNAT/GNAT

make

## Ejecutar con Altura Específica```



```bashGenera el ejecutable `gnat` optimizado para Ubuntu Linux.

# Altura = 3 con M=5 (default)

make run-l HEIGHT=3---



# Altura = 5 con M=3## Ver Configuración Actual

make run-l HEIGHT=5 M=3

``````bash

make info

---```



## Ejecutar Todos los Experimentos**Ejemplo de salida:**

```

```bash=== Configuración GNAT ===

# Todas las alturas: HEIGHT = {3, 5, 10, 15, 20}Altura (HEIGHT): 5

make run-experimentsArity (M): 5

Total de Pivotes: 155 = 5  (5^5 - 1)/(5-1)

# Solo consultas de rangoDataset: dataset.txt

make run-experiments-rangeIndex: index.idx

Query: query.txt

# Solo consultas k-NN```

make run-experiments-knn

```---



---## Ejecutar con Altura Específica



## Explorar Diferentes Aridades```bash

# Altura = 3 (Pivotes  31 con M=5)

```bashmake run-l HEIGHT=3

# Prueba M = {3, 5, 7, 9} con altura fija

make run-vary-m HEIGHT=5# Altura = 10 (Pivotes  1.22M con M=5)

```make run-l HEIGHT=10



---# Con arity diferente (por ejemplo m=3)

make run-l HEIGHT=5 M=3

## Comandos Disponibles```



| Comando | Descripción |---

|---------|-------------|

| `make` | Compila el programa |## Ejecutar Experimentos Completos

| `make clean` | Limpia archivos generados |

| `make info` | Muestra configuración y pivotes totales |```bash

| `make run-l HEIGHT=X [M=Y]` | Ejecuta con altura X y arity Y |# Todas las alturas del paper (HEIGHT = {3, 5, 10, 15, 20})

| `make run-experiments` | Ejecuta con HEIGHT={3,5,10,15,20} |make run-experiments

| `make run-experiments-range` | Solo range queries |

| `make run-experiments-knn` | Solo k-NN queries |# Solo consultas de rango

| `make run-vary-m HEIGHT=X` | Explora diferentes valores de M |make run-experiments-range



---# Solo consultas k-NN

make run-experiments-knn

## Parámetros```



| Parámetro | Variable | Default | Descripción |---

|-----------|----------|---------|-------------|

| Altura | `HEIGHT` | 5 | Niveles del árbol |## Explorar Diferentes Aridades

| Arity | `M` | 5 | Centros por nodo |

| Dataset | `DATASET` | dataset.txt | Archivo de datos |```bash

| Index | `INDEX` | index.idx | Archivo de índice |# Experimentar con M = {3, 5, 7, 9} con altura fija

| Query | `QUERY` | query.txt | Archivo de consultas |make run-vary-m HEIGHT=5

| Radio | `RADIUS` | 1000.0 | Radio para range queries |```

| k | `K` | 20 | Vecinos para k-NN |

---

---

## Comandos Disponibles

## Ejemplo

| Comando | Descripción |

```bash|---------|-------------|

# 1. Compilar| `make` | Compila el programa |

make| `make clean` | Limpia ejecutables y archivos temporales |

| `make info` | Muestra configuración actual y pivotes totales |

# 2. Ver configuración| `make run-l HEIGHT=X [M=Y]` | Ejecuta con altura X y arity Y |

make info| `make run-experiments` | Ejecuta con HEIGHT={3,5,10,15,20} |

| `make run-experiments-range` | Solo consultas de rango |

# 3. Ejecutar experimento| `make run-experiments-knn` | Solo consultas k-NN |

make run-l HEIGHT=3 M=3| `make run-vary-m HEIGHT=X` | Explora M={3,5,7,9} con altura fija |



# 4. Ver resultados---

ls -lh output/

```## Parámetros Configurables


| Parámetro | Variable Makefile | Valor por Defecto | Descripción |
|-----------|-------------------|-------------------|-------------|
| Altura máxima | `HEIGHT` | 5 | Niveles del árbol GNAT |
| Arity | `M` | 5 | Centros por nodo (m) |
| Dataset | `DATASET` | dataset.txt | Archivo de entrada |
| Index | `INDEX` | index.idx | Archivo de índice |
| Query | `QUERY` | query.txt | Archivo de consultas |
| Radio | `RADIUS` | 1000.0 | Radio para range queries |
| k | `K` | 20 | Vecinos para k-NN queries |

**Nota:** Total de pivotes = M  (M^HEIGHT - 1)/(M-1)

---

## Ejemplo Completo

```bash
# 1. Compilar
cd GNAT/GNAT
make

# 2. Ver configuración
make info

# 3. Experimento con altura específica
make run-l HEIGHT=3 M=3

# 4. Todos los experimentos del paper
make run-experiments

# 5. Ver resultados
ls -lh output/
```

**Salida esperada:**
```
=== Configuración GNAT ===
Altura (HEIGHT): 3
Arity (M): 3
Total de Pivotes: 13 = 3  (3^3 - 1)/(3-1)
Dataset: dataset.txt
Index: index.idx
Query: query.txt

=== Compilando GNAT ===
Ejecutando: HEIGHT=3 M=3
[Construction] Time: 2.34s
[Range Query] Time: 0.012s/query, Results: 45
[k-NN Query] k=20, Time: 0.008s/query
```

---

## Referencias del Paper

**Paper:** "On the Hierarchy of Covering-based Space-partitioning Structures for Similarity Search" (Section 5.1)

**Clasificación:** GNAT es un índice **HÍBRIDO**:
- Usa particionamiento compacto (m centros por nivel)
- Aplica técnicas pivot-based (MBBs para poda)

**Configuración del paper:**
- Arity (m) = 5
- Altura controlada por `MaxHeight`
- MBBs almacenan rangos [min, max] de distancias

**Citas clave:**
- "GHT can be generalized to m-ary trees, yielding GNAT"
- "GNAT stores the minimum bounding box MBB_ij"
- "GNAT uses centers in the same level as pivots"
- "The additional MBBs stored in non-leaf nodes enable pruning using Lemma 4.1"
- "GNAT and MVPT are multi-arity trees. Here, we set arity to 5"
