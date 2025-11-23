#include <bits/stdc++.h>
#include "sat.hpp"
#include "../../datasets/paths.hpp"

using namespace std;

// ============================================================
// CONFIGURACIÓN DEL PAPER
// ============================================================

// Selectividades (MRQ)
static const vector<double> SELECTIVITIES = {0.02, 0.04, 0.08, 0.16, 0.32};

// K para MkNN
static const vector<int> K_VALUES = {5, 10, 20, 50, 100};

// Datasets evaluados
static const vector<string> DATASETS = {"LA", "Words", "Color"};

// SAT no tiene parámetros configurables (construye el árbol completo)
// Para mantener consistencia con otros índices, ejecutaremos con un solo "config"

// ============================================================
// MAIN — EXPERIMENTACIÓN COMPLETA
// ============================================================
int main()
{
    srand(12345);

    // CREAR SUBCARPETA 'RESULTS'
    std::filesystem::create_directories("results");
    
    for (const string& dataset : DATASETS)
    {
        // ------------------------------------------------------------
        // 1. Resolver dataset físico
        // ------------------------------------------------------------
        string dbfile = path_dataset(dataset);
        if (dbfile == "") {
            cerr << "[WARN] Dataset no encontrado: " << dataset << "\n";
            continue;
        }

        // ------------------------------------------------------------
        // 2. Cargar dataset con su métrica
        // ------------------------------------------------------------
        unique_ptr<ObjectDB> db;
        
        if (dataset == "LA")              db = make_unique<VectorDB>(dbfile, 2);
        else if (dataset == "Color")      db = make_unique<VectorDB>(dbfile, 1);
        else if (dataset == "Synthetic")  db = make_unique<VectorDB>(dbfile, 999999);
        else if (dataset == "Words")      db = make_unique<StringDB>(dbfile);
        else continue;

        int nObjects = db->size();
        
        cerr << "\n==========================================\n";
        cerr << "[INFO] Dataset: " << dataset << "   N=" << nObjects << "\n";
        cerr << "==========================================\n";

        // ------------------------------------------------------------
        // 3. Cargar queries y radios
        // ------------------------------------------------------------
        vector<int> queries = load_queries_file(path_queries(dataset));
        auto radii = load_radii_file(path_radii(dataset));

        if (queries.empty()) {
            cerr << "[WARN] Queries ausentes, omitiendo dataset: " << dataset << "\n";
            continue;
        }

        // ------------------------------------------------------------
        // 4. JSON Output
        // ------------------------------------------------------------
        string jsonOut = "results/results_SAT_" + dataset + ".json";
        ofstream J(jsonOut);
        J << "[\n";
        bool firstOutput = true;

        // ============================================================
        // 5. Construir SAT (sin parámetros, construcción única)
        // ============================================================

        cerr << "[INFO] Construyendo SAT...\n";
        
        SAT sat(db.get());
        
        auto tStart = chrono::high_resolution_clock::now();
        sat.build();
        auto tEnd = chrono::high_resolution_clock::now();
        
        long long buildTime = chrono::duration_cast<chrono::milliseconds>(tEnd - tStart).count();
        int height = sat.get_height();
        int numPivots = sat.get_num_pivots();
        
        cerr << "[INFO] SAT construido: altura=" << height 
             << ", nodos=" << numPivots 
             << ", tiempo=" << buildTime << " ms\n";

        // ========================================================
        //  MRQ (range queries)
        // ========================================================
        cerr << "[INFO] Ejecutando MRQ queries...\n";
        
        for (double sel : SELECTIVITIES)
        {
            if (radii.find(sel) == radii.end())
                continue;

            double R = radii[sel];
            long long totalD = 0, totalT = 0;

            for (int q : queries)
            {
                vector<int> out;
                sat.clear_counters();
                sat.rangeSearch(q, R, out);

                totalD += sat.get_compDist();
                totalT += sat.get_queryTime();
            }

            double avgD = double(totalD) / queries.size();
            double avgT = double(totalT) / queries.size() / 1000.0; // μs → ms

            if (!firstOutput) J << ",\n";
            firstOutput = false;

            J << fixed << setprecision(6);
            J << "{"
              << "\"index\":\"SAT\","
              << "\"dataset\":\"" << dataset << "\","
              << "\"category\":\"" << (dataset == "Words" ? "strings" : "vectors") << "\","
              << "\"num_pivots\":" << numPivots << ","
              << "\"num_centers_path\":null,"
              << "\"height\":" << height << ","
              << "\"query_type\":\"MRQ\","
              << "\"selectivity\":" << sel << ","
              << "\"radius\":" << R << ","
              << "\"k\":null,"
              << "\"compdists\":" << avgD << ","
              << "\"time_ms\":" << avgT << ","
              << "\"n_queries\":" << queries.size()
              << "}";
        }

        // ========================================================
        //  MkNN (k-NN queries)
        // ========================================================
        cerr << "[INFO] Ejecutando MkNN queries...\n";
        
        for (int k : K_VALUES)
        {
            long long totalD = 0, totalT = 0;
            double sumRadius = 0.0;

            for (int q : queries)
            {
                sat.clear_counters();
                auto res = sat.knnQuery(q, k);

                totalD += sat.get_compDist();
                totalT += sat.get_queryTime();

                if (!res.empty())
                    sumRadius += res.back().first; // k-ésima distancia
            }

            double avgD = double(totalD) / queries.size();
            double avgT = double(totalT) / queries.size() / 1000.0; // μs → ms
            double avgRadius = sumRadius / queries.size();

            if (!firstOutput) J << ",\n";
            firstOutput = false;

            J << fixed << setprecision(6);
            J << "{"
              << "\"index\":\"SAT\","
              << "\"dataset\":\"" << dataset << "\","
              << "\"category\":\"" << (dataset == "Words" ? "strings" : "vectors") << "\","
              << "\"num_pivots\":" << numPivots << ","
              << "\"num_centers_path\":null,"
              << "\"height\":" << height << ","
              << "\"query_type\":\"MkNN\","
              << "\"selectivity\":null,"
              << "\"radius\":" << avgRadius << ","
              << "\"k\":" << k << ","
              << "\"compdists\":" << avgD << ","
              << "\"time_ms\":" << avgT << ","
              << "\"n_queries\":" << queries.size()
              << "}";
        }

        // ------------------------------------------------------------
        // 6. Cerrar JSON para este dataset
        // ------------------------------------------------------------
        J << "\n]\n";
        J.close();
        
        cerr << "[DONE] Archivo generado: " << jsonOut << "\n";
    }

    return 0;
}
