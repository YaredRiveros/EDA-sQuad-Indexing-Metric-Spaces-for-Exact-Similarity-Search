# CPT — Benchmark for Disk-Based Experiments  
**(Reproduction of Section 6 — Chen et al., *Indexing Metric Spaces for Exact Similarity Search*, using Clustered Pivot Tables)**

This module runs the full experimental pipeline to evaluate the **Clustered Pivot Table (CPT)** in a **secondary-memory** setting, following the experimental setup described in the CPT paper and in Chen et al. (2022).

> The benchmark uses:
>
> - **Metric Range Queries (MRQ)** with selectivities `{2%, 4%, 8%, 16%, 32%}`  
> - **Multi-k Nearest Neighbor Queries (MkNN)** with `k ∈ {5, 10, 20, 50, 100}`  
> - The same **100 random queries** and **precomputed radii** used by all other indexes  
> - The four datasets tested in the paper: `LA`, `Words`, `Color`, `Synthetic`  
> - The same **HFI pivots** (and pivot counts `l`) as in the LAESA benchmark  

---

## Why CPT is different

CPT is a **disk-based, pivot-based** index obtained by combining:

1. A **pivot table in main memory** (as in LAESA):  
   - A set of `l` pivots selected by HFI.  
   - A matrix of distances from each object to each pivot.

2. A **clustered layout of objects in secondary memory**, derived from an **M-tree in disk**:  
   - Objects are stored in **pages**, each page corresponding to a **leaf node** of an M-tree.  
   - This layout groups similar objects together on disk, improving I/O locality.

Thus, CPT separates:

- **Pruning power** → given by pivots and triangle inequality (Lemma 4.1, as in LAESA).  
- **I/O behavior** → given by the page layout induced by the disk-based M-tree.

In the experiments:

- The **M-tree is built once** using the same configuration as the standalone M-tree index (page size, node capacity).  
- CPT only reads the **M-tree index file** (e.g. `LA.mtree_index`) to derive the clustered page layout, and **does not** use the tree structure during query time.  
- CPT acts as a **pivot-table on top of a clustered datafile**.

---

## Parameters evaluated

CPT shares the same **number of pivots `l`** with LAESA and other pivot-based indexes.

For each dataset we evaluate **five CPT configurations**, one for each value of `l`:

| Experiment | Number of pivots `l` |
|-----------:|----------------------|
| CPT-1      | 3                    |
| CPT-2      | 5                    |
| CPT-3      | 10                   |
| CPT-4      | 15                   |
| CPT-5      | 20                   |

For each `(dataset, l)` combination:

1. **Pivots** are loaded from the same HFI files used by LAESA.  
2. The **pivot table** (distances object–pivot) is built in main memory.  
3. The **page layout** is extracted from the **M-tree index file**:

   - The file `<dataset>.mtree_index` is produced beforehand by the M-tree benchmark.  
   - Its leaves (nodes with `isLeaf = true`) are scanned sequentially;  
   - The list of `objId` stored in each leaf becomes a **CPT data page**.

We record, for each configuration:

- Number of pivots `l`  
- Average distance computations  
- Average running time  
- Average number of **page reads** (data pages accessed)  

(All averaged over the same 100 queries.)

---

## MRQ (Metric Range Query)

For each selectivity `s ∈ {0.02, 0.04, 0.08, 0.16, 0.32}`:

- The target radius `r_s` is loaded from  
  `prepared_experiment/radii/<dataset>_radii.json`
- Distances from the query to all pivots are computed and stored in memory.  
- Then CPT performs a **page scan**:

  1. For each **data page**:
     - For each object in the page, a **lower bound** (Lemma 4.1) is computed using pivot distances.  
     - Objects whose lower bound is greater than `r_s` are **pruned**.  
  2. If **no object** in the page survives, the entire page is skipped (no I/O counted).  
  3. Otherwise:
     - The page is considered **read once from disk** (`pageReads++`).  
     - Exact distances `d(q, o)` are computed **only** for the surviving candidates.  
     - Objects within distance `r_s` are added to the result.

The same set of 100 queries and radii is used for all indexes.

---

## MkNN Queries

For each `k ∈ {5, 10, 20, 50, 100}`:

