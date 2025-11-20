// mbpt.hpp
// MB+-tree - Implementación completa según Chen et al.
// Hash partitioning con ρ-split functions, B+-tree ordenado, y RAF
// Cumple con Definition 4.3 y todas las características del paper

#ifndef MBPT_DISK_HPP
#define MBPT_DISK_HPP

#include "../../objectdb.hpp"
#include <vector>
#include <algorithm>
#include <random>
#include <limits>
#include <fstream>
#include <cmath>
#include <cstdint>
#include <chrono>
#include <stdexcept>
#include <queue>
#include <map>
#include <set>
#include <iostream>

/*
 ======================================================================
                    MB+-tree (Chen et al.)
 ======================================================================
  ✔ Hash partitioning con ρ-split function (Definition 4.3)
  ✔ Block tree: almacena center c y medium distance dmed
  ✔ B+-tree ordenado: indexa keys compuestas (pk || dk)
  ✔ RAF: Random Access File para objetos
  ✔ MRQ: Usa Lemma 4.7 y búsqueda por rango de distance keys
  ✔ MkNNQ: Estrategia 3 - encuentra k candidatos por keys, luego MRQ
  ✔ Métricas: compDist, pageReads, pageWrites, queryTime
 ======================================================================
*/

class MBPT_Disk {
public:
    static constexpr int DEFAULT_PAGE_BYTES = 4096;
    static constexpr int DEFAULT_LEAF_CAP = 50;

    // Entrada en RAF
    struct RAFEntry {
        int32_t id;
        uint64_t key;  // partition_key || distance_key
    };

    // Nodo del block tree (almacena info de partición)
    struct BlockNode {
        bool isLeaf;
        int level;
        int blockValue;           // partition key acumulada (path in tree)
        int center = -1;          // partition center c
        double dmed = 0.0;        // medium distance
        double rho = 0.0;         // ρ parameter para ρ-split
        double maxDist = 0.0;     // max distance to center (para normalización)
        int left = -1;            // child for d(o,c) ∈ [0, dmed-ρ]
        int right = -1;           // child for d(o,c) ∈ (dmed-ρ, ∞)
        std::vector<int> objects; // temporal durante construcción
        int leafIdx = -1;
    };

    struct LeafInfo {
        int32_t blockValue;
        int64_t offset;
        int32_t count;
    };

private:
    const ObjectDB* db;
    int n;
    int pageBytes;
    int leafCap;
    int pagesPerNode;
    double rho;  // ρ global para todas las ρ-split functions

    // Estructuras de datos
    std::vector<BlockNode> blockNodes;
    std::vector<LeafInfo> leaves;
    std::vector<RAFEntry> rafEntries;  // RAF in-memory (sorted by key)
    
    // B+-tree: map ordenado key -> object ids
    // IMPORTANTE: Usamos multimap porque múltiples objetos pueden compartir la misma key
    std::multimap<uint64_t, int32_t> btreeIndex;

    // Archivos
    std::string rafPath;
    std::string idxPath;
    mutable FILE* rafFp = nullptr;

    // Métricas
    mutable long long compDist = 0;
    mutable long long pageReads = 0;
    mutable long long pageWrites = 0;
    mutable long long queryTime = 0;

public:
    MBPT_Disk(const ObjectDB* db_, double rho_ = 0.0, int pageBytes_ = DEFAULT_PAGE_BYTES, int leafCap_ = DEFAULT_LEAF_CAP)
    : db(db_), n(db_ ? db_->size() : 0), pageBytes(pageBytes_), leafCap(leafCap_), rho(rho_)
    {
        if (!db) throw std::runtime_error("DB null");
        pagesPerNode = std::max<int>(1, pageBytes / 4096);
    }

    ~MBPT_Disk() {
        if (rafFp) fclose(rafFp);
    }

    void clear_counters() const { compDist = pageReads = pageWrites = queryTime = 0; }
    long long get_compDist()  const { return compDist; }
    long long get_pageReads() const { return pageReads; }
    long long get_pageWrites() const { return pageWrites; }
    long long get_queryTime() const { return queryTime; }
    
    // DEBUG: métodos temporales para diagnóstico
    size_t debug_get_blockNodes_size() const { return blockNodes.size(); }
    size_t debug_get_leaves_size() const { return leaves.size(); }
    void debug_print_root() const {
        if (!blockNodes.empty()) {
            const auto& root = blockNodes[0];
            std::cout << "  Root: isLeaf=" << root.isLeaf << " center=" << root.center 
                      << " left=" << root.left << " right=" << root.right << std::endl;
        }
    }

private:
    inline double distObj(int a, int b) const {
        compDist++;
        return db->distance(a,b);
    }

