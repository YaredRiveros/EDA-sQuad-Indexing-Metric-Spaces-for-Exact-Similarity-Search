#include <bits/stdc++.h>
#include "pm_tree.hpp"
#include "../../datasets/paths.hpp"
#include <filesystem>

using namespace std;

// ============================================================
// CONFIGURACIÓN DEL PAPER (igual que Chen2022)
// ============================================================

// Selectividades (MRQ)
static const vector<double> SELECTIVITIES = {0.02, 0.04, 0.08, 0.16, 0.32};

// K para MkNN
static const vector<int> K_VALUES = {5, 10, 20, 50, 100};

// Números de pivotes l
static const vector<int> L_VALUES = {3, 5, 10, 15, 20};

// Datasets evaluados
static const vector<string> DATASETS = {"LA", "Words", "Color", "Synthetic"};


// ============================================================
// Cargar pivots desde JSON (precomputed by HFI, igual que LAESA)
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
// MAIN — EXPERIMENTACIÓN COMPLETA PARA PM-tree
// ============================================================
int main() {
    ios::sync_with_stdio(false);
    cin.tie(nullptr);

    std::filesystem::create_directories("results");

    for (const string& dataset : DATASETS) {
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

        if (dataset == "LA") {
            db = make_unique<VectorDB>(dbfile, 2);       // L2
        }
        else if (dataset == "Color") {
            db = make_unique<VectorDB>(dbfile, 1);       // L1
        }
        else if (dataset == "Synthetic") {
            db = make_unique<VectorDB>(dbfile, 999999);  // L∞
        }
        else if (dataset == "Words") {
            db = make_unique<StringDB>(dbfile);          // Edit distance
        }
        else {
            cerr << "[WARN] Tipo de dataset desconocido: " << dataset << "\n";
            continue;
        }

        int nObjects = db->size();
        cerr << "\n==========================================\n";
        cerr << "[INFO] Dataset: " << dataset
             << "   N=" << nObjects << "\n";
        cerr << "==========================================\n";

        if (nObjects == 0) {
            cerr << "[WARN] Dataset vacío, se omite.\n";
            continue;
        }

        // ------------------------------------------------------------
        // 3. Cargar queries + radios
        // ------------------------------------------------------------
        vector<int> queries = load_queries_file(path_queries(dataset));
        auto radii = load_radii_file(path_radii(dataset));

        if (queries.empty()) {
            cerr << "[WARN] No hay queries para " << dataset << "\n";
            continue;
        }

        // ------------------------------------------------------------
        // 4. Calcular arity (nodeCapacity) como en M-tree
        // ------------------------------------------------------------
        int pageBytes;
        if (dataset == "LA" || dataset == "Words")
            pageBytes = 4096;      // 4KB
        else
            pageBytes = 40960;     // 40KB

        int entryBytes =
            sizeof(int32_t) +      // objId
            sizeof(double)  +      // radius
            sizeof(double)  +      // parentDist
            sizeof(int64_t);       // childOffset

        int nodeCapacity = pageBytes / entryBytes;

        // ------------------------------------------------------------
        // 5. Archivo JSON
        // ------------------------------------------------------------
        string jsonOut = "results/results_PMTREE_" + dataset + ".json";
        ofstream J(jsonOut);
        if (!J.is_open()) {
            cerr << "[ERROR] No se pudo abrir " << jsonOut << " para escritura.\n";
            continue;
        }

        J << "[\n";
        bool firstOutput = true;

        // ------------------------------------------------------------
        // 6. Bucle sobre números de pivotes l
        // ------------------------------------------------------------
        for (int l : L_VALUES) {
            // Cargar pivots HFI para este dataset y l
            string pivfile = path_pivots(dataset, l);
            vector<int> pivots = load_pivots_json(pivfile);

            if (pivots.empty()) {
                cerr << "[WARN] No hay pivots para dataset=" << dataset
                     << " l=" << l << " (" << pivfile << ")\n";
                continue;
            }

            cerr << "\n------------------------------------------\n";
            cerr << "[INFO] Construyendo PM-tree para dataset=" << dataset
                 << " con l=" << l << " pivots\n";
            cerr << "------------------------------------------\n";

            // Construir PM-tree sobre el mismo M-tree en disco (<dataset>.mtree_index)
            PMTree pmt(db.get(), l);
            pmt.buildFromMTree(dataset);
            pmt.overridePivots(pivots);   // construye distMatrix + HR/PD

            // =======================
            //  MRQ (Metric Range Query)
            // =======================
            for (double sel : SELECTIVITIES) {
                if (radii.find(sel) == radii.end())
                    continue;

                double R = radii[sel];
                long long totalD     = 0;
                long long totalT     = 0;
                long long totalPages = 0;

                for (int q : queries) {
                    vector<int> out;
                    pmt.clear_counters();
                    pmt.rangeSearch(q, R, out);

                    totalD     += pmt.get_compDist();
                    totalT     += pmt.get_queryTime();
                    totalPages += pmt.get_pageReads();
                }

                double avgD     = double(totalD) / queries.size();
                double avgTus   = double(totalT) / queries.size();
                double avgPages = double(totalPages) / queries.size();

                if (!firstOutput) J << ",\n";
                firstOutput = false;

                J << fixed << setprecision(6);
                J << "{"
                  << "\"index\":\"PMTREE\","
                  << "\"dataset\":\"" << dataset << "\","
                  << "\"category\":\"DM\","
                  << "\"num_pivots\":" << l << ","
                  << "\"num_centers_path\":1,"     // mismo que M-tree
                  << "\"arity\":" << nodeCapacity << ","
                  << "\"query_type\":\"MRQ\","
                  << "\"selectivity\":" << sel << ","
                  << "\"radius\":" << R << ","
                  << "\"k\":null,"
                  << "\"compdists\":" << avgD << ","
                  << "\"time_ms\":" << (avgTus / 1000.0) << ","
                  << "\"pages\":" << avgPages << ","
                  << "\"n_queries\":" << queries.size() << ","
                  << "\"run_id\":1"
                  << "}";
            }

            // =======================
            //  MkNN
            // =======================
            for (int k : K_VALUES) {
                long long totalD     = 0;
                long long totalT     = 0;
                long long totalPages = 0;

                for (int q : queries) {
                    vector<pair<double,int>> out;
                    pmt.clear_counters();
                    pmt.knnSearch(q, k, out);

                    totalD     += pmt.get_compDist();
                    totalT     += pmt.get_queryTime();
                    totalPages += pmt.get_pageReads();
                }

                double avgD     = double(totalD) / queries.size();
                double avgTus   = double(totalT) / queries.size();
                double avgPages = double(totalPages) / queries.size();

                J << ",\n";
                J << fixed << setprecision(6);
                J << "{"
                  << "\"index\":\"PMTREE\","
                  << "\"dataset\":\"" << dataset << "\","
                  << "\"category\":\"DM\","
                  << "\"num_pivots\":" << l << ","
                  << "\"num_centers_path\":1,"
                  << "\"arity\":" << nodeCapacity << ","
                  << "\"query_type\":\"MkNN\","
                  << "\"selectivity\":null,"
                  << "\"radius\":null,"
                  << "\"k\":" << k << ","
                  << "\"compdists\":" << avgD << ","
                  << "\"time_ms\":" << (avgTus / 1000.0) << ","
                  << "\"pages\":" << avgPages << ","
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
