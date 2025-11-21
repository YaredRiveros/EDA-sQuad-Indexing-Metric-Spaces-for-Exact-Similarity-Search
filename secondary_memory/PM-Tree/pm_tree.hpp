#ifndef PM_TREE_HPP
#define PM_TREE_HPP

#include "objectdb.hpp"

#include <vector>
#include <queue>
#include <cmath>
#include <chrono>
#include <algorithm>
#include <limits>
#include <stdexcept>
#include <cstdint>
#include <cstdio>
#include <map>
#include <iostream>

// ============================================================
// PMTree: Pivoting Metric Tree (disk-based structure loaded
// from an existing M-tree index file, plus global pivots).
//
// This follows the description in Chen et al. (2022):
//  - M-tree is used to cluster objects on disk.
//  - Each leaf entry stores the mapped vector (distances to pivots).
//  - Each non-leaf entry stores an MBB that contains all mapped
//    vectors in its child leaf entries.
//  - MRQ and MkNN traverse the M-tree as usual, but also apply
//    Lemma 4.1 (pivot lower bounds) for additional pruning.
// ============================================================
class PMTree {
public:
    // Metrics (queries)
    mutable long long compDistQuery  = 0;  // distances during queries
    mutable long long compDistBuild  = 0;  // distances to build pivot table
    mutable long long pageReads      = 0;  // logical page/node reads
    mutable long long queryTime      = 0;  // accumulated time in µs

    explicit PMTree(const ObjectDB* db_, int nPivots_)
        : db(db_),
          n(db_ ? db_->size() : 0),
          nPivots(nPivots_),
          rootIndex(-1)
    {
        if (!db) {
            nPivots = 0;
            return;
        }
        if (nPivots > n) nPivots = n;
    }

    // ------------------------------------------------------------
    // Override global pivots (HFI pivots from JSON).
    // This does not build anything by itself; once both pivots and
    // the M-tree structure are available, we call recomputePivotData().
    // ------------------------------------------------------------
    void overridePivots(const std::vector<int>& newPivots) {
        if (!db) return;
        if ((int)newPivots.size() != nPivots) {
            std::cerr << "[PMTree] overridePivots: invalid size ("
                      << newPivots.size() << " != " << nPivots << ")\n";
            return;
        }
        for (int p : newPivots) {
            if (p < 0 || p >= n) {
                std::cerr << "[PMTree] overridePivots: pivot id out of range: "
                          << p << "\n";
                return;
            }
        }
        pivots = newPivots;
        // Try to recompute pivot data if the tree is already loaded.
        recomputePivotData();
    }

    int get_num_pivots() const { return nPivots; }