    // Normaliza distancia a Ks bits (para distance key)
    uint32_t normalizeDistance(double dist, double maxDist, int bits = 16) const {
        if (maxDist <= 0.0) return 0;
        double normalized = dist / maxDist;
        if (normalized < 0.0) normalized = 0.0;
        if (normalized > 1.0) normalized = 1.0;
        uint32_t maxVal = (1u << bits) - 1;
        return (uint32_t)(normalized * maxVal);
    }

    // Compone key: partition_key (Kb bits) || distance_key (Kd bits)
    uint64_t composeKey(int partitionKey, uint32_t distanceKey, int pkBits = 32, int dkBits = 16) const {
        uint64_t pk = (uint64_t)partitionKey & ((1ull << pkBits) - 1);
        uint64_t dk = (uint64_t)distanceKey & ((1ull << dkBits) - 1);
        return (pk << dkBits) | dk;
    }

    // Extrae partition key de una key compuesta
    int extractPartitionKey(uint64_t key, int dkBits = 16) const {
        return (int)(key >> dkBits);
    }

    // Extrae distance key de una key compuesta
    uint32_t extractDistanceKey(uint64_t key, int dkBits = 16) const {
        return (uint32_t)(key & ((1ull << dkBits) - 1));
    }

public:
    // =================================================
    // BUILD
    // =================================================
    void build(const std::string& base) {
        using clock = std::chrono::high_resolution_clock;
        auto t0 = clock::now();

        blockNodes.clear();
        leaves.clear();
        rafEntries.clear();
        btreeIndex.clear();
        pageWrites = 0;

        rafPath = base + ".mbpt_raf";
        idxPath = base + ".mbpt_index";

        std::vector<int> objs(n);
        for (int i = 0; i < n; i++) objs[i] = i;

        // Crear raíz del block tree
        BlockNode root;
        root.isLeaf = false;
        root.level = 0;
        root.blockValue = 0;
        root.objects = objs;
        blockNodes.push_back(root);

        // Construir block tree recursivamente con ρ-split
        buildBlockTree(0);

        // Generar RAF entries y B+-tree index
        for (size_t i = 0; i < blockNodes.size(); i++) {
            BlockNode& B = blockNodes[i];
            if (!B.isLeaf) continue;

            LeafInfo L;
            L.blockValue = B.blockValue;
            L.offset = rafEntries.size();
            L.count = B.objects.size();

            for (int id : B.objects) {
                double dist = (B.center >= 0) ? distObj(id, B.center) : 0.0;
                uint32_t dk = normalizeDistance(dist, B.maxDist);
                uint64_t key = composeKey(B.blockValue, dk);
                
                RAFEntry entry;
                entry.id = id;
                entry.key = key;
                rafEntries.push_back(entry);
                
                // Multimap permite múltiples valores por key
                btreeIndex.insert({key, id});
            }

            int leafIdx = leaves.size();
            leaves.push_back(L);
            B.leafIdx = leafIdx;
            B.objects.clear();
        }

        // Ordenar RAF entries por key
        std::sort(rafEntries.begin(), rafEntries.end(), 
            [](const RAFEntry& a, const RAFEntry& b) { return a.key < b.key; });

        // Escribir RAF a disco
        {
            std::ofstream out(rafPath, std::ios::binary | std::ios::trunc);
            if (!out) throw std::runtime_error("cannot write RAF file");
            if (!rafEntries.empty())
                out.write(reinterpret_cast<const char*>(rafEntries.data()), 
                         rafEntries.size() * sizeof(RAFEntry));
        }
        pageWrites += (long long)rafEntries.size() / (pageBytes / sizeof(RAFEntry)) + 1;

        // Escribir index (block tree)
        {
            std::ofstream out(idxPath, std::ios::binary | std::ios::trunc);
            if (!out) throw std::runtime_error("cannot write index file");
            
            int32_t numNodes = blockNodes.size();
            out.write((char*)&numNodes, sizeof(numNodes));
            out.write((char*)&rho, sizeof(rho));
            out.write((char*)&pageBytes, sizeof(pageBytes));
            out.write((char*)&n, sizeof(n));

            for (const auto& B : blockNodes) {
                uint8_t isLeaf = B.isLeaf ? 1 : 0;
                out.write((char*)&isLeaf, sizeof(isLeaf));
                out.write((char*)&B.level, sizeof(B.level));
                out.write((char*)&B.blockValue, sizeof(B.blockValue));
                out.write((char*)&B.center, sizeof(B.center));
                out.write((char*)&B.dmed, sizeof(B.dmed));
                out.write((char*)&B.rho, sizeof(B.rho));
                out.write((char*)&B.maxDist, sizeof(B.maxDist));
                out.write((char*)&B.left, sizeof(B.left));
                out.write((char*)&B.right, sizeof(B.right));
                out.write((char*)&B.leafIdx, sizeof(B.leafIdx));
            }
        }
        pageWrites += (long long)blockNodes.size() * pagesPerNode;

        // Abrir RAF para queries
        rafFp = std::fopen(rafPath.c_str(), "rb");
        if (!rafFp) throw std::runtime_error("cannot reopen RAF");

        auto t1 = clock::now();
        double ms = std::chrono::duration<double, std::milli>(t1 - t0).count();
        std::cerr << "[MBPT] Build OK (" << ms << " ms) blocks=" << blockNodes.size() 
                  << " leaves=" << leaves.size() << " raf_entries=" << rafEntries.size() << "\n";
    }

private:
    // Construcción recursiva del block tree con ρ-split
    void buildBlockTree(int nodeIdx) {
        // IMPORTANTE: NO usar referencias a blockNodes[nodeIdx] porque
        // push_back puede realocar el vector e invalidar referencias
        
        if ((int)blockNodes[nodeIdx].objects.size() <= leafCap) {
            blockNodes[nodeIdx].isLeaf = true;
            // Seleccionar center para hoja
            blockNodes[nodeIdx].center = selectCenter(blockNodes[nodeIdx].objects);
            blockNodes[nodeIdx].maxDist = computeMaxDist(blockNodes[nodeIdx].objects, blockNodes[nodeIdx].center);
            return;
        }

        // Seleccionar partition center
        int center = selectCenter(blockNodes[nodeIdx].objects);
        blockNodes[nodeIdx].center = center;
        blockNodes[nodeIdx].rho = rho;

        // Calcular distancias y encontrar mediana
        std::vector<std::pair<double, int>> distances;
        distances.reserve(blockNodes[nodeIdx].objects.size());
        double maxD = 0.0;
        
        for (int id : blockNodes[nodeIdx].objects) {
            double d = distObj(id, center);
            distances.emplace_back(d, id);
            if (d > maxD) maxD = d;
        }
        
        std::sort(distances.begin(), distances.end());
        double dmed = distances[distances.size() / 2].first;
        blockNodes[nodeIdx].dmed = dmed;
        blockNodes[nodeIdx].maxDist = maxD;

        // ρ-split: dividir según Definition 4.3
        // Bucket 0: d(o,c) ∈ [0, dmed-ρ]
        // Bucket 1: d(o,c) ∈ (dmed-ρ, ∞)
        std::vector<int> leftObjs, rightObjs;
        double threshold = dmed - rho;
        
        for (const auto& [d, id] : distances) {
            if (d <= threshold) {
                leftObjs.push_back(id);
            } else {
                rightObjs.push_back(id);
            }
        }

        // Si no se puede dividir, convertir en hoja
        if (leftObjs.empty() || rightObjs.empty()) {
            blockNodes[nodeIdx].isLeaf = true;
            return;
        }

        // Crear nodos hijos - guardar valores antes de push_back
        int currentBlockValue = blockNodes[nodeIdx].blockValue;
        int currentLevel = blockNodes[nodeIdx].level;
        
        int leftIdx = blockNodes.size();
        BlockNode leftNode;
        leftNode.isLeaf = false;
        leftNode.level = currentLevel + 1;
        leftNode.blockValue = (currentBlockValue << 1) | 0;
        leftNode.objects = std::move(leftObjs);
        blockNodes.push_back(leftNode);

        int rightIdx = blockNodes.size();
        BlockNode rightNode;
        rightNode.isLeaf = false;
        rightNode.level = currentLevel + 1;
        rightNode.blockValue = (currentBlockValue << 1) | 1;
        rightNode.objects = std::move(rightObjs);
        blockNodes.push_back(rightNode);

        blockNodes[nodeIdx].left = leftIdx;
        blockNodes[nodeIdx].right = rightIdx;

        // Recursión
        buildBlockTree(leftIdx);
        buildBlockTree(rightIdx);

        blockNodes[nodeIdx].objects.clear();
    }

