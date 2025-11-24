#include <bits/stdc++.h>
#include "fqt.hpp"
#include "../../datasets/paths.hpp"

using namespace std;
using namespace chrono;


static const vector<double> SELECTIVITIES = {0.02, 0.04, 0.08, 0.16, 0.32};
static const vector<int>    K_VALUES      = {5, 10, 20, 50, 100};
// Ejecutar todos los datasets disponibles por defecto
static const vector<string> DATASETS      = {"LA", "Words", "Color", "Synthetic"};

// Parámetros FQT: {bucket_size, arity}
struct FQT_Params {
    int bucket;
    int arity;
};

// Parámetros para cada dataset (ajustados según tamaño y características)
static const vector<FQT_Params> PARAMS_LA = {
    {100, 5},  
    { 50, 5},  
    { 20, 5},  
    { 10, 5},  
    {  5, 5}   
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

static const vector<FQT_Params> PARAMS_COLOR = {
    {100, 5},
    { 50, 5},
    { 20, 5},
    { 10, 5},
    {  5, 5}
};

int main(int argc, char** argv)
{
    srand(12345);
    vector<string> datasets;

    if (argc > 1) {
        // Los argumentos [1..argc-1] son nombres de dataset
        for (int i = 1; i < argc; ++i) {    
            datasets.push_back(argv[i]);
        }
    } else {
        datasets = DATASETS;
    }

    std::filesystem::create_directories("results");
    
    for (const string& dataset : datasets)
    {
        string dbfile = path_dataset(dataset);
        if (dbfile == "") {
            cerr << "[WARN] Dataset no encontrado: " << dataset << "\n";
            continue;
        }

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

        vector<int> queries = load_queries_file(path_queries(dataset));
        auto radii = load_radii_file(path_radii(dataset));

        if (queries.empty()) {
            cerr << "[WARN] Queries ausentes, omitiendo dataset: " << dataset << "\n";
            continue;
        }

        cerr << "[INFO] Cargadas " << queries.size() << " queries\n";

        vector<FQT_Params> params;
        if (dataset == "LA")              params = PARAMS_LA;
        else if (dataset == "Words")      params = PARAMS_WORDS;
        else if (dataset == "Color")      params = PARAMS_COLOR;
        else if (dataset == "Synthetic")  params = PARAMS_SYNTH;

        string jsonOut = "results/results_FQT_" + dataset + ".json";
        ofstream J(jsonOut);
        if (!J.is_open()) {
            cerr << "[ERROR] No se pudo abrir JSON para escribir.\n";
            return 1;
        }

        J << "[\n";
        bool firstOutput = true;

        for (size_t config_idx = 0; config_idx < params.size(); config_idx++)
        {
            auto& param = params[config_idx];
            
            cerr << "[INFO] ===== Config " << (config_idx+1) << "/" << params.size()
                 << ": bucket=" << param.bucket << ", arity=" << param.arity << " =====\n";

            // Preparar pivotes definidos por test
            std::vector<int> target_heights = {3,5,10,15,20};
            int target_height = 0;
            if (config_idx < target_heights.size()) target_height = target_heights[config_idx];
            if (target_height <= 0) target_height = 3;

            std::vector<int> pivots_list;
            if (db->size() > 0) {
                int step = std::max(1, db->size() / target_height);
                for (int p = 0; p < target_height; ++p) {
                    int idx = (p * step) % db->size();
                    pivots_list.push_back(idx);
                }
            }

            auto t1 = high_resolution_clock::now();
            // Pasar pivotes definidos por test al constructor de FQT
            FQT tree(db.get(), param.bucket, param.arity, pivots_list);
            tree.build();
            auto t2 = high_resolution_clock::now();
            
            double buildTime = duration_cast<milliseconds>(t2 - t1).count();
            long long buildDists = tree.getCompdists();
            int height = tree.getHeight();

            cerr << "[INFO] Construcción: " << buildTime << " ms, " 
                 << buildDists << " compdists, altura=" << height << "\n";

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
