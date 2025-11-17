// mindex_disk.hpp
#ifndef MINDEX_DISK_HPP
#define MINDEX_DISK_HPP

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

/*
  M-Index* (versión inspirada en EGNAT_Disk + MBB del paper)
  - cluster-tree en memoria/índice file (nodes[])
  - RAF / leaf file con objetos + precomputed distances to pivots
  - Simplificación: en vez de B+ real, usamos vectores ordenados por key(o) por cluster
  - Funciones: build(base), rangeSearch(qId,R,out), knnSearch(q,k,out)
  Referencias: Pivot-based Metric Indexing (M-index, M-index*). :contentReference[oaicite:4]{index=4}
  Ejemplo de diseño/IO y métricas tomado de EGNAT_Disk. :contentReference[oaicite:5]{index=5}
*/

class MIndex_Disk {
public:
    struct Tmp { 
        double key; 
        int id; 
        std::vector<double> dists; 
        int pivotIdx; 
    };

    struct LeafEntry {
        int32_t id;           // object id
        // distances to pivots stored consecutively in RAF after leaf entries
        // For compactness we store only id here and leave distances in RAF
    };

    struct ClusterNode {
        bool isLeaf;
        // hyperplane cluster keys (mapped real number): minkey / maxkey used by M-index
        double minkey, maxkey;
        // MBB over pivots: for P pivots, MBB: minDist[p], maxDist[p]
        std::vector<double> minDist; // size = P
        std::vector<double> maxDist; // size = P

        // children indices in nodes vector (empty if leaf)
        std::vector<int32_t> children;

        // For leaf: offset/count to RAF entries (objects' ids and their precomputed distances)
        int64_t rafOffset; // index into leaf table
        int32_t count;

        ClusterNode(): isLeaf(false), minkey(0), maxkey(0), rafOffset(-1), count(0) {}
    };

private:
    const ObjectDB* db;
    int n;
    int P; // number of pivots
    std::vector<int32_t> pivots; // pivot ids

    std::vector<ClusterNode> nodes;    // cluster-tree nodes
    // RAF: for each object stored sequentially: object id + distances to P pivots (double*P)
    std::string rafPath;
    mutable FILE* rafFp = nullptr;

    // auxiliary: for leaf clusters we also keep an in-memory array of keys sorted
    struct ClusterLeafMeta {
        int32_t nodeIdx;
        std::vector<double> keys; // mapped key(o) sorted
        std::vector<int32_t> objIds; // aligned with keys
        int64_t rafOffset; // offset in RAF for first of cluster
    };
    std::vector<ClusterLeafMeta> leafMeta; // one per leaf cluster

    // Metrics
    mutable long long compDist = 0;
    mutable long long pageReads = 0;
    mutable long long pageWrites = 0;
    mutable long long queryTime = 0;

    // convenience
    inline double distObj(int a, int b) const { compDist++; return db->distance(a,b); }

public:
    MIndex_Disk(const ObjectDB* db_, int numPivots=5): db(db_), P(numPivots) {
        if (!db) throw std::runtime_error("DB null");
        n = db->size();
    }
    ~MIndex_Disk() {
        if (rafFp) fclose(rafFp);
    }

    void clear_counters() const { compDist = pageReads = pageWrites = queryTime = 0; }
    long long get_compDist() const { return compDist; }
    long long get_pageReads() const { return pageReads; }
    long long get_pageWrites() const { return pageWrites; }
    long long get_queryTime() const { return queryTime; }