    // Selecciona center heurísticamente
    int selectCenter(const std::vector<int>& objs) const {
        if (objs.empty()) return -1;
        
        static thread_local std::mt19937_64 rng{std::random_device{}()};
        std::uniform_int_distribution<size_t> dist(0, objs.size() - 1);
        
        // Heurística: elegir objeto aleatorio y buscar el más lejano
        int center = objs[dist(rng)];
        double maxD = -1.0;
        
        for (int id : objs) {
            double d = distObj(center, id);
            if (d > maxD) {
                maxD = d;
                center = id;
            }
        }
        
        return center;
    }

    double computeMaxDist(const std::vector<int>& objs, int center) const {
        if (center < 0 || objs.empty()) return 1.0;
        
        double maxD = 0.0;
        for (int id : objs) {
            double d = distObj(center, id);
            if (d > maxD) maxD = d;
        }
        return maxD > 0.0 ? maxD : 1.0;
    }

public:
    // =================================================
    // RANGE SEARCH (MRQ)
    // =================================================
    void rangeSearch(int qId, double R, std::vector<int>& out) const {
        using clock = std::chrono::high_resolution_clock;
        auto t0 = clock::now();
        out.clear();

        // Atravesar block tree usando Lemma 4.7
        std::vector<int> candidateLeaves;
        traverseBlockTree(0, qId, R, candidateLeaves);

        // Para cada hoja candidata, buscar en B+-tree por rango de distance keys
        for (int leafIdx : candidateLeaves) {
            if (leafIdx < 0 || leafIdx >= (int)leaves.size()) continue;
            
            const LeafInfo& L = leaves[leafIdx];
            
            // Encontrar el BlockNode correspondiente
            int blockNodeIdx = -1;
            for (size_t i = 0; i < blockNodes.size(); i++) {
                if (blockNodes[i].isLeaf && blockNodes[i].leafIdx == leafIdx) {
                    blockNodeIdx = i;
                    break;
                }
            }
            
            if (blockNodeIdx < 0 || blockNodes[blockNodeIdx].center < 0) continue;
            
            const BlockNode& B = blockNodes[blockNodeIdx];

            // Calcular distancia query-center
            double dqc = distObj(qId, B.center);
            
            // Rango de distance keys: [dqc - R, dqc + R]
            double minDist = std::max(0.0, dqc - R);
            double maxDist = dqc + R;
            
            uint32_t minDK = normalizeDistance(minDist, B.maxDist);
            uint32_t maxDK = normalizeDistance(maxDist, B.maxDist);
            
            uint64_t minKey = composeKey(B.blockValue, minDK);
            uint64_t maxKey = composeKey(B.blockValue, maxDK);

            // Búsqueda por rango en B+-tree
            auto itLow = btreeIndex.lower_bound(minKey);
            auto itHigh = btreeIndex.upper_bound(maxKey);
            
            pageReads += pagesPerNode;

            for (auto it = itLow; it != itHigh; ++it) {
                int candidateId = it->second;
                double d = distObj(qId, candidateId);
                if (d <= R) {
                    out.push_back(candidateId);
                }
            }
        }

        auto t1 = clock::now();
        queryTime += std::chrono::duration_cast<std::chrono::microseconds>(t1 - t0).count();
    }

