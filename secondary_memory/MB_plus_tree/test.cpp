// test.cpp - MB+-tree benchmark siguiendo especificación del paper
#include "mbpt.hpp"
#include "objectdb.hpp"
#include "datasets/paths.hpp"
#include <bits/stdc++.h>

using namespace std;

// Parámetros según el paper
static const vector<double> SELECTIVITIES = {0.02, 0.04, 0.08, 0.16, 0.32};
static const vector<int> K_VALUES = {5, 10, 20, 50, 100};

void test_dataset(const string& dataset) {
    cout << "\n##########################################\n";
    cout << "### TESTING DATASET: " << dataset << "\n";
    cout << "### (MB+-tree)\n";
    cout << "##########################################\n";

    string dbfile = path_dataset(dataset);
    
    unique_ptr<ObjectDB> db;
    if (dataset == "LA") {
        db = make_unique<VectorDB>(dbfile, 2);
    } else if (dataset == "Synthetic") {
        db = make_unique<VectorDB>(dbfile, 0);
    } else if (dataset == "Words") {
        db = make_unique<StringDB>(dbfile);
    } else {
        cout << "[ERROR] Dataset desconocido: " << dataset << "\n";
        return;
    }
    
    cout << "\n==========================================\n";
    cout << "[MB+-tree] Dataset: " << dataset
         << "   N=" << db->size() << "\n";
    cout << "==========================================\n";

    // Cargar queries y radii
    vector<int> queries = load_queries_file(path_queries(dataset));
    auto radii = load_radii_file(path_radii(dataset));
    
    if (queries.empty()) {
        cout << "[WARN] No hay queries para " << dataset << "\n";
        return;
    }
    
    cout << "\n[QUERIES] Cargadas " << queries.size() << " queries\n";
    cout << "[QUERIES] Radii para " << radii.size() << " selectividades\n";

    // Construir MB+-tree
    cout << "\n[BUILD] Construyendo MB+-tree con rho=0.1...\n";
    MBPT_Disk mbpt(db.get(), 0.1);
    
    auto t0 = chrono::high_resolution_clock::now();
    mbpt.build("index_mbpt_" + dataset);
    auto t1 = chrono::high_resolution_clock::now();
    auto buildTime = chrono::duration_cast<chrono::milliseconds>(t1 - t0).count();
    
    cout << "[BUILD] Tiempo: " << buildTime << " ms\n";

    // Crear archivo JSON de salida
    filesystem::create_directories("results");
    string jsonOut = "results/results_MBPT_" + dataset + ".json";
    ofstream J(jsonOut);
    J << "[\n";
    bool firstOutput = true;

    // Valores por defecto según el paper
    const double DEFAULT_SELECTIVITY = 0.08;
    const int DEFAULT_K = 20;

    // ===================================================================
    // EXPERIMENTO 1: Variar selectividad en MRQ
    // ===================================================================
    cout << "\n========================================\n";
    cout << "[EXP 1] Variando SELECTIVIDAD en MRQ\n";
    cout << "========================================\n";
    
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
            mbpt.clear_counters();
            
            t0 = chrono::high_resolution_clock::now();
            mbpt.rangeSearch(queries[i], radius, out);
            t1 = chrono::high_resolution_clock::now();
            
            totalD += mbpt.get_compDist();
            totalT += chrono::duration_cast<chrono::microseconds>(t1 - t0).count();
            totalPages += mbpt.get_pageReads();
        }

        if (!firstOutput) J << ",\n";
        firstOutput = false;

        double avgD = double(totalD) / queries.size();
        double avgTms = double(totalT) / (1000.0 * queries.size());
        double avgPA = double(totalPages) / queries.size();

        J << fixed << setprecision(6)
          << "{"
          << "\"index\":\"MB+-tree\","
          << "\"dataset\":\"" << dataset << "\","
          << "\"category\":\"DM\","
          << "\"num_pivots\":null,"
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

    // ===================================================================
    // EXPERIMENTO 2: Variar k en MkNN
    // ===================================================================
    cout << "\n========================================\n";
    cout << "[EXP 2] Variando K en MkNN\n";
    cout << "========================================\n";
    
    for (int k : K_VALUES) {
        long long totalD = 0, totalT = 0, totalPages = 0;
        
        cout << "  k=" << k << "... " << flush;

        for (size_t i = 0; i < queries.size(); i++) {
            vector<pair<double,int>> out;
            mbpt.clear_counters();
            
            t0 = chrono::high_resolution_clock::now();
            mbpt.knnSearch(queries[i], k, out);
            t1 = chrono::high_resolution_clock::now();
            
            totalD += mbpt.get_compDist();
            totalT += chrono::duration_cast<chrono::microseconds>(t1 - t0).count();
            totalPages += mbpt.get_pageReads();
        }

        if (!firstOutput) J << ",\n";
        firstOutput = false;

        double avgD = double(totalD) / queries.size();
        double avgTms = double(totalT) / (1000.0 * queries.size());
        double avgPA = double(totalPages) / queries.size();

        J << fixed << setprecision(6)
          << "{"
          << "\"index\":\"MB+-tree\","
          << "\"dataset\":\"" << dataset << "\","
          << "\"category\":\"DM\","
          << "\"num_pivots\":null,"
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

    J << "\n]\n";
    J.close();
    
    cout << "\n[JSON] Archivo generado: " << jsonOut << "\n";

    cout << "\n==========================================\n";
    cout << "[MB+-tree] " << dataset << " completado\n";
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
