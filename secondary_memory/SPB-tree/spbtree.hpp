// spbtree.hpp
// Implementación del SPB-tree (Space-filling curve + Pivot + B+-tree)
// siguiendo la descripción de Chen et al. (SPB-tree).
//
// - Los pivotes se asumen preseleccionados (HFI) y cargados externamente.
// - Pivot mapping: φ(o) = <d(o, pi)> usando ObjectDB::distance.
// - SFC mapping: Z-order (Morton). El paper permite Z-order o Hilbert.
// - B+-tree con MBB en el espacio de pivotes.
// - MRQ: Range Query Algorithm (RQA) basado en Lemma 1 y 2.
// - MkNN: kNN Query Algorithm (NNA) basado en Lemma 3 y 4.

#pragma once
#include <bits/stdc++.h>
#include "../../objectdb.hpp"
#include "../../datasets/paths.hpp"

using namespace std;

/* -------------------------
   Data object and RAF
   ------------------------- */
struct DataObject {
    uint64_t id;
    vector<double> payload; // no se usa para distancias, solo para RAF si quieres
};

class RAF {
    static constexpr size_t PAGE_SIZE = 4096; // 4KB físicos

    string filename;

    // Fichero binario abierto persistentemente (lectura/escritura aleatoria)
    mutable fstream file;

    // Mapa objeto -> offset físico
    unordered_map<uint64_t, streampos> offsets;

    // Páginas físicas únicas tocadas en la query actual
    mutable unordered_set<uint64_t> pagesVisited;

    // Factor lógico de páginas (Chen: 40KB = 10×4KB para Color/Synthetic)
    size_t logicalPageFactor;

    void ensureOpenForRW() const {
        if (!file.is_open()) {
            // reabrir en modo lectura + escritura, sin truncar
            const_cast<RAF*>(this)->file.open(
                filename,
                ios::binary | ios::in | ios::out
            );
            if (!file.is_open()) {
                throw runtime_error("RAF: cannot reopen file " + filename);
            }
        }
    }

public:
    RAF(const string &fname, size_t logicalFactor = 1)
        : filename(fname), logicalPageFactor(logicalFactor)
    {
        // Crear/truncar el fichero y dejarlo abierto en RW
        file.open(filename, ios::binary | ios::in | ios::out | ios::trunc);
        if (!file.is_open()) {
            throw runtime_error("RAF: cannot open file " + filename);
        }
    }

    ~RAF() {
        if (file.is_open()) {
            file.close();
        }
    }

    // Permite reutilizar el mismo RAF al reconstruir el índice
    void resetFile() {
        if (file.is_open()) {
            file.close();
        }
        offsets.clear();
        pagesVisited.clear();
        file.open(filename, ios::binary | ios::in | ios::out | ios::trunc);
        if (!file.is_open()) {
            throw runtime_error("RAF: cannot reset file " + filename);
        }
    }

    // Escribe un objeto al final del fichero y devuelve el offset
    streampos append(const DataObject &o) {
        ensureOpenForRW();

        // Ir al final para append explícito
        file.seekp(0, ios::end);
        streampos pos = file.tellp();

        // Escribir (id, len, payload)
        file.write(reinterpret_cast<const char*>(&o.id), sizeof(o.id));
        uint64_t len = o.payload.size();
        file.write(reinterpret_cast<const char*>(&len), sizeof(len));
        if (len > 0) {
            file.write(reinterpret_cast<const char*>(o.payload.data()),
                       len * sizeof(double));
        }
        file.flush(); // forzar que se materialice en disco

        offsets[o.id] = pos;
        return pos;
    }

