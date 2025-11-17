#include <bits/stdc++.h>
#include "mindex_disk.hpp"
#include "../../objectdb.hpp"
#include "../../datasets/paths.hpp"

using namespace std;

// Selectividades y k-values como en EGNAT
static const vector<double> SELECTIVITIES = {0.0001, 0.001, 0.01, 0.02, 0.05};
static const vector<int> K_VALUES = {1, 5, 10, 20, 50};

void test_dataset(const string& dataset) {
    cout << "\n\n";
    cout << "##########################################\n";
    cout << "### TESTING DATASET: " << dataset << "\n";
    cout << "##########################################\n";

    string dbfile = path_dataset(dataset);
    
    unique_ptr<ObjectDB> db;
    if (dataset == "LA") {
        db = make_unique<VectorDB>(dbfile, 2);
    } else if (dataset == "Synthetic") {
        db = make_unique<VectorDB>(dbfile, 2);
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

    // Parámetros
    int numPivots = 5;

    // Crear M-Index
    MIndex_Disk midx(db.get(), numPivots);
    string base = "midx_indexes/" + dataset;

    // Crear directorio si no existe
    filesystem::create_directories("midx_indexes");

    // Build
    auto t0 = chrono::high_resolution_clock::now();
    midx.build(base);
    auto t1 = chrono::high_resolution_clock::now();
    auto buildTime = chrono::duration_cast<chrono::milliseconds>(t1 - t0).count();
    
    cout << "\n[BUILD] Tiempo: " << buildTime << " ms\n";
    cout << "[BUILD] Índice guardado en: " << base << ".midx_raf\n";

    // Cargar queries
    vector<int> queries = load_queries_file(path_queries(dataset));
    auto radii = load_radii_file(path_radii(dataset));
    
    if (queries.empty()) {
        cout << "[WARN] No hay queries para " << dataset << "\n";
        return;
    }
    
    cout << "\n[QUERIES] Cargadas " << queries.size() << " queries\n";
    cout << "[QUERIES] Radii para " << radii.size() << " selectividades\n";

    // Crear archivo JSON de salida
    filesystem::create_directories("results");
    string jsonOut = "results/results_MIndex_" + dataset + ".json";
    ofstream J(jsonOut);
    J << "[\n";
    bool firstOutput = true;

    // ===================================================================
    // MRQ - Range Queries
    // ===================================================================
    cout << "\n=== GENERANDO RESULTADOS JSON ===\n";
    cout << "Ejecutando Range Queries...\n";
    
    for (double selectivity : SELECTIVITIES) {
        if (!radii.count(selectivity)) {
            cout << "  [SKIP] Selectividad " << selectivity << " no disponible\n";
            continue;
        }

        double radius = radii[selectivity];
        long long totalD = 0, totalT = 0, totalPages = 0;
        
        cout << "  [Range] Selectividad " << selectivity << " (radius=" << radius << ")... " << flush;

        for (size_t i = 0; i < queries.size(); i++) {
            vector<int> out;
            midx.clear_counters();
            
            t0 = chrono::high_resolution_clock::now();
            midx.rangeSearch(queries[i], radius, out);
            t1 = chrono::high_resolution_clock::now();
            
            totalD += midx.get_compDist();
            totalT += chrono::duration_cast<chrono::microseconds>(t1 - t0).count();
            totalPages += midx.get_pageReads();
            
            // Progress indicator cada 20 queries
            if ((i+1) % 20 == 0) cout << "." << flush;
        }

        if (!firstOutput) J << ",\n";
        firstOutput = false;

        double avgD = double(totalD) / queries.size();
        double avgTms = double(totalT) / (1000.0 * queries.size());
        double avgPA = double(totalPages) / queries.size();

        J << fixed << setprecision(6)
          << "{"
          << "\"index\":\"MIndex*\","
          << "\"dataset\":\"" << dataset << "\","
          << "\"category\":\"DM\","
          << "\"num_pivots\":" << numPivots << ","
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
          
        cout << " OK (" << (int)avgD << " compdists, " << avgTms << " ms)\n";
    }

    // ===================================================================
    // MkNN - k-NN Queries
    // ===================================================================
    cout << "\nEjecutando k-NN Queries...\n";
    
    for (int k : K_VALUES) {
        long long totalD = 0, totalT = 0, totalPages = 0;
        
        cout << "  [k-NN] k=" << k << "... " << flush;

        for (size_t i = 0; i < queries.size(); i++) {
            vector<pair<double,int>> out;
            midx.clear_counters();
            
            t0 = chrono::high_resolution_clock::now();
            midx.knnSearch(queries[i], k, out);
            t1 = chrono::high_resolution_clock::now();
            
            totalD += midx.get_compDist();
            totalT += chrono::duration_cast<chrono::microseconds>(t1 - t0).count();
            totalPages += midx.get_pageReads();
            
            // Progress indicator cada 20 queries
            if ((i+1) % 20 == 0) cout << "." << flush;
        }

        if (!firstOutput) J << ",\n";
        firstOutput = false;

        double avgD = double(totalD) / queries.size();
        double avgTms = double(totalT) / (1000.0 * queries.size());
        double avgPA = double(totalPages) / queries.size();

        J << fixed << setprecision(6)
          << "{"
          << "\"index\":\"MIndex*\","
          << "\"dataset\":\"" << dataset << "\","
          << "\"category\":\"DM\","
          << "\"num_pivots\":" << numPivots << ","
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
          
        cout << " OK (" << (int)avgD << " compdists, " << avgTms << " ms)\n";
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

    vector<string> datasets = {"LA", "Synthetic", "Words"};
    
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
