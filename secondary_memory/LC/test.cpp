#include <bits/stdc++.h>
#include "lc.hpp"
#include "objectdb.hpp"
#include "datasets/paths.hpp"

using namespace std;

// Selectividades para MRQ
static const vector<double> SELECTIVITIES = {0.02, 0.04, 0.08, 0.16, 0.32};

// k para MkNN
static const vector<int> K_VALUES = {5, 10, 20, 50, 100};

// Datasets del paper
static const vector<string> DATASETS = {"LA", "Words", "Color", "Synthetic"};

int main() {
    srand(12345);

    std::filesystem::create_directories("results");
    std::filesystem::create_directories("lc_indexes");

    for (const string &dataset : DATASETS) {

        string dbfile = path_dataset(dataset);
        if (dbfile == "") {
            cerr << "[WARN] Dataset no encontrado: " << dataset << "\n";
            continue;
        }

        unique_ptr<ObjectDB> db;

        if (dataset == "LA")          db = make_unique<VectorDB>(dbfile, 2);
        else if (dataset == "Color")  db = make_unique<VectorDB>(dbfile, 1);
        else if (dataset == "Synthetic") db = make_unique<VectorDB>(dbfile, 0); // L∞
        else if (dataset == "Words")  db = make_unique<StringDB>(dbfile);
        else continue;

        cerr << "\n==========================================\n";
        cerr << "[LC] Dataset: " << dataset
             << "   N=" << db->size() << "\n";
        cerr << "==========================================\n";

        vector<int> queries = load_queries_file(path_queries(dataset));
        auto radii = load_radii_file(path_radii(dataset));

        if (queries.empty()) {
            cerr << "[WARN] No hay queries para " << dataset << "\n";
            continue;
        }

        string jsonOut = "results/results_LC_" + dataset + ".json";
        ofstream J(jsonOut);
        J << "[\n";
        bool firstOutput = true;

        // ========= PAGE SIZE según el paper =========
        int pageBytes = (dataset == "Color" || dataset == "Synthetic") ?
                        40960 : 4096;

        // ========= Crear LC Disk =========
        LC_Disk lc(db.get(), pageBytes);

        // ========= Construir índice físico =========
        string base = "lc_indexes/" + dataset;
        lc.build(base);      // escribe en disco: base.lc_index y base.lc_node
        lc.restore(base);    // abre base.lc_node y carga base.lc_index

        int numClusters = lc.get_num_clusters();

        // ===================================================================
        // MRQ
        // ===================================================================
        for (double sel : SELECTIVITIES) {
            if (!radii.count(sel)) continue;

            double R = radii[sel];
            long long totalD = 0, totalT = 0, totalPages = 0;

            for (int q : queries) {
                vector<int> out;
                lc.clear_counters();
                lc.rangeSearch(q, R, out);

                totalD     += lc.get_compDist();
                totalT     += lc.get_queryTime();
                totalPages += lc.get_pageReads();
            }

            if (!firstOutput) J << ",\n";
            firstOutput = false;

            double avgD   = double(totalD)     / queries.size();
            double avgTms = double(totalT)     / (1000.0 * queries.size());
            double avgPA  = double(totalPages) / queries.size();

            J << fixed << setprecision(6)
              << "{"
              << "\"index\":\"LC\","
              << "\"dataset\":\"" << dataset << "\","
              << "\"category\":\"DM\","
              << "\"num_pivots\":0,"
              << "\"num_centers_path\":1,"
              << "\"arity\":null,"
              << "\"query_type\":\"MRQ\","
              << "\"selectivity\":" << sel << ","
              << "\"radius\":" << R << ","
              << "\"k\":null,"
              << "\"compdists\":" << avgD << ","
              << "\"time_ms\":" << avgTms << ","
              << "\"pages\":" << avgPA << ","
              << "\"n_queries\":" << queries.size() << ","
              << "\"run_id\":1"
              << "}";
        }

        // ===================================================================
        // MkNN
        // ===================================================================
        for (int k : K_VALUES) {
            long long totalD = 0, totalT = 0, totalPages = 0;

            for (int q : queries) {
                vector<pair<double,int>> out;
                lc.clear_counters();
                lc.knnSearch(q, k, out);

                totalD     += lc.get_compDist();
                totalT     += lc.get_queryTime();
                totalPages += lc.get_pageReads();
            }

            if (!firstOutput) J << ",\n";
            firstOutput = false;

            double avgD   = double(totalD)     / queries.size();
            double avgTms = double(totalT)     / (1000.0 * queries.size());
            double avgPA  = double(totalPages) / queries.size();

            J << fixed << setprecision(6)
              << "{"
              << "\"index\":\"LC\","
              << "\"dataset\":\"" << dataset << "\","
              << "\"category\":\"DM\","
              << "\"num_pivots\":0,"
              << "\"num_centers_path\":1,"
              << "\"arity\":null,"
              << "\"query_type\":\"MkNN\","
              << "\"selectivity\":null,"
              << "\"radius\":null,"
              << "\"k\":" << k << ","
              << "\"compdists\":" << avgD << ","
              << "\"time_ms\":" << avgTms << ","
              << "\"pages\":" << avgPA << ","
              << "\"n_queries\":" << queries.size() << ","
              << "\"run_id\":1"
              << "}";
        }

        J << "\n]\n";
        cerr << "[DONE] Archivo generado: " << jsonOut << "\n";
    }

    return 0;
}