    // ------------------------------
    // Build: select pivots, compute mapping key(o) and distances, build cluster-tree
    // ------------------------------
    void build(const std::string& base) {
        using clock = std::chrono::high_resolution_clock;
        auto t0 = clock::now();

        // choose P pivots: here simple random sampling (paper suggests state-of-the-art selector)
        pivots.clear();
        pivots.reserve(P);
        std::vector<int> perm(n);
        for (int i=0;i<n;i++) perm[i]=i;
        std::shuffle(perm.begin(), perm.end(), std::mt19937{std::random_device{}()});
        for (int i=0;i<P;i++) pivots.push_back(perm[i]);

        // compute d+ = max distance observed from sample (approx)
        double dplus = 0.0;
        for (int i=0;i<std::min(n,1000); ++i) {
            for (int j=0;j<P; ++j) {
                double d = distObj(i, pivots[j]);
                if (d > dplus) dplus = d;
            }
        }
        if (dplus <= 0) dplus = 1.0;

        // prepare RAF file
        rafPath = base + ".midx_raf";
        std::ofstream outf(rafPath, std::ios::binary | std::ios::trunc);
        if (!outf) throw std::runtime_error("cannot write RAF");

        // For each object compute nearest pivot index pi and key(o) and write distances
        std::vector<Tmp> items; items.reserve(n);
        for (int id=0; id<n; ++id) {
            std::vector<double> d(P);
            double best = std::numeric_limits<double>::infinity();
            int bestIdx = 0;
            for (int j=0;j<P;j++){
                d[j] = distObj(id, pivots[j]);
                if (d[j] < best) { best = d[j]; bestIdx=j; }
            }
            double key = d[bestIdx] + (double)bestIdx * dplus;
            Tmp t; t.key = key; t.id = id; t.dists = std::move(d); t.pivotIdx = bestIdx;
            items.push_back(std::move(t));
        }

        // sort by key to form initial clusters by hyperplane nearest pivot groups
        std::sort(items.begin(), items.end(), [](const Tmp&a,const Tmp&b){ return a.key < b.key; });

        // Partition into clusters: simple heuristic: create clusters per pivot contiguous ranges
        // (paper describes dynamic cluster-tree; here we create top-level clusters per pivot)
        nodes.clear(); leafMeta.clear();
        for (int pIdx=0;pIdx<P;pIdx++){
            // gather contiguous items whose pivotIdx == pIdx
            int start = -1;
            for (size_t i=0;i<items.size();++i){
                if (items[i].pivotIdx==pIdx) { if (start<0) start = i; }
                else if (start>=0) {
                    int end = i;
                    createLeafCluster(items, start, end, pIdx, dplus, outf);
                    start = -1;
                }
            }
            // tail
            // find tail region
            int start2=-1;
            for (size_t i=0;i<items.size();++i){
                if (items[i].pivotIdx==pIdx) { start2 = i; break; }
            }
            if (start2>=0) {
                // find last index
                int last=-1;
                for (size_t i=items.size(); i>0; --i) if (items[i-1].pivotIdx==pIdx) { last = (int)i; break; }
                if (last>start2) createLeafCluster(items, start2, last, pIdx, dplus, outf);
            }
        }

        outf.close();

        // open RAF for queries
        rafFp = std::fopen(rafPath.c_str(),"rb");
        if (!rafFp) throw std::runtime_error("cannot reopen RAF");

        auto t1 = clock::now();
        queryTime += std::chrono::duration_cast<std::chrono::microseconds>(t1-t0).count();
    }

private:
    // helper to create a leaf cluster from items[start:end)
    void createLeafCluster(const std::vector<Tmp>& items, int start, int end, int pIdx,
                           double dplus, std::ofstream &outf) {
        // build ClusterNode leaf
        ClusterNode C;
        C.isLeaf = true;
        C.minDist.assign(P, std::numeric_limits<double>::infinity());
        C.maxDist.assign(P, 0.0);
        C.minkey = items[start].key;
        C.maxkey = items[end-1].key;
        C.rafOffset = -1;
        C.count = end - start;
        // write entries to RAF: record offset in number-of-entries units (we'll store id + P doubles)
        int64_t offset = (int64_t)outf.tellp();
        C.rafOffset = offset;
        for (int i=start;i<end;++i){
            int32_t id = items[i].id;
            // write id
            outf.write((char*)&id, sizeof(int32_t));
            // write P doubles for distances to pivots
            for (int j=0;j<P;j++){
                double dd = items[i].dists[j];
                outf.write((char*)&dd, sizeof(double));
                // update MBB
                C.minDist[j] = std::min(C.minDist[j], dd);
                C.maxDist[j] = std::max(C.maxDist[j], dd);
            }
        }

        int nodeIdx = nodes.size();
        nodes.push_back(C);

        // build leafMeta for faster in-memory filtering: load keys and ids
        ClusterLeafMeta meta;
        meta.nodeIdx = nodeIdx;
        meta.rafOffset = C.rafOffset;
        meta.keys.reserve(C.count);
        meta.objIds.reserve(C.count);
        // We can reconstruct keys as minkey..maxkey if needed, but we recorded them in sorted order previously:
        // For simplicity we will not reread file now; instead we will append keys from the caller
        // (This function is called during build where items are available.)
        for (int i=start;i<end;++i){
            meta.keys.push_back(items[i].key);
            meta.objIds.push_back(items[i].id);
        }
        leafMeta.push_back(std::move(meta));

        // increment pageWrites metric approximation
        pageWrites += 1;
    }

public:
    // ------------------------------
    // Range search: MRQ(q, R)
    //-------------------------------
    void rangeSearch(int qId, double R, std::vector<int>& out) const {
        using clock = std::chrono::high_resolution_clock;
        auto t0 = clock::now();
        out.clear();

        // Precompute distances from q to pivots
        std::vector<double> dq(P);
        for (int j=0;j<P;j++) dq[j] = distObj(qId, pivots[j]);

        // For each leaf cluster, test MBB intersection with SR(q) using Lemma 1
        for (const auto &meta : leafMeta) {
            const ClusterNode &C = nodes[meta.nodeIdx];
            bool intersects = true;
            for (int j=0;j<P && intersects;j++) {
                if (dq[j] + R < C.minDist[j] || dq[j] - R > C.maxDist[j]) intersects = false;
            }
            if (!intersects) continue;

            // load cluster entries from RAF (sequential read)
            pageReads += 1;
            // seek and read entries
            std::fseek(rafFp, meta.rafOffset, SEEK_SET);
            for (int i=0;i<(int)meta.objIds.size(); ++i) {
                int32_t oid;
                size_t rr = std::fread(&oid, sizeof(int32_t), 1, rafFp);
                (void)rr;
                std::vector<double> dists(P);
                for (int j=0;j<P;j++){
                    size_t r2 = std::fread(&dists[j], sizeof(double), 1, rafFp);
                    (void)r2;
                }
                // lemma1: if any pivot gives |d(o,pi)-d(q,pi)| > R then prune object
                bool pruned = false;
                for (int j=0;j<P;j++){
                    if (std::fabs(dists[j] - dq[j]) > R) { pruned = true; break; }
                }
                if (pruned) continue;
                double d = distObj(qId, oid);
                if (d <= R) out.push_back(oid);
            }
        }

        auto t1 = clock::now();
        queryTime += std::chrono::duration_cast<std::chrono::microseconds>(t1-t0).count();
    }

