// test.cpp - OmniR-tree benchmark (escenario memoria secundaria de Chen)
// - l = 5 pivotes fijos
// - page size lógico = 4KB (definido en omnirtree.hpp)
// - formato JSON compatible con DIndex / EGNAT

#include <bits/stdc++.h>
#include <filesystem>
#include "omnirtree.hpp"
#include "../../objectdb.hpp"
#include "../../datasets/paths.hpp"

using namespace std;

// Selectividades del paper (Chen 2022)
static const vector<double> SELECTIVITIES = {0.02, 0.04, 0.08, 0.16, 0.32};

// Ks para MkNN
static const vector<int> K_VALUES = {5, 10, 20, 50, 100};

// Datasets del entorno experimental
static const vector<string> DATASETS = {"LA", "Words", "Color", "Synthetic"};

int main() {
    srand(12345);

    std::filesystem::create_directories("results");
    std::filesystem::create_directories("omni_indexes");

    for (const string &dataset : DATASETS) {

        // ---------------------------------------------------------------
        // Cargar dataset
        // ---------------------------------------------------------------
        string dbfile = path_dataset(dataset);
        if (dbfile == "") {
            cerr << "[WARN] Dataset no encontrado: " << dataset << "\n";
            continue;
        }

        unique_ptr<ObjectDB> db;

        if (dataset == "LA")
            db = make_unique<VectorDB>(dbfile, 2);
        else if (dataset == "Color")
            db = make_unique<VectorDB>(dbfile, 1);
        else if (dataset == "Synthetic")
            db = make_unique<VectorDB>(dbfile, 999999);
        else if (dataset == "Words")
            db = make_unique<StringDB>(dbfile);
        else {
            cerr << "[WARN] Dataset no soportado: " << dataset << "\n";
            continue;
        }

        cerr << "\n==========================================\n";
        cerr << "[OmniR-tree] Dataset: " << dataset
             << "   N=" << db->size() << "\n";
        cerr << "==========================================\n";

        // ---------------------------------------------------------------
        // Cargar queries y radios precomputados
        // ---------------------------------------------------------------
        vector<int> queries = load_queries_file(path_queries(dataset));
        auto radii          = load_radii_file(path_radii(dataset));

        if (queries.empty()) {
            cerr << "[WARN] No hay queries para " << dataset << "\n";
            continue;
        }

        // ---------------------------------------------------------------
        // Archivo JSON de salida
        // ---------------------------------------------------------------
        string jsonOut = "results/results_OmniRTree_" + dataset + ".json";
        ofstream J(jsonOut);
        J << "[\n";
        bool firstOutput = true;

        // ===============================================================
        // Configuración fija de memoria secundaria (Chen 2022):
        //   - l = 5 pivotes
        //   - mismos pivotes HFI para todos los métodos pivot-based
        // ===============================================================
        const int num_pivots     = 5;
        const int rtree_node_cap = 32;   // capacidad de nodo del R-tree

        cerr << "\n------------------------------------------\n";
        cerr << "[CONFIG] num_pivots = " << num_pivots << " (memoria secundaria)\n";
        cerr << "------------------------------------------\n";

        // Archivo RAF (uno por dataset + l)
        string rafFile = "omni_indexes/" + dataset + "_l" +
                         to_string(num_pivots) + "_raf.bin";

        // Crear OmniR-tree
        OmniRTree omni(rafFile, db.get(), num_pivots, rtree_node_cap);

        // Pivotes HFI precomputados (mismos para todos los índices pivot-based)
        string pivotsFile = path_pivots(dataset, num_pivots);
        if (pivotsFile == "") {
            cerr << "[WARN] No hay pivotes HFI para "
                 << dataset << " con l=" << num_pivots << "\n";
            J << "\n]\n";
            J.close();
            continue;
        }

        cerr << "[BUILD] Construyendo OmniR-tree (l="
             << num_pivots << ") con pivotes HFI...\n";

        auto t0 = chrono::high_resolution_clock::now();
        omni.build(pivotsFile);
        auto t1 = chrono::high_resolution_clock::now();
        auto buildTimeMs =
            chrono::duration_cast<chrono::milliseconds>(t1 - t0).count();

        cerr << "[BUILD] Completado en " << buildTimeMs << " ms\n";

        // ===============================================================
        // MRQ - Variar selectividad (escenario de memoria secundaria)
        // ===============================================================
        cerr << "\n[MRQ] Ejecutando queries de rango (l="
             << num_pivots << ")...\n";

        for (double sel : SELECTIVITIES) {
            if (!radii.count(sel)) {
                cerr << "  [SKIP] Selectividad " << sel
                     << " no disponible para " << dataset << "\n";
                continue;
            }

            double R = radii[sel];
            long long totalD = 0;       // distancias totales (pivotes + verificación)
            long long totalT = 0;       // tiempo total (µs)
            long long totalPages = 0;   // page reads promedio

            cerr << "  sel=" << sel << " (R=" << R << ")... " << flush;

            for (int q : queries) {
                vector<int> out;
                omni.clear_counters();

                auto qt0 = chrono::high_resolution_clock::now();
                omni.rangeSearch(q, R, out);
                auto qt1 = chrono::high_resolution_clock::now();

                totalD     += omni.get_compDist();
                totalT     += chrono::duration_cast<
                                  chrono::microseconds>(qt1 - qt0).count();
                totalPages += omni.get_pageReads();
            }

            double avgD   = double(totalD)     / queries.size();
            double avgTms = double(totalT)     / (1000.0 * queries.size());
            double avgPA  = double(totalPages) / queries.size();

            if (!firstOutput) J << ",\n";
            firstOutput = false;

            J << fixed << setprecision(6)
              << "{"
              << "\"index\":\"OmniR-tree\","
              << "\"dataset\":\"" << dataset << "\","
              << "\"category\":\"DM\","          // Distance Mapping / pivot-based
              << "\"num_pivots\":" << num_pivots << ","
              << "\"num_centers_path\":null,"
              << "\"arity\":null,"
              << "\"query_type\":\"MRQ\","
              << "\"selectivity\":" << sel << ","
              << "\"radius\":" << R << ","
              << "\"k\":null,"
              << "\"compdists\":" << avgD << ","
              << "\"time_ms\":" << avgTms << ","
              << "\"pages\":" << avgPA << ","
              << "\"n_queries\":" << queries.size() << ","
              << "\"run_id\":1"
              << "}";

            cerr << "OK (avg " << (long long)avgD
                 << " compdists, " << avgPA << " páginas)\n";
        }

        // ===============================================================
        // MkNN - Variar k (escenario de memoria secundaria)
        // ===============================================================
        cerr << "\n[MkNN] Ejecutando queries k-NN (l="
             << num_pivots << ")...\n";

        for (int k : K_VALUES) {
            long long totalD = 0;
            long long totalT = 0;
            long long totalPages = 0;

            cerr << "  k=" << k << "... " << flush;

            for (int q : queries) {
                vector<pair<double,int>> out;
                omni.clear_counters();

                auto qt0 = chrono::high_resolution_clock::now();
                omni.knnSearch(q, k, out);
                auto qt1 = chrono::high_resolution_clock::now();

                totalD     += omni.get_compDist();
                totalT     += chrono::duration_cast<
                                  chrono::microseconds>(qt1 - qt0).count();
                totalPages += omni.get_pageReads();
            }

            double avgD   = double(totalD)     / queries.size();
            double avgTms = double(totalT)     / (1000.0 * queries.size());
            double avgPA  = double(totalPages) / queries.size();

            if (!firstOutput) J << ",\n";
            firstOutput = false;

            J << fixed << setprecision(6)
              << "{"
              << "\"index\":\"OmniR-tree\","
              << "\"dataset\":\"" << dataset << "\","
              << "\"category\":\"DM\","
              << "\"num_pivots\":" << num_pivots << ","
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

            cerr << "OK (avg " << (long long)avgD
                 << " compdists, " << avgPA << " páginas)\n";
        }

        J << "\n]\n";
        J.close();

        cerr << "\n[DONE] Archivo generado: " << jsonOut << "\n";
        cerr << "==========================================\n";
    }

    return 0;
}
