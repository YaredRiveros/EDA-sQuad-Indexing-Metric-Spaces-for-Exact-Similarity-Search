// omnirtree.hpp - OmniR-tree adaptado para ObjectDB y benchmarking
// Basado en Omni-family (Chen et al. 2022)
// Implementa pivot mapping + R-tree indexing con RAF simulado

#pragma once
#include "../../objectdb.hpp"
#include <bits/stdc++.h>
#include <fstream>

using namespace std;

/*** -----------------------------
    RAF (Random Access File) simulator
    ----------------------------- ***/
class RAF {
    string filename;
    unordered_map<int, streampos> offsets;
    
public:
    RAF(const string& fname) : filename(fname) {}
    
    void clear() {
        offsets.clear();
        ofstream ofs(filename, ios::binary | ios::trunc);
        ofs.close();
    }
    
    streampos append(int objId, const vector<double>& data) {
        ofstream ofs(filename, ios::binary | ios::app);
        streampos pos = ofs.tellp();
        
        // Write: objId (4 bytes), size (8 bytes), data (size * 8 bytes)
        ofs.write((const char*)&objId, sizeof(objId));
        uint64_t sz = data.size();
        ofs.write((const char*)&sz, sizeof(sz));
        ofs.write((const char*)data.data(), sz * sizeof(double));
        
        ofs.close();
        offsets[objId] = pos;
        return pos;
    }
    
    vector<double> read(int objId) {
        if (offsets.find(objId) == offsets.end()) {
            throw runtime_error("RAF: object not found");
        }
        
        ifstream ifs(filename, ios::binary);
        ifs.seekg(offsets[objId]);
        
        int id;
        uint64_t sz;
        ifs.read((char*)&id, sizeof(id));
        ifs.read((char*)&sz, sizeof(sz));
        
        vector<double> data(sz);
        ifs.read((char*)data.data(), sz * sizeof(double));
        ifs.close();
        
        return data;
    }
};

/*** -----------------------------
    MBB (Minimum Bounding Box)
    ----------------------------- ***/
struct MBB {
    vector<double> low;
    vector<double> high;
    
    MBB() {}
    
    MBB(size_t dim) {
        low.assign(dim, numeric_limits<double>::infinity());
        high.assign(dim, -numeric_limits<double>::infinity());
    }
    
    void expandWithPoint(const vector<double>& p) {
        if (low.empty()) {
            low = p;
            high = p;
            return;
        }
        for (size_t i = 0; i < p.size(); i++) {
            low[i] = min(low[i], p[i]);
            high[i] = max(high[i], p[i]);
        }
    }
    
    void expandWithMBB(const MBB& m) {
        if (low.empty()) {
            low = m.low;
            high = m.high;
            return;
        }
        for (size_t i = 0; i < low.size(); i++) {
            low[i] = min(low[i], m.low[i]);
            high[i] = max(high[i], m.high[i]);
        }
    }
    
    size_t dim() const { return low.size(); }
    
    // Intersección con hiper-rectángulo de búsqueda: [q_i - r, q_i + r]
    bool intersectsHyperRect(const vector<double>& qMap, double r) const {
        for (size_t i = 0; i < qMap.size(); i++) {
            double a = qMap[i] - r;
            double b = qMap[i] + r;
            if (high[i] < a || low[i] > b) return false;
        }
        return true;
    }
    
    // Lower bound usando triangular inequality en espacio pivotado
    double lowerBoundToQuery(const vector<double>& qMap) const {
        double lb = 0.0;
        for (size_t i = 0; i < qMap.size(); i++) {
            double qv = qMap[i];
            double lo = low[i];
            double hi = high[i];
            double md = 0.0;
            
            if (qv < lo) md = lo - qv;
            else if (qv > hi) md = qv - hi;
            
            lb = max(lb, md);
        }
        return lb;
    }
    
    double volume() const {
        double vol = 1.0;
        for (size_t i = 0; i < low.size(); i++) {
            vol *= max(1e-12, high[i] - low[i]);
        }
        return vol;
    }
};

/*** -----------------------------
    R-tree Node & Entry
    ----------------------------- ***/
struct RTreeEntry {
    MBB box;
    bool isLeafEntry;
    
