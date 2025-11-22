# Main-Memory Metric Index Structures — Experimental Framework

This directory contains implementations and benchmarking infrastructure for evaluating metric index structures in main memory, following the methodology described in *"Indexing Metric Spaces for Exact Similarity Search"* (Chen et al).

---

## 📁 Directory Structure

```
main_memory/
├── BST/                    # Binary Spatial Tree (Compact-Partitioning)
│   ├── bst.hpp
│   ├── test.cpp
│   ├── BST_benchmark.md
│   └── results/
│
├── LAESA/                  # Linear Approximating Eliminating Search (Pivot-Based)
│   ├── laesa.hpp
│   ├── test.cpp
│   ├── LAESA_benchmark.md
│   └── results/
│
├── BKT/                    # Burkhard-Keller Tree (Pivot-Based)
│   ├── bkt.hpp
│   ├── test.cpp
│   ├── BKT_benchmark.md
│   └── results/
│
├── mvpt/                   # Multi-Vantage Point Tree (Pivot-Based)
│   ├── mvpt.hpp
│   ├── test.cpp
│   ├── MVPT_benchmark.md
│   └── results/
│
├── EPT/                    # EPT* - Extended Pivot Table with Pivot Selection Algorithm
│   ├── main.cpp            # Original EPT* implementation
│   ├── test.cpp            # Unified benchmark wrapper
│   ├── Interpreter.cpp/h   # Dataset parsing
│   ├── CONFIGURACION_EXPERIMENTOS.md
│   └── results/
│
├── FQT/                    # Fixed Queries Tree (C implementation)
│   ├── fqt.c/h             # FQT core implementation
│   ├── test.cpp            # C++ benchmark wrapper
│   ├── CONFIGURACION_EXPERIMENTOS.md
│   └── results/
│
├── GNAT/GNAT/              # Geometric Near-neighbor Access Tree
│   ├── GNAT.cpp/h          # GNAT core implementation
│   ├── db.cpp/h            # Database abstraction
│   ├── test.cpp            # Unified benchmark wrapper
│   ├── CONFIGURACION_EXPERIMENTOS.md
│   └── results/
│
├── run_all_benchmarks.sh   # Master script to run all 7 structures
├── run_all_benchmarks.ps1  # PowerShell version (Windows)
├── aggregate_results.py    # Python script for result aggregation
├── COMPARATIVE_ANALYSIS.md # Detailed comparison documentation
└── README.md              # This file
```

---

## 🎯 Index Structures (7 Total)

### Compact-Partitioning Based

#### BST (Binary Spatial Tree)
- **Type**: Tree-based, spatial partitioning
- **Parameters**: Height, bucket size
- **Best for**: Low-dimensional vector spaces
- **Documentation**: `BST/BST_benchmark.md`

### Pivot-Based

#### LAESA (Linear Approximating and Eliminating Search Algorithm)
- **Type**: Flat structure with pivot matrix
- **Parameters**: Number of pivots (l)
- **Best for**: Small datasets, known good pivots
- **Documentation**: `LAESA/LAESA_benchmark.md`

#### EPT* (Extended Pivot Table with Pivot Selection Algorithm)
- **Type**: Advanced pivot-based with per-object pivot selection
- **Parameters**: L (pivots per object), l_c=40 (candidate pivots)
- **Features**: MaxPruning + PSA for optimal pivot selection
- **Best for**: High-quality pivot selection, different pivots per object
- **Documentation**: `EPT/CONFIGURACION_EXPERIMENTOS.md`

### Tree-Based (Pivot + Partitioning Hybrid)

#### BKT (Burkhard-Keller Tree)
- **Type**: Tree with metric-based partitioning
- **Parameters**: Bucket size, step parameter
- **Best for**: Discrete or small metric spaces
- **Documentation**: `BKT/BKT_benchmark.md`

#### FQT (Fixed Queries Tree)
- **Type**: Unbalanced tree with fixed pivots per level
- **Parameters**: HEIGHT (l pivots), arity (branching factor), bsize (bucket)
- **Features**: Same pivot at same level, C implementation
- **Best for**: Discrete distances, unbalanced partitioning
- **Documentation**: `FQT/CONFIGURACION_EXPERIMENTOS.md`

