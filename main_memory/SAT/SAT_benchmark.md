# SAT — Benchmark for Main-Memory Experiments  
**(Reproduction of Section 6 — Chen et al., *Indexing Metric Spaces for Exact Similarity Search*)**

This module runs the full experimental pipeline to evaluate the **Spatial Approximation Tree (SAT)** in a main-memory setting, following the experimental setup described in the paper.

>The benchmark uses:
>
> - **Metric Range Queries (MRQ)** with selectivities `{2%, 4%, 8%, 16%, 32%}`  
> - **Multi-k Nearest Neighbor Queries (MkNN)** with `k ∈ {5, 10, 20, 50, 100}`  
> - The same **100 random queries** and **precomputed radii** used by all other indexes  
> - The four datasets tested in the paper:  `LA`, `Words`, `Color`, `Synthetic`  

---

## Why SAT is different

The SAT is also part of the **FQ family**, but:

- it is **not** a pivot-based index (no global pivots, no precomputed radii),  
- it does **not** expose tunable construction parameters like bucket size or fanout,  
- its structure is fully determined by:
  - the choice of local centers during construction,  
  - the rule for attaching new objects to existing centers,  
  - the intrinsic geometry of the dataset.  

In the original implementation, each node stores:

- a **center object**,  
- the **maximum distance** (`maxDist`) to any object in its subtree,  
- a list of **children**, each of which is itself a SAT node.

Thus, performance cannot be controlled by fixing “the number of pivots” or a bucket size. Instead, the SAT benchmark evaluates **the single, natural configuration** produced by the standard construction algorithm, and reports:

- Actual height of the tree  
- Number of centers (nodes) generated  
- Average distance computations  
- Average running time  

(All averaged over the same 100 queries.)

---

## Parameters evaluated

Unlike BKT, SAT does **not** use external construction parameters such as bucket size or ring width. For each dataset we evaluate **one SAT configuration**:

- All objects in the dataset are inserted into a single SAT built with the original algorithm.
- Local centers are created automatically when an object cannot be better explained by existing centers.
- Node radii (`maxDist`) are derived from the distances observed during construction.

We record, per dataset:

- Actual tree height (`num_centers_path`)  
- Number of centers / nodes (`num_pivots`)  
- Average distance computations  
- Average running time  

---

## MRQ (Metric Range Query)

For each selectivity `s ∈ {0.02, 0.04, 0.08, 0.16, 0.32}`:

- The radius `r_s` is loaded from  
  `prepared_experiment/radii/<dataset>_radii.json`
- A recursive search is executed from the root:
  - A subtree is **pruned** if the ball centered at the node’s center with radius `maxDist`
    does not intersect the query ball of radius `r_s` (triangle-inequality pruning).
  - SAT-specific pruning is applied using the local structure of centers and their
    maximum radii, so that children whose subtrees cannot intersect the query ball are skipped.

The same 100 queries and radii are used across all indexes for comparability.

---

## MkNN Queries

For each `k ∈ {5, 10, 20, 50, 100}`:

- A **best-first traversal** is used:
  - Nodes are stored in a priority queue ordered by a **lower bound** on the distance
    from the query to any object in that node’s subtree.
  - At each step, the node with smallest lower bound is expanded.
- For each visited node:
  - Its center is considered as a candidate neighbor.
  - Children are inserted into the priority queue with updated lower bounds derived from:
    - the distance from the query to the child’s center, and  
    - the child’s maximum subtree radius.
- A **max-heap of size `k`** tracks the current nearest neighbors, and any node whose
  lower bound exceeds the current `k`-NN radius is pruned.

Again, all indexes share the same query set, so distances and times are directly comparable.

---

## Output Format

Each dataset produces a JSON file:

```
results_SAT_<dataset>.json
```

Each entry includes:

```json
{
  "index": "SAT",
  "dataset": "LA",
  "category": "FQ",
  "num_pivots": 1037,
  "num_centers_path": 12,
  "query_type": "MRQ",
  "selectivity": 0.08,
  "radius": 0.00388,
  "k": null,
  "compdists": 95421.37,
  "time_ms": 0.43,
  "n_queries": 100,
  "run_id": 1
}
```

- `num_pivots`: number of SAT nodes / centers created during construction  
- `num_centers_path`: height of the tree (maximum root-to-leaf path length)  
- `compdists`: average number of distance computations per query  
- `time_ms`: average running time per query in milliseconds  

---

# Running the benchmark

1.  Compile:

    ```bash
    g++ -O3 -std=c++17 sat_test.cpp -o sat_test
    ```

2. Run:

    ```bash
    ./sat_test
    ```

**All datasets, MRQ/MkNN settings, and JSON outputs are generated automatically for the SAT index.**

> [!NOTE]
> The SAT is also unbalanced; its height and number of centers depend on the dataset and on the construction order.
>
> It does not share global pivots or radii with LAESA, MVP-tree, or other pivot-based indexes; instead, it maintains local centers and subtree radii.
>
> All comparisons use the same queries and radii as in the paper, so the reported distance computations and times are directly comparable to other FQ-family and pivot-based structures.