    // Lectura real desde disco + conteo de páginas físicas
    DataObject read(uint64_t id) const {
        ensureOpenForRW();

        auto it = offsets.find(id);
        if (it == offsets.end())
            throw runtime_error("RAF: id not found");

        long long off = static_cast<long long>(it->second);
        uint64_t pageId = static_cast<uint64_t>(
            off / static_cast<long long>(PAGE_SIZE)
        );
        pagesVisited.insert(pageId);

        // Posicionarse y leer registro
        file.seekg(it->second);
        if (!file.good()) {
            throw runtime_error("RAF: seekg failed");
        }

        DataObject o;
        file.read(reinterpret_cast<char*>(&o.id), sizeof(o.id));
        uint64_t len = 0;
        file.read(reinterpret_cast<char*>(&len), sizeof(len));

        if (!file.good()) {
            throw runtime_error("RAF: read header failed");
        }

        o.payload.assign(len, 0.0);
        if (len > 0) {
            file.read(reinterpret_cast<char*>(o.payload.data()),
                      len * sizeof(double));
            if (!file.good()) {
                throw runtime_error("RAF: read payload failed");
            }
        }

        return o;
    }

    // Páginas lógicas según Chen (ej. 10× para Color/Synthetic)
    long long get_pageReads() const {
        return static_cast<long long>(pagesVisited.size()) *
               static_cast<long long>(logicalPageFactor);
    }

    void clear_pageReads() {
        pagesVisited.clear();
    }
};

struct PivotTable {
    vector<DataObject> pivots; // solo usamos id
    ObjectDB *db;
    mutable long long compDist = 0; // # de distancias (pivot + query-real)

    PivotTable() : db(nullptr) {}
    PivotTable(ObjectDB *database) : db(database) {}

    // Pivotes aleatorios (fallback cuando no hay HFI precomputado)
    void selectRandomPivots(const vector<DataObject> &objs,
                            size_t l, uint64_t seed = 42)
    {
        pivots.clear();
        if (l == 0 || objs.empty())
            return;
        mt19937_64 rng(seed);
        vector<size_t> idx(objs.size());
        iota(idx.begin(), idx.end(), 0);
        shuffle(idx.begin(), idx.end(), rng);
        for (size_t i = 0; i < l && i < idx.size(); ++i)
            pivots.push_back(objs[idx[i]]);
    }

    // Cargar pivotes desde ids precomputados (HFI), ids 0-based.
    void setPivotsFromIds(const vector<int> &pivotIds,
                          const vector<DataObject> &objs,
                          size_t l)
    {
        pivots.clear();
        if (l == 0 || objs.empty())
            return;
        for (size_t i = 0; i < pivotIds.size() && pivots.size() < l; ++i) {
            int pid = pivotIds[i];
            if (pid < 0)
                continue;
            size_t idx = static_cast<size_t>(pid);
            if (idx < objs.size())
                pivots.push_back(objs[idx]);
        }
    }

    // φ(o) = <d(o, pi)>
    vector<double> mapObject(uint64_t objId) const {
        vector<double> v;
        v.reserve(pivots.size());
        for (auto &p : pivots) {
            double d = db->distance((int)objId, (int)p.id);
            compDist++;
            v.push_back(d);
        }
        return v;
    }

    long long get_compDist() const { return compDist; }
    void clear_compDist() { compDist = 0; }
};

/* -------------------------
   SFC mapping: Morton (Z-order)
   ------------------------- */
struct SFCMapper {
    size_t dims;
    unsigned bits_per_dim;
    vector<double> minv, maxv;

    SFCMapper(size_t dims_ = 0) : dims(dims_), bits_per_dim(0) {}

    void configure(const vector<vector<double>> &mappedVectors) {
        if (mappedVectors.empty())
            return;
        dims = mappedVectors[0].size();
        minv.assign(dims, numeric_limits<double>::infinity());
        maxv.assign(dims, -numeric_limits<double>::infinity());
        for (auto &vec : mappedVectors) {
            for (size_t i = 0; i < dims; i++) {
                minv[i] = min(minv[i], vec[i]);
                maxv[i] = max(maxv[i], vec[i]);
            }
        }
        bits_per_dim = max(1u, (unsigned)(64 / max<size_t>(1, dims)));
        if (bits_per_dim * dims > 64) {
            bits_per_dim = 64 / dims;
            if (bits_per_dim == 0)
                bits_per_dim = 1;
        }
    }

