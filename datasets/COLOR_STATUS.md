# Color Dataset - Quick Reference

## Resumen

- **Nombre**: CoPhIR MPEG-7 Color Features (subset 100k)
- **Objetos**: 95,476 vectores de 282 dimensiones
- **Métrica**: L1 (Manhattan distance)
- **Fuente**: https://www.fi.muni.cz/~xslanin/lmi/

## Estado del Preprocesamiento

✅ **COMPLETADO**

### Archivos Generados

**NOTA**: `Color.txt` (121 MB) no está en el repositorio por su tamaño. 
Para generar el dataset:
```bash
cd datasets
bash download_color.sh
python convert_color.py
```

```
datasets/
├── Color.txt (95,476 objetos × 282 dims) [NO EN REPO - GENERAR LOCALMENTE]
└── dataset_processing/prepared_experiment/
    ├── queries/Color_queries.json (100 queries aleatorias)
    ├── radii/Color_radii.json (5 selectividades)
    └── pivots/
        ├── Color_pivots_3.json
        ├── Color_pivots_5.json
        ├── Color_pivots_10.json
        ├── Color_pivots_15.json
        └── Color_pivots_20.json
```

### Radios Calculados

Para las selectividades [0.02, 0.04, 0.08, 0.16, 0.32]:

```json
{
  "0.02": 3763.11,
  "0.04": 4106.10,
  "0.08": 4484.69,
  "0.16": 4946.69,
  "0.32": 5591.93
}
```

## Uso en Experimentos

El dataset Color ya está listo para ser usado con **cualquier índice** del proyecto (memoria principal o secundaria), siguiendo exactamente el mismo patrón que LA, Words y Synthetic.

### Ejemplo de uso en C++:

```cpp
#include "paths.hpp"

// Cargar Color como VectorDB (L1 metric)
VectorDB* db = new VectorDB(path_dataset("Color"), 1);  // p=1 para L1

// Cargar queries y radios
vector<int> queries = load_queries_file(path_queries("Color"));
auto radii = load_radii_file(path_radii("Color"));

// Experimentos MRQ
for (auto [sel, R] : radii) {
    for (int qid : queries) {
        auto results = index->MRQ(qid, R);
        // ...
    }
}
```

## Siguiente Paso

El dataset Color está **completamente preparado** y listo para ejecutar experimentos con los índices existentes:

- **Memoria Principal**: EGNAT, EPT, FQT, GNAT
- **Memoria Secundaria**: OmniR-tree, D-index, SPB-tree, EGNAT, M-index*, MB+-tree

**IMPORTANTE**: No ejecutar aún, como solicitaste. El dataset está listo para cuando decidas correr los benchmarks.

## Comparación con otros datasets

| Dataset   | Objetos | Dims | Métrica      | Archivo Size |
|-----------|---------|------|--------------|--------------|
| LA        | 1.07M   | 2    | L2           | ~22 MB       |
| Words     | 597K    | var  | Edit Dist    | ~8 MB        |
| Synthetic | 28.6K   | 20   | L∞           | ~5 MB        |
| **Color** | **95.5K** | **282** | **L1**   | **~67 MB**   |

Color es el dataset de **mayor dimensionalidad** (282 dims), ideal para evaluar índices en espacios de alta dimensión con métrica L1.