    // ------------------------------------------------------------
    // Load tree structure from an existing M-tree index file.
    // basePath: same base used by MTree_Disk (basePath.mtree_index)
    // We read all nodes into RAM and reconstruct the topology, so
    // that we can use pivot-based pruning on top of the same tree.
    // ------------------------------------------------------------
    void buildFromMTree(const std::string& basePath) {
        nodes.clear();
        rootIndex = -1;

        if (!db) {
            std::cerr << "[PMTree] buildFromMTree: db is null\n";
            return;
        }

        std::string indexPath = basePath + ".mtree_index";
        FILE* fp = std::fopen(indexPath.c_str(), "rb");
        if (!fp) {
            std::cerr << "[PMTree] buildFromMTree: cannot open " << indexPath << "\n";
            return;
        }

        int64_t rootOffset = -1;
        if (std::fread(&rootOffset, sizeof(int64_t), 1, fp) != 1) {
            std::cerr << "[PMTree] buildFromMTree: corrupted file (no rootOffset)\n";
            std::fclose(fp);
            return;
        }

        struct RawEntry {
            int32_t objId;
            double  radius;
            double  parentDist;
            int64_t childOffset;
        };

        struct RawNode {
            bool isLeaf;
            int32_t count;
            int64_t offset;
            std::vector<RawEntry> entries;
        };

        std::vector<RawNode> rawNodes;
        std::map<int64_t,int> offsetToIndex;

        while (true) {
            int64_t pos = std::ftell(fp);
            bool isLeaf;
            int32_t count;

            size_t r1 = std::fread(&isLeaf, sizeof(bool), 1, fp);
            if (r1 != 1) {
                if (!std::feof(fp)) {
                    std::cerr << "[PMTree] buildFromMTree: fread(isLeaf) failed\n";
                }
                break;
            }
            if (std::fread(&count, sizeof(int32_t), 1, fp) != 1) {
                std::cerr << "[PMTree] buildFromMTree: fread(count) failed\n";
                break;
            }

            RawNode node;
            node.isLeaf = isLeaf;
            node.count  = count;
            node.offset = pos;
            node.entries.resize(count);

            for (int i = 0; i < count; ++i) {
                RawEntry e;
                if (std::fread(&e.objId,      sizeof(int32_t), 1, fp) != 1 ||
                    std::fread(&e.radius,     sizeof(double),  1, fp) != 1 ||
                    std::fread(&e.parentDist, sizeof(double),  1, fp) != 1 ||
                    std::fread(&e.childOffset,sizeof(int64_t), 1, fp) != 1) {
                    std::cerr << "[PMTree] buildFromMTree: fread(entry) failed\n";
                    rawNodes.clear();
                    std::fclose(fp);
                    return;
                }
                node.entries[i] = e;
            }

            int idx = (int)rawNodes.size();
            rawNodes.push_back(std::move(node));
            offsetToIndex[pos] = idx;
        }

        std::fclose(fp);

        if (rawNodes.empty()) {
            std::cerr << "[PMTree] buildFromMTree: no nodes found in index\n";
            return;
        }

        auto itRoot = offsetToIndex.find(rootOffset);
        if (itRoot == offsetToIndex.end()) {
            std::cerr << "[PMTree] buildFromMTree: rootOffset not found in nodes\n";
            return;
        }
        rootIndex = itRoot->second;

        // Convert RawNode -> internal Node representation
        nodes.resize(rawNodes.size());
        for (size_t i = 0; i < rawNodes.size(); ++i) {
            nodes[i].isLeaf = rawNodes[i].isLeaf;
            nodes[i].entries.resize(rawNodes[i].count);
            for (int j = 0; j < rawNodes[i].count; ++j) {
                const RawEntry& re = rawNodes[i].entries[j];
                Entry& e = nodes[i].entries[j];
                e.objId      = re.objId;
                e.radius     = re.radius;
                e.parentDist = re.parentDist;
                e.child      = -1;          // will be set below
                e.childOffset= re.childOffset;
            }
        }

        // Now resolve child offsets into node indices
        for (size_t i = 0; i < nodes.size(); ++i) {
            if (nodes[i].isLeaf) continue;
            for (Entry& e : nodes[i].entries) {
                if (e.childOffset < 0) {
                    e.child = -1;
                } else {
                    auto it = offsetToIndex.find(e.childOffset);
                    if (it == offsetToIndex.end()) {
                        std::cerr << "[PMTree] buildFromMTree: childOffset not found\n";
                        e.child = -1;
                    } else {
                        e.child = it->second;
                    }
                }
            }
        }

        std::cerr << "[PMTree] Loaded " << nodes.size()
                  << " nodes from " << indexPath << "\n";

        // Once we have a tree and pivots, build pivot-related data.
        recomputePivotData();
    }

    // ------------------------------------------------------------
    // Metrics API
    // ------------------------------------------------------------
    void clear_counters() const {
        compDistQuery = 0;
        pageReads     = 0;
        queryTime     = 0;
    }

    long long get_compDist()      const { return compDistQuery;  }
    long long get_compDistBuild() const { return compDistBuild;  }
    long long get_pageReads()     const { return pageReads;      }
    long long get_queryTime()     const { return queryTime;      }

