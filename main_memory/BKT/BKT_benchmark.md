# BKT — Benchmark for Main-Memory Experiments  
**(Reproduction of Section 6 — Chen et al., *Indexing Metric Spaces for Exact Similarity Search*)**

This module runs the full experimental pipeline to evaluate the **Burkhard–Keller Tree (BKT)** in a main-memory setting, following the   parameterization described in the paper.

>The benchmark uses:
>
> - **Metric Range Queries (MRQ)** with selectivities `{2%, 4%, 8%, 16%, 32%}`  
> - **Multi-k Nearest Neighbor Queries (MkNN)** with `k ∈ {5, 10, 20, 50, 100}`  
> - The same **100 random queries** and **precomputed radii** used by all other indexes  
> - The four datasets tested in the paper:  `LA`, `Words`, `Color`, `Synthetic`  

---

## Why BKT is different

The BKT *is not* a pivot-based index.   It belongs to the **FQ family**, and:

- does **not** use global pivots (HFI),
- selects **local pivots randomly** during construction,
- its number of pivots depends on:
  - the bucket size,  
  - the ring width (`step`),  
  - the structure produced during build.  

Thus, performance cannot be controlled by fixing “the number of pivots”.  Instead, **we vary the construction parameters** to obtain BKTs of increasing heights (analogous to the values `l = 3, 5, 10, 15, 20` used in the paper).

---

## Parameters evaluated

For each dataset we evaluate five BKT configurations:

| Experiment | Bucket size | Step size |
|------------|-------------|-----------|
| BKT-1 | 50 | 16 |
| BKT-2 | 30 | 8  |
| BKT-3 | 20 | 4  |
| BKT-4 | 10 | 2  |
| BKT-5 | 5  | 1  |

> Unlike pivot-based structures, BKT does not use global pivots and does not use  radiuses computed from the dataset. Its behavior is controlled only by two parameters: bucket size (b) and ring width (step). In the original C implementation distributed with the paper, 'step' is a small integer constant (e.g., 1, 2, 4, 8, 16). We therefore follow the same design to reproduce the  experimental settings of Section 6.


We record:

- Actual height of the tree  
- Number of pivots generated  
- Average distance computations  
- Average running time  

(All averaged over the same 100 queries.)

---

## MRQ (Metric Range Query)

For each selectivity `s ∈ {0.02, 0.04, 0.08, 0.16, 0.32}`:

- The radius `r_s` is loaded from  
  `prepared_experiment/radii/<dataset>_radii.json`
- A depth-first search is executed  
- Lemma 4.1 prunes subtrees whose ring `[d, d+step)` does not intersect the search region

---

## MkNN Queries

For each `k ∈ {5, 10, 20, 50, 100}`:

- Best-first traversal is used  
- Nodes are explored in order of their minimum possible distance to the query  
- Lemma 4.1 is applied again to prune nodes that cannot contain closer objects  
- A max-heap of size `k` tracks the current nearest neighbors  

---

## Output Format

Each dataset produces a JSON file:

```
results_BKT_<dataset>.json
```

Each entry includes:

```json
{
  "index": "BKT",
  "dataset": "LA",
  "category": "FQ",
  "num_pivots": 982, 
  "num_centers_path": 14,
  "query_type": "MRQ",
  "selectivity": 0.08,
  "radius": 0.00388,
  "k": null,
  "compdists": 128433.55,
  "time_ms": 0.51,
  "n_queries": 100,
  "run_id": 1
}
```

# Running the benchmark

1.  Compile:
  
    g++ -O3 -std=c++17 test.cpp -o bkt_test

2. Run:

    ./bkt_test

**All datasets, parameters, MRQ/MkNN settings, and JSON outputs are generated automatically.**


> [!NOTE]
> The BKT is unbalanced; its height depends on the parameters.
> 
> Using the same pivots as LAESA or MVP-tree would turn BKT into FQT, so this benchmark follows the correct setup from the paper.
> 
> All comparisons use the same queries and radii for fairness.