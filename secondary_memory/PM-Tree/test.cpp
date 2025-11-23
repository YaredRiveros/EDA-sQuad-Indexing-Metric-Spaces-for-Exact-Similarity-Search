// pm_test.cpp - Benchmark de PM-tree (memoria secundaria, estilo Chen)

#include <bits/stdc++.h>
#include "pm_tree.hpp"
#include "../../datasets/paths.hpp"
#include <filesystem>

using namespace std;

// ============================================================
// CONFIG (igual al paper Chen2022)
// ============================================================

// Selectividades para MRQ
static const vector<double> SELECTIVITIES = {0.02, 0.04, 0.08, 0.16, 0.32};

// Valores de k para MkNN
static const vector<int> K_VALUES = {5, 10, 20, 50, 100};

// Cantidad de pivots l evaluados (para memoria secundaria, Chen fija l=5)
static const vector<int> L_VALUES = {5};

// Datasets evaluados
static const vector<string> DATASETS = {"LA", "Color", "Synthetic", "Words"};

// ============================================================
// CARGAR PIVOTS (JSON) HFI
// ============================================================
vector<int> load_pivots_json(const string& path) {
    vector<int> piv;

    if (path == "" || !file_exists(path)) {
        cerr << "[WARN] Pivot JSON not found: " << path << "\n";
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

        if (!tok.empty() && all_of(tok.begin(), tok.end(), ::isdigit))
            piv.push_back(stoi(tok));
    }
    return piv;
}