    // Leaf entry
    int objectId;
    vector<double> mappedPoint;
    
    // Internal entry
    struct RTreeNode* child;
    
    RTreeEntry() : isLeafEntry(true), objectId(-1), child(nullptr) {}
};

struct RTreeNode {
    bool isLeaf;
    vector<RTreeEntry> entries;
    
    RTreeNode(bool leaf = false) : isLeaf(leaf) {}
};

/*** -----------------------------
    R-tree implementation
    ----------------------------- ***/
class RTree {
    size_t maxEntries;
    size_t minEntries;
    RTreeNode* root;
    
    void freeNode(RTreeNode* n) {
        if (!n) return;
        if (!n->isLeaf) {
            for (auto& e : n->entries) freeNode(e.child);
        }
        delete n;
    }
    
    MBB computeNodeMBB(RTreeNode* node) {
        if (node->entries.empty()) return MBB();
        MBB m(node->entries[0].box.dim());
        for (auto& e : node->entries) {
            m.expandWithMBB(e.box);
        }
        return m;
    }
    
    vector<double> center(const MBB& m) {
        vector<double> c(m.dim());
        for (size_t i = 0; i < m.dim(); i++) {
            c[i] = (m.low[i] + m.high[i]) / 2.0;
        }
        return c;
    }
    
    pair<RTreeNode*, RTreeNode*> splitNode(RTreeNode* node) {
        RTreeNode* n1 = new RTreeNode(node->isLeaf);
        RTreeNode* n2 = new RTreeNode(node->isLeaf);
        
        size_t n = node->entries.size();
        if (n == 0) return {n1, n2};
        
        // Linear split: pick two seeds with max separation
        double bestDist = -1;
        size_t s1 = 0, s2 = 0;
        for (size_t i = 0; i < n; i++) {
            for (size_t j = i + 1; j < n; j++) {
                vector<double> c1 = center(node->entries[i].box);
                vector<double> c2 = center(node->entries[j].box);
                double dist = 0;
                for (size_t k = 0; k < c1.size(); k++) {
                    double d = c1[k] - c2[k];
                    dist += d * d;
                }
                if (dist > bestDist) {
                    bestDist = dist;
                    s1 = i;
                    s2 = j;
                }
            }
        }
        
        n1->entries.push_back(node->entries[s1]);
        n2->entries.push_back(node->entries[s2]);
        
        // Assign rest by minimal enlargement
        for (size_t i = 0; i < n; i++) {
            if (i == s1 || i == s2) continue;
            
            RTreeEntry& e = node->entries[i];
            MBB mb1 = computeNodeMBB(n1);
            MBB mb2 = computeNodeMBB(n2);
            
            MBB m1 = mb1; m1.expandWithMBB(e.box);
            MBB m2 = mb2; m2.expandWithMBB(e.box);
            
            double area1 = m1.volume() - mb1.volume();
            double area2 = m2.volume() - mb2.volume();
            
            if (area1 < area2) n1->entries.push_back(e);
            else n2->entries.push_back(e);
        }
        
        delete node;
        return {n1, n2};
    }
    
    void insertRec(RTreeNode* node, const RTreeEntry& e) {
        if (node->isLeaf) {
            node->entries.push_back(e);
        } else {
            // Choose child minimizing area enlargement
            double bestInc = numeric_limits<double>::infinity();
            size_t bestIdx = 0;
            
            for (size_t i = 0; i < node->entries.size(); i++) {
                MBB oldBox = node->entries[i].box;
                MBB merged = oldBox;
                merged.expandWithMBB(e.box);
                
                double oldArea = oldBox.volume();
                double newArea = merged.volume();
                double inc = newArea - oldArea;
                
                if (inc < bestInc) {
                    bestInc = inc;
                    bestIdx = i;
                }
            }
            
            insertRec(node->entries[bestIdx].child, e);
            node->entries[bestIdx].box.expandWithMBB(e.box);
            
            // Split if needed
            if (node->entries[bestIdx].child->entries.size() > maxEntries) {
                auto [n1, n2] = splitNode(node->entries[bestIdx].child);
                
                RTreeEntry e1, e2;
                e1.isLeafEntry = false;
                e1.child = n1;
                e1.box = computeNodeMBB(n1);
                
                e2.isLeafEntry = false;
                e2.child = n2;
                e2.box = computeNodeMBB(n2);
                
                node->entries.erase(node->entries.begin() + bestIdx);
                node->entries.insert(node->entries.begin() + bestIdx, e2);
                node->entries.insert(node->entries.begin() + bestIdx, e1);
            }
        }
    }
    
public:
    RTree(size_t maxE = 16) : maxEntries(maxE) {
        minEntries = maxEntries / 2;
        root = new RTreeNode(true);
    }
    