- CPT uses an **I/O-friendly kNN strategy** inspired by the CPT paper:

  1. **Pre-scan phase**  
     - A small non-clustered prefix of objects (a fraction of the dataset, e.g. 2%) is scanned in ID order.  
     - Exact distances to these objects are computed, and an initial `k`-NN radius `τ` is obtained.

  2. **Clustered scan phase**  
     - The CPT pages derived from the M-tree leaves are scanned **in physical order**.  
     - For each page:
       - A **lower bound** is computed for each object (using pivots).  
       - Objects whose lower bound is ≥ `τ` are pruned.  
       - If at least one candidate remains, the page is considered **read once** (`pageReads++`), and exact distances are computed for candidates only.  
       - The `k` nearest neighbors and the radius `τ` are updated dynamically.

- **No global reordering** of objects by lower bound is performed (unlike pure LAESA), preserving sequential access to pages and yielding realistic disk I/O behavior.

Again, all indexes share the same query set, so distances, times and page reads are directly comparable.

---

## Output Format

Each dataset produces a JSON file:

```text
results_CPT_<dataset>.json
```

Each entry corresponds to a single experimental configuration (a specific `l` and either an MRQ selectivity or a MkNN value `k`), and includes:

```json
{
  "index": "CPT",
  "dataset": "LA",
  "category": "DM",
  "num_pivots": 10,
  "num_centers_path": null,
  "arity": null,
  "query_type": "MRQ",
  "selectivity": 0.08,
  "radius": 0.00388,
  "k": null,
  "compdists": 95421.37,
  "time_ms": 0.43,
  "pages": 12.37,
  "n_queries": 100,
  "run_id": 1
}
```

- `index`: `"CPT"` (Clustered Pivot Table)  
- `dataset`: dataset name (`"LA"`, `"Words"`, `"Color"`, `"Synthetic"`)  
- `category`: `"DM"` (disk-based metric index)  
- `num_pivots`: number of pivots `l`  
- `num_centers_path`: not applicable (kept as `null`)  
- `arity`: not applicable for CPT (kept as `null`)  
- `query_type`: `"MRQ"` or `"MkNN"`  
- `selectivity`: query selectivity for MRQ (or `null` for MkNN)  
- `radius`: MRQ radius `r_s` (or `null` for MkNN)  
- `k`: MkNN parameter (or `null` for MRQ)  
- `compdists`: average number of distance computations per query  
- `time_ms`: average running time per query in milliseconds  
- `pages`: average number of **data pages read** per query (I/O cost)  
- `n_queries`: number of queries (100 in these experiments)  
- `run_id`: run identifier (default `1`)  

---

## Running the benchmark

1. **Precondition: build the M-tree index files**

   Before running CPT, you must build the M-tree indexes used for clustering:

   ```bash
   g++ -O3 -std=c++17 mtree_test.cpp -o mtree_test

   ./mtree_test
   ```

   This produces files like:

   - `LA.mtree_index`  
   - `Words.mtree_index`  
   - `Color.mtree_index`  
   - `Synthetic.mtree_index`  

   in the current directory (or in the paths configured in your code).

2. **Compile CPT benchmark:**

   ```bash
   g++ -O3 -std=c++17 cpt_test.cpp -o cpt_test
   ```

3. **Run CPT benchmark:**

   ```bash
   ./cpt_test
   ```

This will:

- Load each dataset (`LA`, `Words`, `Color`, `Synthetic`);  
- For each dataset and each `l ∈ {3, 5, 10, 15, 20}`:
  - Load HFI pivots and build the pivot table in memory;  
  - Derive the page layout from `<dataset>.mtree_index`;  
  - Run all MRQ selectivities and MkNN settings;  
- Write the results to:

```text
results_CPT_<dataset>.json
```

> [!NOTE]
> CPT separates **pruning power** (pivots in memory) from **I/O behavior** (clustered pages derived from the M-tree).  
> Pivots and queries are identical to those used for LAESA and other pivot-based indexes, ensuring fair comparison of distance computations and I/O costs.  
> The only difference is that CPT exploits a clustered layout of objects on disk to reduce page reads for both MRQ and MkNN queries.
