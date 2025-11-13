# BST Benchmark — Main-Memory Experiments

This module runs the **full experimental pipeline** for evaluating the **Binary Spatial Tree (BST)** structure, following the parameter settings   described in *"Indexing Metric Spaces for Exact Similarity Search"* (Chen et al).

The goal of this test is to reproduce the performance evaluation of:
- **Metric Range Queries (MRQ)** using specified selectivities
- **Multi-k Nearest Neighbor (MkNN)** queries for several values of *k*
- **Multiple tree heights**, analogous to the *number of pivots l* in the paper
- **Four datasets**: `LA`, `Words`, `Color`, `Synthetic`

The test automatically:
1. Loads each dataset with its correct metric (L2, L1, L∞, or edit distance)
2. Loads precomputed queries and radii  
   (`prepared_experiment/queries/`, `prepared_experiment/radii/`)
3. Builds a BST index for each height in `{3, 5, 10, 15, 20}`
4. Executes all MRQ/MkNN queries
5. Computes:
   - Average number of distance computations
   - Average execution time (μs)
6. Appends the results into a JSON file per dataset:

```
results_BST_<dataset>.json
```

---

## 📁 Folder Structure

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
│ LA_queries.json
│ Words_queries.json
│ ...
├── radii/
│ LA_radii.json
│ ...
└── pivots/
```

`paths.hpp` automatically resolves all paths inside the project.

---

## **How the Test Works**

### 1. Dataset Loading
Each dataset uses the correct metric:

| Dataset     | Metric         | Implementation                     |
|-------------|----------------|------------------------------------|
| **LA**      | L2 norm        | `VectorDB(file, 2)`                |
| **Color**   | L1 norm        | `VectorDB(file, 1)`                |
| **Synthetic**| L∞ norm       | `VectorDB(file, large_p)`          |
| **Words**   | Edit distance  | `StringDB(file)`                   |

---

### 2. Tree Construction (BST)

For each height `h ∈ {3, 5, 10, 15, 20}`:

- A new BST is created
- Height is enforced using `maxHeight`
- Complexity counters are cleared before each query

This mirrors the role of the *“number of pivots l”* from the original paper.

---

### 3. Executed Queries

#### **Metric Range Query (MRQ)**  
Selectivity values (paper Table 6):

```
2%, 4%, 8%, 16%, 32%
```

Each corresponds to a precomputed radius stored in:

```
prepared_experiment/radii/<dataset>_radii.json
```


#### **MkNN Queries**

Values of k:

```
5, 10, 20, 50, 100
```

---

### 4. Measurements Collected

For every query:
- Total distance computations  
- Total execution time (μs)

Then averaged over all queries (typically 100).

---

## Output Format

For each dataset, a JSON file is created:

- results/results_BST_LA.json
- results/results_BST_Words.json
- results/results_BST_Color.json
- results/results_BST_Synthetic.json


Each entry follows the unified experiment format:

```json
{
  "index": "BST",
  "dataset": "LA",
  "category": "CP",
  "num_pivots": 0,
  "num_centers_path": 10,
  "arity": null,
  "query_type": "MRQ",
  "selectivity": 0.08,
  "radius": 957.99,
  "k": null,
  "compdists": 368942.7,
  "time_ms": 1.86015,
  "n_queries": 100,
  "run_id": 1
}

These JSON files can be merged later by a Python aggregation script.
```

---

# Running the Test

1. Compile

    g++ -O3 -std=gnu++17 test.cpp -o bst_test

2. Run

    ./bst_test

No arguments required — everything is automatic.

> [!NOTE]
> If a dataset does not exist or cannot load its queries/radii, it is automatically skipped.
> All results are appended to the JSON file without overwriting previous runs.
> This module is part of the Main Memory index family (no disk I/O pages).
> Distance metrics are handled internally by ObjectDB.