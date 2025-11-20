#include <bits/stdc++.h>
#include <filesystem>
#include <chrono>
#include "main.h"
#include "Objvector.h"
#include "Interpreter.h"
#include "Tuple.h"
#include "Cache.h"
#include "../../datasets/paths.hpp"

using namespace std;
using namespace chrono;

// ============================================================
// CONFIGURACIÓN EXACTA DEL PAPER (Tabla 6)
// ============================================================

static const vector<double> SELECTIVITIES = {0.02, 0.04, 0.08, 0.16, 0.32};
static const vector<int>    K_VALUES      = {5, 10, 20, 50, 100};
static const vector<string> DATASETS      = {"Synthetic", "Words", "LA"};  // Synthetic primero (más rápido)
static const vector<int> L_VALUES = {10, 15, 20, 25, 30}; // Número de pivotes por objeto

// ============================================================
// FUNCIONES EXTERNAS DE main.cpp (EPT*)
// ============================================================
extern double compDists;
extern Interpreter* pi;
extern int **pivotSeq;
extern set<int> pivotSet;
extern Tuple ** gb;
extern float* DB;
extern Tuple* G;
extern int * sequence;
extern int LGroup;
extern float* nobj;
extern double recyc_times;

// Funciones de EPT* definidas en main.cpp
extern void algorithm3(char* fname);
extern void saveIndex(Tuple * t, char *fname);
extern bool readIndex(Tuple* t, char* fname);
extern double KNNQuery(float* q, int k);
extern int rangeQuery(float* q, double radius, int qj);
extern double calculateDistance(float* o1, int v2);

#define num_cand 40
extern int * cand;
extern bool *ispivot;
extern double** O_P_matrix;
extern vector<int> samples_objs;

// ============================================================
// CARGA DE QUERIES VECTORIALES (específico de EPT)
// ============================================================

// Generar queries aleatorias desde DB (array global de EPT)
vector<vector<float>> generate_random_queries(Interpreter* pi, int num_queries = 100) {
    vector<vector<float>> queries;
    
    if (!pi || pi->num == 0 || pi->dim == 0) {
        cerr << "[ERROR] Interpreter no válido o dataset vacío\n";
        return queries;
    }
    
    if (!DB) {
        cerr << "[ERROR] DB no inicializado\n";
        return queries;
    }
    
    if (pi->num < num_queries) {
        num_queries = pi->num;
    }
    
    // Generar índices aleatorios únicos
    set<int> selected_indices;
    while (selected_indices.size() < (size_t)num_queries) {
        int idx = rand() % pi->num;
        selected_indices.insert(idx);
    }
    
    // Extraer los vectores correspondientes desde DB
    for (int idx : selected_indices) {
        vector<float> query_vec;
        // DB es un array lineal: DB[idx * dim + d] para acceder al elemento d del objeto idx
        for (int d = 0; d < pi->dim; d++) {
            query_vec.push_back(DB[idx * pi->dim + d]);
        }
        queries.push_back(query_vec);
    }
    
    cerr << "[INFO] Generadas " << queries.size() << " queries aleatorias desde el dataset\n";
    return queries;
}