    // ------------------------------------------------------------
    // MRQ: Range Query
    //  - same traversal strategy as M-tree (DFS),
    //  - but entries can also be pruned using pivot-based lower
    //    bounds (Lemma 4.1) thanks to the MBB in pivot space.
    // ------------------------------------------------------------
    void rangeSearch(int queryId, double radius,
                     std::vector<int>& result) const
    {
        using clock = std::chrono::high_resolution_clock;
        auto t0 = clock::now();

        result.clear();
        if (!db || n == 0 || nPivots == 0 || nodes.empty() || rootIndex < 0) {
            auto t1 = clock::now();
            queryTime += std::chrono::duration_cast<std::chrono::microseconds>(t1 - t0).count();
            return;
        }

        // 1. Distances from query to pivots (in-memory).
        std::vector<double> qPiv(nPivots);
        for (int j = 0; j < nPivots; ++j) {
            qPiv[j] = db->distance(queryId, pivots[j]);
            compDistQuery++;
        }

        // 2. DFS over the tree
        std::function<void(int)> dfs = [&](int nodeIdx) {
            const Node& node = nodes[nodeIdx];
            pageReads++;

            if (node.isLeaf) {
                // Leaf: check each object using pivot LB + exact distance.
                for (const Entry& e : node.entries) {
                    int objId = e.objId;
                    if (objId < 0 || objId >= n) continue;

                    double lbPiv = lowerBoundObject(qPiv, objId);
                    if (lbPiv > radius) continue;

                    double d = db->distance(queryId, objId);
                    compDistQuery++;
                    if (d <= radius) {
                        result.push_back(objId);
                    }
                }
            } else {
                // Internal node: check each entry (subtree).
                for (const Entry& e : node.entries) {
                    if (e.child < 0) continue;

                    // 1) Pivot-based lower bound for the subtree
                    double lbPiv = lowerBoundEntry(qPiv, e);
                    if (lbPiv > radius) continue;

                    // 2) Ball-based lower bound (Lemma 4.2)
                    double dQC = db->distance(queryId, e.objId);
                    compDistQuery++;
                    double lbBall = std::max(dQC - e.radius, 0.0);

                    if (lbBall > radius) continue;

                    // Subtree may intersect the query ball
                    dfs(e.child);
                }
            }
        };

        dfs(rootIndex);

        auto t1 = clock::now();
        queryTime += std::chrono::duration_cast<std::chrono::microseconds>(t1 - t0).count();
    }

