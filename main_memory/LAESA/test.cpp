#include <bits/stdc++.h>
#include "laesa.hpp"
#include "../../datasets/paths.hpp"
#include <filesystem>
using namespace std;

// ============================================================
// CONFIGURACIÓN EXACTA DEL PAPER (Tabla 6)
// ============================================================

static const vector<double> SELECTIVITIES = {0.02, 0.04, 0.08, 0.16, 0.32};
static const vector<int>    K_VALUES      = {5, 10, 20, 50, 100};
static const vector<int>    PIVOT_COUNTS  = {3, 5, 10, 15, 20};
// static const vector<string> DATASETS      = {"LA", "Words", "Color", "Synthetic"};
static const vector<string> DATASETS = {"LA"};


// ============================================================
// Cargar pivots desde JSON (precomputed by HFI)
// ============================================================

vector<int> load_pivots_json(const string& path) {
    vector<int> piv;

    if (path == "" || !file_exists(path)) {
        cerr << "[WARN] Pivot JSON missing: " << path << "\n";
        return piv;
    }

    ifstream f(path);
    string tok;
    while (f >> tok) {
        tok.erase(remove_if(tok.begin(), tok.end(),
                            [](char c){
                                return c=='['||c==']'||c==','||c=='"'||c==':';
                            }),
                  tok.end());

        if (tok == "pivots") continue;
        if (!tok.empty() && all_of(tok.begin(), tok.end(), ::isdigit))
            piv.push_back(stoi(tok));
    }
    return piv;
}


// ============================================================
// MAIN — EXPERIMENTACIÓN COMPLETA
// ============================================================
int main(int argc, char** argv)
{
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

    std::filesystem::create_directories("results");

    for (const string& dataset : datasets)
    {
        // ------------------------------------------------------------
        // 1. Resolver dataset físico
        // ------------------------------------------------------------
        string dbfile = path_dataset(dataset);

        if (dbfile == "") {
            cerr << "[WARN] Dataset no encontrado, omitido: " << dataset << "\n";
            continue;
        }

        // ------------------------------------------------------------
        // 2. Cargar base de datos con la métrica correspondiente
        // ------------------------------------------------------------
        unique_ptr<ObjectDB> db;

        if (dataset == "LA")
            db = make_unique<VectorDB>(dbfile, 2);        // L2 norm
        else if (dataset == "Color")
            db = make_unique<VectorDB>(dbfile, 1);        // L1 norm
        else if (dataset == "Synthetic")
            db = make_unique<VectorDB>(dbfile, 999999);   // L∞ norm
        else if (dataset == "Words")
            db = make_unique<StringDB>(dbfile);           // Edit distance
        else {
            cerr << "[WARN] Dataset desconocido: " << dataset << "\n";
            continue;
        }

        int nObjects = db->size();
        cerr << "\n============================================\n";
        cerr << "[INFO] Dataset: " << dataset
             << "   N=" << nObjects
             << "   File=" << dbfile << "\n";
        cerr << "============================================\n";

        if (nObjects == 0) {
            cerr << "[WARN] Dataset vacío\n";
            continue;
        }

        // ------------------------------------------------------------
        // 3. Cargar queries + radios
        // ------------------------------------------------------------
        vector<int> queries = load_queries_file(path_queries(dataset));
        auto radii = load_radii_file(path_radii(dataset));

        if (queries.empty()) {
            cerr << "[WARN] Queries ausentes, omitiendo dataset\n";
            continue;
        }

        // ------------------------------------------------------------
        // 4. Archivo JSON final
        // ------------------------------------------------------------
        string jsonOut = "results/results_LAESA_" + dataset + ".json";
        ofstream J(jsonOut);
        J << "[\n";
        bool firstOutput = true;


        // ============================================================
        // 5. PROBAR nPivots = {3, 5, 10, 15, 20}
        // ============================================================
        for (int nPivots : PIVOT_COUNTS)
        {
            cerr << "\n------------------------------------------\n";
            cerr << "[INFO] Construyendo LAESA con " << nPivots << " pivots...\n";
            cerr << "------------------------------------------\n";

            // cargar pivotes desde JSON
            string pivFile = path_pivots(dataset, nPivots);
            vector<int> pivots = load_pivots_json(pivFile);

            if (pivots.empty()) {
                cerr << "[WARN] Pivots ausentes para l=" << nPivots << "\n";
                continue;
            }

            // construir índice
            LAESA laesa(db.get(), nPivots);
            laesa.overridePivots(pivots);


            // ========================================================
            //  MRQ (range queries)
            // ========================================================
            for (double sel : SELECTIVITIES)
            {
                if (!radii.count(sel)) continue;

                double R = radii[sel];
                long long totalD = 0;

                auto t1 = chrono::high_resolution_clock::now();

                for (int q : queries)
                {
                    vector<int> out;
                    laesa.rangeSearch(q, R, out);
                    totalD += getCompDists();
                }

                auto t2 = chrono::high_resolution_clock::now();
                double totalTimeUS =
                    chrono::duration_cast<chrono::microseconds>(t2 - t1).count();

                double avgD = double(totalD) / queries.size();
                double avgT = totalTimeUS / queries.size();

                if (!firstOutput) J << ",\n";
                firstOutput = false;

                J << fixed << setprecision(6);
                J << "{"
                  << "\"index\":\"LAESA\","
                  << "\"dataset\":\"" << dataset << "\","
                  << "\"category\":\"HFI\","
                  << "\"num_pivots\":" << nPivots << ","
                  << "\"num_centers_path\":null,"
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
                    laesa.knnSearch(q, k, out);
                    totalD += getCompDists();
                }

                auto t2 = chrono::high_resolution_clock::now();
                double totalTimeUS =
                    chrono::duration_cast<chrono::microseconds>(t2 - t1).count();

                double avgD = double(totalD) / queries.size();
                double avgT = totalTimeUS / queries.size();

                if (!firstOutput) J << ",\n";
                firstOutput = false;

                J << fixed << setprecision(6);
                J << "{"
                  << "\"index\":\"LAESA\","
                  << "\"dataset\":\"" << dataset << "\","
                  << "\"category\":\"HFI\","
                  << "\"num_pivots\":" << nPivots << ","
                  << "\"num_centers_path\":null,"
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

        } // end loop pivots

        J << "\n]\n";
        J.close();

        cerr << "[DONE] Archivo generado: " << jsonOut << "\n";
    }

    return 0;
}
