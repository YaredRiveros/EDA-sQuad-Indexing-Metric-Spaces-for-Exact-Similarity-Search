# MVPT Benchmark — Main-Memory Experiments

This module runs the **full experimental pipeline** for evaluating the **Multi-Vantage Point Tree (MVPT)** structure, following the parameter settings described in *"Indexing Metric Spaces for Exact Similarity Search"* (Chen et al).

The goal of this test is to reproduce the performance evaluation of:
- **Metric Range Queries (MRQ)** using specified selectivities
- **Multi-k Nearest Neighbor (MkNN)** queries for several values of *k*
- **Multiple bucket sizes** to control tree depth
- **Four datasets**: `LA`, `Words`, `Color`, `Synthetic`

The test automatically:
1. Loads each dataset with its correct metric (L2, L1, L∞, or edit distance)
2. Loads precomputed queries and radii  
   (`prepared_experiment/queries/`, `prepared_experiment/radii/`)
3. Builds an MVPT index for each bucket size in `{5, 10, 20, 50, 100}`
4. Executes all MRQ/MkNN queries
5. Computes:
   - Average number of distance computations
   - Average execution time (μs)
6. Appends the results into a JSON file per dataset:

```
results_MVPT_<dataset>.json
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

### 2. Tree Construction (MVPT)

For each bucket size `b ∈ {5, 10, 20, 50, 100}`:

- A new MVPT is created with **arity = 5** (as specified in the paper)
- Bucket size controls tree depth indirectly
- Global counter `compdists` is used to track distance computations

The paper states: *"GNAT and MVPT are multi-arity trees. Here, we set arity to 5"*

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
- Total distance computations (via global `compdists` counter)
- Total execution time (μs)

Then averaged over all queries (typically 100).

---

## Output Format

For each dataset, a JSON file is created:

- results/results_MVPT_LA.json
- results/results_MVPT_Words.json
- results/results_MVPT_Color.json
- results/results_MVPT_Synthetic.json


Each entry follows the unified experiment format:

```json
{
  "index": "MVPT",
  "dataset": "LA",
  "category": "PB",
  "num_pivots": 0,
  "num_centers_path": 0,
  "arity": 5,
  "bucket_size": 10,
  "query_type": "MRQ",
  "selectivity": 0.08,
  "radius": 957.99,
  "k": null,
  "compdists": 245678.3,
  "time_ms": 1.24532,
  "n_queries": 100,
  "run_id": 1
}
```

These JSON files can be merged later by a Python aggregation script.

---

## Running the Test

1. Compile

    ```bash
    g++ -O3 -std=gnu++17 test.cpp -o mvpt_test
    ```

2. Run

    ```bash
    ./mvpt_test
    ```

No arguments required — everything is automatic.

> [!NOTE]
> - If a dataset does not exist or cannot load its queries/radii, it is automatically skipped.
> - All results are appended to the JSON file without overwriting previous runs.
> - This module is part of the Main Memory index family (no disk I/O pages).
> - Distance metrics are handled internally by ObjectDB.
> - MVPT uses a global counter `compdists` for tracking distance computations.
> - **Arity is fixed to 5** as specified in the paper for all experiments.
