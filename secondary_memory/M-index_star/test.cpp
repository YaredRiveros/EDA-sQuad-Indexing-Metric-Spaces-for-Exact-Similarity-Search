#include <bits/stdc++.h>
#include "mindex.hpp"
#include "../../objectdb.hpp"
#include "../../datasets/paths.hpp"

using namespace std;

// Selectividades y k-values según el paper
static const vector<double> SELECTIVITIES = {0.02, 0.04, 0.08, 0.16, 0.32};
static const vector<int>    K_VALUES      = {5, 10, 20, 50, 100};
static const vector<int>    PIVOT_VALUES  = {3, 5, 10, 15, 20};

// ------------------------------------------------------------
// Cargar pivotes HFI desde JSON (igual que en otros tests)
// ------------------------------------------------------------
vector<int> load_pivots_json(const string& path) {
    vector<int> piv;

    if (path == "" || !file_exists(path)) {
        cerr << "[WARN] Pivot JSON not found: " << path << "\n";
        return piv;
    }

    ifstream f(path);
    string tok;
    while (f >> tok) {
        tok.erase(remove_if(tok.begin(), tok.end(),
                            [](char c){
                                return c=='['||c==']'||c==','||c=='"'||c==':';
                            }),
                  tok.end());

        if (!tok.empty() && all_of(tok.begin(), tok.end(), ::isdigit))
            piv.push_back(stoi(tok));
    }
    return piv;
}

