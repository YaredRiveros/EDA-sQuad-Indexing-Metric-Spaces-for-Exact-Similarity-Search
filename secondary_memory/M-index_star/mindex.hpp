// mindex_improved.hpp
#ifndef MINDEX_IMPROVED_HPP
#define MINDEX_IMPROVED_HPP

#include "../../objectdb.hpp"
#include <vector>
#include <string>
#include <fstream>
#include <cstdio>
#include <cstdint>
#include <limits>
#include <queue>
#include <algorithm>
#include <chrono>
#include <stdexcept>
#include <cmath>
#include <random>
#include <filesystem>
#include <map>

/*
  M-Index* MEJORADO con:
  - B+-tree simulado mediante std::map para indexar mapped keys
  - Lemma 4.5 para validación directa de objetos
  - Range search optimizado con búsqueda por intervalos de keys
  - Best-first traversal para k-NN
*/

class MIndex_Improved {
public:
    struct RAFEntry {
        int32_t id;
        std::vector<double> dists; // distancias a P pivotes
        double key;                 // mapped key
    };

    struct ClusterNode {
        bool isLeaf;
        double minkey, maxkey;
        std::vector<double> minDist; // MBB: min distance to each pivot
        std::vector<double> maxDist; // MBB: max distance to each pivot
        std::vector<int32_t> children;
        int64_t rafOffset;
        int32_t count;
        ClusterNode(): isLeaf(false), minkey(0), maxkey(0), rafOffset(-1), count(0) {}
    };

private:
    const ObjectDB* db;
    int n;
    int P; // number of pivots
    std::vector<int32_t> pivots;
    double dplus; // maximum distance observed

    std::vector<ClusterNode> nodes;
    
    // B+-tree simulation: map from key -> RAF entry
    std::map<double, std::vector<RAFEntry>> btreeIndex;
    
    std::string rafPath;
    mutable FILE* rafFp = nullptr;

    // Metrics
    mutable long long compDist = 0;
    mutable long long pageReads = 0;
    mutable long long pageWrites = 0;
    mutable long long queryTime = 0;

    inline double distObj(int a, int b) const { compDist++; return db->distance(a,b); }

public:
    MIndex_Improved(const ObjectDB* db_, int numPivots=5): db(db_), P(numPivots), dplus(0) {
        if (!db) throw std::runtime_error("DB null");
        n = db->size();
    }
    ~MIndex_Improved() {
        if (rafFp) fclose(rafFp);
    }

    void clear_counters() const { compDist = pageReads = pageWrites = queryTime = 0; }
    long long get_compDist() const { return compDist; }
    long long get_pageReads() const { return pageReads; }
    long long get_pageWrites() const { return pageWrites; }
    long long get_queryTime() const { return queryTime; }

