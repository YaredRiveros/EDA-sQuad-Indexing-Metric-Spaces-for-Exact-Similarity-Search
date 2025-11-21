#include <bits/stdc++.h>
#include "mtree.hpp"          // <— tu implementación final del M-tree Disk
#include "../../objectdb.hpp"
#include "../../datasets/paths.hpp"

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
    std::filesystem::create_directories("mtree_indexes");

    for (const string &dataset : DATASETS) {

        string dbfile = path_dataset(dataset);
        if (dbfile == "") {
            cerr << "[WARN] Dataset no encontrado: " << dataset << "\n";
            continue;
        }

        unique_ptr<ObjectDB> db;

        if (dataset == "LA")          db = make_unique<VectorDB>(dbfile, 2);
        else if (dataset == "Color")  db = make_unique<VectorDB>(dbfile, 1);
        else if (dataset == "Synthetic") db = make_unique<VectorDB>(dbfile, 0);
        else if (dataset == "Words")  db = make_unique<StringDB>(dbfile);
        else continue;

        cerr << "\n==========================================\n";
        cerr << "[MTree] Dataset: " << dataset
             << "   N=" << db->size() << "\n";
        cerr << "==========================================\n";

        vector<int> queries = load_queries_file(path_queries(dataset));
        auto radii = load_radii_file(path_radii(dataset));

        if (queries.empty()) {
            cerr << "[WARN] No hay queries para " << dataset << "\n";
            continue;
        }

        string jsonOut = "results/results_MTree_" + dataset + ".json";
        ofstream J(jsonOut);
        J << "[\n";
        bool firstOutput = true;

        // ========= Ajuste de pageBytes según Chen =========
        // El M-tree usa nodos más grandes, igual que Color/Synthetic en Chen
        int pageBytes = (dataset == "Color" || dataset == "Synthetic") ?
                        40960 : 4096;

        // Aridad aproximada = pageBytes / tamañoEntrada
        // Tamaño entrada: objId(4) + radius(8) + parentDist(8) + child(8) = 28B
        int entryBytes = 4 + 8 + 8 + 8;
        int nodeCapacity = max(4, pageBytes / entryBytes);

        // ========= Crear M-tree Disk =========
        MTree_Disk mt(db.get(), nodeCapacity);

        // ========= Construcción física =========
        string base = "mtree_indexes/" + dataset;
        mt.build(base);       // escribe base.mtree_index
        mt.restore(base);     // vuelve a abrir para queries

        // ===================================================================
        // MRQ
        // ===================================================================
        cout << "Searching...\n";
        for (double sel : SELECTIVITIES) {
            if (!radii.count(sel)) continue;

            double R = radii[sel];
            long long totalD = 0, totalT = 0, totalPages = 0;

            for (int q : queries) {
                vector<int> out;
                mt.clear_counters();
                mt.rangeSearch(q, R, out);

                totalD     += mt.get_compDist();
                totalT     += mt.get_queryTime();
                totalPages += mt.get_pageReads();
            }

            if (!firstOutput) J << ",\n";
            firstOutput = false;

            double avgD   = double(totalD)     / queries.size();
            double avgTms = double(totalT)     / (1000.0 * queries.size());
            double avgPA  = double(totalPages) / queries.size();

            J << fixed << setprecision(6)
              << "{"
              << "\"index\":\"MTREE\","
              << "\"dataset\":\"" << dataset << "\","
              << "\"category\":\"DM\","
              << "\"num_pivots\":0,"
              << "\"num_centers_path\":1,"
              << "\"arity\":" << nodeCapacity << ","
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
                mt.clear_counters();
                mt.knnSearch(q, k, out);

                totalD     += mt.get_compDist();
                totalT     += mt.get_queryTime();
                totalPages += mt.get_pageReads();
            }

            if (!firstOutput) J << ",\n";
            firstOutput = false;

            double avgD   = double(totalD)     / queries.size();
            double avgTms = double(totalT)     / (1000.0 * queries.size());
            double avgPA  = double(totalPages) / queries.size();

            J << fixed << setprecision(6)
              << "{"
              << "\"index\":\"MTREE\","
              << "\"dataset\":\"" << dataset << "\","
              << "\"category\":\"DM\","
              << "\"num_pivots\":0,"
              << "\"num_centers_path\":1,"
              << "\"arity\":" << nodeCapacity << ","
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