// ============================================================
// pm_test — ejecución completa
// ============================================================
int main() {
    ios::sync_with_stdio(false);
    cin.tie(nullptr);

    std::filesystem::create_directories("results");

    for (const string& dataset : DATASETS) {

        // -----------------------------------------
        // 1. Resolver dataset físico
        // -----------------------------------------
        string dbfile = path_dataset(dataset);
        if (dbfile.empty() || !file_exists(dbfile)) {
            cerr << "[WARN] Dataset not found: " << dataset << "\n";
            continue;
        }

        // -----------------------------------------
        // 2. Cargar dataset con la métrica correcta
        // -----------------------------------------
        unique_ptr<ObjectDB> db;

        if (dataset == "LA") {
            db = make_unique<VectorDB>(dbfile, 2);       // L2
        } else if (dataset == "Color") {
            db = make_unique<VectorDB>(dbfile, 1);       // L1
        } else if (dataset == "Synthetic") {
            db = make_unique<VectorDB>(dbfile, 999999);  // L∞
        } else if (dataset == "Words") {
            db = make_unique<StringDB>(dbfile);          // Levenshtein
        } else {
            cerr << "[WARN] PM-tree no se aplica a " << dataset << "\n";
            continue;
        }

        int nObjects = db->size();
        cerr << "\n==========================================\n";
        cerr << "[PMTREE] Dataset: " << dataset
             << "   N=" << nObjects << "\n";
        cerr << "==========================================\n";

        if (nObjects == 0) {
            cerr << "[WARN] Empty dataset\n";
            continue;
        }

        // -----------------------------------------
        // 3. Cargar queries (IDs) y radios
        // -----------------------------------------
        vector<int> queries = load_queries_file(path_queries(dataset));
        auto radii          = load_radii_file(path_radii(dataset));

        if (queries.empty()) {
            cerr << "[WARN] No queries found\n";
            continue;
        }

        cerr << "[QUERIES] " << queries.size() << " queries cargadas\n";
        cerr << "[QUERIES] Radii para " << radii.size()
             << " selectividades\n";

        // -----------------------------------------
        // 4. Archivo JSON de salida
        // -----------------------------------------
        string jsonOut = "results/results_PMTREE_" + dataset + ".json";
        ofstream J(jsonOut);
        if (!J.is_open()) {
            cerr << "[ERROR] Cannot create output JSON\n";
            continue;
        }

        J << "[\n";
        bool first = true;

        // -----------------------------------------
        // 5. Loop sobre número de pivots l
        // -----------------------------------------
        for (int l : L_VALUES) {

            string pivfile = path_pivots(dataset, l);
            vector<int> pivots = load_pivots_json(pivfile);

            if (pivots.empty()) {
                cerr << "[WARN] No pivots for l=" << l << " en " << dataset << "\n";
                continue;
            }

            cerr << "\n------------------------------------------\n";
            cerr << "[INFO] Construyendo PM-tree con l=" << l
                 << " pivots para " << dataset << "\n";
            cerr << "------------------------------------------\n";

            // Construir PM-tree a partir del índice M-tree en disco
            PMTree pmt(db.get(), l);
            pmt.buildFromMTree(dataset);   // lee <dataset>.mtree_index
            pmt.overridePivots(pivots);    // HFI pivots

            using clock = std::chrono::high_resolution_clock;

            // ====================================================
            // MRQ
            // ====================================================
            cerr << "\n[MRQ] Ejecutando selectividades...\n";
            for (double sel : SELECTIVITIES) {
                if (!radii.count(sel)) continue;

                double R = radii[sel];
                long long totalD = 0;
                long long totalT = 0;
                long long totalPages = 0;

                cerr << "  sel=" << sel << " (R=" << R << ")... " << flush;

                for (int q : queries) {
                    vector<int> out;
                    pmt.clear_counters();

                    auto t0 = clock::now();
                    pmt.rangeSearch(q, R, out);
                    auto t1 = clock::now();

                    totalD     += pmt.get_compDist();
                    totalT     += chrono::duration_cast<
                                      chrono::microseconds>(t1 - t0).count();
                    totalPages += pmt.get_pageReads();
                }

                double avgD     = double(totalD) / queries.size();
                double avgTms   = double(totalT) / (1000.0 * queries.size());
                double avgPages = double(totalPages) / queries.size();

                if (!first) J << ",\n";
                first = false;

                J << fixed << setprecision(6)
                  << "{"
                  << "\"index\":\"PMTREE\","
                  << "\"dataset\":\"" << dataset << "\","
                  << "\"category\":\"DM\","
                  << "\"num_pivots\":" << l << ","
                  << "\"num_centers_path\":1,"
                  << "\"arity\":null,"
                  << "\"query_type\":\"MRQ\","
                  << "\"selectivity\":" << sel << ","
                  << "\"radius\":" << R << ","
                  << "\"k\":null,"
                  << "\"compdists\":" << avgD << ","
                  << "\"time_ms\":" << avgTms << ","
                  << "\"pages\":" << avgPages << ","
                  << "\"n_queries\":" << queries.size() << ","
                  << "\"run_id\":1"
                  << "}";

                cerr << "OK (avg " << (long long)avgD
                     << " compdists, " << avgPages << " páginas)\n";
            }

            // ====================================================
            // MkNN
            // ====================================================
            cerr << "\n[MkNN] Ejecutando valores de k...\n";
            for (int k : K_VALUES) {
                long long totalD = 0;
                long long totalT = 0;
                long long totalPages = 0;

                cerr << "  k=" << k << "... " << flush;

                for (int q : queries) {
                    vector<pair<double,int>> out;
                    pmt.clear_counters();

                    auto t0 = clock::now();
                    pmt.knnSearch(q, k, out);
                    auto t1 = clock::now();

                    totalD     += pmt.get_compDist();
                    totalT     += chrono::duration_cast<
                                      chrono::microseconds>(t1 - t0).count();
                    totalPages += pmt.get_pageReads();
                }

                double avgD     = double(totalD) / queries.size();
                double avgTms   = double(totalT) / (1000.0 * queries.size());
                double avgPages = double(totalPages) / queries.size();

                if (!first) J << ",\n";
                first = false;

                J << fixed << setprecision(6)
                  << "{"
                  << "\"index\":\"PMTREE\","
                  << "\"dataset\":\"" << dataset << "\","
                  << "\"category\":\"DM\","
                  << "\"num_pivots\":" << l << ","
                  << "\"num_centers_path\":1,"
                  << "\"arity\":null,"
                  << "\"query_type\":\"MkNN\","
                  << "\"selectivity\":null,"
                  << "\"radius\":null,"
                  << "\"k\":" << k << ","
                  << "\"compdists\":" << avgD << ","
                  << "\"time_ms\":" << avgTms << ","
                  << "\"pages\":" << avgPages << ","
                  << "\"n_queries\":" << queries.size() << ","
                  << "\"run_id\":1"
                  << "}";

                cerr << "OK (avg " << (long long)avgD
                     << " compdists, " << avgPages << " páginas)\n";
            }
        }

        J << "\n]\n";
        J.close();
        cerr << "[DONE] " << jsonOut << "\n";
    }

    return 0;
}