    ~RTree() { freeNode(root); }
    
    void insert(const vector<double>& mappedPoint, int objectId) {
        RTreeEntry e;
        e.isLeafEntry = true;
        e.objectId = objectId;
        e.mappedPoint = mappedPoint;
        e.box = MBB(mappedPoint.size());
        e.box.expandWithPoint(mappedPoint);
        
        insertRec(root, e);
        
        if (root->entries.size() > maxEntries) {
            auto [n1, n2] = splitNode(root);
            RTreeNode* newRoot = new RTreeNode(false);
            
            RTreeEntry e1, e2;
            e1.isLeafEntry = false;
            e1.child = n1;
            e1.box = computeNodeMBB(n1);
            
            e2.isLeafEntry = false;
            e2.child = n2;
            e2.box = computeNodeMBB(n2);
            
            newRoot->entries.push_back(e1);
            newRoot->entries.push_back(e2);
            root = newRoot;
        }
    }
    
    void rangeRec(RTreeNode* node, const vector<double>& qMap, double r, vector<int>& res) {
        for (auto& e : node->entries) {
            if (!e.box.intersectsHyperRect(qMap, r)) {
                continue; // Prune by Lemma 4.1
            }
            
            if (node->isLeaf) {
                res.push_back(e.objectId);
            } else {
                rangeRec(e.child, qMap, r, res);
            }
        }
    }
    
    vector<int> rangeQuery(const vector<double>& qMap, double r) {
        vector<int> result;
        rangeRec(root, qMap, r, result);
        return result;
    }
    
    struct NodePQ {
        RTreeNode* node;
        double lb;
        bool operator<(const NodePQ& o) const { return lb > o.lb; }
    };
    
    struct EntryPQ {
        int oid;
        double lb;
        bool operator<(const EntryPQ& o) const { return lb > o.lb; }
    };
    
    vector<pair<double, int>> knnQuery(const vector<double>& qMap, size_t k,
                                        function<double(int)> verifyDistFunc) {
        priority_queue<NodePQ> nodePQ;
        priority_queue<EntryPQ> leafPQ;
        vector<pair<double, int>> result;
        
        nodePQ.push({root, root->entries.empty() ? 0.0 : computeNodeMBB(root).lowerBoundToQuery(qMap)});
        
        while ((!nodePQ.empty() || !leafPQ.empty()) && result.size() < k) {
            if (!leafPQ.empty()) {
                double bestLeafLB = leafPQ.top().lb;
                if (!nodePQ.empty() && nodePQ.top().lb < bestLeafLB) {
                    auto nitem = nodePQ.top();
                    nodePQ.pop();
                    RTreeNode* node = nitem.node;
                    
                    if (node->isLeaf) {
                        for (auto& e : node->entries) {
                            double lb = e.box.lowerBoundToQuery(qMap);
                            leafPQ.push({e.objectId, lb});
                        }
                    } else {
                        for (auto& e : node->entries) {
                            double lb = e.box.lowerBoundToQuery(qMap);
                            nodePQ.push({e.child, lb});
                        }
                    }
                    continue;
                }
            }
            
            if (!leafPQ.empty()) {
                auto ent = leafPQ.top();
                leafPQ.pop();
                double reald = verifyDistFunc(ent.oid);
                result.push_back({reald, ent.oid});
                if (result.size() == k) break;
                continue;
            }
            
            if (!nodePQ.empty()) {
                auto nitem = nodePQ.top();
                nodePQ.pop();
                RTreeNode* node = nitem.node;
                
                if (node->isLeaf) {
                    for (auto& e : node->entries) {
                        double lb = e.box.lowerBoundToQuery(qMap);
                        leafPQ.push({e.objectId, lb});
                    }
                } else {
                    for (auto& e : node->entries) {
                        double lb = e.box.lowerBoundToQuery(qMap);
                        nodePQ.push({e.child, lb});
                    }
                }
            }
        }
        
        sort(result.begin(), result.end());
        if (result.size() > k) result.resize(k);
        return result;
    }
};