    // ------------------------------------------------------------
    // MkNN: k Nearest Neighbor Query
    //  - best-first search over nodes, ordered by a lower bound
    //    combining:
    //      * ball-based lower bound (M-tree),
    //      * pivot-based lower bound using MBB (PM-tree).
    // ------------------------------------------------------------
    void knnSearch(int queryId, int k,
                   std::vector<std::pair<double,int>>& out) const
    {
        using clock = std::chrono::high_resolution_clock;
        auto t0 = clock::now();

        out.clear();
        if (!db || n == 0 || nPivots == 0 || nodes.empty() || rootIndex < 0 || k <= 0) {
            auto t1 = clock::now();
            queryTime += std::chrono::duration_cast<std::chrono::microseconds>(t1 - t0).count();
            return;
        }

        compDistQuery = 0;
        pageReads     = 0;

        // 1. Distances query–pivots
        std::vector<double> qPiv(nPivots);
        for (int j = 0; j < nPivots; ++j) {
            qPiv[j] = db->distance(queryId, pivots[j]);
            compDistQuery++;
        }

        // 2. Priority queue of nodes (min-heap by lower bound)
        struct NodeCand {
            double lb;
            int    nodeIdx;
        };
        struct NodeCmp {
            bool operator()(const NodeCand& a, const NodeCand& b) const {
                return a.lb > b.lb; // min-heap
            }
        };
        std::priority_queue<NodeCand, std::vector<NodeCand>, NodeCmp> pq;

        // 3. Max-heap of best neighbors found so far
        std::priority_queue<std::pair<double,int>> best; // (dist, objId)
        double tau = std::numeric_limits<double>::infinity();

        // Initial bound for root: 0
        pq.push(NodeCand{0.0, rootIndex});

        while (!pq.empty()) {
            NodeCand cur = pq.top();
            pq.pop();

            if (cur.lb >= tau) break;

            const Node& node = nodes[cur.nodeIdx];
            pageReads++;

            if (node.isLeaf) {
                // Check each object in this leaf
                for (const Entry& e : node.entries) {
                    int objId = e.objId;
                    if (objId < 0 || objId >= n) continue;

                    double lbPiv = lowerBoundObject(qPiv, objId);
                    if (lbPiv >= tau) continue;

                    double d = db->distance(queryId, objId);
                    compDistQuery++;

                    if ((int)best.size() < k) {
                        best.push({d, objId});
                        if ((int)best.size() == k) {
                            tau = best.top().first;
                        }
                    } else if (d < best.top().first) {
                        best.pop();
                        best.push({d, objId});
                        tau = best.top().first;
                    }
                }
            } else {
                // Expand internal node
                for (const Entry& e : node.entries) {
                    if (e.child < 0) continue;

                    // 1) Pivot-based lower bound for subtree
                    double lbPiv = lowerBoundEntry(qPiv, e);
                    if (lbPiv >= tau) continue;

                    // 2) Ball-based lower bound (Lemma 4.2)
                    double dQC = db->distance(queryId, e.objId);
                    compDistQuery++;
                    double lbBall = std::max(dQC - e.radius, 0.0);

                    double lb = std::max(lbPiv, lbBall);
                    if (lb >= tau) continue;

                    pq.push(NodeCand{lb, e.child});
                }
            }
        }

        // Extract neighbors sorted by distance ascending
        std::vector<std::pair<double,int>> tmp;
        while (!best.empty()) {
            tmp.push_back(best.top());
            best.pop();
        }
        std::reverse(tmp.begin(), tmp.end());
        out.swap(tmp);

        auto t1 = clock::now();
        queryTime += std::chrono::duration_cast<std::chrono::microseconds>(t1 - t0).count();
    }

private:
    const ObjectDB* db = nullptr;
    int n        = 0;  // #objects
    int nPivots  = 0;  // #pivots

    std::vector<int> pivots;                      // pivot IDs
    std::vector<std::vector<double>> distMatrix;  // [obj][pivot] = d(obj,pivot)

    struct Entry {
        int    objId      = -1;
        double radius     = 0.0;
        double parentDist = 0.0;
        int    child      = -1;
        int64_t childOffset = -1; // only used during buildFromMTree

        std::vector<double> minPiv; // per-pivot min distance in subtree
        std::vector<double> maxPiv; // per-pivot max distance in subtree
    };

    struct Node {
        bool isLeaf = false;
        std::vector<Entry> entries;
    };

    std::vector<Node> nodes;
    int rootIndex;  // index of root node in 'nodes'

    // ------------------------------------------------------------
    // Recompute all pivot-related data:
    //  - full distMatrix[obj][pivot]
    //  - minPiv/maxPiv for each entry (MBB in pivot space)
    // ------------------------------------------------------------
void recomputePivotData() {
    if (!db || n == 0 || nPivots == 0 || nodes.empty() || rootIndex < 0)
        return;

    // *** IMPORTANTE: solo si ya tenemos pivots configurados ***
    if ((int)pivots.size() != nPivots) {
        // Aún no se llamaron a overridePivots, o hay inconsistencia.
        return;
    }

    // 1) Build full pivot table
    distMatrix.assign(n, std::vector<double>(nPivots));
    compDistBuild = 0;
    for (int i = 0; i < n; ++i) {
        for (int j = 0; j < nPivots; ++j) {
            distMatrix[i][j] = db->distance(i, pivots[j]);
            compDistBuild++;
        }
    }

    // 2) Compute min/max per entry (bottom-up)
    std::vector<bool> visited(nodes.size(), false);
    computeEntryBounds(rootIndex, visited);
}