    // =================================================
    // KNN SEARCH (MkNNQ) - Estrategia 3 del paper
    // =================================================
    void knnSearch(int qId, int k, std::vector<std::pair<double, int>>& out) const {
        using clock = std::chrono::high_resolution_clock;
        auto t0 = clock::now();
        out.clear();

        // Paso 1: Encontrar k candidatos NN según keys (no distancias reales)
        std::vector<std::pair<uint64_t, int>> candidates;
        findKCandidatesByKeys(qId, k, candidates);

        // Paso 2: Calcular NDk (distancia del k-ésimo candidato)
        double NDk = 0.0;
        if (!candidates.empty()) {
            std::vector<std::pair<double, int>> realDists;
            for (const auto& [key, id] : candidates) {
                double d = distObj(qId, id);
                realDists.emplace_back(d, id);
            }
            std::sort(realDists.begin(), realDists.end());
            if (realDists.size() >= (size_t)k) {
                NDk = realDists[k - 1].first;
            } else if (!realDists.empty()) {
                NDk = realDists.back().first;
            }
        }

        // Paso 3: Transformar a MRQ(q, NDk)
        std::vector<int> rangeResult;
        rangeSearch(qId, NDk, rangeResult);

        // Paso 4: Ordenar por distancia real y retornar top-k
        std::vector<std::pair<double, int>> finalResults;
        for (int id : rangeResult) {
            double d = distObj(qId, id);
            finalResults.emplace_back(d, id);
        }
        std::sort(finalResults.begin(), finalResults.end());
        
        for (size_t i = 0; i < finalResults.size() && i < (size_t)k; i++) {
            out.push_back(finalResults[i]);
        }

        auto t1 = clock::now();
        queryTime += std::chrono::duration_cast<std::chrono::microseconds>(t1 - t0).count();
    }

private:
    // Traversa block tree para encontrar hojas que intersectan (q, R)
    void traverseBlockTree(int nodeIdx, int qId, double R, std::vector<int>& outLeaves) const {
        const BlockNode& B = blockNodes[nodeIdx];
        
        if (B.isLeaf) {
            if (B.leafIdx >= 0) outLeaves.push_back(B.leafIdx);
            return;
        }

        if (B.center < 0) {
            // Conservador: visitar ambos hijos
            if (B.left >= 0) traverseBlockTree(B.left, qId, R, outLeaves);
            if (B.right >= 0) traverseBlockTree(B.right, qId, R, outLeaves);
            return;
        }

        double dqc = distObj(qId, B.center);
        double threshold = B.dmed - B.rho;

        // Lemma 4.7: decidir qué subtrees visitar
        // Left: d(o,c) ∈ [0, dmed-ρ]
        // Right: d(o,c) ∈ (dmed-ρ, ∞)
        
        // Para que la esfera B(q,R) intersecte con Left: dqc - R <= dmed - ρ
        // Para que la esfera B(q,R) intersecte con Right: dqc + R > dmed - ρ
        
        bool visitLeft = (dqc - R <= threshold);
        bool visitRight = (dqc + R > threshold);

        if (visitLeft && B.left >= 0) {
            traverseBlockTree(B.left, qId, R, outLeaves);
        }
        if (visitRight && B.right >= 0) {
            traverseBlockTree(B.right, qId, R, outLeaves);
        }
    }

private:
    // Encuentra k candidatos más cercanos según keys (sin calcular distancias reales)
    void findKCandidatesByKeys(int qId, int k, std::vector<std::pair<uint64_t, int>>& candidates) const {
        // Heurística: encontrar la hoja donde probablemente está q
        std::vector<int> nearLeaves;
        traverseBlockTree(0, qId, 0.0, nearLeaves);

        if (nearLeaves.empty()) {
            // Fallback: usar todas las hojas
            for (size_t i = 0; i < leaves.size(); i++) {
                nearLeaves.push_back(i);
            }
        }

        // Para cada hoja cercana, calcular la key que tendría q
        std::vector<std::pair<uint64_t, uint64_t>> queryKeys; // (key, distance to key)
        
        for (int leafIdx : nearLeaves) {
            if (leafIdx < 0 || leafIdx >= (int)leaves.size()) continue;
            
            const LeafInfo& L = leaves[leafIdx];
            
            // Encontrar el BlockNode correspondiente
            int blockNodeIdx = -1;
            for (size_t i = 0; i < blockNodes.size(); i++) {
                if (blockNodes[i].isLeaf && blockNodes[i].leafIdx == leafIdx) {
                    blockNodeIdx = i;
                    break;
                }
            }
            
            if (blockNodeIdx < 0 || blockNodes[blockNodeIdx].center < 0) continue;
            
            const BlockNode& B = blockNodes[blockNodeIdx];

            double dqc = distObj(qId, B.center);
            uint32_t dk = normalizeDistance(dqc, B.maxDist);
            uint64_t qkey = composeKey(B.blockValue, dk);
            
            queryKeys.emplace_back(qkey, 0);
        }

        if (queryKeys.empty()) return;

        // Buscar en B+-tree los k entries más cercanos a las query keys
        std::set<int> seenIds;
        
        for (const auto& [qkey, _] : queryKeys) {
            if ((int)candidates.size() >= k * 3) break;
            
            auto it = btreeIndex.lower_bound(qkey);
            if (it == btreeIndex.end() && !btreeIndex.empty()) {
                it = std::prev(btreeIndex.end());
            }
            
            // Añadir elemento inicial si existe
            if (it != btreeIndex.end() && seenIds.insert(it->second).second) {
                candidates.emplace_back(it->first, it->second);
            }
            
            // Expandir hacia adelante
            auto itForward = it;
            for (int i = 0; i < k && itForward != btreeIndex.end(); i++) {
                if (seenIds.insert(itForward->second).second) {
                    candidates.emplace_back(itForward->first, itForward->second);
                }
                ++itForward;
            }
            
            // Expandir hacia atrás
            auto itBackward = it;
            for (int i = 0; i < k && itBackward != btreeIndex.begin(); i++) {
                --itBackward;
                if (seenIds.insert(itBackward->second).second) {
                    candidates.emplace_back(itBackward->first, itBackward->second);
                }
            }
        }
    }
};

#endif // MBPT_DISK_HPP
