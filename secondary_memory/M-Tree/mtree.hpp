#ifndef MTREE_DISK_HPP
#define MTREE_DISK_HPP

#include "../../objectdb.hpp"

#include <vector>
#include <queue>
#include <cmath>
#include <chrono>
#include <algorithm>
#include <limits>
#include <stdexcept>
#include <cstdint>
#include <cstdio>
#include <iostream>

class MTree_Disk {
public:
    
    // to evaluate and compare queries 
    mutable long long compDist   = 0;  // # distancias
    mutable long long pageReads  = 0;  // páginas (nodos) leídos
    mutable long long pageWrites = 0;  // páginas (nodos) escritos en build
    mutable long long queryTime  = 0;  // tiempo acumulado en µs (solo queries)


    explicit MTree_Disk(const ObjectDB* db_, int nodeCapacity_ = 64)
        : db(db_), n(db_ ? db_->size() : 0),
          nodeCapacity(std::max(4, nodeCapacity_)),
          leafCapacity(std::max(4, nodeCapacity_)),
          fpIndex(nullptr),
          rootOffset(-1)
    {}

    ~MTree_Disk() 
    {
        if (fpIndex) {
            std::fclose(fpIndex);
            fpIndex = nullptr;
        }
    }

    void clear_counters() const 
    {
        compDist  = 0;
        pageReads = 0;
        queryTime = 0;
    }
    long long get_compDist()   const { return compDist;   }
    long long get_pageReads()  const { return pageReads;  }
    long long get_pageWrites() const { return pageWrites; }
    long long get_queryTime()  const { return queryTime;  }

    

    
    void build(const std::string& basePath) {
        if (!db) throw std::runtime_error("[MTree_Disk] db == nullptr");

        indexPath = basePath + ".mtree_index";

        fpIndex = std::fopen(indexPath.c_str(), "wb+");
        if (!fpIndex)
            throw std::runtime_error("[MTree_Disk] No se pudo crear " + indexPath);

        pageWrites = 0;

        // placeholder <> offset
        int64_t placeholder = -1;
        std::fwrite(&placeholder, sizeof(int64_t), 1, fpIndex);

        std::vector<int> objs(n);
        for (int i = 0; i < n; ++i) objs[i] = i;

        NodeRAM* rootRAM = build_recursive(objs, -1);

        // post-order save
        rootOffset = writeNodeRec(rootRAM);

        // real offset (from root) in the header
        std::fflush(fpIndex);
        std::fseek(fpIndex, 0, SEEK_SET);
        std::fwrite(&rootOffset, sizeof(int64_t), 1, fpIndex);
        std::fflush(fpIndex);

        // Free RAM
        freeTree(rootRAM);

        std::fclose(fpIndex);
        fpIndex = nullptr;
    }

    // load an existing index and load 'rootOffset'
    void restore(const std::string& basePath) {
        indexPath = basePath + ".mtree_index";

        if (fpIndex) {
            std::fclose(fpIndex);
            fpIndex = nullptr;
        }

        fpIndex = std::fopen(indexPath.c_str(), "rb");
        if (!fpIndex)
            throw std::runtime_error("[MTree_Disk] No se pudo abrir " + indexPath);

        if (std::fread(&rootOffset, sizeof(int64_t), 1, fpIndex) != 1) {
            std::fclose(fpIndex);
            fpIndex = nullptr;
            throw std::runtime_error("[MTree_Disk] Archivo corrupto (no rootOffset)");
        }

        if (rootOffset < 0)
            std::cerr << "[MTree_Disk] Advertencia: rootOffset < 0\n";
    }

    // MRQ: Range (Lemma 4.2)
    void rangeSearch(int qId, double R, std::vector<int>& out) const 
    {
        using clock = std::chrono::high_resolution_clock;
        auto t0 = clock::now();

        out.clear();
        if (!fpIndex)
            throw std::runtime_error("[MTree_Disk] rangeSearch: fpIndex==nullptr (¿faltó restore?)");
        if (rootOffset < 0) return; // índice vacío

        // DFS + Lemma 4.2 + parent filtering
        dfs_range(rootOffset, -1, 0.0, qId, R, out);

        auto t1 = clock::now();
        queryTime += std::chrono::duration_cast<std::chrono::microseconds>(t1 - t0).count();
    }