    vector<uint64_t> scalarize(const vector<double> &v) const {
        vector<uint64_t> res(dims);
        for (size_t i = 0; i < dims; i++) {
            double lo = minv[i], hi = maxv[i];
            double x = v[i];
            if (hi - lo < 1e-12) {
                res[i] = 0;
                continue;
            }
            double t = (x - lo) / (hi - lo);
            if (t < 0) t = 0;
            if (t > 1) t = 1;
            uint64_t maxq = (bits_per_dim == 64
                             ? (uint64_t)-1
                             : ((1ULL << bits_per_dim) - 1ULL));
            uint64_t q = (uint64_t)floor(t * (double)maxq + 0.5);
            res[i] = q;
        }
        return res;
    }

    uint64_t mortonKey(const vector<uint64_t> &coords) const {
        uint64_t key = 0;
        for (unsigned b = 0; b < bits_per_dim; ++b) {
            for (size_t d = 0; d < dims; ++d) {
                uint64_t bit = (coords[d] >> b) & 1ULL;
                key <<= 1;
                key |= bit;
            }
        }
        return key;
    }

    uint64_t map(const vector<double> &v) const {
        auto q = scalarize(v);
        return mortonKey(q);
    }
};

/* -------------------------
   MBB (bounding box) en espacio de pivotes
   ------------------------- */
struct MBB {
    vector<double> minv, maxv;

    MBB() {}
    MBB(size_t d) {
        minv.assign(d, numeric_limits<double>::infinity());
        maxv.assign(d, -numeric_limits<double>::infinity());
    }

    void expandWithPoint(const vector<double> &v) {
        if (minv.empty()) {
            minv = v;
            maxv = v;
            return;
        }
        for (size_t i = 0; i < v.size(); ++i) {
            minv[i] = min(minv[i], v[i]);
            maxv[i] = max(maxv[i], v[i]);
        }
    }

    void expandWithMBB(const MBB &o) {
        if (minv.empty()) {
            minv = o.minv;
            maxv = o.maxv;
            return;
        }
        for (size_t i = 0; i < minv.size(); ++i) {
            minv[i] = min(minv[i], o.minv[i]);
            maxv[i] = max(maxv[i], o.maxv[i]);
        }
    }

    // L∞ lower bound entre φ(q) y este MBB
    double lowerBoundToQuery(const vector<double> &q) const {
        double lb = 0.0;
        for (size_t i = 0; i < q.size(); ++i) {
            double x = q[i];
            double v = 0.0;
            if (x < minv[i])      v = minv[i] - x;
            else if (x > maxv[i]) v = x - maxv[i];
            else                  v = 0.0;
            if (v > lb) lb = v;
        }
        return lb;
    }
};

/* -------------------------
   Region RR(r) en espacio de pivotes
   ------------------------- */
struct RangeRegion {
    vector<double> minv, maxv; // por dimensión

    static RangeRegion fromQuery(const vector<double> &qmap, double r) {
        RangeRegion rr;
        size_t d = qmap.size();
        rr.minv.resize(d);
        rr.maxv.resize(d);
        for (size_t i = 0; i < d; ++i) {
            rr.minv[i] = max(0.0, qmap[i] - r);
            rr.maxv[i] = qmap[i] + r;
        }
        return rr;
    }

    bool containsPoint(const vector<double> &v) const {
        for (size_t i = 0; i < v.size(); ++i) {
            if (v[i] < minv[i] || v[i] > maxv[i])
                return false;
        }
        return true;
    }

    bool containsBox(const MBB &b) const {
        if (b.minv.empty())
            return false;
        for (size_t i = 0; i < b.minv.size(); ++i) {
            if (b.minv[i] < minv[i] || b.maxv[i] > maxv[i])
                return false;
        }
        return true;
    }

    bool intersectsBox(const MBB &b) const {
        if (b.minv.empty())
            return false;
        for (size_t i = 0; i < b.minv.size(); ++i) {
            if (b.maxv[i] < minv[i] || b.minv[i] > maxv[i])
                return false;
        }
        return true;
    }
};

/* -------------------------
   B+ tree en memoria sobre la SFC
   ------------------------- */
struct BPlusEntry {
    bool isLeaf;
    vector<BPlusEntry *> children; // si no es hoja

