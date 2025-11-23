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
  M-Index* mejorado con:
  - B+-tree simulado mediante std::map para indexar mapped keys
  - Lemma 4.5 para validación directa de objetos
  - Range search optimizado con búsqueda por intervalos de keys
  - Best-first traversal para k-NN

  *** CORREGIDO ***
  - Soporta pivotes HFI externos mediante overridePivots(...)
  - Si overridePivots se llama antes de build(), se usan esos pivotes
    y NO se seleccionan pivotes aleatorios.
*/

class MIndex_Improved {
public:
    struct RAFEntry {
        int32_t id;
        std::vector<double> dists; // distancias a P pivotes
        double key;                // mapped key
    };

    struct ClusterNode {
        bool isLeaf;
        double minkey, maxkey;
        std::vector<double> minDist; // MBB: min distance to each pivot
        std::vector<double> maxDist; // MBB: max distance to each pivot
        std::vector<int32_t> children;
        int64_t rafOffset;
        int32_t count;
        ClusterNode()
            : isLeaf(false), minkey(0.0), maxkey(0.0),
              rafOffset(-1), count(0) {}
    };

private:
    const ObjectDB* db;
    int n;
    int P;                         // number of pivots
    std::vector<int32_t> pivots;   // pivot ids
    bool pivotsFixed = false;      // true si vienen de HFI/override
    double dplus;                  // maximum distance observed

    std::vector<ClusterNode> nodes;

    // B+-tree simulation: map from key -> RAF entry list
    std::map<double, std::vector<RAFEntry>> btreeIndex;

    std::string rafPath;
    mutable FILE* rafFp = nullptr;

    // Metrics
    mutable long long compDist   = 0;
    mutable long long pageReads  = 0;
    mutable long long pageWrites = 0;
    mutable long long queryTime  = 0;

    inline double distObj(int a, int b) const {
        compDist++;
        return db->distance(a,b);
    }

public:
    MIndex_Improved(const ObjectDB* db_, int numPivots = 5)
        : db(db_), n(0), P(numPivots), dplus(0.0)
    {
        if (!db) throw std::runtime_error("MIndex_Improved: DB null");
        n = db->size();
        if (P <= 0) throw std::runtime_error("MIndex_Improved: numPivots must be > 0");
        if (P > n)  P = n;
    }

    ~MIndex_Improved() {
        if (rafFp) std::fclose(rafFp);
    }

    // --------------------------------------------------
    // API de métricas
    // --------------------------------------------------
    void clear_counters() const {
        compDist   = 0;
        pageReads  = 0;
        pageWrites = 0;
        queryTime  = 0;
    }
    long long get_compDist()   const { return compDist;   }
    long long get_pageReads()  const { return pageReads;  }
    long long get_pageWrites() const { return pageWrites; }
    long long get_queryTime()  const { return queryTime;  }

    int get_num_pivots() const { return P; }

    // --------------------------------------------------
    // overridePivots: usar pivotes HFI externos
    //
    // Debes cargar los pivotes desde tu JSON (HFI) y
    // luego llamar a:
    //
    //   mindex.overridePivots(pivotsHFI);
    //   mindex.build("index_base");
    //
    // Si NO llamas a overridePivots, build() seleccionará
    // pivotes aleatorios como antes.
    // --------------------------------------------------
    void overridePivots(const std::vector<int>& external) {
        if (!db) throw std::runtime_error("overridePivots: DB null");
        if ((int)external.size() != P) {
            throw std::runtime_error(
                "overridePivots: size mismatch (expected " +
                std::to_string(P) + ", got " +
                std::to_string(external.size()) + ")");
        }
        for (int id : external) {
            if (id < 0 || id >= n) {
                throw std::runtime_error(
                    "overridePivots: pivot id out of range: " +
                    std::to_string(id));
            }
        }
        pivots.assign(external.begin(), external.end());
        pivotsFixed = true;
    }

