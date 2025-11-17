#ifndef EGNAT_DISK_HPP
#define EGNAT_DISK_HPP

#include "../../objectdb.hpp"
#include <vector>
#include <queue>
#include <random>
#include <cmath>
#include <fstream>
#include <cstdio>
#include <cstdint>
#include <limits>
#include <chrono>
#include <stdexcept>
#include <algorithm>

/*
 ======================================================================
              EGNAT-Disk (versión estática para Chen et al.)
 ======================================================================

  ✔ Árbol GNAT multinivel (build top-down)
  ✔ Pivotes por nivel (m = 5 por defecto)
  ✔ Hoja: objetos con dist-to-parent (Lemma 4.2)
  ✔ Nodo interno: rangos min/max (Lemma 4.1)
  ✔ DFS Range con poda GNAT exacta
  ✔ DFS kNN con radio dinámico (segundo enfoque de Chen)
  ✔ pageBytes = {4096, 40960} según dataset
  ✔ Métricas: compdist, pageReads, pageWrites, queryTime
  ✔ Archivos:
         base.egn_leaf  (store leaf entries)
         base.egn_index (store the index tree)
 ======================================================================
*/

class EGNAT_Disk {
public:
    static constexpr int MAX_M = 16;

    struct LeafEntry {
        int32_t id;
        double  distParent;
    };

    struct InternalNode {
        int32_t m;
        int32_t pivot[MAX_M];
        int32_t child[MAX_M]; // índice en nodes[]
        double  minv[MAX_M][MAX_M];
        double  maxv[MAX_M][MAX_M];
    };

    struct LeafInfo {
        int32_t parentPivot;
        int64_t offset;  // offset en LeafEntry
        int32_t count;
    };

    struct Node {
        bool isLeaf;
        InternalNode in;
        int leafIdx;
    };

private:
    const ObjectDB* db;

    int n;
    int dim;
    int m;
    int pageBytes;
    int leafCap;
    int pagesPerNode;

    std::vector<Node> nodes;
    std::vector<LeafInfo> leaves;
    std::vector<LeafEntry> leafEntries;
    int root = -1;

    // Archivos
    std::string leafPath;
    mutable FILE* leafFp = nullptr;

    // Métricas
    mutable long long compDist = 0;
    mutable long long pageReads = 0;
    mutable long long pageWrites = 0;
    mutable long long queryTime = 0;

public:
    EGNAT_Disk(const ObjectDB* db_, int m_ = 5, int pageBytes_ = 4096)
    : db(db_), n(db_ ? db_->size() : 0),
      m(m_), pageBytes(pageBytes_)
    {
        if (!db) throw std::runtime_error("DB null");
        if (m <= 0 || m > MAX_M)
            throw std::runtime_error("invalid m");

        leafCap     = std::max<int>(1, int(pageBytes / sizeof(LeafEntry)));
        pagesPerNode = std::max<int>(1, int(pageBytes / 4096));
    }


    ~EGNAT_Disk() {
        if (leafFp) fclose(leafFp);
    }

    void clear_counters() const {
        compDist = pageReads = queryTime = 0;
    }