    // MkNN (best-first, ball lower-bound, Lemma 4.2) =========
    void knnSearch(int qId, int k, std::vector<std::pair<double,int>>& out) const 
    {
        using clock = std::chrono::high_resolution_clock;
        auto t0 = clock::now();

        out.clear();
        if (!fpIndex)
            throw std::runtime_error("[MTree_Disk] knnSearch: fpIndex==nullptr (¿faltó restore?)");
        if (rootOffset < 0 || k <= 0) return;

        // min-heap (of candidates)
        struct NodeCand {
            double lb;              // lower bound a la región (bola)
            int64_t offset;         // offset del nodo hijo
            int    parentCenterId;  // id del routing object padre
            double distParentQ;
        };
        struct CmpCand {
            bool operator()(const NodeCand& a, const NodeCand& b) const 
            {
                return a.lb > b.lb; // min-heap
            }
        };

        std::priority_queue<NodeCand, std::vector<NodeCand>, CmpCand> pq;

        // max-heap (based on distance)
        using Res = std::pair<double,int>;
        std::priority_queue<Res> best;

        NodeDisk root;
        readNode(rootOffset, root); // page read

        for (const auto& e : root.entries) 
        {
            double dQR = dist(qId, e.objId);          
            double lb   = std::max(0.0, dQR - e.radius);

            if (root.isLeaf) 
                insertBest(best, k, dQR, e.objId);
            else 
                pq.push(NodeCand{lb, e.childOffset, e.objId, dQR});
            
        }

        // best-first search
        while (!pq.empty()) {
            double worst = best.empty()
                         ? std::numeric_limits<double>::infinity()
                         : best.top().first;

            NodeCand cand = pq.top();
            if (cand.lb > worst) break; // ninguna región más puede mejorar
            pq.pop();

            NodeDisk node;
            readNode(cand.offset, node); // pageRead 

            if (node.isLeaf) {
                for (const auto& e : node.entries) 
                {
                    double d = dist(qId, e.objId);
                    insertBest(best, k, d, e.objId);
                }
            } else {
                for (const auto& e : node.entries) 
                {
                    double dQR = dist(qId, e.objId);  // δ(Q,R)
                    double lb   = std::max(0.0, dQR - e.radius);  // lower bound

                    double worstNow = best.empty()
                                      ? std::numeric_limits<double>::infinity()
                                      : best.top().first;
                    if (lb > worstNow) continue; // (Lemma 4.2)

                    pq.push(NodeCand{lb, e.childOffset, e.objId, dQR});
                }
            }
        }

        // save results in a vector
        std::vector<Res> tmp;
        while (!best.empty()) {
            tmp.push_back(best.top());
            best.pop();
        }
        std::reverse(tmp.begin(), tmp.end());
        out = std::move(tmp);

        auto t1 = clock::now();
        queryTime += std::chrono::duration_cast<std::chrono::microseconds>(t1 - t0).count();
    }

private:
    const ObjectDB* db;
    int n;
    int nodeCapacity;  // m (máx entradas por nodo)
    int leafCapacity;  // máx entradas por hoja

    std::string indexPath;
    mutable FILE* fpIndex;
    int64_t rootOffset;

    // ---- Nodo en RAM (para construcción tipo bulk-loading M-tree) ----
    struct NodeRAM {
        bool isLeaf;
        struct EntryRAM {
            int objId;        // objeto o routing object
            double radius;    // r_R (solo si no es hoja)
            double parentDist;
            NodeRAM* child;   // nullptr en hoja
        };
        std::vector<EntryRAM> entries;
        explicit NodeRAM(bool leaf = true) : isLeaf(leaf) {}
    };

    // ---- Nodo en disco para consultas ----
    struct EntryDisk 
    {
        int32_t objId;
        double  radius;
        double  parentDist;
        int64_t childOffset; // -1 en hojas
    };

    struct NodeDisk 
    {
        bool isLeaf;
        int32_t count;
        std::vector<EntryDisk> entries;
    };


