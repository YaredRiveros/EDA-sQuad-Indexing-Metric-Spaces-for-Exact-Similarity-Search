#include <bits/stdc++.h>
#include <filesystem>
#include <chrono>
#include "db.h"
#include "GNAT.h"
#include "datasets/paths.hpp"

using namespace std;
using namespace chrono;

// ============================================================
// CONFIGURACIÓN EXACTA DEL PAPER (Tabla 6)
// ============================================================

static const vector<double> SELECTIVITIES = {0.02, 0.04, 0.08, 0.16, 0.32};
static const vector<int>    K_VALUES      = {5, 10, 20, 50, 100};
static const vector<string> DATASETS      = {"Synthetic"};  // Completar Synthetic
static const vector<int> HEIGHT_VALUES = {3, 5, 10, 15, 20};

// Variable global de GNAT
int MaxHeight;

// ============================================================
// FUNCIONES AUXILIARES
// ============================================================

string dataset_category(const string& dataset) {
    if (dataset == "LA") return "vectors";
    if (dataset == "Color") return "vectors";
    if (dataset == "Synthetic") return "vectors";
    if (dataset == "Words") return "strings";
    return "unknown";
}

// ============================================================
// MAIN — EXPERIMENTACIÓN COMPLETA
// ============================================================
int main()
{
    // Crear directorio results
    std::filesystem::create_directories("results");

    for (const string& dataset : DATASETS)
    {
        // ------------------------------------------------------------
        // 1. Resolver dataset físico
        // ------------------------------------------------------------
        string dbfile = path_dataset(dataset);
        if (dbfile == "" || !std::filesystem::exists(dbfile)) {
            cerr << "[WARN] Dataset no encontrado, omitido: " << dataset << "\n";
            continue;
        }

        cerr << "\n==========================================\n";
        cerr << "[INFO] Dataset: " << dataset << "   File=" << dbfile << "\n";
        cerr << "==========================================\n";

        // ------------------------------------------------------------
        // 2. Cargar base de datos (tipos correctos)
        // ------------------------------------------------------------
        unique_ptr<db_t> db;

        if (dataset == "LA")
        {
            db = make_unique<L2_db_t>();
        }
        else if (dataset == "Color")
        {
            db = make_unique<L1_db_t>();
        }
        else if (dataset == "Synthetic")
        {
            db = make_unique<Linf_db_t>();
        }
        else if (dataset == "Words")
        {
            db = make_unique<str_db_t>();
        }
        else
        {
            cerr << "[WARN] Tipo de dataset no reconocido: " << dataset << "\n";
            continue;
        }

        // Limitar Words a 30K objetos desde la carga
        // (el archivo tiene 597K líneas pero las queries fueron generadas para ~26K)
        if (dataset == "Words") {
            cerr << "[INFO] Cargando Words con límite de 30000 objetos\n";
            db->read(dbfile, 30000);
        } else {
            db->read(dbfile);
        }

        int nObjects = db->size();
        if (nObjects == 0) {
            cerr << "[WARN] Dataset vacío, omitido: " << dataset << "\n";
            continue;
        }

        cerr << "[INFO] Objetos: " << nObjects << "\n";

        // ------------------------------------------------------------
        // 3. Cargar queries y radios (precomputados desde JSON)
        // ------------------------------------------------------------
        vector<int> queries = load_queries_file(path_queries(dataset));
        auto radii          = load_radii_file(path_radii(dataset));

        if (queries.empty()) {
            cerr << "[WARN] Queries ausentes, omitiendo dataset: " << dataset << "\n";
            continue;
        }

        // Filtrar queries que están fuera del rango de objetos cargados
        if (dataset == "Words") {
            vector<int> valid_queries;
            for (int q : queries) {
                if (q < nObjects) {
                    valid_queries.push_back(q);
                }
            }
            cerr << "[INFO] Filtrando queries: " << queries.size() << " -> " << valid_queries.size() << " (dentro de rango [0," << nObjects << "))\n";
            queries = valid_queries;
            
            if (queries.empty()) {
                cerr << "[WARN] No hay queries válidas después del filtrado, omitiendo dataset: " << dataset << "\n";
                continue;
            }
        }

        cerr << "[INFO] Loaded " << queries.size() << " queries\n";
        cerr << "[INFO] Loaded " << radii.size() << " radii\n";

        // ------------------------------------------------------------
        // 4. Archivo JSON
        // ------------------------------------------------------------
        string jsonOut = "results/results_GNAT_" + dataset + ".json";
        ofstream J(jsonOut);
        if (!J.is_open()) {
            cerr << "[ERROR] No se pudo abrir JSON para escribir.\n";
            return 1;
        }

        J << "[\n";

        // ------------------------------------------------------------
        // 5. Experimentos con diferentes valores de HEIGHT
        // ------------------------------------------------------------
        bool firstConfig = true;

        for (int HEIGHT : HEIGHT_VALUES)
        {
            cerr << "[INFO] ============ HEIGHT=" << HEIGHT << " ============\n";
            
            MaxHeight = HEIGHT; // Variable global de GNAT
            int arity = 5; // M (arity) fijo como en el main.cpp original
            
            // CONSTRUCCIÓN DEL ÍNDICE
            cerr << "[INFO] Construyendo índice GNAT con HEIGHT=" << HEIGHT << ", M=" << arity << "...\n";
            GNAT_t index(db.get(), arity);
            
            auto t1 = high_resolution_clock::now();
            unsigned long prevDistCount = index.dist_call_cnt;
            
            index.build();
            
            auto t2 = high_resolution_clock::now();
            double buildTime = duration_cast<milliseconds>(t2 - t1).count();
            double buildDists = index.dist_call_cnt - prevDistCount;

            cerr << "[INFO] Construcción completada: " << buildTime << " ms, " 
                 << buildDists << " compdists\n";

            // ========================================
            // EXPERIMENTOS MkNN
            // ========================================
            cerr << "[INFO] Ejecutando MkNN queries...\n";
            
            for (int k : K_VALUES)
            {
                cerr << "  k=" << k << "...\n";
                
                // Reset contadores
                index.dist_call_cnt = 0;
                auto start = high_resolution_clock::now();
                
                double avgRadius = 0;
                index.knnSearch(queries, k, avgRadius);
                
                auto end = high_resolution_clock::now();
                double totalTime = duration_cast<milliseconds>(end - start).count();
                
                double avgTime = totalTime / queries.size();
                double avgDists = (double)index.dist_call_cnt / queries.size();

                // JSON output
                if (!firstConfig) J << ",\n";
                firstConfig = false;

                J << "  {\n";
                J << "    \"index\": \"GNAT\",\n";
                J << "    \"dataset\": \"" << dataset << "\",\n";
                J << "    \"category\": \"" << dataset_category(dataset) << "\",\n";
                J << "    \"num_pivots\": " << HEIGHT << ",\n";
                J << "    \"num_centers_path\": null,\n";
                J << "    \"arity\": " << arity << ",\n";
                J << "    \"bucket_size\": null,\n";
                J << "    \"query_type\": \"MkNN\",\n";
                J << "    \"selectivity\": null,\n";
                J << "    \"radius\": " << avgRadius << ",\n";
                J << "    \"k\": " << k << ",\n";
                J << "    \"compdists\": " << avgDists << ",\n";
                J << "    \"time_ms\": " << avgTime << ",\n";
                J << "    \"n_queries\": " << queries.size() << ",\n";
                J << "    \"run_id\": \"GNAT_" << dataset << "_H" << HEIGHT << "_k" << k << "\"\n";
                J << "  }";
            }

            // ========================================
            // EXPERIMENTOS MRQ
            // ========================================
            cerr << "[INFO] Ejecutando MRQ queries...\n";

            for (const auto& entry : radii)
            {
                double sel = entry.first;   // selectivity es la clave del map
                double radius = entry.second; // radius es el valor
                
                cerr << "  selectivity=" << sel << ", radius=" << radius << "...\n";
                
                // Reset contadores
                index.dist_call_cnt = 0;
                auto start = high_resolution_clock::now();
                
                int totalResults = 0;
                index.rangeSearch(queries, radius, totalResults);
                
                auto end = high_resolution_clock::now();
                double totalTime = duration_cast<milliseconds>(end - start).count();
                
                double avgTime = totalTime / queries.size();
                double avgDists = (double)index.dist_call_cnt / queries.size();
                double avgResults = (double)totalResults / queries.size();

                // JSON output
                J << ",\n";
                J << "  {\n";
                J << "    \"index\": \"GNAT\",\n";
                J << "    \"dataset\": \"" << dataset << "\",\n";
                J << "    \"category\": \"" << dataset_category(dataset) << "\",\n";
                J << "    \"num_pivots\": " << HEIGHT << ",\n";
                J << "    \"num_centers_path\": null,\n";
                J << "    \"arity\": " << arity << ",\n";
                J << "    \"bucket_size\": null,\n";
                J << "    \"query_type\": \"MRQ\",\n";
                J << "    \"selectivity\": " << sel << ",\n";
                J << "    \"radius\": " << radius << ",\n";
                J << "    \"k\": null,\n";
                J << "    \"compdists\": " << avgDists << ",\n";
                J << "    \"time_ms\": " << avgTime << ",\n";
                J << "    \"n_queries\": " << queries.size() << ",\n";
                J << "    \"run_id\": \"GNAT_" << dataset << "_H" << HEIGHT << "_sel" << sel << "\"\n";
                J << "  }";
            }
        }

        J << "\n]\n";
        J.close();

        cerr << "[INFO] Resultados guardados en: " << jsonOut << "\n";
    }

    cerr << "\n[DONE] GNAT benchmark completado.\n";
    return 0;
}
