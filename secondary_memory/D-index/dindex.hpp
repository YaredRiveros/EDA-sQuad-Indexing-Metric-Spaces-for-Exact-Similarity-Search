#pragma once
#include <bits/stdc++.h>
#include <filesystem>
#include "../../objectdb.hpp"
#include "../../datasets/paths.hpp"

using namespace std;
namespace fs = std::filesystem;

struct DataObject {
    int id;   // índice 0..N-1 dentro de ObjectDB
};

class DIndexRAF {
    static constexpr size_t PAGE_SIZE = 4096; // 4KB físicos

    string filename;
    unordered_map<int, streampos> offsets;

    // Páginas físicas únicas tocadas (para todas las consultas)
    mutable unordered_set<uint64_t> pagesVisited;

public:
    explicit DIndexRAF(const string &fname)
        : filename(fname)
    {
        // Crear / truncar archivo
        ofstream ofs(filename, ios::binary | ios::trunc);
    }

    // Para reconstruir el índice (nuevo build)
    void resetFile() {
        offsets.clear();
        pagesVisited.clear();
        ofstream ofs(filename, ios::binary | ios::trunc);
    }

    streampos append(int id) {
        ofstream ofs(filename, ios::binary | ios::app);
        streampos pos = ofs.tellp();

        int32_t v = static_cast<int32_t>(id);
        ofs.write(reinterpret_cast<const char*>(&v), sizeof(v));
        ofs.close();

        offsets[id] = pos;
        return pos;
    }

    // Lectura real desde disco: posiciona el puntero, lee el id,
    // y marca la página física como visitada.
    void read(int id) const {
        auto it = offsets.find(id);
        if (it == offsets.end())
            throw runtime_error("DIndexRAF: id not found");

        long long off = static_cast<long long>(it->second);
        uint64_t pageId =
            static_cast<uint64_t>(off / static_cast<long long>(PAGE_SIZE));
        pagesVisited.insert(pageId);

        ifstream ifs(filename, ios::binary);
        if (!ifs)
            throw runtime_error("DIndexRAF: cannot open RAF file for read");

        ifs.seekg(it->second);
        int32_t tmp;
        ifs.read(reinterpret_cast<char*>(&tmp), sizeof(tmp));
    }

    long long get_pageReads() const {
        return static_cast<long long>(pagesVisited.size());
    }

    void clear_pageReads() {
        pagesVisited.clear();
    }
};

inline vector<int> loadHFIPivots(const string &path) {
    vector<int> pivs;

    if (!fs::exists(path)) {
        cerr << "[HFI] No HFI pivot file: " << path << "\n";
        return pivs;
    }

    ifstream f(path);
    string tok;

    while (f >> tok) {
        tok.erase(remove_if(tok.begin(), tok.end(),
                            [](char c){ return c=='['||c==']'||c==','; }),
                  tok.end());
        if (!tok.empty() && all_of(tok.begin(), tok.end(), ::isdigit))
            pivs.push_back(stoi(tok));
    }

    cerr << "[HFI] Loaded " << pivs.size() << " pivots from " << path << "\n";
    return pivs;
}

inline uint32_t encodeKey(const vector<char>& code) {
    uint32_t k = 0;
    for (char c : code) {
        k *= 3;
        if (c == 'L')      k += 0;
        else if (c == 'R') k += 1;
        else               k += 2;  // '-'
    }
    return k;
}

inline double lbInterval(double q, const pair<double,double>& I) {
    if (q < I.first)  return I.first - q;
    if (q > I.second) return q - I.second;
    return 0.0;
}

struct Bucket {
    vector<pair<double,double>> intervals; // Intervalos por nivel
    vector<int> ids;                       // IDs de objetos (0..N-1)
};

class DIndex {
private:
    ObjectDB* db;
    int N;
    size_t L;      // nro de pivotes / niveles
    double rho;

    DIndexRAF raf;

    vector<int>    pivotIds;
    vector<double> pivotMedians;

    vector<double> distMatrix;  // N x L

