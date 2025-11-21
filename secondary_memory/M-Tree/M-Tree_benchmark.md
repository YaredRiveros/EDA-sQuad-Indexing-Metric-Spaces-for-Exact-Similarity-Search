# M-Tree — Benchmark for Disk-Based Experiments  
(Reproduction of Section 6 — Chen et al., “Indexing Metric Spaces for Exact Similarity Search”)

This module runs the full experimental pipeline to evaluate the M-tree in a
disk-based setting, following the configuration used in the secondary-memory
experiments of the paper.

The benchmark evaluates:

- Metric Range Queries (MRQ) using selectivities  
  {2%, 4%, 8%, 16%, 32%}

- Multi-k Nearest Neighbor Queries (MkNN) with  
  k ∈ {5, 10, 20, 50, 100}

- The same 100 random queries and precomputed radii shared by all indexes

- The four datasets in the paper:  
  LA, Words, Color, Synthetic

The C++ driver (test.cpp) automatically:

1. Loads each dataset (ObjectDB) using the correct metric
2. Builds a disk-based M-tree index
3. Restores the index from disk
4. Executes MRQ and MkNN for all queries
5. Outputs a JSON file with averaged metrics



## M-tree in this benchmark



The M-tree is a dynamic, balanced search tree based on ball partitioning:

- Each entry e stores:
  - A routing object R (center)
  - A covering radius r (maximum distance from R to any object in its subtree)
  - A parent distance PD = dist(R, parent(R))
  - A pointer to a child node (internal nodes) or a data object (leaves)

- The index is organized in levels:
  - Internal nodes store routing objects (balls)
  - Leaves store data objects

In our implementation:

- The tree is constructed in RAM using a ball-partitioning strategy
  (farthest-first centers and assignment by nearest center)
- Each node is then serialized to disk as a single logical page
- M-tree nodes do not store raw object data; objects live in the ObjectDB
  and are accessed only via their IDs

This matches the semantics described for the M-tree family in Chen et al.:

- Ball partitioning
- Covering radii
- Parent distances
- Disk pages as units of cost (PA: Page Accesses)



## Disk layout


The disk-based M-tree uses a single index file:

```
  base.mtree_index
```


The file layout is:

- 8 bytes: rootOffset (int64_t)
- Sequence of nodes, each serialized as:

  isLeaf      (bool)
  count       (int32_t)
  For each entry (count times):
    - objId       (int32_t)   // routing object or data object
    - radius      (double)    // covering radius r_R (0 for leaves)
    - parentDist  (double)    // dist(R, parent(R)) or dist(D, - parent(R))
    - childOffset (int64_t)   // -1 for leaf entries, offset for internal children

When restoring, the benchmark:

- Opens base.mtree_index
- Reads the rootOffset from the header
- During queries, it uses fseek + fread to load nodes as needed
- Each node read counts as one page access (pageReads)



## Page size and node arity


Chen’s experiments measure PA (Page Accesses) assuming 4KB pages.
For high-dimensional datasets, M-tree nodes require larger physical pages.

In this benchmark:

- Logical page size is dataset dependent:
  - LA, Words      → 4 KB
  - Color, Synthetic → 40 KB (interpreted as 10 logical pages of 4 KB)

- Each node is stored as one logical page, and its arity m (nodeCapacity)
  is derived from the page size as:

$$
    entryBytes ≈ 4 (objId) + 8 (radius) + 8 (parentDist) + 8 
$$

$$
    (childOffset) = 28 bytes
$$

$$
    nodeCapacity ≈ pageBytes / entryBytes
$$

The benchmark passes nodeCapacity to the M-tree constructor so that the
branching factor matches the effective disk page size.

For PA:

- Each node read from base.mtree_index is counted as one page access:
  pages = pageReads

- For 40KB pages, this is consistent with Chen’s interpretation of one
  “large” page corresponding to 10 logical 4KB pages for tree-based indexes.



# Metrics recorded

For each dataset and query type, the benchmark records:

- **compdists**: average number of distance computations
- **time_ms**: average running time per query (milliseconds)
- **pages**: average Page Accesses (PA) per query
- **n_queries**: number of queries (100)

> ### Index-specific fields:
>
> - index: "MTREE"
> - dataset: "LA", "Words", "Color", or "Synthetic"
> - category: "DM" (disk-based metric index)
> - num_pivots: 0 (M-tree does not use pivots)
> - num_centers_path: 1 (M-tree has multiple levels but we report 1 to mirror the flat LC schema; the effective path length is encoded in the tree)
> - arity: nodeCapacity used in this run
> - query_type: "MRQ" or "MkNN"
> - selectivity: s for MRQ, null for MkNN
> - radius: radius used for MRQ, null for MkNN
> - k: k for MkNN, null for MRQ
> - run_id: run identifier (1 in this benchmark)


## MRQ (Metric Range Query)


For each selectivity $s$ in {0.02, 0.04, 0.08, 0.16, 0.32}:

1. The radius R_s is loaded from the radii JSON for the dataset. 
2. For each query q in the 100-query set, the M-tree is traversed top-down.

The algorithm uses two levels of pruning:

A) Parent filtering (M-tree Lemma)

Given a parent routing object P and a child routing object R, with:

- dPQ = dist(P, q), known from the parent level
- dPR = parentDist stored in the entry (dist(P, R))
- rR  = radius of R
- Rq  = query radius

If

```
  |dPQ − dPR| > Rq + rR
```

then the ball centered at R cannot intersect the ball B(q, Rq), so the
subtree rooted at R is pruned without computing dist(q, R).

B) Basic ball intersection (Lemma 4.2)

If parent filtering does not prune, the algorithm computes dQR = dist(q, R).
If

```
  dQR > Rq + rR
```

then the ball centered at R does not intersect B(q, Rq), and the subtree is pruned.

At a leaf:

- Each entry stores a data object D with radius = 0.
- If dist(q, D) ≤ Rq, D is added to the answer set.

Each node read from disk increments pageReads by 1, and visiting all
entries in that node incurs the corresponding distance computations.


## MkNN queries


For each $k$ in {5, 10, 20, 50, 100}:

- The benchmark performs k-nearest neighbor queries using a best-first search:

1. A min-heap (priority queue) of candidate subtrees is maintained.
   Each candidate stores:
   - lb: lower bound distance from q to the ball region
   - offset: disk offset of the node
   - parentCenterId, distParentQ: used for potential parent-based reasoning

2. A max-heap of size k stores the current best k objects:
   - Each entry is (distance, objectId)
   - The top element is the worst (farthest) among the current best k

3. Initialization:
   - The root node is read from disk (pageReads++).
   - For each entry R in the root:
     - Compute dQR = dist(q, R)
     - Compute lb = max(0, dQR − rR)
     - If the root is a leaf, update the best-k heap with (dQR, objectId).
     - If the root is internal, push the child node with key lb into the
       candidate min-heap.

4. Best-first expansion:
   - While the candidate heap is not empty:
     - Let worst be the worst distance among the current best k (∞ if we
       have fewer than k results).
     - Pop the candidate with smallest lb.
     - If lb > worst, terminate (no unexplored region can improve the result).
     - Read the corresponding node from disk (pageReads++).
     - If the node is a leaf:
       - For each object D in the node, compute dQD and update the best-k heap.
     - If the node is internal:
       - For each routing object R in the node:
         - Compute dQR
         - Compute lb = max(0, dQR − rR)
         - If lb ≤ worst, push the child into the candidate heap; otherwise prune.

The result is a list of k objects sorted by increasing distance to q.
The average compdists, time_ms and pages are recorded over all 100 queries.



## Output format


For each dataset, the benchmark produces a JSON file:

```
  results_MTree_<dataset>.json
```

Each entry has the schema:

```
  {
    "index": "MTREE",
    "dataset": "LA",
    "category": "DM",
    "num_pivots": 0,
    "num_centers_path": 1,
    "arity": 128,
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


The exact value of "arity" depends on pageBytes and entry size, and is computed in the test driver from:

$$
  nodeCapacity ≈ pageBytes / entryBytes
$$


# Running the benchmark


1. Compile the test:

    
       g++ -O3 -std=c++17 test.cpp -o mtree_test

   where test.cpp is the driver that:
   - includes "mtree_disk.hpp"
   - loads datasets via ObjectDB and paths.hpp
   - builds and restores the M-tree index
   - runs MRQ and MkNN
   - writes JSON files to the results/ folder

2. Run:

        ./mtree_test

This will:

- Build M-tree indexes in mtree_indexes/
- Execute MRQ and MkNN for LA, Words, Color and Synthetic
- Produce JSON summaries in results/results_MTree_<dataset>.json


> [!NOTE]
> **The M-tree uses no pivots (num_pivots = 0).**  
> Unlike pivot-based methods (e.g., LAESA, PM-tree), the M-tree routes objects
> using *routing objects* (centers) and *covering radii*, not pivot distances.
>
> **The M-tree is hierarchical, but for reporting we set num_centers_path = 1**  
> to match the output schema used by Chen et al. for all non-pivot structures.
> The actual tree depth varies with dataset and node capacity.
>
> **Color and Synthetic datasets require large pages (40KB = 10×4KB)**  
> because their high-dimensional objects require large node capacities to store
> routing entries. Chen et al. interpret each 40KB page as *ten logical 4KB trees pages*,
> and we follow the same convention for Page Accesses (PA).
>
> **LA and Words use standard 4KB pages**, resulting in smaller node capacities
> and therefore deeper M-trees.
>
> **Page Accesses (PA) are counted as: 1 page read = 1 node read**  
> This is exactly the metric reported in Figures 24–27 of the paper.
>
> **All data objects remain in the ObjectDB**  
> M-tree entries store IDs, radii, parent distances, and child pointers, but not full data.
> This matches the disk-based M-tree model assumed in Chen’s evaluation.