void test_dataset(const string& dataset) {
    cout << "\n\n";
    cout << "##########################################\n";
    cout << "### TESTING DATASET: " << dataset << "\n";
    cout << "### (M-Index* MEJORADO)\n";
    cout << "##########################################\n";

    string dbfile = path_dataset(dataset);

    unique_ptr<ObjectDB> db;
    if (dataset == "LA") {
        db = make_unique<VectorDB>(dbfile, 2);
    } else if (dataset == "Color") {
        db = make_unique<VectorDB>(dbfile, 1);
    } else if (dataset == "Synthetic") {
        db = make_unique<VectorDB>(dbfile, 999999); // L∞ como en otros tests
    } else if (dataset == "Words") {
        db = make_unique<StringDB>(dbfile);
    } else {
        cout << "[ERROR] Dataset desconocido: " << dataset << "\n";
        return;
    }

    cout << "\n==========================================\n";
    cout << "[M-Index*] Dataset: " << dataset
         << "   N=" << db->size() << "\n";
    cout << "==========================================\n";

    // Cargar queries y radii
    vector<int> queries = load_queries_file(path_queries(dataset));
    auto radii          = load_radii_file(path_radii(dataset));

    if (queries.empty()) {
        cout << "[WARN] No hay queries para " << dataset << "\n";
        return;
    }

    cout << "\n[QUERIES] Cargadas " << queries.size() << " queries\n";
    cout << "[QUERIES] Radii para " << radii.size() << " selectividades\n";

    // Crear directorios
    filesystem::create_directories("midx_indexes");
    filesystem::create_directories("results");

    string jsonOut = "results/results_MIndex_" + dataset + ".json";
    ofstream J(jsonOut);
    J << "[\n";
    bool firstOutput = true;

    // Valores por defecto (para EXP 2 y EXP 3)
    const int    DEFAULT_PIVOTS      = 5;
    const double DEFAULT_SELECTIVITY = 0.08;
    const int    DEFAULT_K           = 20;

    auto t0 = chrono::high_resolution_clock::now();
    auto t1 = chrono::high_resolution_clock::now();

    // ===================================================================
    // EXPERIMENTO 1: Variar número de pivotes (sel = 0.08 fija)
    // ===================================================================
    cout << "\n========================================\n";
    cout << "[EXP 1] Variando PIVOTES (sel=" << DEFAULT_SELECTIVITY << " fijo)\n";
    cout << "========================================\n";

    if (!radii.count(DEFAULT_SELECTIVITY)) {
        cout << "[ERROR] Selectividad por defecto " << DEFAULT_SELECTIVITY
             << " no disponible\n";
    } else {
        double radius = radii[DEFAULT_SELECTIVITY];

        for (int numPivots : PIVOT_VALUES) {
            cout << "\n[BUILD] Construyendo con " << numPivots << " pivotes...\n";

            // Cargar pivotes HFI
            string pivPath = path_pivots(dataset, numPivots);
            vector<int> pivots = load_pivots_json(pivPath);
            if ((int)pivots.size() != numPivots) {
                cout << "[WARN] Pivotes HFI para P=" << numPivots
                     << " no disponibles o incompletos. SKIP.\n";
                continue;
            }

            MIndex_Improved midx(db.get(), numPivots);
            midx.overridePivots(pivots);   // usar HFI
            string base = "midx_indexes/" + dataset + "_p" + to_string(numPivots);

            t0 = chrono::high_resolution_clock::now();
            midx.build(base);
            t1 = chrono::high_resolution_clock::now();
            auto buildTime =
                chrono::duration_cast<chrono::milliseconds>(t1 - t0).count();

            cout << "[BUILD] Tiempo: " << buildTime << " ms\n";

            // Ejecutar MRQ con selectividad fija
            long long totalD = 0, totalT = 0, totalPages = 0;

            cout << "  Ejecutando MRQ (sel=" << DEFAULT_SELECTIVITY
                 << ", R=" << radius << ")... " << flush;

            for (size_t i = 0; i < queries.size(); i++) {
                vector<int> out;
                midx.clear_counters();

                t0 = chrono::high_resolution_clock::now();
                midx.rangeSearch(queries[i], radius, out);
                t1 = chrono::high_resolution_clock::now();

                totalD     += midx.get_compDist();
                totalT     += chrono::duration_cast<chrono::microseconds>(t1 - t0).count();
                totalPages += midx.get_pageReads();
            }

            if (!firstOutput) J << ",\n";
            firstOutput = false;

            double avgD   = double(totalD) / queries.size();
            double avgTms = double(totalT) / (1000.0 * queries.size());
            double avgPA  = double(totalPages) / queries.size();

            J << fixed << setprecision(6)
              << "{"
              << "\"index\":\"MIndex*\","
              << "\"dataset\":\"" << dataset << "\","
              << "\"category\":\"DM\","
              << "\"num_pivots\":" << numPivots << ","
              << "\"num_centers_path\":null,"
              << "\"arity\":null,"
              << "\"query_type\":\"MRQ\","
              << "\"selectivity\":" << DEFAULT_SELECTIVITY << ","
              << "\"radius\":" << radius << ","
              << "\"k\":null,"
              << "\"compdists\":" << avgD << ","
              << "\"time_ms\":" << avgTms << ","
              << "\"pages\":" << avgPA << ","
              << "\"n_queries\":" << queries.size() << ","
              << "\"run_id\":1"
              << "}";
              
            cout << " OK (" << (int)avgD << " compdists)\n";
        }
    }

    // ===================================================================
    // EXPERIMENTO 2: Variar selectividad en MRQ (pivotes=5)
    // ===================================================================
    cout << "\n========================================\n";
    cout << "[EXP 2] Variando SELECTIVIDAD (pivotes=" << DEFAULT_PIVOTS << " fijo)\n";
    cout << "========================================\n";

    {
        cout << "\n[BUILD] Construyendo con " << DEFAULT_PIVOTS << " pivotes...\n";

        // Cargar pivotes HFI P=5
        string pivPath = path_pivots(dataset, DEFAULT_PIVOTS);
        vector<int> pivots = load_pivots_json(pivPath);
        if ((int)pivots.size() != DEFAULT_PIVOTS) {
            cout << "[WARN] Pivotes HFI para P=" << DEFAULT_PIVOTS
                 << " no disponibles o incompletos. SKIP EXP 2.\n";
        } else {
            MIndex_Improved midx(db.get(), DEFAULT_PIVOTS);
            midx.overridePivots(pivots);
            string base = "midx_indexes/" + dataset + "_p" + to_string(DEFAULT_PIVOTS);

            t0 = chrono::high_resolution_clock::now();
            midx.build(base);
            t1 = chrono::high_resolution_clock::now();
            auto buildTime =
                chrono::duration_cast<chrono::milliseconds>(t1 - t0).count();

            cout << "[BUILD] Tiempo: " << buildTime << " ms\n";
            cout << "\n[MRQ] Ejecutando Range Queries variando selectividad...\n";

            for (double selectivity : SELECTIVITIES) {
                if (!radii.count(selectivity)) {
                    cout << "  [SKIP] Selectividad " << selectivity << " no disponible\n";
                    continue;
                }

                double radius = radii[selectivity];
                long long totalD = 0, totalT = 0, totalPages = 0;

                cout << "  sel=" << selectivity << " (R=" << radius << ")... " << flush;

                for (size_t i = 0; i < queries.size(); i++) {
                    vector<int> out;
                    midx.clear_counters();

                    t0 = chrono::high_resolution_clock::now();
                    midx.rangeSearch(queries[i], radius, out);
                    t1 = chrono::high_resolution_clock::now();

                    totalD     += midx.get_compDist();
                    totalT     += chrono::duration_cast<chrono::microseconds>(t1 - t0).count();
                    totalPages += midx.get_pageReads();
                }

                if (!firstOutput) J << ",\n";
                firstOutput = false;

                double avgD   = double(totalD) / queries.size();
                double avgTms = double(totalT) / (1000.0 * queries.size());
                double avgPA  = double(totalPages) / queries.size();

                J << fixed << setprecision(6)
                  << "{"
                  << "\"index\":\"MIndex*\","
                  << "\"dataset\":\"" << dataset << "\","
                  << "\"category\":\"DM\","
                  << "\"num_pivots\":" << DEFAULT_PIVOTS << ","
                  << "\"num_centers_path\":null,"
                  << "\"arity\":null,"
                  << "\"query_type\":\"MRQ\","
                  << "\"selectivity\":" << selectivity << ","
                  << "\"radius\":" << radius << ","
                  << "\"k\":null,"
                  << "\"compdists\":" << avgD << ","
                  << "\"time_ms\":" << avgTms << ","
                  << "\"pages\":" << avgPA << ","
                  << "\"n_queries\":" << queries.size() << ","
                  << "\"run_id\":1"
                  << "}";
                  
                cout << " OK (" << (int)avgD << " compdists)\n";
            }
        }
    }

    // ===================================================================
    // EXPERIMENTO 3: Variar k en MkNN (pivotes=5)
    // ===================================================================
    cout << "\n========================================\n";
    cout << "[EXP 3] Variando K en MkNN (pivotes=" << DEFAULT_PIVOTS << " fijo)\n";
    cout << "========================================\n";

    {
        cout << "\n[BUILD] Usando índice con " << DEFAULT_PIVOTS << " pivotes...\n";

        string pivPath = path_pivots(dataset, DEFAULT_PIVOTS);
        vector<int> pivots = load_pivots_json(pivPath);
        if ((int)pivots.size() != DEFAULT_PIVOTS) {
            cout << "[WARN] Pivotes HFI para P=" << DEFAULT_PIVOTS
                 << " no disponibles o incompletos. SKIP EXP 3.\n";
        } else {
            MIndex_Improved midx(db.get(), DEFAULT_PIVOTS);
            midx.overridePivots(pivots);
            string base = "midx_indexes/" + dataset + "_p" + to_string(DEFAULT_PIVOTS);
            midx.build(base);

            cout << "\n[MkNN] Ejecutando k-NN Queries variando k...\n";

            for (int k : K_VALUES) {
                long long totalD = 0, totalT = 0, totalPages = 0;

                cout << "  k=" << k << "... " << flush;

                for (size_t i = 0; i < queries.size(); i++) {
                    vector<pair<double,int>> out;
                    midx.clear_counters();

                    t0 = chrono::high_resolution_clock::now();
                    midx.knnSearch(queries[i], k, out);
                    t1 = chrono::high_resolution_clock::now();

                    totalD     += midx.get_compDist();
                    totalT     += chrono::duration_cast<chrono::microseconds>(t1 - t0).count();
                    totalPages += midx.get_pageReads();
                }

                if (!firstOutput) J << ",\n";
                firstOutput = false;

                double avgD   = double(totalD) / queries.size();
                double avgTms = double(totalT) / (1000.0 * queries.size());
                double avgPA  = double(totalPages) / queries.size();

                J << fixed << setprecision(6)
                  << "{"
                  << "\"index\":\"MIndex*\","
                  << "\"dataset\":\"" << dataset << "\","
                  << "\"category\":\"DM\","
                  << "\"num_pivots\":" << DEFAULT_PIVOTS << ","
                  << "\"num_centers_path\":null,"
                  << "\"arity\":null,"
                  << "\"query_type\":\"MkNN\","
                  << "\"selectivity\":null,"
                  << "\"radius\":null,"
                  << "\"k\":" << k << ","
                  << "\"compdists\":" << avgD << ","
                  << "\"time_ms\":" << avgTms << ","
                  << "\"pages\":" << avgPA << ","
                  << "\"n_queries\":" << queries.size() << ","
                  << "\"run_id\":1"
                  << "}";
                  
                cout << " OK (" << (int)avgD << " compdists)\n";
            }
        }
    }

    J << "\n]\n";
    J.close();

    cout << "\n[JSON] Archivo generado: " << jsonOut << "\n";

    cout << "\n==========================================\n";
    cout << "[M-Index*] " << dataset << " completado\n";
    cout << "==========================================\n";
}

int main() {
    srand(12345);

    vector<string> datasets = {"LA", "Words", "Color", "Synthetic"};

    for (const auto& dataset : datasets) {
        try {
            test_dataset(dataset);
        } catch (const exception& e) {
            cout << "\n[ERROR] al procesar " << dataset << ": " << e.what() << "\n";
        }
    }

    cout << "\n\n";
    cout << "##########################################\n";
    cout << "### TODAS LAS PRUEBAS COMPLETADAS\n";
    cout << "##########################################\n";

    return 0;
}