#### MVPT (Multi-Vantage Point Tree)
- **Type**: M-ary VP-tree generalization
- **Parameters**: Arity (m), bucket size
- **Best for**: Balanced exploration with multiple ranges
- **Documentation**: `mvpt/MVPT_benchmark.md`

#### GNAT (Geometric Near-neighbor Access Tree)
- **Type**: M-ary tree with multiple pivots per node
- **Parameters**: HEIGHT (tree depth), M (arity/pivots per node)
- **Features**: Best-first search for kNN, depth-first for range
- **Best for**: Balanced trees with geometric pivot selection
- **Documentation**: `GNAT/GNAT/CONFIGURACION_EXPERIMENTOS.md`

---

## 🚀 Quick Start

### Prerequisites

```bash
# C++ compiler with C++17 support
g++ --version  # Should be >= 7.0

# Python 3 with pandas
python3 --version
pip3 install pandas openpyxl
```

### Run All Benchmarks

**Linux/Mac (Bash):**
```bash
cd main_memory

# Make script executable
chmod +x run_all_benchmarks.sh

# Run all 7 structures: BST, LAESA, BKT, MVPT, EPT*, FQT, GNAT
./run_all_benchmarks.sh
```

**Windows (PowerShell):**
```powershell
cd main_memory

# Run all 7 structures
.\run_all_benchmarks.ps1
```

This will:
1. Compile each structure's test program (handling C, C++, and different build systems)
2. Run experiments on all 4 datasets (LA, Words, Color, Synthetic)
3. Execute MRQ (5 selectivities) and MkNN (5 k values) for each configuration
4. Generate JSON result files in unified format
5. Create log files for debugging

**Estimated time**: 20-90 minutes depending on CPU and dataset sizes (7 structures × 4 datasets)

### Aggregate Results

```bash
# After benchmarks complete
python3 aggregate_results.py
```

This generates:
- `consolidated_results.csv` — All results in tabular format
- `consolidated_results.json` — All results in JSON format
- `summary_MRQ.csv` — Range query summary
- `summary_MkNN.csv` — k-NN query summary
- `summary_by_pivots.csv` — Comparison by pivot count

---

## 📊 Experimental Setup

### Datasets

Four standard metric datasets are used:

| Dataset     | Size  | Type    | Metric     | Source                    |
|-------------|-------|---------|------------|---------------------------|
| **LA**      | ~100K | Vectors | L2 (Eucl.) | Geographic coordinates    |
| **Color**   | ~100K | Vectors | L1 (Manh.) | Image histograms         |
| **Synthetic** | ~100K | Vectors | L∞ (Cheb.) | Artificial data          |
| **Words**   | ~100K | Strings | Edit Dist. | Dictionary/corpus        |

Dataset files expected in: `../../datasets/`

### Query Types

#### Metric Range Queries (MRQ)
- **Selectivities**: 2%, 4%, 8%, 16%, 32%
- Precomputed radii for consistent comparison
- Finds all objects within radius r

#### Multi-k Nearest Neighbor (MkNN)
- **k values**: 5, 10, 20, 50, 100
- Finds k closest objects
- Returns sorted by distance

### Metrics Collected

1. **compdists**: Number of distance computations (hardware-independent)
2. **time_ms**: Query execution time in milliseconds (CPU-dependent)

Both averaged over 100 queries per configuration.

---

## 📈 Individual Benchmarks

Each structure has its own test program and documentation.

### Running Individual Benchmarks

```bash
# BST
cd BST
g++ -O3 -std=gnu++17 test.cpp -o bst_test
./bst_test

# LAESA
cd ../LAESA
g++ -O3 -std=gnu++17 test.cpp -o laesa_test
./laesa_test

# BKT
cd ../BKT
g++ -O3 -std=gnu++17 test.cpp -o bkt_test
./bkt_test

# MVPT
cd ../mvpt
g++ -O3 -std=gnu++17 test.cpp -o mvpt_test
./mvpt_test
```

### Result Locations

After running, results are in:
- `BST/results/results_BST_<dataset>.json`
- `LAESA/results/results_LAESA_<dataset>.json`
- `BKT/results/results_BKT_<dataset>.json`
- `mvpt/results/results_MVPT_<dataset>.json`

---

## 🔬 Comparative Analysis

Detailed comparison methodology and expected performance patterns are documented in:

**`COMPARATIVE_ANALYSIS.md`**

Key topics covered:
- Performance characteristics of each structure
- Fair comparison methodology (pivot equivalence)
- Statistical analysis guidelines
- Visualization suggestions
- Known limitations

---

## 📝 Output Format

All benchmarks produce JSON with unified schema:

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

Fields may be `null` depending on structure and query type.

---

## 🛠️ Development Guide

### Adding a New Index Structure

1. **Create directory**: `main_memory/NEWINDEX/`

2. **Implement header**: `newindex.hpp`
   - Must support ObjectDB interface
   - Implement `rangeSearch()` and `knnSearch()`
   - Track distance computations via counter

3. **Create test program**: `test.cpp`
   - Follow template from existing structures
   - Use unified JSON output format
   - Load datasets via `paths.hpp`

4. **Document benchmark**: `NEWINDEX_benchmark.md`
   - Describe structure and parameters
   - Explain how to run tests
   - Document output format

5. **Update master scripts**:
   - Add to `run_all_benchmarks.sh`
   - Update `COMPARATIVE_ANALYSIS.md`
   - Update this README

### Code Style

- **C++ Standard**: C++17 or later
- **Optimization**: Always compile with `-O3`
- **Headers**: Use `#include "../../datasets/paths.hpp"` for dataset resolution
- **Counters**: Track `compdists` consistently (member variable or global)
- **Results**: Write to `results/results_INDEXNAME_DATASET.json`

---

## 🐛 Troubleshooting

### Compilation Errors

```bash
# Check compiler version
g++ --version

# Verify paths
ls ../../datasets/paths.hpp
ls ../../objectdb.hpp

# Try explicit includes
g++ -O3 -std=gnu++17 -I../.. test.cpp -o test
```

### Missing Datasets

```bash
# Check dataset directory
ls ../../datasets/*.txt

# Verify paths.hpp configuration
cat ../../datasets/paths.hpp
```

### Empty Results

Check that precomputed files exist:
```bash
ls ../../datasets/prepared_experiment/queries/
ls ../../datasets/prepared_experiment/radii/
```

### Slow Execution

- Large datasets (>100K objects) take longer
- MkNN queries more expensive than MRQ
- Run on dedicated CPU (no background processes)
- Check available RAM (LAESA needs n×l distances)

---

## 📚 References

### Paper

Chen et al., *"Indexing Metric Spaces for Exact Similarity Search"*

Key sections:
- **Table 6**: Experimental parameters
- **Section 4**: Index structure descriptions
- **Section 6**: Comparative methodology

### External Dependencies

- **ObjectDB**: Base interface for metric objects (`../../objectdb.hpp`)
- **paths.hpp**: Dataset path resolution (`../../datasets/paths.hpp`)
- Standard C++17 library (no external libraries required)

---

## ⚙️ System Requirements

### Minimum

- **CPU**: x86_64 architecture
- **RAM**: 4 GB (8 GB recommended for large datasets)
- **Disk**: 10 GB free space for datasets + results
- **OS**: Linux, macOS, or Windows with WSL

### Recommended

- **CPU**: Modern multi-core (for faster compilation)
- **RAM**: 16 GB (for LAESA with large l)
- **Compiler**: GCC 9+ or Clang 10+

---

## 📧 Support

For issues or questions:
1. Check structure-specific `*_benchmark.md` files
2. Review `COMPARATIVE_ANALYSIS.md` for methodology
3. Examine log files in each structure's directory
4. Check stderr output for warnings

---

## ✅ Validation Checklist

Before running final experiments:

- [ ] All datasets present in `../../datasets/`
- [ ] Precomputed queries/radii exist
- [ ] All structures compile without warnings
- [ ] Test run on small dataset succeeds
- [ ] Same CPU/machine for all benchmarks
- [ ] Sufficient disk space for results
- [ ] Python environment with pandas installed
- [ ] `run_all_benchmarks.sh` is executable

---

## 📅 Version History

- **v1.0** (Nov 2025): Initial framework with BST, LAESA, BKT, MVPT
  - Unified JSON output format
  - Automated benchmark execution
  - Result aggregation scripts
  - Comprehensive documentation

---

**Status**: Ready for production experiments  
**Last Updated**: November 2025  
**Maintainer**: EDA-sQuad Project Team