    // Returns min/max pivot distances for all entries in subtree rooted at nodeIdx.
    void computeEntryBounds(int nodeIdx, std::vector<bool>& visited) {
        if (nodeIdx < 0 || nodeIdx >= (int)nodes.size()) return;
        if (visited[nodeIdx]) return;
        visited[nodeIdx] = true;

        Node& node = nodes[nodeIdx];

        if (node.isLeaf) {
            // Leaf: each entry is an object; its min/max are its own distances.
            for (Entry& e : node.entries) {
                int objId = e.objId;
                e.minPiv.resize(nPivots);
                e.maxPiv.resize(nPivots);
                for (int j = 0; j < nPivots; ++j) {
                    double v = distMatrix[objId][j];
                    e.minPiv[j] = v;
                    e.maxPiv[j] = v;
                }
            }
        } else {
            // Internal node: compute bounds for children first
            for (Entry& e : node.entries) {
                if (e.child >= 0) {
                    computeEntryBounds(e.child, visited);
                }
            }
            // Then aggregate child entries into this entry's min/max
            for (Entry& e : node.entries) {
                if (e.child < 0) {
                    // No child; treat as empty subtree
                    e.minPiv.assign(nPivots, std::numeric_limits<double>::infinity());
                    e.maxPiv.assign(nPivots, 0.0);
                    continue;
                }
                Node& childNode = nodes[e.child];
                e.minPiv.assign(nPivots, std::numeric_limits<double>::infinity());
                e.maxPiv.assign(nPivots, 0.0);

                for (Entry& ce : childNode.entries) {
                    if (ce.minPiv.empty()) continue;
                    for (int j = 0; j < nPivots; ++j) {
                        e.minPiv[j] = std::min(e.minPiv[j], ce.minPiv[j]);
                        e.maxPiv[j] = std::max(e.maxPiv[j], ce.maxPiv[j]);
                    }
                }
            }
        }
    }

    // ------------------------------------------------------------
    // Pivot-based lower bound for a subtree entry (MBB vs query pivots).
    // For each pivot p:
    //   we know d(p, o) ∈ [minPiv[p], maxPiv[p]] for all o in the subtree.
    // By triangle inequality:
    //   d(q, o) >= |d(q,p) - d(p,o)|
    // Therefore, the minimum over that interval is:
    //   0           if d(q,p) ∈ [a,b]
    //   a - d(q,p)  if d(q,p) < a
    //   d(q,p) - b  if d(q,p) > b
    // We then take max over all pivots.
    // ------------------------------------------------------------
    double lowerBoundEntry(const std::vector<double>& qPiv, const Entry& e) const {
        double lb = 0.0;
        for (int j = 0; j < nPivots; ++j) {
            double x = qPiv[j];
            double a = e.minPiv[j];
            double b = e.maxPiv[j];
            if (a == std::numeric_limits<double>::infinity() && b == 0.0) {
                // empty info, skip
                continue;
            }
            double v = 0.0;
            if (x < a) v = a - x;
            else if (x > b) v = x - b;
            else v = 0.0;
            if (v > lb) lb = v;
        }
        return lb;
    }

    // ------------------------------------------------------------
    // Pivot-based lower bound for a single object (using full distMatrix).
    //   d(q,o) >= max_j | d(q,p_j) - d(o,p_j) |
    // ------------------------------------------------------------
    double lowerBoundObject(const std::vector<double>& qPiv, int objId) const {
        double lb = 0.0;
        const std::vector<double>& v = distMatrix[objId];
        for (int j = 0; j < nPivots; ++j) {
            double diff = std::fabs(qPiv[j] - v[j]);
            if (diff > lb) lb = diff;
        }
        return lb;
    }
};

#endif // PM_TREE_HPP
