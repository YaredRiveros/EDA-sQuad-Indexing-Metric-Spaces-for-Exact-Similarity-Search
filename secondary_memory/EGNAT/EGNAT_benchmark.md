# EGNAT — Benchmark for Disk-Based Experiments  
(Reproduction of Section 6 — Chen et al., “Indexing Metric Spaces for Exact Similarity Search”)

This module runs the full experimental pipeline to evaluate the EGNAT index in a disk-based setting, following the configuration used in the secondary-memory experiments of Chen et al. (2020).

The benchmark evaluates:

- Metric Range Queries (MRQ), using selectivities  
  {2%, 4%, 8%, 16%, 32%}

- Multi-k Nearest Neighbor Queries (MkNN), with  
  k ∈ {5, 10, 20, 50, 100}

- The same 100 random queries and precomputed radii used for all other indexes

- The four datasets used in the paper:  
  LA, Words, Color, Synthetic

The C++ driver (test.cpp):

1. Loads each dataset into an ObjectDB with the correct metric  
2. Builds a disk-based EGNAT index  
3. Executes MRQ and MkNN for all queries  
4. Computes averaged metrics across queries  
5. Produces a JSON file with the measurements needed for reproduction



# EGNAT in this benchmark

EGNAT (Evolutionary Geometric Near-neighbor Access Tree) is a disk-based hybrid metric structure derived from GNAT.  
It integrates:

- GNAT-style pivot partitioning (m pivots per internal node)
- Min/max distance tables for generalized hyperplane pruning (GNAT Lemma)
- Distance-to-parent pruning at leaves (EGNAT extension)
- Disk-based node storage, where each node occupies one logical page

In Chen’s evaluation, EGNAT is used in static form:

- No insertions or deletions performed
- No evolution step or ghost hyperplanes required
- Tree is built once over the entire dataset
- Pivots are chosen independently at each level
- Leaf entries store dist(obj, pivot_parent), enabling Lemma 4.2 pruning

This benchmark implements exactly that configuration.



# Disk layout

EGNAT uses two disk files per index:

```
  base.egn_index    (internal nodes)
  base.egn_leaf     (leaf entries)
```

An internal node is serialized as:

```
isLeaf      : uint8_t (0 or 1)
If internal:
    m           : int32_t                # number of pivots (typically 5)
    pivot[m]    : int32_t[m]             # pivot object IDs
    child[m]    : int32_t[m]             # child node indices (or -1)
    minv[m][m]  : double                 # min distances to pivot j in subtree
    maxv[m][m]  : double                 # max distances to pivot j in subtree
If leaf:
    parentPivot : int32_t                # ID of pivot in parent
    offset      : int64_t                # offset into egn_leaf file
    count       : int32_t                # number of LeafEntry elements
```

Leaf entries are stored separately in `base.egn_leaf`:

```
LeafEntry {
    int32_t id;
    double  distParent;   # dist(id, parentPivot)
}
```

Each leaf entry occupies:

```
4 bytes (id) + 8 bytes (distParent) = 12 bytes
```

Leaf pages are written contiguously; the offset in LeafInfo is measured in units of `LeafEntry`.



# Page size and arity

Following Chen et al.:

- For datasets LA and Words, the logical page size is 4KB  
- For Color and Synthetic, the page size is 40KB, interpreted as 10 logical pages of 4KB

EGNAT does not use node-capacity based arity.  
Instead, it always uses:

```
m = 5 pivots per internal node
```

This matches the configuration used for all multi-pivot and hybrid methods in the paper.

Page accesses (PA) are counted as:

- One internal-node load = 1 page read  
- One leaf load = pagesPerNode page reads, where  
  pagesPerNode = pageBytes / 4096  
(this is 10 for 40 KB pages, or 1 for 4 KB pages).



# Metrics recorded

For each dataset and each query type, the benchmark records:

- compdists: average number of distance computations  
- time_ms: average running time per query (milliseconds)  
- pages: average Page Accesses (PA) per query  
- n_queries: number of queries performed (100)

Index-specific fields are:

- index: "EGNAT"
- dataset: dataset name (LA, Words, Color, Synthetic)
- category: "DM" (disk-based metric index)
- num_pivots: 5 (arity of GNAT partitioning)
- num_centers_path: null (not used for GNAT-like structures)
- arity: 5 (pivot arity)
- query_type: "MRQ" or "MkNN"
- selectivity: s for MRQ, null for MkNN
- radius: radius used for MRQ, null for MkNN
- k: k for MkNN, null for MRQ
- run_id: run identifier (always 1 in this benchmark)



# MRQ (Metric Range Query)

For each selectivity s:

1. The corresponding radius R_s is loaded from the precomputed radii JSON.  
2. For each query q in the 100-query set, the EGNAT tree is traversed depth-first.  
3. Pruning rules:

A) GNAT range pruning (generalized hyperplane pruning)  
The child j of pivot c is pruned if:

```
[d_min(c,q) - R_s, d_min(c,q) + R_s] does not intersect [minv[j][c], maxv[j][c]]
```

B) Pivot-ball pruning  
The pivot itself is included if dist(q, pivot) ≤ R_s.

C) Leaf dist-to-parent pruning (EGNAT Lemma 4.2)  
For a leaf entry with stored distParent:

```
if |distParent - dist(q, parentPivot)| > R_s → prune
```

D) Direct verification of remaining candidates.

4. Each internal node read counts as 1 page access.  
5. Each leaf accessed counts as pagesPerNode page accesses.



# MkNN queries

For each k in {5, 10, 20, 50, 100}, the benchmark performs k-nearest-neighbor search using:

- Best-first pivot exploration
- Dynamic pruning based on current worst k-distance
- GNAT lower-bound pruning

At each internal node:

- For each pivot p, compute d(q,p).  
- A lower bound for the child is computed using the GNAT range table.  
- If this lower bound exceeds the current worst k-distance, prune.  
- Otherwise enqueue the child.

At leaves:

- dist-to-parent pruning is applied  
- Only candidates that pass are evaluated  
- The k-best heap is updated

Page accesses are counted the same way as MRQ.



# Output format

For each dataset, the benchmark writes a JSON file:

```
results/results_EGNAT_<dataset>.json
```

Each entry has the schema:

```
{
  "index": "EGNAT",
  "dataset": "LA",
  "category": "DM",
  "num_pivots": 5,
  "num_centers_path": null,
  "arity": 5,
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



# Running the benchmark

1. Compile the driver:

```
g++ -O3 -std=c++17 test_egn.cpp -o egn_test
```

2. Run the benchmark:

```
./egn_test
```

This will:

- Build the EGNAT indexes in egn_indexes/  
- Run MRQ and MkNN over all queries  
- Produce JSON outputs in results/results_EGNAT_<dataset>.json  
