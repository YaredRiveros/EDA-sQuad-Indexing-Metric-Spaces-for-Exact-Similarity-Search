// ============================================================================
// DIndex.hpp - Implementación optimizada del D-Index
// Con pivotes HFI + compatibilidad con Chen (2022) y Dohnal (2003)
//
//  ✔ Usa pivotes HFI si existen
//  ✔ Fallback a pivotes random como Chen
//  ✔ Distancias almacenadas en matriz N×L contigua (RAM estable)
//  ✔ Keys compactas base-3 (L,R,-)
//  ✔ Buckets ligeros y precomputados
//  ✔ MRQ y MkNN exactos como Chen
//
//  Requisitos:
//  - ObjectDB debe implementar distance(id1, id2)
//  - IDs son valores consecutivos desde 1 .. N
// ============================================================================

#pragma once
#include <bits/stdc++.h>
#include "../../objectdb.hpp"
#include "../../datasets/paths.hpp"

using namespace std;

// ============================================================================
// --- Utilidades --------------------------------------------------------------
// ============================================================================

// Lee pivotes desde archivo HFI (mismo formato que GNAT, EGNAT, Omni)
inline vector<uint32_t> loadHFIPivots(const string &path) {
    vector<uint32_t> pivs;

    if (!filesystem::exists(path)) {
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
        if (c == 'L') k += 0;
        else if (c == 'R') k += 1;
        else k += 2;   // '-'
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
    vector<uint32_t> ids;                 // IDs de objetos
};

// ============================================================================
// --- DIndex Class ------------------------------------------------------------
// ============================================================================
class DIndex {
private:
    ObjectDB* db;
    size_t N;
    size_t L;     // Número de pivotes
    double rho;   // Parámetro ρ-split

    vector<uint32_t> pivotIds;
    vector<double> pivotMedians;

    vector<double> distMatrix;   // Tamaño = N×L  (id-1)*L + lvl

    vector<Bucket> buckets;
    unordered_map<uint32_t, size_t> bucketIndex;

    long long compDist = 0;
    long long pageReads = 0;

public:
    DIndex(const string&, ObjectDB* database, size_t numLevels, double rho_)
        : db(database), L(numLevels), rho(rho_)
    {
        N = db->size();
        pivotIds.resize(L);
        pivotMedians.resize(L);
        distMatrix.resize(N * L);  // RAM estable, sin hashing
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

        cerr << "[DIndex] Computing distance matrix...\n";
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
        cerr << "[DIndex] Using random pivots (HFI unavailable).\n";

        mt19937_64 rng(seed);
        unordered_set<uint32_t> used;

        for (size_t i = 0; i < L; i++) {
            while (true) {
                uint32_t id = objs[rng() % objs.size()].id;
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

        for (uint32_t id = 1; id <= N; id++) {
            for (size_t j = 0; j < L; j++) {
                double d = db->distance(id, pivotIds[j]);
                distMatrix[(id-1)*L + j] = d;
                compDist++;
            }
        }
    }

    // ========================================================================
    // Medianas por nivel
    // ========================================================================
    void computeMedians() {
        vector<double> tmp(N);

        for (size_t j = 0; j < L; j++) {
            for (uint32_t id = 1; id <= N; id++)
                tmp[id-1] = distMatrix[(id-1)*L + j];

            size_t k = N / 2;
            nth_element(tmp.begin(), tmp.begin() + k, tmp.end());
            pivotMedians[j] = tmp[k];
        }
    }

    // ========================================================================
    // Build buckets
    // ========================================================================
    void buildBuckets() {
        vector<char> code(L);

        for (uint32_t id = 1; id <= N; id++) {
            const double* row = &distMatrix[(id-1)*L];

            bool assigned = false;

            for (size_t lvl = 0; lvl < L; lvl++) {
                double d = row[lvl];
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

    // ========================================================================
    // Añadir a bucket
    // ========================================================================
    void addToBucket(uint32_t id, const vector<char>& code) {
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

            if (code[lvl] == 'L')
                b.intervals[lvl] = {0.0, max(0.0, med - rho)};

            else if (code[lvl] == 'R')
                b.intervals[lvl] = {med + rho, numeric_limits<double>::infinity()};

            else
                b.intervals[lvl] = {max(0.0, med - rho), med + rho};
        }
    }

    // ========================================================================
    // MRQ (Chen 2022)
    // ========================================================================
    vector<uint32_t> MRQ(uint32_t qid, double r) {
        vector<double> q(L);

        for (size_t i = 0; i < L; i++) {
            q[i] = db->distance(qid, pivotIds[i]);
            compDist++;
        }

        vector<uint32_t> out;

        for (auto &b : buckets) {
            double LB = 0;

            for (size_t lvl = 0; lvl < L; lvl++) {
                LB = max(LB, lbInterval(q[lvl], b.intervals[lvl]));
                if (LB > r)
                    break;
            }

            if (LB <= r)
                out.insert(out.end(), b.ids.begin(), b.ids.end());
        }

        return out;
    }

    // ========================================================================
    // MkNN (Chen 2022)
    // ========================================================================
    vector<pair<uint32_t,double>> MkNN(uint32_t qid, size_t k) {
        double R = rho;
        vector<pair<uint32_t,double>> final;

        for (int iter = 0; iter < 5; iter++) {
            auto cand = MRQ(qid, R);

            vector<pair<uint32_t,double>> vec;
            vec.reserve(cand.size());

            for (uint32_t id : cand) {
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

