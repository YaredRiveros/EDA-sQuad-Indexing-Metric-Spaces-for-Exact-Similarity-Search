// dsaclt_test.cpp - Benchmark de DSACL-tree (memoria principal, estilo Chen)

#include <bits/stdc++.h>
#include "DSATCLT.hpp"
#include "../../datasets/paths.hpp"
#include <filesystem>

using namespace std;

// ============================================================
// CONFIG (igual al paper Chen2022)
// ============================================================

// Selectividades para MRQ
static const vector<double> SELECTIVITIES = {0.02, 0.04, 0.08, 0.16, 0.32};

// Valores de k para MkNN
static const vector<int> K_VALUES = {5, 10, 20, 50, 100};

// Datasets evaluados
static const vector<string> DATASETS = {"LA", "Color", "Synthetic", "Words"};

// Parámetros del DSACL-tree (puedes ajustarlos según Chen)
static const int DSACLT_MAX_ARITY   = 32;
static const int DSACLT_K_CLUSTER   = 10;

// ============================================================
// dsaclt_test — ejecución completa
// ============================================================
int main() {
    ios::sync_with_stdio(false);
    cin.tie(nullptr);

    std::filesystem::create_directories("results");

    for (const string& dataset : DATASETS) {

        // -----------------------------------------
        // 1. Resolver dataset físico
        // -----------------------------------------
        string dbfile = path_dataset(dataset);
        if (dbfile.empty() || !file_exists(dbfile)) {
            cerr << "[WARN] Dataset not found: " << dataset << "\n";
            continue;
        }

        // -----------------------------------------
        // 2. Cargar dataset con la métrica correcta
        // -----------------------------------------
        unique_ptr<ObjectDB> db;

        if (dataset == "LA") {
            db = make_unique<VectorDB>(dbfile, 2);       // L2
        } else if (dataset == "Color") {
            db = make_unique<VectorDB>(dbfile, 1);       // L1
        } else if (dataset == "Synthetic") {
            db = make_unique<VectorDB>(dbfile, 999999);  // L∞
        } else if (dataset == "Words") {
            db = make_unique<StringDB>(dbfile);          // Levenshtein
        } else {
            cerr << "[WARN] DSACLT no se aplica a " << dataset << "\n";
            continue;
        }

        int nObjects = db->size();
        cerr << "\n==========================================\n";
        cerr << "[DSACLT] Dataset: " << dataset
             << "   N=" << nObjects << "\n";
        cerr << "==========================================\n";

        if (nObjects == 0) {
            cerr << "[WARN] Empty dataset\n";
            continue;
        }

        // -----------------------------------------
        // 3. Cargar queries (IDs) y radios
        // -----------------------------------------
        vector<int> queries = load_queries_file(path_queries(dataset));
        auto radii          = load_radii_file(path_radii(dataset));

        if (queries.empty()) {
            cerr << "[WARN] No queries found\n";
            continue;
        }

        cerr << "[QUERIES] " << queries.size() << " queries cargadas\n";
        cerr << "[QUERIES] Radii para " << radii.size()
             << " selectividades\n";

        // -----------------------------------------
        // 4. Construir índice DSACLT
        // -----------------------------------------
        cerr << "\n------------------------------------------\n";
        cerr << "[INFO] Construyendo DSACLT para " << dataset
             << " (MaxArity=" << DSACLT_MAX_ARITY
             << ", kCluster=" << DSACLT_K_CLUSTER << ")\n";
        cerr << "------------------------------------------\n";

        DSACLT index(db.get(), DSACLT_MAX_ARITY, DSACLT_K_CLUSTER);

        using clock = std::chrono::high_resolution_clock;
        auto tBuild0 = clock::now();
        index.build();
        auto tBuild1 = clock::now();

        double buildMs = std::chrono::duration_cast<
                             std::chrono::microseconds>(tBuild1 - tBuild0).count()
                         / 1000.0;

        cerr << "[BUILD] Tiempo construcción: " << buildMs << " ms\n";

        // -----------------------------------------
        // 5. Archivo JSON de salida
        // -----------------------------------------
        string jsonOut = "results/results_DSACLT_" + dataset + ".json";
        ofstream J(jsonOut);
        if (!J.is_open()) {
            cerr << "[ERROR] Cannot create output JSON\n";
            continue;
        }

        J << "[\n";
        bool first = true;

        // ====================================================
        // MRQ
        // ====================================================
        cerr << "\n[MRQ] Ejecutando selectividades...\n";
        for (double sel : SELECTIVITIES) {
            if (!radii.count(sel)) continue;

            double R = radii[sel];
            long long totalD = 0;
            long long totalT = 0;
            long long totalPages = 0;

            cerr << "  sel=" << sel << " (R=" << R << ")... " << flush;

            for (int q : queries) {
                index.clear_counters();

                auto t0 = clock::now();
                auto out = index.MRQ(q, R);
                auto t1 = clock::now();

                (void)out; // solo nos importan contadores

                totalD     += index.get_compDist();
                totalT     += chrono::duration_cast<
                                  chrono::microseconds>(t1 - t0).count();
                totalPages += index.get_pageReads();
            }

            double avgD     = double(totalD) / queries.size();
            double avgTms   = double(totalT) / (1000.0 * queries.size());
            double avgPages = double(totalPages) / queries.size();

            if (!first) J << ",\n";
            first = false;

            J << fixed << setprecision(6)
              << "{"
              << "\"index\":\"DSACLT\","
              << "\"dataset\":\"" << dataset << "\","
              << "\"category\":\"DM\","
              << "\"num_pivots\":null,"              // DSACLT no usa pivotes
              << "\"num_centers_path\":null,"        // no aplica
              << "\"arity\":" << DSACLT_MAX_ARITY << ","
              << "\"query_type\":\"MRQ\","
              << "\"selectivity\":" << sel << ","
              << "\"radius\":" << R << ","
              << "\"k\":null,"
              << "\"compdists\":" << avgD << ","
              << "\"time_ms\":" << avgTms << ","
              << "\"pages\":" << avgPages << ","
              << "\"n_queries\":" << queries.size() << ","
              << "\"run_id\":1"
              << "}";

            cerr << "OK (avg " << (long long)avgD
                 << " compdists, " << avgPages << " páginas)\n";
        }

        // ====================================================
        // MkNN
        // ====================================================
        cerr << "\n[MkNN] Ejecutando valores de k...\n";
        for (int k : K_VALUES) {
            long long totalD = 0;
            long long totalT = 0;
            long long totalPages = 0;

            cerr << "  k=" << k << "... " << flush;

            for (int q : queries) {
                index.clear_counters();

                auto t0 = clock::now();
                auto out = index.MkNN(q, k);
                auto t1 = clock::now();

                (void)out;

                totalD     += index.get_compDist();
                totalT     += chrono::duration_cast<
                                  chrono::microseconds>(t1 - t0).count();
                totalPages += index.get_pageReads();
            }

            double avgD     = double(totalD) / queries.size();
            double avgTms   = double(totalT) / (1000.0 * queries.size());
            double avgPages = double(totalPages) / queries.size();

            if (!first) J << ",\n";
            first = false;

            J << fixed << setprecision(6)
              << "{"
              << "\"index\":\"DSACLT\","
              << "\"dataset\":\"" << dataset << "\","
              << "\"category\":\"DM\","
              << "\"num_pivots\":null,"
              << "\"num_centers_path\":null,"
              << "\"arity\":" << DSACLT_MAX_ARITY << ","
              << "\"query_type\":\"MkNN\","
              << "\"selectivity\":null,"
              << "\"radius\":null,"
              << "\"k\":" << k << ","
              << "\"compdists\":" << avgD << ","
              << "\"time_ms\":" << avgTms << ","
              << "\"pages\":" << avgPages << ","
              << "\"n_queries\":" << queries.size() << ","
              << "\"run_id\":1"
              << "}";

            cerr << "OK (avg " << (long long)avgD
                 << " compdists, " << avgPages << " páginas)\n";
        }

        J << "\n]\n";
        J.close();
        cerr << "[DONE] " << jsonOut << "\n";
    }

    return 0;
}
