#include <bits/stdc++.h>
#include <filesystem>
#include "sat.hpp"
#include "../../datasets/paths.hpp"

using namespace std;

// ============================================================
// CONFIGURACIÓN DEL PAPER (Chen-style)
// ============================================================

// Selectividades (MRQ)
static const vector<double> SELECTIVITIES = {0.02, 0.04, 0.08, 0.16, 0.32};

// K para MkNN
static const vector<int> K_VALUES = {5, 10, 20, 50, 100};

// Datasets evaluados
static const vector<string> DATASETS = {"LA", "Words", "Color", "Synthetic"};

// Valores "equivalentes" de número de pivotes m = {3,5,10,15,20}
static const vector<int> L_VALUES = {3, 5, 10, 15, 20};

// ============================================================
// MAIN — EXPERIMENTACIÓN COMPLETA PARA SAT
// ============================================================
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
        // Si no se pasa nada, usa el set por defecto
        datasets = DATASETS;
    }

    // CREAR SUBCARPETA 'results'
    std::filesystem::create_directories("results");

    for (const string &dataset : datasets)
    {
        // ------------------------------------------------------------
        // 1. Resolver dataset físico
        // ------------------------------------------------------------
        string dbfile = path_dataset(dataset);
        if (dbfile == "")
        {
            cerr << "[WARN] Dataset no encontrado: " << dataset << "\n";
            continue;
        }

        // ------------------------------------------------------------
        // 2. Cargar dataset con su métrica
        // ------------------------------------------------------------
        unique_ptr<ObjectDB> db;

        if (dataset == "LA")
            db = make_unique<VectorDB>(dbfile, 2);
        else if (dataset == "Color")
            db = make_unique<VectorDB>(dbfile, 1);
        else if (dataset == "Synthetic")
            db = make_unique<VectorDB>(dbfile, 999999);
        else if (dataset == "Words")
            db = make_unique<StringDB>(dbfile);
        else
            continue;

        int nObjects = db->size();

        cerr << "\n==========================================\n";
        cerr << "[INFO] Dataset: " << dataset
             << "   N=" << nObjects << "\n";
        cerr << "==========================================\n";

        // ------------------------------------------------------------
        // 3. Cargar queries + radios
        // ------------------------------------------------------------
        vector<int> queries = load_queries_file(path_queries(dataset));
        auto radii = load_radii_file(path_radii(dataset));

        if (queries.empty())
        {
            cerr << "[WARN] No hay queries para " << dataset << "\n";
            continue;
        }

        // ------------------------------------------------------------
        // 4. JSON Output
        // ------------------------------------------------------------
        string jsonOut = "results/results_SAT_" + dataset + ".json";
        ofstream J(jsonOut);
        if (!J.is_open())
        {
            cerr << "[ERROR] No se pudo abrir " << jsonOut << " para escritura.\n";
            continue;
        }

        J << "[\n";
        bool firstOutput = true;

        // ============================================================
        // 5. Construir SAT (una sola vez por dataset)
        // ============================================================

        cerr << "[INFO] Construyendo SAT...\n";

        SAT sat(db.get());

        auto tStart = chrono::high_resolution_clock::now();
        sat.build();
        auto tEnd = chrono::high_resolution_clock::now();

        long long buildTime = chrono::duration_cast<chrono::milliseconds>(tEnd - tStart).count();
        int height = sat.get_height();
        int numCenters = sat.get_num_pivots(); // número de nodos/centros del SAT

        cerr << "[INFO] SAT construido: altura=" << height
             << "   nodos=" << numCenters
             << "   tiempo=" << buildTime << " ms\n";

        // ========================================================
        //  MRQ (range queries)
        // ========================================================
        cerr << "[INFO] Ejecutando MRQ queries...\n";

        for (double sel : SELECTIVITIES)
        {
            // Si no hay radio precomputado para esta selectividad, saltamos
            if (radii.find(sel) == radii.end())
                continue;

            double R = radii[sel];

            // Para cada valor "equivalente" de número de pivotes m
            for (int li = 0; li < (int)L_VALUES.size(); ++li)
            {
                int l_value = L_VALUES[li];

                long long totalD = 0;
                long long totalT = 0;

                // Ejecutar TODAS las queries para este radio
                for (int q : queries)
                {
                    vector<int> out;
                    sat.clear_counters();
                    sat.rangeSearch(q, R, out);

                    totalD += sat.get_compDist();
                    totalT += sat.get_queryTime(); // μs acumulados
                }

                double avgD = static_cast<double>(totalD) / static_cast<double>(queries.size());
                double avgT = static_cast<double>(totalT) / static_cast<double>(queries.size()); // μs/query

                if (!firstOutput)
                    J << ",\n";
                firstOutput = false;

                J << fixed << setprecision(6);
                J << "{"
                  << "\"index\":\"SAT\","
                  << "\"dataset\":\"" << dataset << "\","
                  // categoría: compact-partitioning
                  << "\"category\":\"CP\","
                  // num_pivots = L (3,5,10,15,20) para graficar en el eje X como Chen
                  << "\"num_pivots\":" << l_value << ","
                  // num_centers_path: usamos la altura del SAT como #centros en un camino
                  << "\"num_centers_path\":" << height << ","
                  << "\"arity\":null,"
                  << "\"query_type\":\"MRQ\","
                  << "\"selectivity\":" << sel << ","
                  << "\"radius\":" << R << ","
                  << "\"k\":null,"
                  << "\"compdists\":" << avgD << ","
                  << "\"time_ms\":" << (avgT / 1000.0) << ","  // μs → ms
                  << "\"n_queries\":" << queries.size() << ","
                  << "\"run_id\":1"
                  << "}";
            }
        }

        // ========================================================
        //  MkNN (k-NN queries)
        // ========================================================
        cerr << "[INFO] Ejecutando MkNN queries...\n";

        for (int k : K_VALUES)
        {
            // Para cada valor "equivalente" de número de pivotes m
            for (int li = 0; li < (int)L_VALUES.size(); ++li)
            {
                int l_value = L_VALUES[li];

                long long totalD = 0;
                long long totalT = 0;
                double sumRadius = 0.0;

                // Ejecutar TODAS las queries para este k
                for (int q : queries)
                {
                    sat.clear_counters();
                    auto res = sat.knnQuery(q, k);

                    totalD += sat.get_compDist();
                    totalT += sat.get_queryTime(); // μs acumulados

                    if (!res.empty())
                        sumRadius += res.back().first; // distancia al k-ésimo vecino
                }

                double avgD = static_cast<double>(totalD) / static_cast<double>(queries.size());
                double avgT = static_cast<double>(totalT) / static_cast<double>(queries.size()); // μs/query
                double avgRadius = sumRadius / static_cast<double>(queries.size());

                if (!firstOutput)
                    J << ",\n";
                firstOutput = false;

                J << fixed << setprecision(6);
                J << "{"
                  << "\"index\":\"SAT\","
                  << "\"dataset\":\"" << dataset << "\","
                  << "\"category\":\"CP\","
                  << "\"num_pivots\":" << l_value << ","
                  << "\"num_centers_path\":" << height << ","
                  << "\"arity\":null,"
                  << "\"query_type\":\"MkNN\","
                  << "\"selectivity\":null,"
                  << "\"radius\":" << avgRadius << ","   // radio efectivo medio (distancia al k-ésimo)
                  << "\"k\":" << k << ","
                  << "\"compdists\":" << avgD << ","
                  << "\"time_ms\":" << (avgT / 1000.0) << ","
                  << "\"n_queries\":" << queries.size() << ","
                  << "\"run_id\":1"
                  << "}";
            }
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
