# LC — Benchmark for Disk-Based Experiments  
**(Reproduction of Section 6.3 — Chen et al., *Indexing Metric Spaces for Exact Similarity Search*)**

This module runs the full experimental pipeline to evaluate **LC (List of Clusters)** in a
**disk-based setting**, following the exact configuration described in the *secondary-memory*
experiments of the paper.

> The benchmark evaluates:
>
> - **Metric Range Queries (MRQ)** using selectivities  
>   `{2%, 4%, 8%, 16%, 32%}`
>
> - **Multi-k Nearest Neighbor Queries (MkNN)** with  
>   `k ∈ {5, 10, 20, 50, 100}`
>
> - The same **100 random queries** and **precomputed radii** used by all other indexes
>
> - The four datasets in the paper:  
>   `LA`, `Words`, `Color`, `Synthetic`

---

## Why LC is different

LC is **not** a pivot-based index and not a tree.

It belongs to the **compact partitioning** family:

- No pivots  
- No tree structure  
- No hierarchical routing  
- Each cluster contains a **center** and a **variable number of nearby objects**  
- All data objects are **stored directly in the index on disk**

Because LC stores raw objects inside clusters, it is highly dependent on the **page size**.
The paper states:

> “LC, DSACLT, and EGNAT store the data objects directly in their index structures.  
> Thus, they need a large page size for high-dimensional data…  
> 
> A 40KB page can be regarded as 10 4KB pages for one tree node or cluster.”

Therefore:

| Dataset     | Dimensionality | Page Size | Page equivalents |
|-------------|----------------|-----------|------------------|
| LA         | Low            | 4 KB     | 1                |
| Words      | Varied         | 4 KB     | 1                |
| Color      | 282D           | 40 KB    | 10               |
| Synthetic  | 205D           | 40 KB    | 10               |

During a query, **visiting one cluster costs pagesPerCluster page accesses**, matching the
semantics used in Figures 25–26 of the paper.

---

## LC Construction

LC follows the exact algorithm described in the paper:

1. Maintain a list `remaining` of unclustered objects  
2. Pick as center the object with maximal accumulated distance (`tdist`)  
3. Compute all distances to center  
4. Take up to `bucketSize` nearest as members  
5. Remove chosen objects from `remaining`  
6. Repeat until all objects are assigned

This yields:

- Approximately `n / bucketSize` clusters  
- One “level” (flat index), so **num_centers_path = 1**  
- No pivots: **num_pivots = 0**

The index is fully written to disk as:

```
<base>.lc_index (cluster metadata) 
<base>.lc_node (physical cluster contents) 
```

and later opened using restore().

## Metrics Recorded

For each dataset and query type, the benchmark records:

compdists : average number of distance computations

time_ms : average running time per query

pages : average Page Accesses (PA)

n_queries : always 100

num_pivots = 0

num_centers_path = 1

arity = null (LC has no branching factor)

All metrics follow the paper’s schema for disk-based metric indexes.

## MRQ (Metric Range Query)

For each selectivity $s$:

- The radius $r_s$ is loaded from prepared_experiment/radii/<dataset>_radii.json

- A cluster is pruned when:

```
dist(q, center) > radius + R
```

- Otherwise, the entire cluster is visited

- Visiting a cluster costs:

$$
pagesPerCluster = pageBytes / 4096
$$


## MkNN Queries

For each $k$ ∈ {5, 10, 20, 50, 100}:

- A max-heap of size k is maintained

- Cluster pruning:

$$
pagesPerCluster logical pages
$$


## Output Format


Each dataset produces a JSON file:

```
results_LC_<dataset>.json
```


Each entry has the schema:

```
{
  "index": "LC",
  "dataset": "LA",
  "category": "DM",
  "num_pivots": 0,
  "num_centers_path": 1,
  "arity": null,
  "query_type": "MRQ",
  "selectivity": 0.08,
  "radius": 957.99,
  "k": null,
  "compdists": 214725.98,
  "time_ms": 7.04,
  "pages": 208.79,
  "n_queries": 100,
  "run_id": 1
}
```

# Runnning the benchmark


1.  Compile:
  
    g++ -O3 -std=c++17 test.cpp -o lc_test

2. Run:

    ./lc_test

**This automatically builds the LC index on disk, restores it, executes MRQ and MkNN for every dataset, and outputs the JSON files.**



> [!NOTE]
> LC uses no pivots (num_pivots = 0)
> LC has one level (num_centers_path = 1)
>
> Color and Synthetic require large pages (40KB), equivalent to 10 logical pages
> 
> LA and Words use standard 4KB pages
