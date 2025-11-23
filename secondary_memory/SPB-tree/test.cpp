#include <bits/stdc++.h>
#include "spbtree.hpp"
#include "../../objectdb.hpp"
#include "../../datasets/paths.hpp"

using namespace std;

// ============================================================
// Parámetros de experimento (Chen 2022)
// ============================================================

// Selectividades del paper (2%, 4%, 8%, 16%, 32%)
static const vector<double> SELECTIVITIES = {
    0.02, 0.04, 0.08, 0.16, 0.32
};

// Valores de k usados en el paper
static const vector<int> K_VALUES = {
    5, 10, 20, 50, 100
};

// Datasets del entorno experimental
static const vector<string> DATASETS = {
    "LA", "Words", "Color", "Synthetic"
};

// Número de pivotes (Chen: l = 5)
static const int NUM_PIVOTS = 5;

// Parámetros del B+ tree (puedes ajustarlos si quieres)
static const size_t LEAF_CAPACITY = 128;
static const size_t FANOUT        = 64;

// ============================================================
// Helpers
// ============================================================

// Autodetección 0-based / 1-based para IDs de queries/pivotes
vector<int> auto_fix_ids(const vector<int> &ids, int nObjects) {
    if (ids.empty()) return ids;

    bool hasZero = false;
    bool hasOutOfRange = false;

    for (int v : ids) {
        if (v == 0) hasZero = true;
        if (v >= nObjects) hasOutOfRange = true;
    }

    // Caso A: ya son 0-based
    if (hasZero && !hasOutOfRange) {
        return ids;
    }

    // Caso B: parecen 1-based → convertir completo
    if (!hasZero) {
        vector<int> fixed;
        fixed.reserve(ids.size());
        for (int v : ids) fixed.push_back(v - 1);
        return fixed;
    }

    // Caso C: mezcla rara → corregir sólo lo que esté en 1..N
    vector<int> fixed;
    fixed.reserve(ids.size());
    for (int v : ids) {
        if (v > 0 && v <= nObjects) fixed.push_back(v - 1);
        else fixed.push_back(v);
    }
    return fixed;
}

// Construir vector<DataObject> para SPB-tree (IDs 0..N-1)
vector<DataObject> build_dataset_for_spb(size_t nObjects) {
    vector<DataObject> dataset;
    dataset.reserve(nObjects);

    for (size_t i = 0; i < nObjects; ++i) {
        DataObject o;
        o.id = static_cast<uint64_t>(i);   // 0-based, coherente con ObjectDB
        dataset.push_back(std::move(o));
    }
    return dataset;
}