    // En hojas: records = (key, objectId, mappedVector)
    vector<tuple<uint64_t, uint64_t, vector<double>>> records;

    MBB box;        // MBB del subárbol o del conjunto de records
    uint64_t minKey, maxKey;

    BPlusEntry(bool leaf = true) : isLeaf(leaf), box(), minKey(0), maxKey(0) {}
};

class BPlusTree {
    BPlusEntry *root;
    size_t leafCapacity;
    size_t fanout;

public:
    BPlusTree(size_t leafCap = 128, size_t fanout_ = 64)
        : root(nullptr), leafCapacity(leafCap), fanout(fanout_) {}

    ~BPlusTree() { clear(root); }

    void clear(BPlusEntry *node) {
        if (!node)
            return;
        if (!node->isLeaf) {
            for (auto c : node->children)
                clear(c);
        }
        delete node;
    }

    BPlusEntry *getRoot() const { return root; }

    void bulkLoad(const vector<tuple<uint64_t, uint64_t, vector<double>>> &recs) {
        clear(root);
        if (recs.empty()) {
            root = nullptr;
            return;
        }

        vector<tuple<uint64_t, uint64_t, vector<double>>> sortedRecs = recs;
        sort(sortedRecs.begin(), sortedRecs.end(),
             [](auto &a, auto &b) { return get<0>(a) < get<0>(b); });

        vector<BPlusEntry *> leaves;
        size_t n = sortedRecs.size();
        size_t i = 0;
        while (i < n) {
            BPlusEntry *leaf = new BPlusEntry(true);
            size_t end = min(n, i + leafCapacity);
            for (size_t j = i; j < end; ++j) {
                leaf->records.push_back(sortedRecs[j]);
                auto &mv = get<2>(sortedRecs[j]);
                leaf->box.expandWithPoint(mv);
                uint64_t k = get<0>(sortedRecs[j]);
                if (leaf->records.size() == 1) {
                    leaf->minKey = leaf->maxKey = k;
                } else {
                    leaf->minKey = min(leaf->minKey, k);
                    leaf->maxKey = max(leaf->maxKey, k);
                }
            }
            leaves.push_back(leaf);
            i = end;
        }

        if (leaves.empty()) {
            root = nullptr;
            return;
        }

        vector<BPlusEntry *> cur = leaves;
        while (cur.size() > 1) {
            vector<BPlusEntry *> next;
            size_t j = 0;
            while (j < cur.size()) {
                BPlusEntry *node = new BPlusEntry(false);
                size_t end = min(cur.size(), j + fanout);
                for (size_t t = j; t < end; ++t) {
                    node->children.push_back(cur[t]);
                    node->box.expandWithMBB(cur[t]->box);
                    if (node->children.size() == 1) {
                        node->minKey = cur[t]->minKey;
                        node->maxKey = cur[t]->maxKey;
                    } else {
                        node->minKey = min(node->minKey, cur[t]->minKey);
                        node->maxKey = max(node->maxKey, cur[t]->maxKey);
                    }
                }
                next.push_back(node);
                j = end;
            }
            cur.swap(next);
        }
        root = cur[0];
    }
};

/* -------------------------
   SPB-tree wrapper (RQA + NNA)
   ------------------------- */
class SPBTree {
    ObjectDB *db;
    RAF raf;
    PivotTable pt;
    SFCMapper sfc;
    BPlusTree bplus;
    size_t l;
    string datasetName;
    bool useHfiPivots;
    vector<tuple<uint64_t, uint64_t, vector<double>>> records;