    vector<Bucket>                 buckets;
    unordered_map<uint32_t,size_t> bucketIndex;

    long long compDist  = 0;
    long long pageReads = 0;

public:
    DIndex(const string& rafFile,
           ObjectDB* database,
           size_t numLevels,
           double rho_)
        : db(database),
          N(0),
          L(numLevels),
          rho(rho_),
          raf(rafFile)
    {
        N = db->size();
        pivotIds.resize(L);
        pivotMedians.resize(L);
        distMatrix.resize(static_cast<size_t>(N) * L);
    }

    //  BUILD
    void build(const vector<DataObject>& objects,
               uint64_t seed,
               const string& pivfile)
    {
        cerr << "[DIndex] BUILD START\n";

        // Limpieza por si se reconstruye
        buckets.clear();
        bucketIndex.clear();
        clear_counters();    // reseteamos stats globales

        // Reset RAF: truncar archivo y vaciar offsets/páginas
        raf.resetFile();

        cerr << "[DIndex] Loading HFI pivots if available...\n";
        selectPivots(objects, seed, pivfile);

        cerr << "[DIndex] Computing distance matrix (N x L)...\n";
        computeDistanceMatrix();

        cerr << "[DIndex] Computing medians...\n";
        computeMedians();

        cerr << "[DIndex] Building buckets...\n";
        buildBuckets();

        cerr << "[DIndex] Writing objects to RAF...\n";
        for (const auto& o : objects) {
            raf.append(o.id);
        }

        cerr << "[DIndex] BUILD OK\n";
    }

    void selectPivots(const vector<DataObject>& objs,
                      uint64_t seed,
                      const string &pivotFile)
    {
        auto hfi = loadHFIPivots(pivotFile);

        if (!hfi.empty() && hfi.size() >= L) {
            for (size_t i = 0; i < L; i++)
                pivotIds[i] = hfi[i];
            cerr << "[DIndex] Using HFI pivots.\n";
            return;
        }

        cerr << "[DIndex] Using random pivots (HFI unavailable or insufficient).\n";

        mt19937_64 rng(seed);
        unordered_set<int> used;

        for (size_t i = 0; i < L; i++) {
            while (true) {
                int id = objs[rng() % objs.size()].id;
                if (!used.count(id)) {
                    pivotIds[i] = id;
                    used.insert(id);
                    break;
                }
            }
        }
    }

    void computeDistanceMatrix() {
        // Distancias de construcción (no se cuentan en las compdist de consultas)
        for (int id = 0; id < N; id++) {
            for (size_t j = 0; j < L; j++) {
                double d = db->distance(id, pivotIds[j]);
                distMatrix[static_cast<size_t>(id) * L + j] = d;
            }
        }
    }

    void computeMedians() {
        vector<double> tmp(static_cast<size_t>(N));

        for (size_t j = 0; j < L; j++) {
            for (int id = 0; id < N; id++)
                tmp[static_cast<size_t>(id)] =
                    distMatrix[static_cast<size_t>(id) * L + j];

            size_t k = static_cast<size_t>(N) / 2;
            nth_element(tmp.begin(), tmp.begin() + k, tmp.end());
            pivotMedians[j] = tmp[k];
        }
    }

    void buildBuckets() {
        vector<char> code(L);

        for (int id = 0; id < N; id++) {
            const double* row =
                &distMatrix[static_cast<size_t>(id) * L];

            bool assigned = false;

            for (size_t lvl = 0; lvl < L; lvl++) {
                double d   = row[lvl];
                double med = pivotMedians[lvl];

                if (d < med - rho) {
                    fill(code.begin(), code.end(), '-');
                    code[lvl] = 'L';
                    addToBucket(id, code);
                    assigned = true;
                    break;
                }

                if (d > med + rho) {
                    fill(code.begin(), code.end(), '-');
                    code[lvl] = 'R';
                    addToBucket(id, code);
                    assigned = true;
                    break;
                }
            }

            if (!assigned) {
                fill(code.begin(), code.end(), '-');
                addToBucket(id, code);
            }
        }
    }