int main() {
    srand(12345);

    // Crear carpetas de salida
    std::filesystem::create_directories("results");
    std::filesystem::create_directories("spb_indexes");

    for (const string &dataset : DATASETS) {

        // --------------------------------------------------------------------
        // Cargar dataset
        // --------------------------------------------------------------------
        string dbfile = path_dataset(dataset);
        if (dbfile == "") {
            cerr << "[WARN] Dataset no encontrado: " << dataset << "\n";
            continue;
        }

        unique_ptr<ObjectDB> db;

        if (dataset == "LA")
            db = make_unique<VectorDB>(dbfile, 2);        // L2
        else if (dataset == "Color")
            db = make_unique<VectorDB>(dbfile, 1);        // L1
        else if (dataset == "Synthetic")
            db = make_unique<VectorDB>(dbfile, 999999);   // L∞
        else if (dataset == "Words")
            db = make_unique<StringDB>(dbfile);           // Levenshtein
        else
            continue;

        cerr << "\n==========================================\n";
        cerr << "[SPB] Dataset: " << dataset
             << "   N=" << db->size() << "\n";
        cerr << "==========================================\n";

        // Queries y radios precomputados
        vector<int> queries = load_queries_file(path_queries(dataset));
        auto radii          = load_radii_file(path_radii(dataset));

        if (queries.empty()) {
            cerr << "[WARN] No queries for " << dataset << "\n";
            continue;
        }

        // Normalizar IDs de queries a 0-based
        queries = auto_fix_ids(queries, db->size());

        // Pivotes HFI precomputados (para l = NUM_PIVOTS)
        string pivPath = path_pivots(dataset, NUM_PIVOTS);
        vector<int> hfiPivots = load_queries_file(pivPath);
        if (hfiPivots.empty()) {
            cerr << "[WARN] No HFI pivots for " << dataset
                 << " (path=" << pivPath << "), usaré pivotes aleatorios.\n";
        } else {
            hfiPivots = auto_fix_ids(hfiPivots, db->size());
        }

        // --------------------------------------------------------------------
        // Archivo JSON de salida
        // --------------------------------------------------------------------
        string jsonOut = "results/results_SPB_" + dataset + ".json";
        ofstream J(jsonOut);
        J << "[\n";
        bool firstOutput = true;

        // --------------------------------------------------------------------
        // Construcción del SPB-tree como en Chen:
        //   - l = 5 pivotes (HFI, mismos para todos los índices)
        //   - RAF simulado (para contar pageReads)
        // --------------------------------------------------------------------
        cerr << "[BUILD] Construyendo SPB-tree (l=" << NUM_PIVOTS << ")...\n";

        string rafFile = "spb_indexes/" + dataset + "_raf.bin";

        SPBTree spb(
            rafFile,
            db.get(),
            NUM_PIVOTS,
            LEAF_CAPACITY,
            FANOUT,
            dataset,
            true   // useHfiPivots (si hay IDs)
        );

        // Construir todos los objetos
        vector<DataObject> allObjects = build_dataset_for_spb(db->size());

        // build() con pivotes HFI (si existen)
        spb.build(allObjects, hfiPivots);
        cerr << "[BUILD] OK.\n";

        // ====================================================================
        // MRQ (Range Queries) – RQA (Algorithm 3)
        // ====================================================================
        cerr << "\n[MRQ] Ejecutando selectividades...\n";

        for (double sel : SELECTIVITIES) {
            if (!radii.count(sel)) continue;

            double R = radii[sel];
            cerr << "  [MRQ] sel=" << sel << "  R=" << R << "\n";

            long long totalD = 0;   // distancias totales (pivot + verificación)
            long long totalT = 0;   // tiempo (µs)
            long long totalP = 0;   // page reads en RAF

            for (int q : queries) {
                spb.clear_counters();

                auto start = chrono::high_resolution_clock::now();
                vector<uint64_t> results = spb.MRQ(q, R);
                auto end   = chrono::high_resolution_clock::now();

                long long elapsed =
                    chrono::duration_cast<chrono::microseconds>(end - start).count();

                // En esta implementación, SPBTree::MRQ ya:
                //  - cuenta distancias a pivotes (φ(q), φ(o))
                //  - cuenta distancias reales d(q,o) en VerifyRQ
                totalD += spb.get_compDist();
                totalT += elapsed;
                totalP += spb.get_pageReads();

                (void)results; // si no quieres usar el resultado aquí
            }

            double avgD   = double(totalD) / queries.size();
            double avgTms = double(totalT) / (1000.0 * queries.size()); // µs → ms
            double avgPg  = double(totalP) / queries.size();

            if (!firstOutput) J << ",\n";
            firstOutput = false;

            J << std::fixed << std::setprecision(6)
              << "{"
              << "\"index\":\"SPBTree\","
              << "\"dataset\":\"" << dataset << "\","
              << "\"category\":\"SPB\","
              << "\"num_pivots\":" << NUM_PIVOTS << ","
              << "\"query_type\":\"MRQ\","
              << "\"selectivity\":" << sel << ","
              << "\"radius\":" << R << ","
              << "\"k\":null,"
              << "\"compdists\":" << avgD << ","
              << "\"time_ms\":" << avgTms << ","
              << "\"pages\":" << avgPg << ","
              << "\"n_queries\":" << queries.size() << ","
              << "\"run_id\":1"
              << "}";
        }

        // ====================================================================
        // MkNN – NNA (Algorithm 4)
        // ====================================================================
        cerr << "\n[MkNN] Ejecutando valores de k...\n";

        for (int k : K_VALUES) {
            cerr << "  [MkNN] k=" << k << "\n";

            long long totalD = 0;
            long long totalT = 0;
            long long totalP = 0;

            for (int q : queries) {
                spb.clear_counters();

                auto start = chrono::high_resolution_clock::now();
                auto knn   = spb.MkNN(q, k);
                auto end   = chrono::high_resolution_clock::now();

                long long elapsed =
                    chrono::duration_cast<chrono::microseconds>(end - start).count();

                // SPBTree::MkNN ya suma:
                //  - distancias a pivotes (φ(q), φ(o))
                //  - distancias reales d(q,o) de los candidatos
                totalD += spb.get_compDist();
                totalT += elapsed;
                totalP += spb.get_pageReads();

                (void)knn; // no necesitamos el contenido para el benchmark global
            }

            double avgD   = double(totalD) / queries.size();
            double avgTms = double(totalT) / (1000.0 * queries.size());
            double avgPg  = double(totalP) / queries.size();

            if (!firstOutput) J << ",\n";
            firstOutput = false;

            J << std::fixed << std::setprecision(6)
              << "{"
              << "\"index\":\"SPBTree\","
              << "\"dataset\":\"" << dataset << "\","
              << "\"category\":\"SPB\","
              << "\"num_pivots\":" << NUM_PIVOTS << ","
              << "\"query_type\":\"MkNN\","
              << "\"selectivity\":null,"
              << "\"radius\":null,"
              << "\"k\":" << k << ","
              << "\"compdists\":" << avgD << ","
              << "\"time_ms\":" << avgTms << ","
              << "\"pages\":" << avgPg << ","
              << "\"n_queries\":" << queries.size() << ","
              << "\"run_id\":1"
              << "}";
        }

        J << "\n]\n";
        cerr << "[DONE] Output generado: " << jsonOut << "\n";
    }

    return 0;
}
