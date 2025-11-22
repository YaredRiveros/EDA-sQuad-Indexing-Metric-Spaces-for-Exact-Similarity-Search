# Framework de Benchmarking — Estructura Completa

```
EDA-sQuad-Indexing-Metric-Spaces-for-Exact-Similarity-Search/
│
├── datasets/                              # Datasets y utilidades
│   ├── LA.txt
│   ├── Words.txt
│   ├── Color.txt
│   ├── Synthetic.txt
│   ├── paths.hpp                         # Resolución de rutas
│   ├── objectdb.hpp                      # Interfaz base para métricas
│   └── prepared_experiment/
│       ├── queries/                      # 100 queries por dataset
│       ├── radii/                        # Radios precomputados
│       └── pivots/                       # Pivotes preseleccionados (LAESA)
│
└── main_memory/                          # 🎯 FRAMEWORK DE BENCHMARKING
    │
    ├── 📘 README.md                      # Documentación general
    ├── 📘 QUICKSTART.md                  # Guía rápida paso a paso
    ├── 📘 COMPARATIVE_ANALYSIS.md        # Análisis comparativo detallado
    ├── 📘 IMPLEMENTATION_SUMMARY.md      # Resumen ejecutivo
    │
    ├── 🔧 run_all_benchmarks.sh          # Script maestro (ejecutar esto)
    ├── 🐍 aggregate_results.py           # Agregación de resultados
    │
    ├── BST/                              # Binary Spatial Tree (CP)
    │   ├── bst.hpp                       # Implementación
    │   ├── test.cpp                      # Benchmark
    │   ├── BST_benchmark.md              # Documentación
    │   ├── bst_test                      # Ejecutable (generado)
    │   ├── BST_benchmark.log             # Log (generado)
    │   └── results/                      # 📊 Resultados JSON
    │       ├── results_BST_LA.json
    │       ├── results_BST_Words.json
    │       ├── results_BST_Color.json
    │       └── results_BST_Synthetic.json
    │
    ├── LAESA/                            # Linear Approx. Eliminating Search (PB)
    │   ├── laesa.hpp
    │   ├── test.cpp
    │   ├── LAESA_benchmark.md
    │   ├── laesa_test
    │   ├── LAESA_benchmark.log
    │   └── results/
    │       ├── results_LAESA_LA.json
    │       ├── results_LAESA_Words.json
    │       ├── results_LAESA_Color.json
    │       └── results_LAESA_Synthetic.json
    │
    ├── BKT/                              # Burkhard-Keller Tree (PB)
    │   ├── bkt.hpp
    │   ├── test.cpp
    │   ├── BKT_benchmark.md
    │   ├── bkt_test
    │   ├── BKT_benchmark.log
    │   └── results/
    │       ├── results_BKT_LA.json
    │       ├── results_BKT_Words.json
    │       ├── results_BKT_Color.json
    │       └── results_BKT_Synthetic.json
    │
    ├── mvpt/                             # Multi-Vantage Point Tree (PB)
    │   ├── mvpt.hpp
    │   ├── test.cpp
    │   ├── MVPT_benchmark.md
    │   ├── mvpt_test
    │   ├── MVPT_benchmark.log
    │   └── results/
    │       ├── results_MVPT_LA.json
    │       ├── results_MVPT_Words.json
    │       ├── results_MVPT_Color.json
    │       └── results_MVPT_Synthetic.json
    │
    ├── EPT/                              # EPT* - Extended Pivot Table w/ PSA
    │   ├── main.cpp                      # Original EPT* implementation
    │   ├── test.cpp                      # ✨ NUEVO: Unified benchmark wrapper
    │   ├── Interpreter.cpp/h             # Dataset parsing
    │   ├── Objvector.cpp/h               # Vector abstraction
    │   ├── Tuple.cpp/h                   # Pivot-distance tuples
    │   ├── Cache.h
    │   ├── CONFIGURACION_EXPERIMENTOS.md
    │   ├── EPT_test
    │   ├── EPT_benchmark.log
    │   └── results/
    │       ├── results_EPT_LA.json
    │       ├── results_EPT_Words.json
    │       ├── results_EPT_Color.json
    │       └── results_EPT_Synthetic.json
    │
    ├── FQT/                              # Fixed Queries Tree (C implementation)
    │   ├── fqt.c/h                       # FQT core (C code)
    │   ├── test.cpp                      # ✨ NUEVO: C++ benchmark wrapper
    │   ├── calcular_bsize.sh
    │   ├── CONFIGURACION_EXPERIMENTOS.md
    │   ├── FQT_test
    │   ├── FQT_benchmark.log
    │   └── results/
    │       ├── results_FQT_LA.json
    │       ├── results_FQT_Words.json
    │       ├── results_FQT_Color.json
    │       └── results_FQT_Synthetic.json
    │
    ├── GNAT/                             # Geometric Near-neighbor Access Tree
    │   └── GNAT/
    │       ├── GNAT.cpp/h                # GNAT core implementation
    │       ├── db.cpp/h                  # Database abstraction layer
    │       ├── test.cpp                  # ✨ NUEVO: Unified benchmark wrapper
    │       ├── index.h
    │       ├── CONFIGURACION_EXPERIMENTOS.md
    │       ├── README.txt
    │       └── results/
    │           ├── results_GNAT_LA.json
    │           ├── results_GNAT_Words.json
    │           ├── results_GNAT_Color.json
    │           └── results_GNAT_Synthetic.json
    │
    └── 📊 RESULTADOS CONSOLIDADOS (generados por aggregate_results.py)
        ├── consolidated_results.csv      # Todos los experimentos
        ├── consolidated_results.json     # Formato JSON
        ├── consolidated_metadata.json    # Metadatos
        ├── summary_MRQ.csv              # Resumen Range Queries
        ├── summary_MkNN.csv             # Resumen k-NN Queries
        ├── summary_by_pivots.csv        # Comparación por pivotes
        └── benchmark_system_info.txt    # Info de CPU
```