    double dist(int a, int b) const {
        compDist++;
        return db->distance(a, b);
    }


NodeRAM* build_recursive(const std::vector<int>& objs, int parentCenterId) {
    NodeRAM* node;

    if ((int)objs.size() <= leafCapacity) {
        // leaf node
        node = new NodeRAM(true);
        node->entries.reserve(objs.size());
        for (int oid : objs) {
            typename NodeRAM::EntryRAM e;
            e.objId = oid;
            e.radius = 0.0;
            e.child  = nullptr;
            if (parentCenterId < 0)
                e.parentDist = 0.0; // raíz
            else
                e.parentDist = dist(oid, parentCenterId);
            node->entries.push_back(e);
        }
        return node;
    }

    node = new NodeRAM(false);

    // 1) Elegir hasta nodeCapacity centros por farthest-first con caching y muestreo
    int maxCenters = std::min(nodeCapacity, (int)objs.size());
    std::vector<int> centers;
    centers.reserve(maxCenters);
    centers.push_back(objs[0]);

    // Parámetros de muestreo (ajustables)
    const size_t sampleThreshold = 10000; // si objs.size() > threshold, usamos muestra
    bool useSampling = objs.size() > sampleThreshold;
    std::vector<int> sampleIdx; // posiciones dentro de objs
    if (useSampling) {
        size_t sampleSize = std::min<size_t>(sampleThreshold, objs.size());
        sampleIdx.reserve(sampleSize);
        size_t step = objs.size() / sampleSize;
        if (step == 0) step = 1;
        for (size_t i = 0; i < objs.size() && sampleIdx.size() < sampleSize; i += step) {
            sampleIdx.push_back((int)i);
        }
        // si faltan, completar
        for (size_t i = 0; sampleIdx.size() < sampleSize; ++i) {
            sampleIdx.push_back((int)(i % objs.size()));
        }
    }

    // minDist guarda la distancia mínima al conjunto actual de centros
    std::vector<double> minDist;
    if (useSampling) {
        minDist.assign(sampleIdx.size(), std::numeric_limits<double>::infinity());
        for (size_t i = 0; i < sampleIdx.size(); ++i) {
            int oid = objs[sampleIdx[i]];
            minDist[i] = dist(oid, centers[0]);
        }
    } else {
        minDist.assign(objs.size(), std::numeric_limits<double>::infinity());
        for (size_t i = 0; i < objs.size(); ++i) {
            int oid = objs[i];
            minDist[i] = dist(oid, centers[0]);
        }
    }

    while ((int)centers.size() < maxCenters) {
        double bestMin = -1.0;
        int bestIdx = -1; // index into objs (not into sampleIdx)
        if (useSampling) {
            for (size_t idx = 0; idx < sampleIdx.size(); ++idx) {
                if (minDist[idx] > bestMin) {
                    bestMin = minDist[idx];
                    bestIdx = sampleIdx[idx];
                }
            }
        } else {
            for (size_t idx = 0; idx < objs.size(); ++idx) {
                if (minDist[idx] > bestMin) {
                    bestMin = minDist[idx];
                    bestIdx = (int)idx;
                }
            }
        }
        if (bestIdx == -1) break;
        int bestObj = objs[bestIdx];

        // Evitar repetir un centro ya elegido (muy raro)
        bool already = false;
        for (int c : centers) if (c == bestObj) { already = true; break; }
        if (already) break;

        centers.push_back(bestObj);

        // actualizar minDist: sólo comparando con el nuevo centro
        if (useSampling) {
            for (size_t idx = 0; idx < sampleIdx.size(); ++idx) {
                int oid = objs[sampleIdx[idx]];
                double d = dist(oid, bestObj);
                if (d < minDist[idx]) minDist[idx] = d;
            }
        } else {
            for (size_t idx = 0; idx < objs.size(); ++idx) {
                int oid = objs[idx];
                double d = dist(oid, bestObj);
                if (d < minDist[idx]) minDist[idx] = d;
            }
        }
    }

    // 2) Asignar cada objeto a su centro más cercano (k comparaciones por objeto)
    std::vector<std::vector<int>> groups(centers.size());
    for (int oid : objs) {
        double bestD = std::numeric_limits<double>::infinity();
        int bestC = 0;
        for (size_t i = 0; i < centers.size(); ++i) {
            double d = dist(oid, centers[i]);
            if (d < bestD) { bestD = d; bestC = (int)i; }
        }
        groups[bestC].push_back(oid);
    }

    // 3) Para cada grupo, calcular radio y recursar
    for (size_t i = 0; i < centers.size(); ++i) {
        if (groups[i].empty()) continue; // centro sin objetos asignados (puede pasar)

        int centerId = centers[i];

        double rad = 0.0;
        for (int oid : groups[i]) {
            double d = dist(centerId, oid);
            if (d > rad) rad = d;
        }

        NodeRAM* child = build_recursive(groups[i], centerId);

        typename NodeRAM::EntryRAM e;
        e.objId = centerId;
        e.radius = rad;
        e.child  = child;
        if (parentCenterId < 0)
            e.parentDist = 0.0; // entrada de raíz
        else
            e.parentDist = dist(centerId, parentCenterId);
        node->entries.push_back(e);
    }

    return node;
}


