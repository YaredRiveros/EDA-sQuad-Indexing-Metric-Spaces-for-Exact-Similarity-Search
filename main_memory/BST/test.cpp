#include <bits/stdc++.h>
#include "bst.hpp"
#include "datasets/paths.hpp"
#include <filesystem>
using namespace std;

// ============================================================
// CONFIGURACIÓN EXACTA DEL PAPER (Tabla 6)
// ============================================================

static const vector<double> SELECTIVITIES = {0.02, 0.04, 0.08, 0.16, 0.32};
static const vector<int>    K_VALUES      = {5, 10, 20, 50, 100};
static const vector<string> DATASETS      = {"LA", "Words", "Color", "Synthetic"};
static const vector<int> HEIGHT_VALUES = {3, 5, 10, 15, 20};


// ============================================================
// MAIN — EXPERIMENTACIÓN COMPLETA
// ============================================================
int main()
{
    // CREAR SUBCARPETA 'RESULTS'
    std::filesystem::create_directories("results");

    for (const string& dataset : DATASETS)
    {
        // ------------------------------------------------------------
        // 1. Resolver dataset físico (LA.txt, Words.txt, Color.txt...)
        // ------------------------------------------------------------
        string dbfile = path_dataset(dataset);

        if (dbfile == "") {
            cerr << "[WARN] Dataset no encontrado, omitido: " << dataset << "\n";
            continue;
        }

        // ------------------------------------------------------------
        // 2. Cargar base de datos (tipos correctos)
        // ------------------------------------------------------------
        unique_ptr<ObjectDB> db;

        if (dataset == "LA")
        {
            db = make_unique<VectorDB>(dbfile, 2);   // L2
        }
        else if (dataset == "Color")
        {
            db = make_unique<VectorDB>(dbfile, 1);   // L1
        }
        else if (dataset == "Synthetic")
        {
            db = make_unique<VectorDB>(dbfile, 999999); // L∞
        }
        else if (dataset == "Words")
        {
            db = make_unique<StringDB>(dbfile); // Edit distance
        }
        else
        {
            cerr << "[WARN] Tipo de dataset no reconocido: " << dataset << "\n";
            continue;
        }

        int nObjects = db->size();
        if (nObjects == 0) {
            cerr << "[WARN] Dataset vacío, omitido: " << dataset << "\n";
            continue;
        }

        cerr << "\n==========================================\n";
        cerr << "[INFO] Dataset: " << dataset
             << "   N=" << nObjects
             << "   File=" << dbfile << "\n";
        cerr << "==========================================\n";


        // ------------------------------------------------------------
        // 3. Cargar queries y radios (precomputados)
        // ------------------------------------------------------------
        vector<int> queries = load_queries_file(path_queries(dataset));
        auto radii          = load_radii_file(path_radii(dataset));

        if (queries.empty()) {
            cerr << "[WARN] Queries ausentes, omitiendo dataset: " << dataset << "\n";
            continue;
        }

        cerr << "[INFO] Loaded " << queries.size() << " queries\n";


        // ------------------------------------------------------------
        // 4. Archivo JSON (modo append)
        // ------------------------------------------------------------
        string jsonOut = "results/results_BST_" + dataset + ".json";
        ofstream J(jsonOut);
        if (!J.is_open()) {
            cerr << "[ERROR] No se pudo abrir JSON para escribir.\n";
            return 1;
        }

        J << "[\n";
        bool firstOutput = true;


        // ============================================================
        // 5. PARA CADA ALTURA = {3,5,10,15,20}
        // ============================================================
        int bucket = 10; // fijo para BST

        for (int hparam : HEIGHT_VALUES)
        {
            cerr << "\n------------------------------------------\n";
            cerr << "[INFO] Construyendo BST con altura = " << hparam << "...\n";
            cerr << "------------------------------------------\n";

            BST bst(db.get(), nObjects, bucket, hparam);
            int realHeight = bst.get_height();

            cerr << "[INFO] Altura real del BST = " << realHeight << "\n";


            
            // ========================================================
            //  MRQ (range queries)
            // ========================================================
            for (double sel : SELECTIVITIES)
            {
                if (radii.find(sel) == radii.end()) {
                    cerr << "[WARN] No hay radio precalculado para selectivity=" << sel << "\n";
                    continue;
                }

                double R = radii[sel];
                long long totalD = 0;

                auto t1 = chrono::high_resolution_clock::now();

                for (int q : queries)
                {
                    vector<int> out;
                    bst.clear_counters();
                    bst.rangeSearch(q, R, out);
                    totalD += bst.get_compDist();
                }

                auto t2 = chrono::high_resolution_clock::now();
                double totalTimeUS = chrono::duration_cast<chrono::microseconds>(t2 - t1).count();

                double avgD = double(totalD) / queries.size();
                double avgT = totalTimeUS / queries.size();

                if (!firstOutput) J << ",\n";
                firstOutput = false;

                J << fixed << setprecision(6);
                J << "{"
                << "\"index\":\"BST\","
                << "\"dataset\":\"" << dataset << "\","
                << "\"category\":\"CP\","
                << "\"num_pivots\":0,"
                << "\"num_centers_path\":" << realHeight << ","
                << "\"arity\":null,"
                << "\"query_type\":\"MRQ\","
                << "\"selectivity\":" << sel << ","
                << "\"radius\":" << R << ","
                << "\"k\":null,"
                << "\"compdists\":" << avgD << ","
                << "\"time_ms\":" << (avgT / 1000.0) << ","
                << "\"n_queries\":" << queries.size() << ","
                << "\"run_id\":1"
                << "}";
            }

            // ========================================================
            //  MkNN
            // ========================================================
            for (int k : K_VALUES)
            {
                long long totalD = 0;

                auto t1 = chrono::high_resolution_clock::now();

                for (int q : queries)
                {
                    vector<ResultElem> out;
                    bst.clear_counters();
                    bst.knnSearch(q, k, out);
                    totalD += bst.get_compDist();
                }

                auto t2 = chrono::high_resolution_clock::now();
                double totalTimeUS = chrono::duration_cast<chrono::microseconds>(t2 - t1).count();

                double avgD = double(totalD) / queries.size();
                double avgT = totalTimeUS / queries.size();

                if (!firstOutput) J << ",\n";
                firstOutput = false;

                J << fixed << setprecision(6);
                J << "{"
                << "\"index\":\"BST\","
                << "\"dataset\":\"" << dataset << "\","
                << "\"category\":\"CP\","
                << "\"num_pivots\":0,"
                << "\"num_centers_path\":" << realHeight << ","
                << "\"arity\":null,"
                << "\"query_type\":\"MkNN\","
                << "\"selectivity\":null,"
                << "\"radius\":null,"
                << "\"k\":" << k << ","
                << "\"compdists\":" << avgD << ","
                << "\"time_ms\":" << (avgT / 1000.0) << ","
                << "\"n_queries\":" << queries.size() << ","
                << "\"run_id\":1"
                << "}";
            }


        }

        J << "\n]\n";
        J.close();

        cerr << "[DONE] Archivo generado: " << jsonOut << "\n";
    }

    return 0;
}