/*** -----------------------------
    OmniRTree wrapper
    ----------------------------- ***/
class OmniRTree {
    ObjectDB* db;
    int num_pivots;
    vector<int> pivots;
    RTree rtree;
    RAF raf;
    
    // Counters
    long long compDist;
    long long pageReads;
    
    // Cache de vectores mapeados
    unordered_map<int, vector<double>> mappedCache;
    
public:
    OmniRTree(ObjectDB* database, int l_pivots, size_t rtreeNodeCap = 32)
        : db(database), num_pivots(l_pivots), rtree(rtreeNodeCap),
          raf("omni_raf.bin"), compDist(0), pageReads(0) {}
    
    void build(const string& indexFile) {
        raf.clear();
        pivots.clear();
        mappedCache.clear();
        
        // Selección aleatoria de pivotes
        vector<int> allIds;
        for (int i = 0; i < db->size(); i++) {
            allIds.push_back(i);
        }
        
        random_shuffle(allIds.begin(), allIds.end());
        for (int i = 0; i < num_pivots && i < (int)allIds.size(); i++) {
            pivots.push_back(allIds[i]);
        }
        
        cout << "[BUILD] Pivotes seleccionados: " << pivots.size() << "\n";
        cout << "[BUILD] Iniciando mapeo e inserción de " << db->size() << " objetos...\n" << flush;
        
        // Construir índice: mapear cada objeto y agregarlo al R-tree
        for (int objId = 0; objId < db->size(); objId++) {
            vector<double> mapped = mapObject(objId);
            mappedCache[objId] = mapped;
            
            // Simular escritura a RAF (no estrictamente necesario para búsqueda)
            // pero lo hacemos para simular el overhead de disco
            vector<double> dummy(1, 0.0);
            raf.append(objId, dummy);
            
            rtree.insert(mapped, objId);
            
            if ((objId + 1) % 1000 == 0 || objId == 0) {
                cout << "  Indexados " << (objId + 1) << " objetos (" 
                     << (int)((objId + 1) * 100.0 / db->size()) << "%)\n" << flush;
            }
        }
        cout << "\n[BUILD] Total objetos indexados: " << db->size() << " (100%)\n";
    }
    
    vector<double> mapObject(int objId) {
        vector<double> mapped(pivots.size());
        for (size_t i = 0; i < pivots.size(); i++) {
            mapped[i] = db->distance(objId, pivots[i]);
        }
        return mapped;
    }
    
    void rangeSearch(int queryId, double radius, vector<int>& result) {
        result.clear();
        compDist = 0;
        pageReads = 0;
        
        // Mapear query
        vector<double> qMap = mapObject(queryId);
        compDist += pivots.size(); // Distancias a pivotes
        
        // Obtener candidatos del R-tree
        vector<int> candidates = rtree.rangeQuery(qMap, radius);
        pageReads += candidates.size(); // Simular acceso a RAF
        
        // Verificar candidatos
        for (int candId : candidates) {
            double d = db->distance(queryId, candId);
            compDist++;
            
            if (d <= radius) {
                result.push_back(candId);
            }
        }
    }
    
    void knnSearch(int queryId, int k, vector<pair<double, int>>& result) {
        result.clear();
        compDist = 0;
        pageReads = 0;
        
        // Mapear query
        vector<double> qMap = mapObject(queryId);
        compDist += pivots.size();
        
        // Función de verificación que cuenta distancias
        auto verifyFunc = [&](int oid) -> double {
            compDist++;
            pageReads++; // Simular acceso a RAF
            return db->distance(queryId, oid);
        };
        
        result = rtree.knnQuery(qMap, k, verifyFunc);
    }
    
    void clear_counters() {
        compDist = 0;
        pageReads = 0;
    }
    
    long long get_compDist() const { return compDist; }
    long long get_pageReads() const { return pageReads; }
};
