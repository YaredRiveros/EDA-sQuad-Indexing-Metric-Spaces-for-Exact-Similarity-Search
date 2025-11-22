// ============================================================================
// DIndex.hpp - Implementación optimizada del D-Index
// Con pivotes HFI + compatibilidad con Chen (2022) y Dohnal (2003)
//
//  ✔ Usa pivotes HFI si existen (pivots/<dataset>_pivots_L.json)
//  ✔ Fallback a pivotes aleatorios si no hay archivo
//  ✔ Distancias almacenadas en matriz N×L contigua (RAM estable)
//  ✔ Keys compactas base-3 (L,R,-)
//  ✔ Buckets ligeros y precomputados
//  ✔ MRQ y MkNN como en Chen
//
//  Requisitos:
//  - ObjectDB (de objectdb.hpp) con: int size(), double distance(int,int)
//  - IDs son índices 0..N-1 (coherente con VectorDB y StringDB)
// ============================================================================

#pragma once
#include <bits/stdc++.h>
#include <filesystem>
#include "../../objectdb.hpp"
#include "../../datasets/paths.hpp"

using namespace std;
namespace fs = std::filesystem;

// ============================================================================
// --- DataObject mínimo ------------------------------------------------------
// ============================================================================
struct DataObject {
    int id;   // índice 0..N-1 dentro de ObjectDB
};

// ============================================================================
// --- Utilidades --------------------------------------------------------------
// ============================================================================

// Lee pivotes desde archivo HFI (mismo formato que GNAT / Omni / EGNAT)
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

// Codifica un vector de {L,R,-} en base 3 → clave bucket compacta
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

// Distancia mínima de q al intervalo [a,b]
inline double lbInterval(double q, const pair<double,double>& I) {
    if (q < I.first)  return I.first - q;
    if (q > I.second) return q - I.second;
    return 0.0;
}

// ============================================================================
// --- Bucket ------------------------------------------------------------------
// ============================================================================
struct Bucket {
    vector<pair<double,double>> intervals; // Intervalos por nivel
    vector<int> ids;                      // IDs de objetos (0..N-1)
};

// ============================================================================
// --- DIndex Class ------------------------------------------------------------
// ============================================================================
class DIndex {
private:
    ObjectDB* db;
    int N;        // número de objetos (db->size())
    size_t L;     // número de pivotes
    double rho;   // parámetro ρ

    vector<int> pivotIds;      // IDs de pivotes (0..N-1)
    vector<double> pivotMedians;

    // Distancias d(obj, pivot): tamaño N×L; índice: id*L + lvl, con id∈[0,N-1]
    vector<double> distMatrix;

    vector<Bucket> buckets;
    unordered_map<uint32_t, size_t> bucketIndex; // key → índice en buckets

    long long compDist = 0;
    long long pageReads = 0;   // no usamos RAF pero mantenemos la API

public:
    DIndex(const string& /*rafFile*/,
           ObjectDB* database,
           size_t numLevels,
           double rho_)
        : db(database), L(numLevels), rho(rho_) {

        N = db->size();
        pivotIds.resize(L);
        pivotMedians.resize(L);
        distMatrix.resize(static_cast<size_t>(N) * L);  // RAM estable
    }

    // ========================================================================
    //  BUILD
    // ========================================================================
    void build(const vector<DataObject>& objects,
               uint64_t seed,
               const string& pivfile)
    {
        cerr << "[DIndex] BUILD START\n";

        cerr << "[DIndex] Loading HFI pivots if available...\n";
        selectPivots(objects, seed, pivfile);

        cerr << "[DIndex] Computing distance matrix (N x L)...\n";
        computeDistanceMatrix();

        cerr << "[DIndex] Computing medians...\n";
        computeMedians();

        cerr << "[DIndex] Building buckets...\n";
        buildBuckets();

        cerr << "[DIndex] BUILD OK\n";
    }