    // ------------------------------
    // Build: M-index* con B+-tree
    // ------------------------------
    void build(const std::string& base) {
        using clock = std::chrono::high_resolution_clock;
        auto t0 = clock::now();

        // Select P pivots randomly
        pivots.clear();
        pivots.reserve(P);
        std::vector<int> perm(n);
        for (int i=0;i<n;i++) perm[i]=i;
        std::shuffle(perm.begin(), perm.end(), std::mt19937{std::random_device{}()});
        for (int i=0;i<P;i++) pivots.push_back(perm[i]);

        // Compute d+ (maximum distance)
        dplus = 0.0;
        int sampleSize = std::min(n, 1000);
        for (int i=0; i<sampleSize; ++i) {
            for (int j=0; j<P; ++j) {
                double d = distObj(i, pivots[j]);
                if (d > dplus) dplus = d;
            }
        }
        if (dplus <= 0) dplus = 1.0;

        // Compute mapped keys and distances for all objects
        std::vector<RAFEntry> entries;
        entries.reserve(n);
        
        for (int id=0; id<n; ++id) {
            RAFEntry entry;
            entry.id = id;
            entry.dists.resize(P);
            
            double minDist = std::numeric_limits<double>::infinity();
            int nearestPivot = 0;
            
            for (int j=0; j<P; j++) {
                entry.dists[j] = distObj(id, pivots[j]);
                if (entry.dists[j] < minDist) {
                    minDist = entry.dists[j];
                    nearestPivot = j;
                }
            }
            
            // M-index* key mapping: key(o) = d(pi, o) + (i-1) * d+
            entry.key = entry.dists[nearestPivot] + nearestPivot * dplus;
            entries.push_back(entry);
        }

        // Sort by key for B+-tree
        std::sort(entries.begin(), entries.end(), 
                  [](const RAFEntry& a, const RAFEntry& b) { return a.key < b.key; });

        // Build B+-tree index (simulated with map)
        btreeIndex.clear();
        for (const auto& entry : entries) {
            btreeIndex[entry.key].push_back(entry);
        }

        // Build cluster tree with MBB
        buildClusterTree(entries);

        // Write RAF file
        rafPath = base + ".midx_raf";
        writeRAF(entries);

        rafFp = std::fopen(rafPath.c_str(), "rb");
        if (!rafFp) throw std::runtime_error("cannot reopen RAF");

        auto t1 = clock::now();
        queryTime += std::chrono::duration_cast<std::chrono::microseconds>(t1-t0).count();
    }

private:
    void buildClusterTree(const std::vector<RAFEntry>& entries) {
        nodes.clear();
        
        // Create one cluster per pivot
        for (int pIdx = 0; pIdx < P; pIdx++) {
            std::vector<const RAFEntry*> clusterEntries;
            
            for (const auto& entry : entries) {
                // Find nearest pivot for this entry
                double minD = std::numeric_limits<double>::infinity();
                int nearest = 0;
                for (int j = 0; j < P; j++) {
                    if (entry.dists[j] < minD) {
                        minD = entry.dists[j];
                        nearest = j;
                    }
                }
                if (nearest == pIdx) {
                    clusterEntries.push_back(&entry);
                }
            }
            
            if (clusterEntries.empty()) continue;
            
            ClusterNode node;
            node.isLeaf = true;
            node.minDist.assign(P, std::numeric_limits<double>::infinity());
            node.maxDist.assign(P, -std::numeric_limits<double>::infinity());
            node.minkey = clusterEntries.front()->key;
            node.maxkey = clusterEntries.back()->key;
            node.count = clusterEntries.size();
            
            // Compute MBB
            for (const auto* e : clusterEntries) {
                for (int j = 0; j < P; j++) {
                    node.minDist[j] = std::min(node.minDist[j], e->dists[j]);
                    node.maxDist[j] = std::max(node.maxDist[j], e->dists[j]);
                }
            }
            
            nodes.push_back(node);
        }
    }

    void writeRAF(const std::vector<RAFEntry>& entries) {
        std::ofstream outf(rafPath, std::ios::binary | std::ios::trunc);
        if (!outf) throw std::runtime_error("cannot write RAF");
        
        for (const auto& entry : entries) {
            outf.write((char*)&entry.id, sizeof(int32_t));
            for (int j = 0; j < P; j++) {
                outf.write((char*)&entry.dists[j], sizeof(double));
            }
            outf.write((char*)&entry.key, sizeof(double));
        }
        outf.close();
        pageWrites += (entries.size() / 100) + 1; // approximate
    }

public:
    // ------------------------------
    // Range Search con B+-tree y Lemmas 4.1, 4.3, 4.5
    // ------------------------------
    void rangeSearch(int qId, double R, std::vector<int>& out) const {
        using clock = std::chrono::high_resolution_clock;
        auto t0 = clock::now();
        out.clear();

        // Precompute distances from q to pivots
        std::vector<double> dq(P);
        for (int j=0; j<P; j++) {
            dq[j] = distObj(qId, pivots[j]);
        }

        // For each cluster, check MBB pruning (Lemma 4.1)
        for (const auto& node : nodes) {
            // Lemma 4.1: Prune cluster if for any pivot j:
            // dq[j] + R < minDist[j] OR dq[j] - R > maxDist[j]
            bool pruned = false;
            for (int j = 0; j < P && !pruned; j++) {
                if (dq[j] + R < node.minDist[j] || dq[j] - R > node.maxDist[j]) {
                    pruned = true;
                }
            }
            if (pruned) continue;

            // Cluster not pruned, search in B+-tree for key range
            // Use key bounds to limit search
            double minKeySearch = node.minkey;
            double maxKeySearch = node.maxkey;
            
            pageReads++; // cluster access
            
            // Search in B+-tree index within key range
            auto itLow = btreeIndex.lower_bound(minKeySearch);
            auto itHigh = btreeIndex.upper_bound(maxKeySearch);
            
            for (auto it = itLow; it != itHigh; ++it) {
                for (const auto& entry : it->second) {
                    // Lemma 4.5: Validate directly if exists pivot satisfying
                    // d(o, pi) <= r - d(q, pi)
                    bool validated = false;
                    for (int j = 0; j < P && !validated; j++) {
                        if (entry.dists[j] <= R - dq[j]) {
                            validated = true;
                            out.push_back(entry.id);
                        }
                    }
                    
                    if (validated) continue;
                    
                    // Lemma 4.3: Prune object if for any pivot j:
                    // |d(o, pj) - d(q, pj)| > R
                    bool objectPruned = false;
                    for (int j = 0; j < P && !objectPruned; j++) {
                        if (std::fabs(entry.dists[j] - dq[j]) > R) {
                            objectPruned = true;
                        }
                    }
                    
                    if (objectPruned) continue;
                    
                    // Cannot prune or validate, compute actual distance
                    double d = distObj(qId, entry.id);
                    if (d <= R) {
                        out.push_back(entry.id);
                    }
                }
            }
        }

        auto t1 = clock::now();
        queryTime += std::chrono::duration_cast<std::chrono::microseconds>(t1-t0).count();
    }

