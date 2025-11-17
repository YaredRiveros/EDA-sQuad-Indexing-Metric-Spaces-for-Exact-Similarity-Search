# M-Index* — Benchmark for Disk-Based Experiments  
(Implementation based on "Pivot-based Metric Indexing" - Novak et al.)

This module implements the M-Index* (M-index star), a disk-based metric index that combines generalized hyperplane partitioning and pivot mapping with Minimum Bounding Boxes (MBB) for enhanced pruning.

The benchmark evaluates:

- Metric Range Queries (MRQ), using selectivities  
  {0.0001, 0.001, 0.01, 0.02, 0.05}

- Multi-k Nearest Neighbor Queries (MkNN), with  
  k ∈ {1, 5, 10, 20, 50}

- The same 100 random queries and precomputed radii used for other indexes

- Three datasets:  
  LA, Synthetic, Words

The C++ driver (test.cpp):

1. Loads each dataset into an ObjectDB with the correct metric  
2. Builds a disk-based M-Index* with B+-tree simulation  
3. Executes MRQ and MkNN for all queries  
4. Computes averaged metrics across queries  
5. Produces a JSON file with the measurements



## M-Index* in this benchmark

M-Index* is a pivot-based metric index that extends M-Index by integrating MBB information. It combines:

- **Pivot partitioning**: Each object is assigned to its nearest pivot
- **Key mapping**: Objects are mapped to real numbers using `key(o) = d(pi, o) + (i-1) × d+`
- **B+-tree indexing**: Keys are indexed for efficient range search
- **MBB (Minimum Bounding Box)**: Min/max distances to all pivots per cluster
- **Multiple pruning lemmas**: Lemma 4.1, 4.3, and 4.5 for filtering

### Key Features Implemented

**Lemma 4.1 - MBB Cluster Pruning:**  
A cluster is pruned if for any pivot j:
```
dq[j] + R < minDist[j]  OR  dq[j] - R > maxDist[j]
```

**Lemma 4.3 - Object Pruning:**  
An object o is pruned if for any pivot j:
```
|d(o, pj) - d(q, pj)| > R
```

**Lemma 4.5 - Object Validation:**  
An object o is validated (without computing d(q,o)) if there exists a pivot pi such that:
```
d(o, pi) ≤ R - d(q, pi)
```
This reduces distance computations significantly.

**Best-First Traversal for k-NN:**  
Clusters are processed in ascending order of their lower bound distance to the query.



## Disk layout

M-Index* uses a Random Access File (RAF) to store objects with precomputed distances:

```
  base.midx_raf    (objects with pivot distances)
```

The RAF is structured as a B+-tree simulation using `std::map<double, vector<RAFEntry>>`:

```
RAFEntry {
    int32_t id;                    // object ID
    vector<double> dists;          // distances to P pivots
    double key;                    // mapped key
}
```

Each cluster maintains:
```
ClusterNode {
    bool isLeaf;
    double minkey, maxkey;         // key range
    vector<double> minDist;        // MBB min distances
    vector<double> maxDist;        // MBB max distances
    int32_t count;                 // objects in cluster
}
```



## Parameters

Following the configuration:

- **Number of pivots**: P = 5
- **Maximum distance**: d+ computed from 1000-sample maximum
- **Page size**: Approximated as 100 entries per page

Key features:
- Static index (no insertions after build)
- Random pivot selection
- Objects partitioned to nearest pivot
- Keys sorted for B+-tree simulation



## Metrics recorded

For each dataset and each query type, the benchmark records:

- **compdists**: average number of distance computations  
- **time_ms**: average running time per query (milliseconds)  
- **pages**: average Page Accesses (PA) per query  
- **n_queries**: number of queries performed (100)

Index-specific fields:

- **index**: "MIndex*_Improved"
- **dataset**: dataset name (LA, Synthetic, Words)
- **category**: "DM" (disk-based metric index)
- **num_pivots**: 5
- **num_centers_path**: null
- **arity**: null
- **query_type**: "MRQ" or "MkNN"
- **selectivity**: s for MRQ, null for MkNN
- **radius**: radius used for MRQ, null for MkNN
- **k**: k for MkNN, null for MRQ
- **run_id**: run identifier (always 1)



## MRQ (Metric Range Query)

For each selectivity s:

1. The corresponding radius R is loaded from precomputed radii
2. For each query q, clusters are traversed
3. Pruning strategy:

**A) Cluster-level MBB pruning (Lemma 4.1)**  
Cluster pruned if any pivot violates MBB bounds

**B) B+-tree range search**  
Only entries within [minkey, maxkey] are accessed

**C) Object validation (Lemma 4.5)**  
Objects validated directly if condition holds

**D) Object pruning (Lemma 4.3)**  
Objects pruned by pivot-distance bounds

**E) Direct verification**  
Remaining objects verified by computing d(q,o)

4. Page accesses counted per cluster and B+-tree access



## MkNN queries

For each k, the benchmark performs k-nearest-neighbor using:

- **Best-first cluster traversal** by lower bound
- **Priority queue** of clusters ordered by distance
- **Dynamic radius pruning** based on current k-th neighbor
- **Early termination** when lower bound exceeds k-th distance

At each cluster:
- Lower bound computed from MBB
- If lower bound ≥ k-th distance, cluster skipped
- B+-tree searched for key range
- Objects pruned by Lemma 4.3
- k-best heap updated

Page accesses counted per cluster visited.



## Output format

For each dataset, the benchmark writes:

```
results/results_MIndex_Improved_<dataset>.json
```

Schema:
```json
{
  "index": "MIndex*_Improved",
  "dataset": "LA",
  "category": "DM",
  "num_pivots": 5,
  "num_centers_path": null,
  "arity": null,
  "query_type": "MRQ",
  "selectivity": 0.02,
  "radius": 408.823,
  "k": null,
  "compdists": 69122.0,
  "time_ms": 21.12,
  "pages": 3.0,
  "n_queries": 100,
  "run_id": 1
}
```



## Running the benchmark

1. Compile:
```bash
g++ -O3 -std=c++17 test.cpp -o mindex_test
```

2. Execute:
```bash
./mindex_test
```

This will:
- Build M-Index* indexes in `midx_indexes/`
- Run MRQ and MkNN over all queries
- Produce JSON outputs in `results/results_MIndex_Improved_<dataset>.json`



## Performance Highlights

**LA Dataset (1M+ objects):**
- Range (sel=0.02): 69,122 compdists, 21.12 ms
- k-NN (k=10): 23,386 compdists, 16.16 ms

**Synthetic Dataset (28K objects):**
- Range (sel=0.02): 3,490 compdists, 1.50 ms
- k-NN (k=10): 1,245 compdists, 0.78 ms

**Words Dataset (597K strings):**
- Range (sel=0.02): 654,927 compdists, 432.70 ms
- k-NN (k=10): 434,067 compdists, 297.16 ms

The implementation shows effective pruning through:
- Lemma 4.5 validation (reduces computations)
- MBB cluster filtering
- B+-tree range limitation
- Best-first k-NN traversal
