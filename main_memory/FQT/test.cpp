#include <bits/stdc++.h>
#include "fqt.hpp"
#include "datasets/paths.hpp"

using namespace std;
using namespace chrono;

// ============================================================
// CONFIGURACIÓN DEL PAPER
// ============================================================

static const vector<double> SELECTIVITIES = {0.02, 0.04, 0.08, 0.16, 0.32};
static const vector<int>    K_VALUES      = {5, 10, 20, 50, 100};
static const vector<string> DATASETS      = {"LA", "Words", "Synthetic"};

// Parámetros FQT: {bucket_size, arity}
// Altura implícita por el número de pivotes seleccionados durante construcción
struct FQT_Params {
    int bucket;
    int arity;
};

// Parámetros para cada dataset (ajustados según tamaño y características)
static const vector<FQT_Params> PARAMS_LA = {
    {100, 5},  // ~height 3
    { 50, 5},  // ~height 5
    { 20, 5},  // ~height 10
    { 10, 5},  // ~height 15
    {  5, 5}   // ~height 20
};

static const vector<FQT_Params> PARAMS_WORDS = {
    {200, 4},
    {100, 4},
    { 50, 4},
    { 20, 4},
    { 10, 4}
};

static const vector<FQT_Params> PARAMS_SYNTH = {
    {100, 5},
    { 50, 5},
    { 20, 5},
    { 10, 5},
    {  5, 5}
};

// ============================================================
// MAIN — EXPERIMENTACIÓN COMPLETA
// ============================================================
int main()
{
    srand(12345);
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

        cerr << "[INFO] Cargadas " << queries.size() << " queries\n";

        // ------------------------------------------------------------
        // 4. Seleccionar parámetros según dataset
        // ------------------------------------------------------------
        vector<FQT_Params> params;
        if (dataset == "LA")              params = PARAMS_LA;
        else if (dataset == "Words")      params = PARAMS_WORDS;
        else if (dataset == "Synthetic")  params = PARAMS_SYNTH;

        // ------------------------------------------------------------
        // 5. Archivo JSON
        // ------------------------------------------------------------
        string jsonOut = "results/results_FQT_" + dataset + ".json";
        ofstream J(jsonOut);
        if (!J.is_open()) {
            cerr << "[ERROR] No se pudo abrir JSON para escribir.\n";
            return 1;
        }

        J << "[\n";
        bool firstOutput = true;

        // ------------------------------------------------------------
        // 6. Experimentos con diferentes configuraciones
        // ------------------------------------------------------------
        for (size_t config_idx = 0; config_idx < params.size(); config_idx++)
        {
            auto& param = params[config_idx];
            
            cerr << "[INFO] ===== Config " << (config_idx+1) << "/" << params.size()
                 << ": bucket=" << param.bucket << ", arity=" << param.arity << " =====\n";

            // CONSTRUCCIÓN
            auto t1 = high_resolution_clock::now();
            FQT tree(db.get(), param.bucket, param.arity);
            tree.build();
            auto t2 = high_resolution_clock::now();
            
            double buildTime = duration_cast<milliseconds>(t2 - t1).count();
            long long buildDists = tree.getCompdists();
            int height = tree.getHeight();

            cerr << "[INFO] Construcción: " << buildTime << " ms, " 
                 << buildDists << " compdists, altura=" << height << "\n";

            // ========================================
            // EXPERIMENTOS MkNN
            // ========================================
            cerr << "[INFO] Ejecutando MkNN queries...\n";
            
            for (int k : K_VALUES)
            {
                tree.resetCompdists();
                auto start = high_resolution_clock::now();
                
                double sumRadius = 0;
                for (int q : queries) {
                    double kth_dist = tree.knn(q, k);
                    sumRadius += kth_dist;
                }
                
                auto end = high_resolution_clock::now();
                double totalTime = duration_cast<milliseconds>(end - start).count();
                
                double avgTime = totalTime / queries.size();
                double avgDists = (double)tree.getCompdists() / queries.size();
                double avgRadius = sumRadius / queries.size();

                if (!firstOutput) J << ",\n";
                firstOutput = false;

                J << "  {\n";
                J << "    \"index\": \"FQT\",\n";
                J << "    \"dataset\": \"" << dataset << "\",\n";
                J << "    \"category\": \"" << (dataset == "Words" ? "strings" : "vectors") << "\",\n";
                J << "    \"num_pivots\": " << height << ",\n";
                J << "    \"num_centers_path\": null,\n";
                J << "    \"arity\": " << param.arity << ",\n";
                J << "    \"bucket_size\": " << param.bucket << ",\n";
                J << "    \"query_type\": \"MkNN\",\n";
                J << "    \"selectivity\": null,\n";
                J << "    \"radius\": " << avgRadius << ",\n";
                J << "    \"k\": " << k << ",\n";
                J << "    \"compdists\": " << avgDists << ",\n";
                J << "    \"time_ms\": " << avgTime << ",\n";
                J << "    \"n_queries\": " << queries.size() << ",\n";
                J << "    \"run_id\": \"FQT_" << dataset << "_b" << param.bucket 
                                              << "_a" << param.arity << "_k" << k << "\"\n";
                J << "  }";
            }

            // ========================================
            // EXPERIMENTOS MRQ
            // ========================================
            cerr << "[INFO] Ejecutando MRQ queries...\n";

            for (const auto& entry : radii)
            {
                double sel = entry.first;
                double radius = entry.second;
                
                tree.resetCompdists();
                auto start = high_resolution_clock::now();
                
                int totalResults = 0;
                for (int q : queries) {
                    int count = tree.range(q, radius);
                    totalResults += count;
                }
                
                auto end = high_resolution_clock::now();
                double totalTime = duration_cast<milliseconds>(end - start).count();
                
                double avgTime = totalTime / queries.size();
                double avgDists = (double)tree.getCompdists() / queries.size();

                J << ",\n";
                J << "  {\n";
                J << "    \"index\": \"FQT\",\n";
                J << "    \"dataset\": \"" << dataset << "\",\n";
                J << "    \"category\": \"" << (dataset == "Words" ? "strings" : "vectors") << "\",\n";
                J << "    \"num_pivots\": " << height << ",\n";
                J << "    \"num_centers_path\": null,\n";
                J << "    \"arity\": " << param.arity << ",\n";
                J << "    \"bucket_size\": " << param.bucket << ",\n";
                J << "    \"query_type\": \"MRQ\",\n";
                J << "    \"selectivity\": " << sel << ",\n";
                J << "    \"radius\": " << radius << ",\n";
                J << "    \"k\": null,\n";
                J << "    \"compdists\": " << avgDists << ",\n";
                J << "    \"time_ms\": " << avgTime << ",\n";
                J << "    \"n_queries\": " << queries.size() << ",\n";
                J << "    \"run_id\": \"FQT_" << dataset << "_b" << param.bucket 
                                              << "_a" << param.arity << "_sel" << sel << "\"\n";
                J << "  }";
            }
        }

        J << "\n]\n";
        J.close();

        cerr << "[INFO] Resultados guardados en: " << jsonOut << "\n";
    }

    cerr << "\n[DONE] FQT benchmark completado.\n";
    return 0;
}