    // ------------------------------
    // k-NN Search con Best-First Traversal
    // ------------------------------
    void knnSearch(int qId, int k, std::vector<std::pair<double,int>>& out) const {
        using clock = std::chrono::high_resolution_clock;
        auto t0 = clock::now();
        out.clear();

        // Precompute distances from q to pivots
        std::vector<double> dq(P);
        for (int j=0; j<P; j++) {
            dq[j] = distObj(qId, pivots[j]);
        }

        // Best-first traversal: priority queue of clusters by lower bound
        struct ClusterCandidate {
            int nodeIdx;
            double lowerBound;
            bool operator>(const ClusterCandidate& o) const {
                return lowerBound > o.lowerBound;
            }
        };
        
        std::priority_queue<ClusterCandidate, std::vector<ClusterCandidate>, 
                            std::greater<ClusterCandidate>> pq;

        // Compute lower bound for each cluster and add to queue
        for (size_t i = 0; i < nodes.size(); i++) {
            const auto& node = nodes[i];
            
            // Lower bound: max over all pivots of max(0, minDist[j] - dq[j], dq[j] - maxDist[j])
            double lb = 0.0;
            for (int j = 0; j < P; j++) {
                double delta = 0.0;
                if (dq[j] < node.minDist[j]) {
                    delta = node.minDist[j] - dq[j];
                } else if (dq[j] > node.maxDist[j]) {
                    delta = dq[j] - node.maxDist[j];
                }
                lb = std::max(lb, delta);
            }
            
            pq.push({(int)i, lb});
        }

        // k-NN heap (max-heap of k nearest)
        std::priority_queue<std::pair<double,int>> knnHeap;
        double radiusK = std::numeric_limits<double>::infinity();

        while (!pq.empty()) {
            auto candidate = pq.top();
            pq.pop();
            
            // Pruning: if lower bound >= current k-th distance, stop
            if ((int)knnHeap.size() == k && candidate.lowerBound >= radiusK) {
                break;
            }

            const auto& node = nodes[candidate.nodeIdx];
            pageReads++;
            
            // Search in B+-tree for this cluster's key range
            auto itLow = btreeIndex.lower_bound(node.minkey);
            auto itHigh = btreeIndex.upper_bound(node.maxkey);
            
            for (auto it = itLow; it != itHigh; ++it) {
                for (const auto& entry : it->second) {
                    // Prune by current k-th distance using Lemma 4.3
                    bool pruned = false;
                    for (int j = 0; j < P && !pruned; j++) {
                        if (std::fabs(entry.dists[j] - dq[j]) > radiusK) {
                            pruned = true;
                        }
                    }
                    if (pruned) continue;
                    
                    // Compute actual distance
                    double d = distObj(qId, entry.id);
                    
                    if ((int)knnHeap.size() < k) {
                        knnHeap.push({d, entry.id});
                        if ((int)knnHeap.size() == k) {
                            radiusK = knnHeap.top().first;
                        }
                    } else if (d < radiusK) {
                        knnHeap.pop();
                        knnHeap.push({d, entry.id});
                        radiusK = knnHeap.top().first;
                    }
                }
            }
        }

        // Extract results
        while (!knnHeap.empty()) {
            out.push_back(knnHeap.top());
            knnHeap.pop();
        }
        std::reverse(out.begin(), out.end());

        auto t1 = clock::now();
        queryTime += std::chrono::duration_cast<std::chrono::microseconds>(t1-t0).count();
    }
};

#endif // MINDEX_IMPROVED_HPP
