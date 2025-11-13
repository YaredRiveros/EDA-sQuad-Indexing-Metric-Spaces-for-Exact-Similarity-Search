# LAESA Benchmark — Main-Memory Experiments

This module runs the **full experimental pipeline** for evaluating the **LAESA** index (Linear Approximating and Eliminating Search Algorithm), following the parameter settings described in *"Indexing Metric Spaces for Exact Similarity Search"* (Chen et al).

The goal of this benchmark is to reproduce the performance evaluation of:

- **Metric Range Queries (MRQ)** using specified selectivities  
- **Multi-k Nearest Neighbor (MkNN)** queries for several values of *k*  
- **Multiple numbers of pivots** (the parameter *l* in the paper)  
- **Four datasets**: `LA`, `Words`, `Color`, `Synthetic`

The test automatically:

1. Loads each dataset with its correct metric (L2, L1, L∞, or edit distance)  
2. Loads *precomputed queries*, *precomputed radii*, and *HFI pivots* from  
   `prepared_experiment/queries/`, `prepared_experiment/radii/`, `prepared_experiment/pivots/`
3. Builds a **LAESA index for each pivot count**  
   (`l ∈ {3, 5, 10, 15, 20}` — Table 6 of the Chen et al. paper)
4. Executes all MRQ and MkNN queries
5. Records:
   - Average number of distance computations  
   - Average execution time (μs)
6. Saves results into:

```
results_LAESA_<dataset>.json
```

---

## Folder Structure

```
datasets/
│
├── LA.txt
├── Words.txt
├── Color.txt
├── Synthetic.txt
│
└── dataset_processing/
    └── prepared_experiment/
        ├── queries/
        │   ├── LA_queries.json
        │   ├── Words_queries.json
        │   └── ...
        ├── radii/
        │   ├── LA_radii.json
        │   └── ...
        └── pivots/
            ├── LA_pivots_3.json
            ├── LA_pivots_5.json
            ├── LA_pivots_10.json
            ├── LA_pivots_15.json
            ├── LA_pivots_20.json
            └── ...
```

Path resolution is handled by `paths.hpp` through:
- `path_dataset(dataset)`
- `path_queries(dataset)`
- `path_radii(dataset)`
- `path_pivots(dataset, l)`

---

## 🔧 How the Benchmark Works

### 1. Dataset Loading

Each dataset is loaded using its correct metric implementation:

| Dataset      | Metric        | Implementation                     |
|--------------|---------------|------------------------------------|
| **LA**       | L2 norm       | `VectorDB(file, 2)`                |
| **Color**    | L1 norm       | `VectorDB(file, 1)`                |
| **Synthetic**| L∞ norm       | `VectorDB(file, large_p)`          |
| **Words**    | Edit distance | `StringDB(file)`                   |

---

### 2. Pivot Loading (HFI)

For every pivot count `l ∈ {3, 5, 10, 15, 20}`:

- The test resolves the pivot file:  
  `path_pivots(dataset, l)` → e.g. `prepared_experiment/pivots/LA_pivots_10.json`
- Reads the list of pivot IDs  
- Builds a LAESA index:
  
```cpp
LAESA laesa(db.get(), l);
laesa.overridePivots(pivots_from_file);
```

This ensures LAESA uses the same HFI pivots used by other pivot-based indexes.

---

### 3. Executed Queries

#### Metric Range Queries (MRQ)

Selectivities (Chen Table 6):

```
2%, 4%, 8%, 16%, 32%
```

For each selectivity, the corresponding radius is loaded from:

```
prepared_experiment/radii/<dataset>_radii.json
```

Then the test calls:

```cpp
laesa.rangeSearch(query_id, radius, out);
```

The benchmark collects:
- Number of distance computations  
- Execution time (µs)

---

#### MkNN Queries

Values of *k*:

```
5, 10, 20, 50, 100
```

For each query:

```cpp
vector<ResultElem> out;
laesa.knnSearch(query_id, k, out);
```

We again record:
- Distance computations  
- Execution time  

---

## Output Format

For each dataset the benchmark produces a JSON file:

- `results/results_LAESA_LA.json`
- `results/results_LAESA_Words.json`
- `results/results_LAESA_Color.json`
- `results/results_LAESA_Synthetic.json`

Each record follows the unified experiment format:

```json
{
  "index": "LAESA",
  "dataset": "LA",
  "category": "HFI",
  "num_pivots": 10,
  "num_centers_path": null,
  "query_type": "MRQ",
  "selectivity": 0.08,
  "radius": 957.99,
  "k": null,
  "compdists": 368942.7,
  "time_ms": 1.86015,
  "n_queries": 100,
  "run_id": 1
}
```

For **MkNN**, the fields `selectivity` and `radius` are `null`, and `k` is set.

These JSON files can be merged with other index results (BST, BKT, GNAT, PM-Tree, etc.) during the analysis phase.

---

## Running the Benchmark

1. **Compile**

```bash
g++ -O3 -std=gnu++17 test_laesa.cpp -o laesa_test
```

2. **Run**

```bash
./laesa_test
```

Everything is automatic — datasets, pivots, queries, and radii are resolved by the loader.

> [!NOTE]
> - If a dataset or its pivots/queries/radii are missing, that configuration is skipped automatically.
> - LAESA is a main-memory index; no disk-page I/O occurs.
> - Distance computations are handled internally by `ObjectDB` and its subclasses.