    long long get_compDist()  const { return compDist; }
    long long get_pageReads() const { return pageReads; }
    long long get_pageWrites()const { return pageWrites; }
    long long get_queryTime() const { return queryTime; }

private:
    inline double distObj(int a, int b) const {
        compDist++;
        return db->distance(a,b);
    }

public:
    // =====================================================
    //                      BUILD
    // =====================================================
    void build(const std::string& base) {
        using clock = std::chrono::high_resolution_clock;
        auto t0 = clock::now();

        nodes.clear();
        leaves.clear();
        leafEntries.clear();
        pageWrites = 0;

        leafPath = base + ".egn_leaf";
        std::string idxPath = base + ".egn_index";

        std::vector<int> obj(n);
        for (int i = 0; i < n; i++) obj[i] = i;

        root = buildNode(obj, -1);

        // --- escribir hojas ---
        {
            std::ofstream out(leafPath, std::ios::binary | std::ios::trunc);
            if (!out) throw std::runtime_error("cannot write leaf file");

            if (!leafEntries.empty()) {
                out.write(reinterpret_cast<const char*>(leafEntries.data()),
                          leafEntries.size() * sizeof(LeafEntry));
            }
        }
        pageWrites += (long long)leaves.size() * pagesPerNode;

        // --- escribir índice (todos los nodos) ---
        {
            std::ofstream out(idxPath, std::ios::binary | std::ios::trunc);
            if (!out) throw std::runtime_error("cannot write index file");

            int32_t N = nodes.size();
            out.write((char*)&N, sizeof(N));
            out.write((char*)&m, sizeof(m));
            out.write((char*)&dim, sizeof(dim));
            out.write((char*)&n, sizeof(n));
            out.write((char*)&pageBytes, sizeof(pageBytes));

            for (auto& nd : nodes) {
                uint8_t lf = nd.isLeaf ? 1 : 0;
                out.write((char*)&lf, sizeof(lf));
                if (lf) {
                    const LeafInfo& L = leaves[nd.leafIdx];
                    out.write((char*)&L.parentPivot, sizeof(int32_t));
                    out.write((char*)&L.offset, sizeof(int64_t));
                    out.write((char*)&L.count, sizeof(int32_t));
                } else {
                    const InternalNode& in = nd.in;
                    out.write((char*)&in.m, sizeof(int32_t));
                    out.write((char*)in.pivot, sizeof(int32_t)*MAX_M);
                    out.write((char*)in.child, sizeof(int32_t)*MAX_M);
                    out.write((char*)in.minv, sizeof(double)*MAX_M*MAX_M);
                    out.write((char*)in.maxv, sizeof(double)*MAX_M*MAX_M);
                }
            }
        }
        pageWrites += (long long)nodes.size() * pagesPerNode;

        // abrir archivo de hojas para queries
        leafFp = std::fopen(leafPath.c_str(),"rb");
        if (!leafFp) throw std::runtime_error("cannot reopen leaves");

        auto t1 = clock::now();
        double ms = std::chrono::duration<double,std::milli>(t1-t0).count();
        std::cerr << "[EGNAT] Build OK (" << ms << " ms)\n";
    }

private:
    // =====================================================
    //            BUILD RECURSIVO MULTINIVEL
    // =====================================================
    int buildNode(const std::vector<int>& objs, int parentPivot) {
        if ((int)objs.size() <= leafCap) {
            LeafInfo L;
            L.parentPivot = parentPivot;
            L.offset = leafEntries.size();
            L.count = objs.size();

            for (int id : objs) {
                LeafEntry e;
                e.id = id;
                e.distParent = (parentPivot<0) ? 0.0 : distObj(id, parentPivot);
                leafEntries.push_back(e);
            }

            int idxLeaf = leaves.size();
            leaves.push_back(L);

            Node nd;
            nd.isLeaf = true;
            nd.leafIdx = idxLeaf;
            nodes.push_back(nd);
            return nodes.size()-1;
        }

        // ---- nodo interno GNAT ----
        InternalNode in{};
        in.m = std::min(m, (int)objs.size());

        // elegir pivotes
        std::vector<int> perm = objs;
        static thread_local std::mt19937_64 rng{std::random_device{}()};
        std::shuffle(perm.begin(), perm.end(), rng);
        for (int i=0;i<in.m;i++){
            in.pivot[i] = perm[i];
        }

        for (int i=0;i<MAX_M;i++){
            in.child[i] = -1;
            for (int j=0;j<MAX_M;j++){
                in.minv[i][j] = std::numeric_limits<double>::infinity();
                in.maxv[i][j] = 0.0;
            }
        }

        // buckets
        std::vector<std::vector<int>> B(in.m);
        for (int j=0;j<in.m;j++) B[j].push_back(in.pivot[j]);

        for (int id : objs) {
            bool isp = false;
            for (int j=0;j<in.m;j++)
                if (id == in.pivot[j]) isp = true;
            if (isp) continue;

            double best = 1e300; int bj=0;
            for (int j=0;j<in.m;j++){
                double d = distObj(id, in.pivot[j]);
                if (d < best){ best=d; bj=j; }
            }
            B[bj].push_back(id);
        }

        // calcular rangos exactos GNAT
        for (int j=0;j<in.m;j++){
            for (int id : B[j]) {
                for (int i=0;i<in.m;i++){
                    double d = distObj(id, in.pivot[i]);
                    in.minv[i][j] = std::min(in.minv[i][j], d);
                    in.maxv[i][j] = std::max(in.maxv[i][j], d);
                }
            }
        }

        // crear nodo
        Node nd;
        nd.isLeaf = false;
        nd.leafIdx = -1;
        nd.in = in;
        nodes.push_back(nd);
        int me = nodes.size()-1;

        // recursión
        for (int j=0;j<in.m;j++){
            std::vector<int> child;
            child.reserve(B[j].size());
            for (int id : B[j])
                if (id != in.pivot[j])
                    child.push_back(id);

            if (child.empty()) continue;

            int ch = buildNode(child, in.pivot[j]);
            nodes[me].in.child[j] = ch;
        }

        return me;
    }

public:
    // =====================================================
    //                        RANGE SEARCH
    // =====================================================
    void rangeSearch(int qId, double R, std::vector<int>& out) const {
        using clock=std::chrono::high_resolution_clock;
        auto t0=clock::now();
        out.clear();
        dfsRange(root, qId, R, out);
        auto t1=clock::now();
        queryTime += std::chrono::duration_cast<std::chrono::microseconds>(t1-t0).count();
    }

private:
    void dfsRange(int nd, int q, double R, std::vector<int>& out) const {
        const Node& N = nodes[nd];
        if (N.isLeaf) {
            pageReads += pagesPerNode;
            const LeafInfo& L = leaves[N.leafIdx];

            double dqp = 0.0;
            if (L.parentPivot>=0)
                dqp = distObj(q, L.parentPivot);

            std::vector<LeafEntry> buf(L.count);
            int64_t off = (int64_t)L.offset * (int64_t)sizeof(LeafEntry);

            std::fseek(leafFp, off, SEEK_SET);
            //std::fread(buf.data(), sizeof(LeafEntry), L.count, leafFp);
            size_t _nread = std::fread(buf.data(), sizeof(LeafEntry), L.count, leafFp);
            (void)_nread;


            for (auto& e : buf) {
                if (std::fabs(e.distParent - dqp) > R) continue;
                double d = distObj(q, e.id);
                if (d <= R) out.push_back(e.id);
            }
            return;
        }

        pageReads += 1;
        const InternalNode& I = N.in;

        double dq[MAX_M];
        dq[0] = distObj(q, I.pivot[0]);
        int c = 0;
        double dmin = dq[0];

        for (int i=1;i<I.m;i++){
            dq[i] = distObj(q, I.pivot[i]);
            if (dq[i] < dmin){ dmin = dq[i]; c = i; }
        }

        double a = dmin - R;
        double b = dmin + R;

        for (int i=0;i<I.m;i++){
            if (dq[i] <= R) out.push_back(I.pivot[i]);

            int ch = I.child[i];
            if (ch < 0) continue;

            double mn = I.minv[i][c];
            double mx = I.maxv[i][c];
            if (b < mn || a > mx) continue;

            dfsRange(ch, q, R, out);
        }
    }

public:
    // =====================================================
    //                        KNN SEARCH
    // =====================================================
    void knnSearch(int q, int k, std::vector<std::pair<double,int>>& out) const {
        using clock=std::chrono::high_resolution_clock;
        auto t0=clock::now();

        out.clear();
        std::priority_queue<std::pair<double,int>> pq;
        dfsKNN(root, q, k, pq);

        while (!pq.empty()) {
            out.push_back(pq.top());
            pq.pop();
        }
        std::reverse(out.begin(), out.end());

        auto t1=clock::now();
        queryTime += std::chrono::duration_cast<std::chrono::microseconds>(t1-t0).count();
    }

private:
    void dfsKNN(int nd, int q, int k,
                std::priority_queue<std::pair<double,int>>& pq) const
    {
        const Node& N = nodes[nd];
        double rk = pq.size()<k ? 1e300 : pq.top().first;

        if (N.isLeaf) {
            pageReads += pagesPerNode;
            const LeafInfo& L = leaves[N.leafIdx];

            double dqp = (L.parentPivot<0)?0.0:distObj(q, L.parentPivot);

            std::vector<LeafEntry> buf(L.count);
            int64_t off = (int64_t)L.offset * (int64_t)sizeof(LeafEntry);
            std::fseek(leafFp, off, SEEK_SET);
            //std::fread(buf.data(), sizeof(LeafEntry), L.count, leafFp);
            size_t _nread2 = std::fread(buf.data(), sizeof(LeafEntry), L.count, leafFp);
            (void)_nread2;


            for (auto& e : buf) {
                double bound = pq.size()<k ? 1e300 : pq.top().first;
                if (std::fabs(e.distParent - dqp) > bound) continue;
                double d = distObj(q, e.id);
                if ((int)pq.size()<k || d < pq.top().first) {
                    pq.emplace(d, e.id);
                    if ((int)pq.size()>k) pq.pop();
                }
            }
            return;
        }

        pageReads += 1;
        const InternalNode& I = N.in;

        double dq[MAX_M];
        dq[0] = distObj(q, I.pivot[0]);
        int c = 0;
        double dmin = dq[0];

        for (int i=1;i<I.m;i++){
            dq[i] = distObj(q, I.pivot[i]);
            if (dq[i]<dmin){ dmin=dq[i]; c=i; }
        }

        // pivotes también son candidatos
        for (int i=0;i<I.m;i++){
            double d = dq[i];
            if ((int)pq.size()<k || d < pq.top().first) {
                pq.emplace(d, I.pivot[i]);
                if ((int)pq.size()>k) pq.pop();
            }
        }

        rk = pq.size()<k ? 1e300 : pq.top().first;
        double a = dmin - rk;
        double b = dmin + rk;

        for (int i=0;i<I.m;i++){
            int ch = I.child[i];
            if (ch<0) continue;

            double mn = I.minv[i][c];
            double mx = I.maxv[i][c];
            if (!(b < mn || a > mx))
                dfsKNN(ch, q, k, pq);
        }
    }
};

#endif
