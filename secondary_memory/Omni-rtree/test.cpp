// test.cpp - OmniR-tree benchmark siguiendo formato EGNAT
#include <bits/stdc++.h>
#include "omnirtree.hpp"
#include "../../objectdb.hpp"
#include "../../datasets/paths.hpp"

using namespace std;

// Selectividades del paper
static const vector<double> SELECTIVITIES = {0.02, 0.04, 0.08, 0.16, 0.32};

// Ks para MkNN
static const vector<int> K_VALUES = {5, 10, 20, 50, 100};

// Datasets del paper
static const vector<string> DATASETS = {"LA", "Words", "Color", "Synthetic"};

int main() {
    srand(12345);

    filesystem::create_directories("results");

    for (const string &dataset : DATASETS) {

        string dbfile = path_dataset(dataset);
        if (dbfile == "") {
            cerr << "[WARN] Dataset no encontrado: " << dataset << "\n";
            continue;
        }

        unique_ptr<ObjectDB> db;

        if (dataset == "LA")              db = make_unique<VectorDB>(dbfile, 2);
        else if (dataset == "Synthetic")  db = make_unique<VectorDB>(dbfile, 0);
        else if (dataset == "Words")      db = make_unique<StringDB>(dbfile);
        else continue;

        cerr << "\n==========================================\n";
        cerr << "[OmniR-tree] Dataset: " << dataset
             << "   N=" << db->size() << "\n";
        cerr << "==========================================\n";

        vector<int> queries = load_queries_file(path_queries(dataset));
        auto radii = load_radii_file(path_radii(dataset));

        if (queries.empty()) {
            cerr << "[WARN] No hay queries para " << dataset << "\n";
            continue;
        }

        string jsonOut = "results/results_OmniRTree_" + dataset + ".json";
        ofstream J(jsonOut);
        J << "[\n";
        bool firstOutput = true;

        // Configuración fija de OmniR-tree (como EGNAT usa config fija)
        // Usamos 8 pivotes (valor intermedio razonable)
        int num_pivots = 8;
        int rtree_node_cap = 32;

        // ---- Crear OmniR-tree ----
        OmniRTree omni(db.get(), num_pivots, rtree_node_cap);

        cerr << "[BUILD] Construyendo OmniR-tree con " << num_pivots << " pivotes...\n";
        string indexBase = "omni_index_" + dataset;
        
        auto t0 = chrono::high_resolution_clock::now();
        omni.build(indexBase);
        auto t1 = chrono::high_resolution_clock::now();
        auto buildTime = chrono::duration_cast<chrono::milliseconds>(t1 - t0).count();
        
        cerr << "[BUILD] Completado en " << buildTime << " ms\n";

        // ===================================================================
        // MRQ - Variar selectividad
        // ===================================================================
        cerr << "\n[MRQ] Ejecutando queries de rango...\n";
        for (double sel : SELECTIVITIES) {
            if (!radii.count(sel)) {
                cerr << "  [SKIP] Selectividad " << sel << " no disponible\n";
                continue;
            }

            double R = radii[sel];
            long long totalD = 0, totalT = 0, totalPages = 0;

            cerr << "  sel=" << sel << " (R=" << R << ")... " << flush;

            for (int q : queries) {
                vector<int> out;
                omni.clear_counters();
                
                auto qt0 = chrono::high_resolution_clock::now();
                omni.rangeSearch(q, R, out);
                auto qt1 = chrono::high_resolution_clock::now();

                totalD     += omni.get_compDist();
                totalT     += chrono::duration_cast<chrono::microseconds>(qt1 - qt0).count();
                totalPages += omni.get_pageReads();
            }

            if (!firstOutput) J << ",\n";
            firstOutput = false;

            double avgD   = double(totalD)     / queries.size();
            double avgTms = double(totalT)     / (1000.0 * queries.size());
            double avgPA  = double(totalPages) / queries.size();

            J << fixed << setprecision(6)
              << "{"
              << "\"index\":\"OmniR-tree\","
              << "\"dataset\":\"" << dataset << "\","
              << "\"category\":\"DM\","
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

            cerr << "OK (avg " << (int)avgD << " compdists)\n";
        }

        // ===================================================================
        // MkNN - Variar k
        // ===================================================================
        cerr << "\n[MkNN] Ejecutando queries k-NN...\n";
        for (int k : K_VALUES) {
            long long totalD = 0, totalT = 0, totalPages = 0;

            cerr << "  k=" << k << "... " << flush;

            for (int q : queries) {
                vector<pair<double,int>> out;
                omni.clear_counters();
                
                auto qt0 = chrono::high_resolution_clock::now();
                omni.knnSearch(q, k, out);
                auto qt1 = chrono::high_resolution_clock::now();

                totalD     += omni.get_compDist();
                totalT     += chrono::duration_cast<chrono::microseconds>(qt1 - qt0).count();
                totalPages += omni.get_pageReads();
            }

            if (!firstOutput) J << ",\n";
            firstOutput = false;

            double avgD   = double(totalD)     / queries.size();
            double avgTms = double(totalT)     / (1000.0 * queries.size());
            double avgPA  = double(totalPages) / queries.size();

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

            cerr << "OK (avg " << (int)avgD << " compdists)\n";
        }

        J << "\n]\n";
        J.close();
        
        cerr << "\n[DONE] Archivo generado: " << jsonOut << "\n";
        cerr << "==========================================\n";
    }

    return 0;
}