    int64_t writeNodeRec(NodeRAM* node) {
        if (!node) return -1;

        // 1) Escribir primero todos los hijos (para conocer offsets)
        std::vector<int64_t> childOffsets;
        childOffsets.reserve(node->entries.size());

        if (!node->isLeaf) {
            for (auto& e : node->entries) {
                int64_t offChild = writeNodeRec(e.child);
                childOffsets.push_back(offChild);
            }
        }

        // 2) Escribir este nodo y devolver su offset
        std::fseek(fpIndex, 0, SEEK_END);
        int64_t offset = std::ftell(fpIndex);

        bool isLeaf = node->isLeaf;
        int32_t count = (int32_t)node->entries.size();

        std::fwrite(&isLeaf, sizeof(bool), 1, fpIndex);
        std::fwrite(&count, sizeof(int32_t), 1, fpIndex);

        for (size_t i = 0; i < node->entries.size(); ++i) {
            const auto& er = node->entries[i];
            EntryDisk ed;
            ed.objId      = (int32_t)er.objId;
            ed.radius     = er.radius;
            ed.parentDist = er.parentDist;
            ed.childOffset= node->isLeaf ? -1 : childOffsets[i];
            std::fwrite(&ed.objId,      sizeof(int32_t), 1, fpIndex);
            std::fwrite(&ed.radius,     sizeof(double),  1, fpIndex);
            std::fwrite(&ed.parentDist, sizeof(double),  1, fpIndex);
            std::fwrite(&ed.childOffset,sizeof(int64_t), 1, fpIndex);
        }

        pageWrites++; // 1 nodo = 1 página lógica

        return offset;
    }

    void freeTree(NodeRAM* node) {
        if (!node) return;
        if (!node->isLeaf) {
            for (auto& e : node->entries) {
                freeTree(e.child);
            }
        }
        delete node;
    }

    // read from disk
    void readNode(int64_t offset, NodeDisk& node) const {
        if (!fpIndex)
            throw std::runtime_error("[MTree_Disk] readNode: fpIndex==nullptr");

        std::fseek(fpIndex, offset, SEEK_SET);
        bool isLeaf;
        int32_t count;
        if (std::fread(&isLeaf, sizeof(bool), 1, fpIndex) != 1)
            throw std::runtime_error("[MTree_Disk] readNode: fread isLeaf falló");
        if (std::fread(&count, sizeof(int32_t), 1, fpIndex) != 1)
            throw std::runtime_error("[MTree_Disk] readNode: fread count falló");

        node.isLeaf = isLeaf;
        node.count  = count;
        node.entries.resize(count);

        for (int i = 0; i < count; ++i) {
            EntryDisk ed;
            if (std::fread(&ed.objId,      sizeof(int32_t), 1, fpIndex) != 1 ||
                std::fread(&ed.radius,     sizeof(double),  1, fpIndex) != 1 ||
                std::fread(&ed.parentDist, sizeof(double),  1, fpIndex) != 1 ||
                std::fread(&ed.childOffset,sizeof(int64_t), 1, fpIndex) != 1) {
                throw std::runtime_error("[MTree_Disk] readNode: fread entry falló");
            }
            node.entries[i] = ed;
        }

        pageReads++; // Page Access
    }

    // dfs for MRQ
    void dfs_range(int64_t offset,
                   int parentCenterId,
                   double distParentQ,
                   int qId,
                   double R,
                   std::vector<int>& out) const
    {
        NodeDisk node;
        readNode(offset, node);

        for (const auto& e : node.entries) {
            // --- Parent filtering (si hay padre) ---
            if (parentCenterId >= 0) {
                double dPQ = distParentQ;  
                double dPR = e.parentDist; 
                if (std::fabs(dPQ - dPR) > R + e.radius) {
                    // la bola (R,r) no intersecta B(Q,R)
                    continue;
                }
            }

            double dQR = dist(qId, e.objId);

            if (dQR > R + e.radius)
                continue; // bola (R,r) no intersecta B(Q,R)

            if (node.isLeaf) {
                // e.objId es un objeto
                if (dQR <= R)
                    out.push_back(e.objId);
            } else {
                // Bola interna: descender al hijo
                dfs_range(e.childOffset, e.objId, dQR, qId, R, out);
            }
        }
    }

    // maintain k-best in a heap
    static void insertBest(std::priority_queue<std::pair<double,int>>& best,
                           int k, double d, int id)
    {
        if ((int)best.size() < k) {
            best.emplace(d, id);
        } else if (d < best.top().first) {
            best.pop();
            best.emplace(d, id);
        }
    }
};

#endif // MTREE_DISK_HPP