    // ------------------------------
    // KNN search (best-effort simple version)
    // ------------------------------
    void knnSearch(int qId, int k, std::vector<std::pair<double,int>>& out) const {
        using clock = std::chrono::high_resolution_clock;
        auto t0 = clock::now();
        out.clear();
        // best-first over clusters trimmed by MBB lower bounds
        // compute dq
        std::vector<double> dq(P);
        for (int j=0;j<P;j++) dq[j] = distObj(qId, pivots[j]);

        // priority queue of (lowerbound, leafMeta idx)
        struct QE{ double lb; int leafIdx; };
        struct Cmp { bool operator()(QE const&a, QE const&b) const { return a.lb > b.lb; } };
        std::priority_queue<QE, std::vector<QE>, Cmp> pq;

        for (size_t i=0;i<leafMeta.size(); ++i){
            const ClusterNode &C = nodes[leafMeta[i].nodeIdx];
            // compute lower bound from MBB: using max_j max(0, minDist[j]-dq[j], dq[j]-maxDist[j])
            double lb = 0.0;
            for (int j=0;j<P;j++){
                double delta = 0.0;
                if (dq[j] < C.minDist[j]) delta = C.minDist[j] - dq[j];
                else if (dq[j] > C.maxDist[j]) delta = dq[j] - C.maxDist[j];
                if (delta > lb) lb = delta; // lower bound via one pivot dimension (conservative)
            }
            pq.push({lb, (int)i});
        }

        std::priority_queue<std::pair<double,int>> best; // max-heap size k
        while(!pq.empty()){
            auto it = pq.top(); pq.pop();
            double clusterLB = it.lb;
            if ((int)best.size() == k && clusterLB >= best.top().first) break; // pruning
            // scan cluster
            const ClusterLeafMeta &meta = leafMeta[it.leafIdx];
            pageReads += 1;
            std::fseek(rafFp, meta.rafOffset, SEEK_SET);
            for (int i=0;i<(int)meta.objIds.size(); ++i){
                int32_t oid; std::fread(&oid, sizeof(int32_t), 1, rafFp);
                std::vector<double> dists(P);
                for (int j=0;j<P;j++) std::fread(&dists[j], sizeof(double), 1, rafFp);
                // pivot validation: Lemma 4 -> if exists pi: d(o,pi) <= r - d(q,pi) then validated
                double bound = best.size()< (size_t)k ? 1e300 : best.top().first;
                bool skip = false;
                for (int j=0;j<P;j++){
                    if (std::fabs(dists[j] - dq[j]) > bound) { skip = true; break; }
                }
                if (skip) continue;
                double d = distObj(qId, oid);
                if ((int)best.size() < k || d < best.top().first) {
                    best.emplace(d, oid);
                    if ((int)best.size() > k) best.pop();
                }
            }
        }

        while(!best.empty()){ out.push_back(best.top()); best.pop(); }
        std::reverse(out.begin(), out.end());

        auto t1 = clock::now();
        queryTime += std::chrono::duration_cast<std::chrono::microseconds>(t1-t0).count();
    }
};

#endif // MINDEX_DISK_HPP