---

## 🎯 Flujo de Trabajo

```
┌─────────────────────────────────────────────────────────────┐
│  1. PREPARACIÓN                                             │
│  • Verificar datasets en ../../datasets/                   │
│  • Verificar queries/radii precomputados                   │
│  • Instalar dependencias (g++, python3, pandas)            │
└─────────────────────────────────────────────────────────────┘
                          ↓
┌─────────────────────────────────────────────────────────────┐
│  2. EJECUCIÓN                                               │
│  chmod +x run_all_benchmarks.sh                            │
│  ./run_all_benchmarks.sh                                   │
│                                                             │
│  Compila y ejecuta:                                        │
│  • BST  (Altura 3,5,10,15,20)                             │
│  • LAESA (Pivotes 3,5,10,15,20)                           │
│  • BKT  (Bucket 5,10,20,50,100)                           │
│  • MVPT (Bucket 5,10,20,50,100, Arity=5)                  │
│                                                             │
│  Por dataset: LA, Words, Color, Synthetic                  │
│  Por query: MRQ (5 selectividades) + MkNN (5 k-values)    │
│                                                             │
│  ⏱️  Tiempo: 10-60 minutos                                 │
└─────────────────────────────────────────────────────────────┘
                          ↓
┌─────────────────────────────────────────────────────────────┐
│  3. AGREGACIÓN                                              │
│  python3 aggregate_results.py                              │
│                                                             │
│  Genera:                                                    │
│  • consolidated_results.csv (todos los datos)              │
│  • summary_MRQ.csv (range queries)                         │
│  • summary_MkNN.csv (k-NN queries)                         │
│  • summary_by_pivots.csv (comparación pivotes)             │
└─────────────────────────────────────────────────────────────┘
                          ↓
┌─────────────────────────────────────────────────────────────┐
│  4. ANÁLISIS                                                │
│  • Importar CSVs a Excel/Python/R                          │
│  • Generar gráficos comparativos                           │
│  • Calcular estadísticas (media, std, min, max)            │
│  • Identificar patrones de rendimiento                     │
└─────────────────────────────────────────────────────────────┘
                          ↓
┌─────────────────────────────────────────────────────────────┐
│  5. DOCUMENTACIÓN                                           │
│  • Actualizar COMPARATIVE_ANALYSIS.md con observaciones    │
│  • Documentar configuraciones óptimas                      │
│  • Registrar limitaciones encontradas                      │
│  • Preparar reporte final                                  │
└─────────────────────────────────────────────────────────────┘
```

---

## 📊 Métricas Evaluadas

### Por cada configuración se mide:

1. **compdists** (Distance Computations)
   - Número promedio de cálculos de distancia
   - Hardware-independent
   - Métrica principal de eficiencia

2. **time_ms** (Query Time)
   - Tiempo promedio de ejecución en milisegundos
   - Hardware-dependent
   - Incluye overhead de estructura

Ambas promediadas sobre **100 queries** por configuración.

---

## 🔬 Configuraciones Evaluadas

### BST (Compact-Partitioning)
- **Parámetro**: Altura h ∈ {3, 5, 10, 15, 20}
- **Fixed**: Bucket size = 10
- **Total**: 5 configs × 4 datasets × 10 query types = 200 experimentos

### LAESA (Pivot-Based)
- **Parámetro**: Pivotes l ∈ {3, 5, 10, 15, 20}
- **Total**: 5 configs × 4 datasets × 10 query types = 200 experimentos

### BKT (Pivot-Based Tree)
- **Parámetro**: Bucket size ∈ {5, 10, 20, 50, 100}
- **Fixed**: Step = 1.0
- **Total**: 5 configs × 4 datasets × 10 query types = 200 experimentos

### MVPT (Pivot-Based M-ary Tree)
- **Parámetros**: Arity = 5 (fixed), Bucket ∈ {5, 10, 20, 50, 100}
- **Total**: 5 configs × 4 datasets × 10 query types = 200 experimentos

### Gran Total
**~800 experimentos** en total

---

## 📈 Formato de Salida Unificado

Todos los resultados siguen este esquema JSON:

```json
{
  "index": "BST|LAESA|BKT|MVPT",
  "dataset": "LA|Words|Color|Synthetic",
  "category": "CP|PB",
  "num_pivots": 5,
  "num_centers_path": 5,
  "arity": 5,
  "bucket_size": 10,
  "query_type": "MRQ|MkNN",
  "selectivity": 0.08,
  "radius": 957.99,
  "k": 20,
  "compdists": 123456.78,
  "time_ms": 1.23456,
  "n_queries": 100,
  "run_id": 1
}
```

Facilita comparación y análisis automatizado.

---

## 🎓 Metodología del Paper

Siguiendo Chen et al., "Indexing Metric Spaces for Exact Similarity Search":

### Comparación Justa
> *"We set the number of pivots used in the pivot-based indexes  
> equaling to the height of compact-partitioning based methods."*

### Implementación
- **BST altura h** ≈ **LAESA pivotes l**
- Mismas queries (100 por dataset)
- Mismos radii (precomputados por selectividad)
- Misma CPU para todas las mediciones

### Parámetros (Tabla 6 del paper)
- **l/h**: {3, 5, 10, 15, 20}
- **Selectividades**: {2%, 4%, 8%, 16%, 32%}
- **k-values**: {5, 10, 20, 50, 100}
- **Arity** (MVPT/GNAT): 5

---

## ✨ Características del Framework

✅ **Modular**: Cada estructura independiente  
✅ **Automatizado**: Un comando ejecuta todo  
✅ **Reproducible**: Datos precomputados fijos  
✅ **Extensible**: Fácil agregar nuevas estructuras  
✅ **Documentado**: 5 archivos de documentación  
✅ **Analizable**: Scripts Python incluidos  

---

## 🚀 Comando Único

```bash
cd main_memory
./run_all_benchmarks.sh && python3 aggregate_results.py
```

Eso es todo! 🎉

---

## 📚 Documentación Disponible

1. **README.md** — Introducción general y guía completa
2. **QUICKSTART.md** — Checklist rápido paso a paso
3. **COMPARATIVE_ANALYSIS.md** — Análisis detallado y metodología
4. **IMPLEMENTATION_SUMMARY.md** — Resumen ejecutivo de implementación
5. **Este archivo (PROJECT_STRUCTURE.md)** — Vista general del proyecto
6. **BST_benchmark.md** — Detalles de BST
7. **LAESA_benchmark.md** — Detalles de LAESA
8. **BKT_benchmark.md** — Detalles de BKT
9. **MVPT_benchmark.md** — Detalles de MVPT

---

**Status**: ✅ Listo para producción  
**Next Step**: Ejecutar `./run_all_benchmarks.sh`  
**Tiempo Estimado**: 10-60 minutos  
**Resultado**: ~800 experimentos completamente documentados