// Determinar categoría del dataset
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
    srand((unsigned int)time(NULL));
    
    // Inicializar variables globales de EPT*
    pi = nullptr;
    G = nullptr;
    DB = nullptr;
    nobj = nullptr;
    cand = nullptr;
    ispivot = nullptr;
    O_P_matrix = nullptr;
    
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
        // 2. Parsear dataset con Interpreter (EPT*)
        // ------------------------------------------------------------
        pi = new Interpreter();
        pi->parseRawData(dbfile.c_str());

        if (pi->num == 0) {
            cerr << "[WARN] Dataset vacío, omitido: " << dataset << "\n";
            delete pi;
            continue;
        }

        cerr << "[INFO] Objetos: " << pi->num << "  Dimensión: " << pi->dim << "\n";

        // Omitir datasets con dimensión 0 (problemas con queries)
        if (pi->dim == 0 && dataset_category(dataset) == "vectors") {
            cerr << "[ERROR] No se pudo leer dimensión " << pi->dim << " de query.\n";
            cerr << "[WARN] Queries ausentes, omitiendo dataset: " << dataset << "\n";
            delete pi;
            continue;
        }

        // ------------------------------------------------------------
        // 3. Generar queries aleatorias y cargar radios
        // ------------------------------------------------------------
        vector<vector<float>> queries = generate_random_queries(pi, 100);
        
        if (queries.empty()) {
            cerr << "[WARN] No se pudieron generar queries, omitiendo dataset: " << dataset << "\n";
            delete pi;
            continue;
        }

        auto radii = load_radii_file(path_radii(dataset));
        if (radii.empty()) {
            cerr << "[WARN] Radios ausentes, omitiendo dataset: " << dataset << "\n";
            delete pi;
            continue;
        }

        cerr << "[INFO] Cargados " << radii.size() << " radii precomputados\n";

        // ------------------------------------------------------------
        // 4. Archivo JSON
        // ------------------------------------------------------------
        string jsonOut = "results/results_EPT_" + dataset + ".json";
        ofstream J(jsonOut);
        if (!J.is_open()) {
            cerr << "[ERROR] No se pudo abrir JSON para escribir.\n";
            delete pi;
            return 1;
        }

        J << "[\n";

        // ------------------------------------------------------------
        // 5. Experimentos con diferentes valores de L
        // ------------------------------------------------------------
        bool firstConfig = true;

        for (int L : L_VALUES)
        {
            cerr << "[INFO] ============ L=" << L << " ============\n";
            
            LGroup = L; // Variable global de EPT*
            string indexfile = "index_EPT_" + dataset + "_L" + to_string(L) + ".idx";

            // CONSTRUCCIÓN DEL ÍNDICE
            cerr << "[INFO] Construyendo índice EPT* con L=" << L << "...\n";
            auto t1 = high_resolution_clock::now();
            compDists = 0;
            
            algorithm3((char*)indexfile.c_str());
            saveIndex(G, (char*)indexfile.c_str());
            
            auto t2 = high_resolution_clock::now();
            double buildTime = duration_cast<milliseconds>(t2 - t1).count();
            double buildDists = compDists;

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
                compDists = 0;
                auto start = high_resolution_clock::now();
                
                double sumRadius = 0;
                for (const auto& q : queries) {
                    double kth_dist = KNNQuery((float*)q.data(), k);
                    sumRadius += kth_dist;
                }
                
                auto end = high_resolution_clock::now();
                double totalTime = duration_cast<milliseconds>(end - start).count();
                
                double avgTime = totalTime / queries.size();
                double avgDists = compDists / queries.size();
                double avgRadius = sumRadius / queries.size();

                // JSON output
                if (!firstConfig) J << ",\n";
                firstConfig = false;

                J << "  {\n";
                J << "    \"index\": \"EPT\",\n";
                J << "    \"dataset\": \"" << dataset << "\",\n";
                J << "    \"category\": \"" << dataset_category(dataset) << "\",\n";
                J << "    \"num_pivots\": " << L << ",\n";
                J << "    \"num_centers_path\": null,\n";
                J << "    \"arity\": null,\n";
                J << "    \"bucket_size\": null,\n";
                J << "    \"query_type\": \"MkNN\",\n";
                J << "    \"selectivity\": null,\n";
                J << "    \"radius\": " << avgRadius << ",\n";
                J << "    \"k\": " << k << ",\n";
                J << "    \"compdists\": " << avgDists << ",\n";
                J << "    \"time_ms\": " << avgTime << ",\n";
                J << "    \"n_queries\": " << queries.size() << ",\n";
                J << "    \"run_id\": \"EPT_" << dataset << "_L" << L << "_k" << k << "\"\n";
                J << "  }";
            }

            // ========================================
            // EXPERIMENTOS MRQ
            // ========================================
            cerr << "[INFO] Ejecutando MRQ queries...\n";

            for (const auto& entry : radii)
            {
                double sel = entry.first;   // selectivity es la clave
                double radius = entry.second; // radius es el valor
                
                cerr << "  selectivity=" << sel << ", radius=" << radius << "...\n";
                
                // Reset contadores
                compDists = 0;
                auto start = high_resolution_clock::now();
                
                int totalResults = 0;
                for (size_t qi = 0; qi < queries.size(); qi++) {
                    int count = rangeQuery((float*)queries[qi].data(), radius, qi);
                    totalResults += count;
                }
                
                auto end = high_resolution_clock::now();
                double totalTime = duration_cast<milliseconds>(end - start).count();
                
                double avgTime = totalTime / queries.size();
                double avgDists = compDists / queries.size();
                double avgResults = (double)totalResults / queries.size();

                // JSON output
                J << ",\n";
                J << "  {\n";
                J << "    \"index\": \"EPT\",\n";
                J << "    \"dataset\": \"" << dataset << "\",\n";
                J << "    \"category\": \"" << dataset_category(dataset) << "\",\n";
                J << "    \"num_pivots\": " << L << ",\n";
                J << "    \"num_centers_path\": null,\n";
                J << "    \"arity\": null,\n";
                J << "    \"bucket_size\": null,\n";
                J << "    \"query_type\": \"MRQ\",\n";
                J << "    \"selectivity\": " << sel << ",\n";
                J << "    \"radius\": " << radius << ",\n";
                J << "    \"k\": null,\n";
                J << "    \"compdists\": " << avgDists << ",\n";
                J << "    \"time_ms\": " << avgTime << ",\n";
                J << "    \"n_queries\": " << queries.size() << ",\n";
                J << "    \"run_id\": \"EPT_" << dataset << "_L" << L << "_sel" << sel << "\"\n";
                J << "  }";
            }

            // NO limpiar aquí - se acumula para múltiples L
            // La limpieza se hace al final del dataset
        }

        J << "\n]\n";
        J.flush();  // Asegurar que se escribe
        J.close();

        cerr << "[INFO] Resultados guardados en: " << jsonOut << "\n";

        // Limpiar estructuras globales del dataset (sin double free)
        // NOTA: No limpiamos porque EPT tiene memoria global compartida
        // que puede causar double free. Dejamos que el OS limpie al terminar.
        pi = nullptr;
        DB = nullptr;
        nobj = nullptr;
        G = nullptr;
        cand = nullptr;
        ispivot = nullptr;
        O_P_matrix = nullptr;

    }

    cerr << "\n[DONE] EPT* benchmark completado.\n";
    return 0;
}
