# Comparative Analysis Report — Main-Memory Metric Index Structures

## Overview

This document provides a comprehensive comparison of all metric index structures implemented in the main-memory module, following the experimental methodology described in *"Indexing Metric Spaces for Exact Similarity Search"* (Chen et al).

---

## 📊 Structures Evaluated

### 1. **Compact-Partitioning Based (CP)**

#### BST (Binary Spatial Tree)
- **Category**: Compact-Partitioning
- **Key Parameters**: 
  - Height: {3, 5, 10, 15, 20}
  - Bucket size: 10 (fixed)
- **Characteristics**:
  - Binary tree structure
  - Spatial partitioning using centers
  - No precomputed pivot distances
  - Height directly controlled via `maxHeight`

---

### 2. **Pivot-Based (PB)**

#### LAESA (Linear Approximating and Eliminating Search Algorithm)
- **Category**: Pivot-Based
- **Key Parameters**:
  - Number of pivots (l): {3, 5, 10, 15, 20}
- **Characteristics**:
  - Full pivot-based approach
  - Precomputes distance matrix [n × l]
  - Uses triangle inequality for pruning
  - Linear scan with lower bound filtering
  - Memory: O(n × l)

#### BKT (Burkhard-Keller Tree)
- **Category**: Pivot-Based (discrete metric variant)
- **Key Parameters**:
  - Bucket sizes: {5, 10, 20, 50, 100}
  - Step parameter: 1.0 (fixed)
- **Characteristics**:
  - Dynamic pivot selection (one per node)
  - Metric-based partitioning
  - Variable depth based on data distribution
  - Bucket size controls tree depth indirectly

#### MVPT (Multi-Vantage Point Tree)
- **Category**: Pivot-Based (tree variant)
- **Key Parameters**:
  - Arity: 5 (fixed, as per paper)
  - Bucket sizes: {5, 10, 20, 50, 100}
- **Characteristics**:
  - M-ary VP-tree generalization
  - One vantage point per internal node
  - Metric-based partitioning with m ranges
  - Paper: *"GNAT and MVPT are multi-arity trees. Here, we set arity to 5"*

---

## 🔬 Experimental Setup

### Datasets

| Dataset     | Size (n) | Dimensionality | Metric         | Type      |
|-------------|----------|----------------|----------------|-----------|
| **LA**      | ~100K    | 3D/4D          | L2 (Euclidean) | Vectors   |
| **Color**   | ~100K    | 112D           | L1 (Manhattan) | Vectors   |
| **Synthetic**| ~100K   | Variable       | L∞ (Chebyshev) | Vectors   |
| **Words**   | ~100K    | Variable       | Edit Distance  | Strings   |

### Query Types

#### Metric Range Queries (MRQ)
- **Selectivities**: {2%, 4%, 8%, 16%, 32%}
- Precomputed radii for each selectivity
- Returns all objects within radius r from query

#### Multi-k Nearest Neighbor (MkNN)
- **k values**: {5, 10, 20, 50, 100}
- Returns k closest objects to query
- Sorted by distance

### Metrics Collected

1. **compdists**: Average number of distance computations per query
2. **time_ms**: Average query execution time in milliseconds
3. Both metrics averaged over 100 queries per configuration

---

## 📈 Expected Performance Patterns

### Compact-Partitioning (BST)

**Strengths**:
- Good spatial locality
- Efficient for low-dimensional spaces
- Height directly controls search depth

**Weaknesses**:
- No pivot-based pruning
- Degrades with high dimensionality
- Must traverse tree structure

**Expected Trends**:
- Higher height → more pruning → fewer compdists
- Time increases with height due to tree traversal overhead

---

### Pivot-Based Structures

#### LAESA

**Strengths**:
- Strongest lower bounds with many pivots
- No tree traversal overhead
- Simple linear scan

**Weaknesses**:
- O(n × l) memory requirement
- Still scans all n objects
- Distance matrix construction cost

**Expected Trends**:
- More pivots → better pruning → fewer compdists
- Linear time complexity regardless of l
- Memory-intensive for large l

#### BKT

**Strengths**:
- Dynamic pivot selection
- Adapts to data distribution
- Good for discrete/small metric spaces

**Weaknesses**:
- Unbalanced tree structure
- Variable performance
- Smaller buckets → deeper trees

**Expected Trends**:
- Smaller bucket → more pivots → better pruning
- But also → deeper tree → more overhead

#### MVPT

**Strengths**:
- M-ary structure reduces depth
- Multiple ranges per level
- Balanced exploration

**Weaknesses**:
- One pivot per node (limited pruning)
- Arity=5 may not be optimal for all datasets
- Tree traversal costs

**Expected Trends**:
- Smaller buckets → deeper tree → more pivots
- Arity=5 provides good depth/branching tradeoff

---

## 🎯 Comparison Methodology

### Fair Comparison Strategy

According to the paper:
> *"We set the number of pivots used in the pivot-based indexes equaling to the height of compact-partitioning based methods."*

**Implementation**:
- BST: Height h = {3, 5, 10, 15, 20}
- LAESA: Pivots l = {3, 5, 10, 15, 20}
- BKT: Variable pivots (depends on tree structure)
- MVPT: Variable pivots (depends on bucket size and arity)

**Note**: BKT and MVPT pivot counts are data-dependent and reported post-construction.

---

## 📝 Output Format

All structures produce JSON files with unified format:

```json
{
  "index": "BST|LAESA|BKT|MVPT",
  "dataset": "LA|Words|Color|Synthetic",
  "category": "CP|PB",
  "num_pivots": <int or 0>,
  "num_centers_path": <int or 0>,
  "arity": <int or null>,
  "bucket_size": <int or null>,
  "query_type": "MRQ|MkNN",
  "selectivity": <double or null>,
  "radius": <double or null>,
  "k": <int or null>,
  "compdists": <double>,
  "time_ms": <double>,
  "n_queries": 100,
  "run_id": 1
}
```

---

## 🔍 Analysis Guidelines

### When analyzing results, consider:

1. **Distance Computations (compdists)**:
   - Primary metric for efficiency
   - Independent of hardware
   - Directly reflects pruning power

2. **Query Time (time_ms)**:
   - Hardware-dependent
   - Includes tree traversal overhead
   - Should be run on same CPU for fair comparison

3. **Scalability**:
   - How performance changes with:
     - Dataset size (n)
     - Number of pivots/height (l/h)
     - Selectivity (for MRQ)
     - k (for MkNN)

4. **Memory Usage**:
   - LAESA: O(n × l) for distance matrix
   - BST/BKT/MVPT: O(n) for tree structure
   - Trade-off: memory vs. pruning power

---

## 🚀 Running All Benchmarks

### Sequential Execution

```bash
# BST
cd main_memory/BST
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

### Batch Script

Create `run_all_benchmarks.sh`:

```bash
#!/bin/bash

STRUCTURES=("BST" "LAESA" "BKT" "mvpt")
MAIN_MEM="main_memory"

for struct in "${STRUCTURES[@]}"; do
    echo "=========================================="
    echo "Building and running: $struct"
    echo "=========================================="
    
    cd "$MAIN_MEM/$struct"
    
    # Compile
    g++ -O3 -std=gnu++17 test.cpp -o ${struct,,}_test
    
    if [ $? -ne 0 ]; then
        echo "[ERROR] Compilation failed for $struct"
        cd ../..
        continue
    fi
    
    # Run
    ./${struct,,}_test
    
    if [ $? -ne 0 ]; then
        echo "[ERROR] Execution failed for $struct"
    else
        echo "[SUCCESS] $struct benchmark completed"
    fi
    
    cd ../..
    echo ""
done

echo "=========================================="
echo "All benchmarks completed!"
echo "=========================================="
```

Make executable and run:
```bash
chmod +x run_all_benchmarks.sh
./run_all_benchmarks.sh
```

---

## 📊 Result Aggregation

After running all benchmarks, results are located in:

```
main_memory/
├── BST/results/results_BST_*.json
├── LAESA/results/results_LAESA_*.json
├── BKT/results/results_BKT_*.json
└── mvpt/results/results_MVPT_*.json
```

### Python Aggregation Script

Create `aggregate_results.py`:

```python
import json
import glob
import pandas as pd

# Collect all JSON files
files = glob.glob("main_memory/*/results/results_*.json")

all_results = []
for file in files:
    with open(file, 'r') as f:
        data = json.load(f)
        all_results.extend(data)

# Convert to DataFrame
df = pd.DataFrame(all_results)

# Save consolidated results
df.to_csv("consolidated_results.csv", index=False)
df.to_json("consolidated_results.json", orient="records", indent=2)

print(f"Consolidated {len(df)} experiment results")
print(f"Indices: {df['index'].unique()}")
print(f"Datasets: {df['dataset'].unique()}")
```

---

## 🔬 Statistical Analysis

### Comparative Metrics

1. **Pruning Efficiency**:
   ```
   Pruning Rate = 1 - (compdists / n)
   ```
   Higher is better.

2. **Speedup vs. Sequential Scan**:
   ```
   Speedup = (n × avg_dist_time) / total_query_time
   ```

3. **Time per Distance Computation**:
   ```
   Overhead = time_ms / compdists
   ```
   Lower indicates less structural overhead.

### Visualization Suggestions

1. **compdists vs. l/h** (line plots per dataset)
2. **time_ms vs. l/h** (line plots per dataset)
3. **compdists vs. selectivity** for MRQ
4. **compdists vs. k** for MkNN
5. **Heatmaps**: Structure × Dataset performance matrix

---

## ⚠️ Important Notes

1. **Same CPU Requirement**: 
   - All benchmarks must run on the same machine
   - CPU-dependent timing results
   - Record CPU specs in results

2. **Precomputed Data**:
   - Queries: `prepared_experiment/queries/`
   - Radii: `prepared_experiment/radii/`
   - Pivots (for LAESA): `prepared_experiment/pivots/`

3. **Reproducibility**:
   - Fixed random seed for query selection
   - Same 100 queries across all structures
   - Same radii for same selectivities

4. **Known Limitations**:
   - BKT/MVPT pivot counts are data-dependent
   - LAESA memory scales with l
   - BST/BKT tree balance not guaranteed

---

## 📚 References

- Chen et al., *"Indexing Metric Spaces for Exact Similarity Search"*
- Table 6: Experimental parameters
- Section on comparative methodology

---

## 📧 Troubleshooting

### Common Issues

1. **Compilation errors**:
   - Check C++17 support: `g++ --version`
   - Verify paths.hpp is accessible
   - Ensure objectdb.hpp is in parent directories

2. **Missing datasets**:
   - Check `datasets/` folder
   - Verify paths in `paths.hpp`
   - Download missing datasets if needed

3. **Empty results**:
   - Verify queries/radii files exist
   - Check JSON write permissions
   - Review stderr output for warnings

4. **Performance issues**:
   - Ensure -O3 optimization flag
   - Check available RAM for large datasets
   - Monitor CPU usage during execution

---

**Last Updated**: November 2025  
**Status**: Ready for execution  
**Next Steps**: Run benchmarks on same CPU, aggregate results, perform statistical analysis