    // ---------------- RQA helper ----------------
    void verifyRQ(const tuple<uint64_t, uint64_t, vector<double>> &rec,
                  uint64_t queryId,
                  const vector<double> &qmap,
                  double r,
                  const RangeRegion &rr,
                  vector<uint64_t> &out)
    {
        uint64_t objId = get<1>(rec);
        const vector<double> &mv = get<2>(rec);

        // Lemma 1: si φ(o) no está en RR(r), descartar sin distancia real
        if (!rr.containsPoint(mv))
            return;

        // Lemma 2: si ∃ pivot pi con d(o,pi) ≤ r - d(q,pi) -> seguro en RQ(q,r)
        bool sure = false;
        for (size_t i = 0; i < mv.size(); ++i) {
            double rhs = r - qmap[i];
            if (rhs >= 0.0 && mv[i] <= rhs) {
                sure = true;
                break;
            }
        }
        if (sure) {
            out.push_back(objId);
            return;
        }

        // Verificación final con distancia real d(q,o)
        raf.read(objId); // acceso real a RAF (cuenta páginas físicas)
        double dist = db->distance((int)queryId, (int)objId);
        pt.compDist++;
        if (dist <= r)
            out.push_back(objId);
    }

public:
    SPBTree(const string &rafFile,
            ObjectDB *database,
            size_t l_,
            size_t leafCap = 128,
            size_t fanout = 64,
            const string &datasetName_ = "",
            bool useHfiPivots_ = true,
            size_t logicalPageFactor = 1)
        : db(database),
          raf(rafFile, logicalPageFactor),
          pt(database),
          sfc(),
          bplus(leafCap, fanout),
          l(l_),
          datasetName(datasetName_),
          useHfiPivots(useHfiPivots_) {}

    void build(vector<DataObject> &dataset,
               const vector<int> &hfiPivotIds = {},
               uint64_t pivotSeed = 42)
    {
        // Reiniciar fichero RAF para esta construcción
        raf.resetFile();

        // Escribir todos los objetos al RAF
        for (auto &o : dataset) {
            raf.append(o);
        }

        // Cargar pivotes HFI si se proporcionan ids
        bool loadedHfi = false;
        if (useHfiPivots && !hfiPivotIds.empty()) {
            pt.setPivotsFromIds(hfiPivotIds, dataset, l);
            if (!pt.pivots.empty()) {
                cerr << "[INFO] SPB-tree: usando " << pt.pivots.size()
                     << " pivotes HFI precomputados\n";
                loadedHfi = true;
            }
        }

        if (!loadedHfi) {
            pt.selectRandomPivots(dataset, l, pivotSeed);
            cerr << "[WARN] SPB-tree: usando pivotes aleatorios (no HFI)\n";
        }

        // Pivot mapping φ(o)
        vector<vector<double>> mapped;
        mapped.reserve(dataset.size());
        for (auto &o : dataset) {
            auto mv = pt.mapObject(o.id);
            mapped.push_back(mv);
        }

        // Configurar SFC
        sfc.configure(mapped);

        // Construir registros (key, id, φ(o))
        records.clear();
        for (size_t i = 0; i < dataset.size(); ++i) {
            uint64_t id = dataset[i].id;
            auto &mv = mapped[i];
            uint64_t key = sfc.map(mv);
            records.push_back({key, id, mv});
        }

        sort(records.begin(), records.end(),
             [](auto &a, auto &b) { return get<0>(a) < get<0>(b); });

        // Bulk load B+ tree
        bplus.bulkLoad(records);
    }

    // ---------------- MRQ usando RQA (Algorithm 3) ----------------
    vector<uint64_t> MRQ(uint64_t queryId, double r) {
        vector<uint64_t> result;
        BPlusEntry *root = bplus.getRoot();
        if (!root)
            return result;

        vector<double> qmap = pt.mapObject(queryId); // φ(q)
        RangeRegion rr = RangeRegion::fromQuery(qmap, r);

        struct NodeItem {
            BPlusEntry *node;
            double lb;
        };
        struct Cmp {
            bool operator()(const NodeItem &a, const NodeItem &b) const {
                return a.lb > b.lb; // min-heap
            }
        };

        priority_queue<NodeItem, vector<NodeItem>, Cmp> H;
        H.push({root, root->box.lowerBoundToQuery(qmap)});

        while (!H.empty()) {
            auto it = H.top();
            H.pop();
            BPlusEntry *N = it.node;

            // Poda por Lemma 1: si MBB(N) no intersecta RR(r), descartar
            if (!rr.intersectsBox(N->box))
                continue;

            if (!N->isLeaf) {
                // Nodo interno: recorrer hijos cuyas MBB intersectan RR(r)
                for (auto c : N->children) {
                    if (rr.intersectsBox(c->box)) {
                        double lb = c->box.lowerBoundToQuery(qmap);
                        H.push({c, lb});
                    }
                }
            } else {
                // Nodo hoja: verificamos cada entrada con VerifyRQ
                for (auto &rec : N->records) {
                    verifyRQ(rec, queryId, qmap, r, rr, result);
                }
            }
        }

        return result;
    }