    void addToBucket(int id, const vector<char>& code) {
        uint32_t key = encodeKey(code);

        if (!bucketIndex.count(key)) {
            size_t idx = buckets.size();
            bucketIndex[key] = idx;
            buckets.emplace_back();
            buildIntervals(buckets.back(), code);
        }

        buckets[bucketIndex[key]].ids.push_back(id);
    }

    void buildIntervals(Bucket& b, const vector<char>& code) {
        b.intervals.resize(L);

        for (size_t lvl = 0; lvl < L; lvl++) {
            double med = pivotMedians[lvl];

            if (code[lvl] == 'L') {
                b.intervals[lvl] = {0.0, max(0.0, med - rho)};
            }
            else if (code[lvl] == 'R') {
                b.intervals[lvl] = {med + rho,
                                    numeric_limits<double>::infinity()};
            }
            else {
                b.intervals[lvl] = {max(0.0, med - rho), med + rho};
            }
        }
    }

private:
    vector<pair<int,double>> MRQ_withDists(int qid, double r) {
        vector<double> q(L);

        // Distancias query -> pivotes (cuentan en compDist)
        for (size_t i = 0; i < L; i++) {
            q[i] = db->distance(qid, pivotIds[i]);
            compDist++;
        }

        vector<pair<int,double>> out;

        for (auto &b : buckets) {
            double LB = 0.0;

            for (size_t lvl = 0; lvl < L; lvl++) {
                LB = max(LB, lbInterval(q[lvl], b.intervals[lvl]));
                if (LB > r) break;
            }

            if (LB > r) continue;

            // Buckets candidatos -> verificar objetos
            for (int id : b.ids) {
                // Leer del RAF
                raf.read(id);

                // Distancia real (cuenta en compDist)
                double d = db->distance(qid, id);
                compDist++;
                if (d <= r)
                    out.push_back({id, d});
            }
        }

        return out;
    }

public:
    
    vector<int> MRQ(int qid, double r) {
        long long comp_before  = compDist;
        long long pages_before = raf.get_pageReads();

        auto vec = MRQ_withDists(qid, r);

        // Dejar los contadores como lo que gastó ESTA MRQ
        compDist  = compDist  - comp_before;
        pageReads = raf.get_pageReads() - pages_before;

        vector<int> out;
        out.reserve(vec.size());
        for (auto &p : vec) out.push_back(p.first);
        return out;
    }

    
    vector<pair<int,double>> MkNN(int qid, size_t k) {
        // Snapshot antes de todo el proceso MkNN
        long long comp_before  = compDist;
        long long pages_before = raf.get_pageReads();

        double R = rho;                     // radio inicial (paper: r_0)
        vector<pair<int,double>> best;      // mejores candidatos actuales

        const int MAX_ITERS = 10;           // límite de refinamientos

        for (int iter = 0; iter < MAX_ITERS; ++iter) {
            auto cand = MRQ_withDists(qid, R);  // ya suma compDist y RAF pages

            if (cand.empty()) {
                // nada dentro de R: agrandamos el radio agresivamente
                R *= 2.0;
                continue;
            }

            // Ordenamos por distancia
            sort(cand.begin(), cand.end(),
                 [](auto &a, auto &b){ return a.second < b.second; });

            if (cand.size() >= k) {
                // Ya tenemos al menos k vecinos dentro de R
                best.assign(cand.begin(), cand.begin() + k);
                break;
            } else {
                // Aún no llegamos a k: guardamos lo mejor y ampliamos el radio
                best = cand;

                double far = cand.back().second;
                R = max(R * 2.0, far * 2.0);
            }
        }

        // Stats = delta consumido solo en esta MkNN
        compDist  = compDist  - comp_before;
        pageReads = raf.get_pageReads() - pages_before;

        return best;
    }

    long long get_compDist() const { return compDist; }
    long long get_pageReads() const { return pageReads; }

    void clear_counters() {
        compDist  = 0;
        pageReads = 0;
        raf.clear_pageReads();
    }
};