    // ------------------------------
    // Build: M-index* con B+-tree
    // ------------------------------
    void build(const std::string& base) {
        using clock = std::chrono::high_resolution_clock;
        auto t0 = clock::now();

        // Reset estado
        nodes.clear();
        btreeIndex.clear();
        if (rafFp) { std::fclose(rafFp); rafFp = nullptr; }
        compDist   = 0;
        pageReads  = 0;
        pageWrites = 0;
        queryTime  = 0;

        // 1) Selección de pivotes
        //    - Si overridePivots() fue llamado, usamos esos pivotes.
        //    - Si no, seleccionamos P pivotes aleatorios (comportamiento antiguo).
        if (!pivotsFixed) {
            pivots.clear();
            pivots.reserve(P);
            std::vector<int> perm(n);
            for (int i = 0; i < n; ++i) perm[i] = i;
            std::shuffle(perm.begin(), perm.end(),
                         std::mt19937{std::random_device{}()});
            for (int i = 0; i < P; ++i) {
                pivots.push_back(perm[i]);
            }
        }
        // (Si pivotsFixed==true, ya tenemos pivots válidos en 'pivots')

        // 2) Calcular d+ (máxima distancia observada a cualquier pivot)
        dplus = 0.0;
        int sampleSize = std::min(n, 1000);
        for (int i = 0; i < sampleSize; ++i) {
            for (int j = 0; j < P; ++j) {
                double d = distObj(i, pivots[j]);
                if (d > dplus) dplus = d;
            }
        }
        if (dplus <= 0.0) dplus = 1.0;

        // 3) Calcular mapped keys y distancias para todos los objetos
        std::vector<RAFEntry> entries;
        entries.reserve(n);

        for (int id = 0; id < n; ++id) {
            RAFEntry entry;
            entry.id = id;
            entry.dists.resize(P);

            double minDist = std::numeric_limits<double>::infinity();
            int nearestPivot = 0;

            for (int j = 0; j < P; ++j) {
                entry.dists[j] = distObj(id, pivots[j]);
                if (entry.dists[j] < minDist) {
                    minDist = entry.dists[j];
                    nearestPivot = j;
                }
            }

            // M-index* key mapping: key(o) = d(pi, o) + i * d+
            // (en el paper usan (i-1)*d+, pero el offset es equivalente
            //  mientras se use consistentemente; aquí mantenemos i*dplus
            //  o (i-1)*dplus según prefieras. Dejamos (i)*dplus para evitar
            //  keys negativas; ajusta si quieres exactamente (i-1)*dplus.)
            entry.key = entry.dists[nearestPivot] + nearestPivot * dplus;
            entries.push_back(std::move(entry));
        }

        // 4) Ordenar por key para B+-tree
        std::sort(entries.begin(), entries.end(),
                  [](const RAFEntry& a, const RAFEntry& b) {
                      return a.key < b.key;
                  });

        // 5) Construir B+-tree simulado
        btreeIndex.clear();
        for (const auto& entry : entries) {
            btreeIndex[entry.key].push_back(entry);
        }

        // 6) Construir cluster "tree" (en esta versión: un nodo hoja por pivot)
        buildClusterTree(entries);

        // 7) Escribir RAF
        rafPath = base + ".midx_raf";
        writeRAF(entries);

        rafFp = std::fopen(rafPath.c_str(), "rb");
        if (!rafFp) throw std::runtime_error("MIndex_Improved: cannot reopen RAF");

        auto t1 = clock::now();
        queryTime +=
            std::chrono::duration_cast<std::chrono::microseconds>(t1 - t0).count();
    }

private:
    // --------------------------------------------------
    // Construir "cluster tree" (aquí: un nodo por pivote)
    // --------------------------------------------------
    void buildClusterTree(const std::vector<RAFEntry>& entries) {
        nodes.clear();

        // Crear un cluster por pivot
        for (int pIdx = 0; pIdx < P; ++pIdx) {
            std::vector<const RAFEntry*> clusterEntries;

            for (const auto& entry : entries) {
                // Encontrar pivot más cercano de este entry
                double minD = std::numeric_limits<double>::infinity();
                int nearest = 0;
                for (int j = 0; j < P; ++j) {
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
            node.count  = (int32_t)clusterEntries.size();

            // Calcular MBB
            for (const auto* e : clusterEntries) {
                for (int j = 0; j < P; ++j) {
                    node.minDist[j] = std::min(node.minDist[j], e->dists[j]);
                    node.maxDist[j] = std::max(node.maxDist[j], e->dists[j]);
                }
            }

            nodes.push_back(std::move(node));
        }
    }

    // --------------------------------------------------
    // Escribir RAF con id, distancias a pivotes y key
    // --------------------------------------------------
    void writeRAF(const std::vector<RAFEntry>& entries) {
        std::ofstream outf(rafPath, std::ios::binary | std::ios::trunc);
        if (!outf) throw std::runtime_error("MIndex_Improved: cannot write RAF");

        for (const auto& entry : entries) {
            outf.write((char*)&entry.id, sizeof(int32_t));
            for (int j = 0; j < P; ++j) {
                outf.write((char*)&entry.dists[j], sizeof(double));
            }
            outf.write((char*)&entry.key, sizeof(double));
        }
        outf.close();

        // Aproximación grosera de páginas escritas
        pageWrites += (long long)(entries.size() / 100) + 1;
    }

public:
    // ------------------------------
    // Range Search con B+-tree y Lemmas 4.1, 4.3, 4.5
    // ------------------------------

    // ------------------------------
// Range Search con B+-tree y Lemmas 4.1, 4.3, 4.5 (CORREGIDO)
// ------------------------------
void rangeSearch(int qId, double R, std::vector<int>& out) const {
    using clock = std::chrono::high_resolution_clock;
    auto t0 = clock::now();
    out.clear();

    if (!db || n == 0 || P == 0 || pivots.empty()) {
        auto t1 = clock::now();
        queryTime += std::chrono::duration_cast<std::chrono::microseconds>(t1 - t0).count();
        return;
    }

    // Distancias q → pivotes (cuentan en compDist)
    std::vector<double> dq(P);
    for (int j = 0; j < P; ++j) {
        dq[j] = distObj(qId, pivots[j]);
    }

    // Recorremos clusters
    for (const auto& node : nodes) {
        // --- PODA DE CLUSTER (LB conservador tipo Lemma 4.1) ---
        double clusterLB = 0.0;
        for (int j = 0; j < P; ++j) {
            // distancia mínima entre dq[j] y el intervalo [minDist,maxDist]
            if (dq[j] < node.minDist[j]) {
                clusterLB = std::max(clusterLB, node.minDist[j] - dq[j]);
            } else if (dq[j] > node.maxDist[j]) {
                clusterLB = std::max(clusterLB, dq[j] - node.maxDist[j]);
            }
        }
        if (clusterLB > R) {
            // bola B(q,R) no puede intersectar ningún objeto del cluster
            continue;
        }

        // Cluster no podado → cuenta como una página lógica
        pageReads++;

        // Buscamos entradas en el rango de keys del cluster
        auto itLow  = btreeIndex.lower_bound(node.minkey);
        auto itHigh = btreeIndex.upper_bound(node.maxkey);

        for (auto it = itLow; it != itHigh; ++it) {
            for (const auto& entry : it->second) {
                // Lemma 4.5: validación directa
                bool validated = false;
                for (int j = 0; j < P && !validated; ++j) {
                    if (entry.dists[j] <= R - dq[j]) {
                        validated = true;
                        out.push_back(entry.id);
                    }
                }
                if (validated) continue;

                // Lemma 4.3: pruning
                bool objectPruned = false;
                for (int j = 0; j < P && !objectPruned; ++j) {
                    if (std::fabs(entry.dists[j] - dq[j]) > R) {
                        objectPruned = true;
                    }
                }
                if (objectPruned) continue;

                // Caso ambiguo: distancia real d(q,o)
                double d = distObj(qId, entry.id);
                if (d <= R) {
                    out.push_back(entry.id);
                }
            }
        }
    }

    auto t1 = clock::now();
    queryTime += std::chrono::duration_cast<std::chrono::microseconds>(t1 - t0).count();
}

    // ------------------------------
    // k-NN Search con Best-First Traversal
    // ------------------------------
    void knnSearch(int qId, int k,
                   std::vector<std::pair<double,int>>& out) const {
        using clock = std::chrono::high_resolution_clock;
        auto t0 = clock::now();
        out.clear();

        if (!db || n == 0 || P == 0 || pivots.empty() || k <= 0) {
            auto t1 = clock::now();
            queryTime += std::chrono::duration_cast<std::chrono::microseconds>(t1 - t0).count();
            return;
        }

        // Precompute distances from q to pivots
        std::vector<double> dq(P);
        for (int j = 0; j < P; ++j) {
            dq[j] = distObj(qId, pivots[j]);
        }

        // Best-first traversal: priority queue of clusters by lower bound
        struct ClusterCandidate {
            int    nodeIdx;
            double lowerBound;
            bool operator>(const ClusterCandidate& o) const {
                return lowerBound > o.lowerBound;
            }
        };

        std::priority_queue<
            ClusterCandidate,
            std::vector<ClusterCandidate>,
            std::greater<ClusterCandidate>
        > pq;

        // Compute lower bound for each cluster and add to queue
        for (size_t i = 0; i < nodes.size(); ++i) {
            const auto& node = nodes[i];

            // Lower bound: max_j max(0, minDist[j] - dq[j], dq[j] - maxDist[j])
            double lb = 0.0;
            for (int j = 0; j < P; ++j) {
                double delta = 0.0;
                if (dq[j] < node.minDist[j]) {
                    delta = node.minDist[j] - dq[j];
                } else if (dq[j] > node.maxDist[j]) {
                    delta = dq[j] - node.maxDist[j];
                }
                if (delta > lb) lb = delta;
            }

            pq.push({(int)i, lb});
        }

        // k-NN heap (max-heap de k mejores)
        std::priority_queue<std::pair<double,int>> knnHeap;
        double radiusK = std::numeric_limits<double>::infinity();

        while (!pq.empty()) {
            auto cand = pq.top();
            pq.pop();

            // Pruning: si el LB del cluster >= radio actual, terminamos
            if ((int)knnHeap.size() == k && cand.lowerBound >= radiusK) {
                break;
            }

            const auto& node = nodes[cand.nodeIdx];
            pageReads++;

            auto itLow  = btreeIndex.lower_bound(node.minkey);
            auto itHigh = btreeIndex.upper_bound(node.maxkey);

            for (auto it = itLow; it != itHigh; ++it) {
                for (const auto& entry : it->second) {
                    // Pruning rápido usando Lemma 4.3 con radioK
                    if (std::isfinite(radiusK)) {
                        bool pruned = false;
                        for (int j = 0; j < P && !pruned; ++j) {
                            if (std::fabs(entry.dists[j] - dq[j]) > radiusK) {
                                pruned = true;
                            }
                        }
                        if (pruned) continue;
                    }

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

        // Extraer resultados ordenados por distancia ascendente
        while (!knnHeap.empty()) {
            out.push_back(knnHeap.top());
            knnHeap.pop();
        }
        std::reverse(out.begin(), out.end());

        auto t1 = clock::now();
        queryTime += std::chrono::duration_cast<std::chrono::microseconds>(t1 - t0).count();
    }
};

#endif // MINDEX_IMPROVED_HPP
