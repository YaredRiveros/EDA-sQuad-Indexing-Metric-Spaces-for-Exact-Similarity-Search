#include <bits/stdc++.h>
#include "cpt.hpp"
#include "../../datasets/paths.hpp"
#include <filesystem>

using namespace std;

// ============================================================
// CONFIGURACIÓN DEL PAPER
// ============================================================

// Selectividades (MRQ)
static const vector<double> SELECTIVITIES = {0.02, 0.04, 0.08, 0.16, 0.32};

// K para MkNN
static const vector<int> K_VALUES = {5, 10, 20, 50, 100};

// Números de pivotes l (mismo esquema que LAESA)
static const vector<int> L_VALUES = {3, 5, 10, 15, 20};

// Datasets evaluados
static const vector<string> DATASETS = {"LA", "Words", "Color", "Synthetic"};


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

        // *** AQUÍ FALTABA EL PARÉNTESIS DE CIERRE ***
        if (!tok.empty() && all_of(tok.begin(), tok.end(), ::isdigit))
            piv.push_back(stoi(tok));
    }
    return piv;
}

// ============================================================
// MAIN — EXPERIMENTACIÓN COMPLETA PARA CPT (memoria secundaria)
// ============================================================
int main() {
    ios::sync_with_stdio(false);
    cin.tie(nullptr);

    // Crear carpeta de resultados
    std::filesystem::create_directories("results");

    for (const string &dataset : DATASETS) {
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
            db = make_unique<VectorDB>(dbfile, 2);
        }
        else if (dataset == "Color") {
            db = make_unique<VectorDB>(dbfile, 1);
        }
        else if (dataset == "Synthetic") {
            db = make_unique<VectorDB>(dbfile, 999999);
        }
        else if (dataset == "Words") {
            db = make_unique<StringDB>(dbfile);
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
        // 4. JSON Output
        // ------------------------------------------------------------
        string jsonOut = "results/results_CPT_" + dataset + ".json";
        ofstream J(jsonOut);
        if (!J.is_open()) {
            cerr << "[ERROR] No se pudo abrir " << jsonOut << " para escritura.\n";
            continue;
        }

        J << "[\n";
        bool firstOutput = true;

        // ------------------------------------------------------------
        // 5. Bucle sobre números de pivotes l
        // ------------------------------------------------------------
        for (int l : L_VALUES) {
            // Cargar pivots HFI para este dataset y l
            string pivfile = path_pivots(dataset, l);

            // *** AQUÍ TENÍAS load_pivots_file → usa load_pivots_json ***
            vector<int> pivots = load_pivots_json(pivfile);

            if (pivots.empty()) {
                cerr << "[WARN] No hay pivots para dataset=" << dataset
                     << " l=" << l << " (" << pivfile << ")\n";
                continue;
            }

            cerr << "\n------------------------------------------\n";
            cerr << "[INFO] Construyendo CPT para dataset=" << dataset
                 << " con l=" << l << " pivots\n";
            cerr << "------------------------------------------\n";

            // Construir CPT (tabla de pivots en RAM)
            CPT cpt(db.get(), l);
            cpt.overridePivots(pivots);

            // Construir layout de páginas desde el índice M-tree en disco.
            // Se asume que ya existe <dataset>.mtree_index en el directorio actual
            // (generado por tu mtree_test).
            cpt.buildFromMTree(dataset);

            // =======================
            //  MRQ (Metric Range Query)
            // =======================
            for (double sel : SELECTIVITIES) {
                if (radii.find(sel) == radii.end())
                    continue;

                double R = radii[sel];
                long long totalD = 0;
                long long totalT = 0;
                long long totalPages = 0;

                for (int q : queries) {
                    vector<int> out;
                    cpt.clear_counters();
                    cpt.rangeSearch(q, R, out);

                    totalD     += cpt.get_compDist();
                    totalT     += cpt.get_queryTime();
                    totalPages += cpt.get_pageReads();
                }

                double avgD     = double(totalD) / queries.size();
                double avgTus   = double(totalT) / queries.size();
                double avgPages = double(totalPages) / queries.size();

                if (!firstOutput) J << ",\n";
                firstOutput = false;

                J << fixed << setprecision(6);
                J << "{"
                  << "\"index\":\"CPT\","
                  << "\"dataset\":\"" << dataset << "\","
                  << "\"category\":\"DM\","
                  << "\"num_pivots\":" << l << ","
                  << "\"num_centers_path\":null,"
                  << "\"arity\":null,"
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
                long long totalD = 0;
                long long totalT = 0;
                long long totalPages = 0;

                for (int q : queries) {
                    vector<CPTResultElem> out;
                    cpt.clear_counters();
                    cpt.knnSearch(q, k, out);

                    totalD     += cpt.get_compDist();
                    totalT     += cpt.get_queryTime();
                    totalPages += cpt.get_pageReads();
                }

                double avgD     = double(totalD) / queries.size();
                double avgTus   = double(totalT) / queries.size();
                double avgPages = double(totalPages) / queries.size();

                J << ",\n";
                J << fixed << setprecision(6);
                J << "{"
                  << "\"index\":\"CPT\","
                  << "\"dataset\":\"" << dataset << "\","
                  << "\"category\":\"DM\","
                  << "\"num_pivots\":" << l << ","
                  << "\"num_centers_path\":null,"
                  << "\"arity\":null,"
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