    // ---------------- MkNN usando NNA (Algorithm 4) ----------------
    vector<pair<uint64_t, double>> MkNN(uint64_t queryId, size_t k) {
        vector<pair<uint64_t, double>> answers;
        if (k == 0)
            return answers;
        BPlusEntry *root = bplus.getRoot();
        if (!root)
            return answers;

        vector<double> qmap = pt.mapObject(queryId); // φ(q)

        struct HeapItem {
            bool isLeafRec;      // false: nodo interno/hoja; true: entrada objeto
            BPlusEntry *node;
            size_t recIdx;       // índice de record si isLeafRec==true
            double lb;           // MIND(q, E)
        };
        struct HCmp {
            bool operator()(const HeapItem &a, const HeapItem &b) const {
                return a.lb > b.lb; // min-heap por MIND
            }
        };

        priority_queue<HeapItem, vector<HeapItem>, HCmp> H;

        // Inicializar con la raíz
        H.push({false, root, 0, root->box.lowerBoundToQuery(qmap)});
        double curNDk = numeric_limits<double>::infinity();

        vector<pair<uint64_t, double>> cand; // candidatos (id, dist real)

        auto updateCurNDk = [&]() {
            if (cand.empty()) {
                curNDk = numeric_limits<double>::infinity();
                return;
            }
            double mx = 0.0;
            for (auto &p : cand)
                mx = max(mx, p.second);
            curNDk = mx;
        };

        while (!H.empty()) {
            auto it = H.top();
            H.pop();

            if (it.lb >= curNDk)
                break; // condición de parada por Lemma 3

            if (!it.isLeafRec) {
                BPlusEntry *node = it.node;
                if (!node->isLeaf) {
                    // Nodo interno: expandir a hijos
                    for (auto c : node->children) {
                        double lb = c->box.lowerBoundToQuery(qmap);
                        H.push({false, c, 0, lb});
                    }
                } else {
                    // Nodo hoja: generar entradas hoja-objeto
                    for (size_t i = 0; i < node->records.size(); ++i) {
                        const auto &rec = node->records[i];
                        const auto &mv = get<2>(rec);
                        double lb = 0.0;
                        for (size_t d = 0; d < mv.size(); ++d) {
                            double md = fabs(qmap[d] - mv[d]);
                            if (md > lb)
                                lb = md;
                        }
                        H.push({true, node, i, lb});
                    }
                }
            } else {
                // Entrada hoja: verificar objeto en RAF + métrica real
                BPlusEntry *node = it.node;
                const auto &rec = node->records[it.recIdx];
                uint64_t objId = get<1>(rec);

                raf.read(objId); // acceso real a RAF
                double dist = db->distance((int)queryId, (int)objId);
                pt.compDist++;

                cand.push_back({objId, dist});
                if (cand.size() > k) {
                    // Mantener sólo los k mejores
                    auto worstIt = max_element(
                        cand.begin(), cand.end(),
                        [](auto &a, auto &b) { return a.second < b.second; });
                    cand.erase(worstIt);
                }
                updateCurNDk();
            }
        }

        sort(cand.begin(), cand.end(),
             [](auto &a, auto &b) { return a.second < b.second; });
        if (cand.size() > k)
            cand.resize(k);
        return cand;
    }

    void stats() const {
        cout << "SPB-tree: pivots=" << l << ", records=" << records.size() << "\n";
    }

    long long get_compDist() const { return pt.get_compDist(); }
    long long get_pageReads() const { return raf.get_pageReads(); }

    void clear_counters() {
        pt.clear_compDist();
        raf.clear_pageReads();
    }
};