    // ========================================================================
    // Selección de pivotes (HFI o random)
    // ========================================================================
    void selectPivots(const vector<DataObject>& objs,
                      uint64_t seed,
                      const string &pivotFile)
    {
        // 1) Intentar cargar pivotes HFI
        auto hfi = loadHFIPivots(pivotFile);

        if (!hfi.empty() && hfi.size() >= L) {
            for (size_t i = 0; i < L; i++)
                pivotIds[i] = hfi[i];
            cerr << "[DIndex] Using HFI pivots.\n";
            return;
        }

        // 2) Fallback → pivotes aleatorios como Chen
        cerr << "[DIndex] Using random pivots (HFI unavailable or insufficient).\n";

        mt19937_64 rng(seed);
        unordered_set<int> used;

        for (size_t i = 0; i < L; i++) {
            while (true) {
                int id = objs[rng() % objs.size()].id; // id 0..N-1
                if (!used.count(id)) {
                    pivotIds[i] = id;
                    used.insert(id);
                    break;
                }
            }
        }
    }

    // ========================================================================
    // Distance Matrix
    // ========================================================================
    void computeDistanceMatrix() {
        compDist = 0;

        for (int id = 0; id < N; id++) {
            for (size_t j = 0; j < L; j++) {
                double d = db->distance(id, pivotIds[j]);  // ObjectDB usa int
                distMatrix[static_cast<size_t>(id) * L + j] = d;
                compDist++;
            }
        }
    }

    // ========================================================================
    // Medianas por nivel
    // ========================================================================
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

    // ========================================================================
    // Build buckets
    // ========================================================================
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
                // bucket de exclusión (todos '-')
                fill(code.begin(), code.end(), '-');
                addToBucket(id, code);
            }
        }
    }

    // ========================================================================
    // Añadir a bucket
    // ========================================================================
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

    // ========================================================================
    // Intervalos del bucket
    // ========================================================================
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

    // ========================================================================
    // MRQ (Chen 2022)
    // ========================================================================
    vector<int> MRQ(int qid, double r) {
        vector<double> q(L);

        // Distancias query → pivotes (cuentan en compDist)
        for (size_t i = 0; i < L; i++) {
            q[i] = db->distance(qid, pivotIds[i]);
            compDist++;
        }

        vector<int> out;

        for (auto &b : buckets) {
            double LB = 0.0;

            for (size_t lvl = 0; lvl < L; lvl++) {
                LB = max(LB, lbInterval(q[lvl], b.intervals[lvl]));
                if (LB > r) break;
            }

            if (LB <= r)
                out.insert(out.end(), b.ids.begin(), b.ids.end());
        }

        return out;
    }

    // ========================================================================
    // MkNN (Chen 2022)
    // ========================================================================
    vector<pair<int,double>> MkNN(int qid, size_t k) {
        double R = rho;
        vector<pair<int,double>> final;

        for (int iter = 0; iter < 5; iter++) {
            auto cand = MRQ(qid, R);

            vector<pair<int,double>> vec;
            vec.reserve(cand.size());

            for (int id : cand) {
                double d = db->distance(qid, id);
                compDist++;
                vec.push_back({id, d});
            }

            sort(vec.begin(), vec.end(),
                 [](auto &a, auto &b){ return a.second < b.second; });

            if (vec.size() >= k) {
                double dk = vec[k-1].second;

                if (dk > R + 1e-12) {
                    R = dk;
                    final.assign(vec.begin(),
                                 vec.begin()+min(k, vec.size()));
                } else {
                    final.assign(vec.begin(), vec.begin()+k);
                    break;
                }
            } else {
                if (!vec.empty())
                    R = max(R, vec.back().second * 2.0);

                final = vec;
            }
        }

        return final;
    }

    // ========================================================================
    // Stats
    // ========================================================================
    long long get_compDist() const { return compDist; }
    long long get_pageReads() const { return pageReads; }

    void clear_counters() {
        compDist = 0;
        pageReads = 0;
    }
};
