#include <bits/stdc++.h>
#include "dindex.hpp"
#include "../../objectdb.hpp"
#include "../../datasets/paths.hpp"

using namespace std;

// Selectividades del paper (Chen 2022)
static const vector<double> SELECTIVITIES = {
    0.02, 0.04, 0.08, 0.16, 0.32
};

// Valores de k usados en el paper
static const vector<int> K_VALUES = {
    5, 10, 20, 50, 100
};

// Datasets del entorno experimental
static const vector<string> DATASETS = {
    "LA", "Words", "Color", "Synthetic"
};

int main() {
    srand(12345);

    std::filesystem::create_directories("results");
    std::filesystem::create_directories("dindex_indexes");

    for (const string &dataset : DATASETS) {

        string dbfile = path_dataset(dataset);
        if (dbfile == "") {
            cerr << "[WARN] Dataset no encontrado: " << dataset << "\n";
            continue;
        }

        unique_ptr<ObjectDB> db;

        if (dataset == "LA")              db = make_unique<VectorDB>(dbfile, 2);
        else if (dataset == "Color")      db = make_unique<VectorDB>(dbfile, 1);
        else if (dataset == "Synthetic")  db = make_unique<VectorDB>(dbfile, 999999);
        else if (dataset == "Words")      db = make_unique<StringDB>(dbfile);
        else continue;

        cerr << "\n==========================================\n";
        cerr << "[D-INDEX] Dataset: " << dataset
             << "   N=" << db->size() << "\n";
        cerr << "==========================================\n";

        vector<int> queries = load_queries_file(path_queries(dataset));
        auto radii = load_radii_file(path_radii(dataset));

        if (queries.empty()) {
            cerr << "[WARN] No queries for " << dataset << "\n";
            continue;
        }

        string jsonOut = "results/results_DIndex_" + dataset + ".json";
        ofstream J(jsonOut);
        J << "[\n";
        bool firstOutput = true;

        // Número de pivotes igual que en el paper (l = 5)
        const int numLevels = 5;
        const double rho = 5.0;

        cerr << "[BUILD] Construyendo D-index con pivotes HFI (l=5, rho=5)...\n";

        string rafFile = "dindex_indexes/" + dataset + "_raf.bin";
        string hfiFile = path_pivots(dataset, numLevels);

        DIndex dindex(rafFile, db.get(), numLevels, rho);

        // Cargar objetos (solo ID)
        vector<DataObject> allObjects;
        allObjects.reserve(db->size());

        cerr << "[BUILD] Cargando " << db->size() << " objetos...\n";
        for (int i = 1; i <= db->size(); i++) {
            DataObject obj;
            obj.id = i;       // payload vacío
            allObjects.push_back(obj);

            if (i % 200000 == 0)
                cerr << "  " << i << " objetos cargados\n";
        }

        cerr << "[BUILD] Iniciando construcción...\n";
        dindex.build(allObjects, 42, hfiFile);
        cerr << "[BUILD] OK.\n";

        // ================================================================
        // MRQ (igual que en Chen)
        // ================================================================
        cerr << "\n[MRQ] Ejecutando selectividades...\n";

        for (double sel : SELECTIVITIES) {
            if (!radii.count(sel)) continue;

            double R = radii[sel];
            cerr << "  [MRQ] sel=" << sel << " R=" << R << "\n";

            long long totalD = 0, totalT = 0, totalPages = 0;

            for (int q : queries) {
                dindex.clear_counters();

                auto start = chrono::high_resolution_clock::now();
                vector<uint32_t> candidates = dindex.MRQ(q, R);
                auto end = chrono::high_resolution_clock::now();

                long long elapsed =
                    chrono::duration_cast<chrono::microseconds>(end - start).count();

                // Verificación EXACTA (estas distancias también cuentan!)
                long long verif = 0;
                for (uint32_t cid : candidates) {
                    double d = db->distance(q, cid);
                    verif++;
                }

                totalD     += dindex.get_compDist() + verif;
                totalT     += elapsed;
                totalPages += dindex.get_pageReads();
            }

            if (!firstOutput) J << ",\n";
            firstOutput = false;

            double avgD   = double(totalD) / queries.size();
            double avgTms = double(totalT) / (1000.0 * queries.size());
            double avgPA  = double(totalPages) / queries.size();

            J << "{"
              << "\"index\":\"DIndex\","
              << "\"dataset\":\"" << dataset << "\","
              << "\"category\":\"D\","
              << "\"num_levels\":" << numLevels << ","
              << "\"rho\":" << rho << ","
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

        // ================================================================
        // MkNN (Chen)
        // ================================================================
        cerr << "\n[MkNN] Ejecutando valores de k...\n";

        for (int k : K_VALUES) {
            cerr << "  [MkNN] k=" << k << "\n";

            long long totalD = 0, totalT = 0, totalPages = 0;

            for (int q : queries) {
                dindex.clear_counters();

                auto start = chrono::high_resolution_clock::now();
                auto knn = dindex.MkNN(q, k);
                auto end  = chrono::high_resolution_clock::now();

                long long elapsed =
                    chrono::duration_cast<chrono::microseconds>(end - start).count();

                totalD     += dindex.get_compDist();
                totalT     += elapsed;
                totalPages += dindex.get_pageReads();
            }

            if (!firstOutput) J << ",\n";
            firstOutput = false;

            double avgD   = double(totalD) / queries.size();
            double avgTms = double(totalT) / (1000.0 * queries.size());
            double avgPA  = double(totalPages) / queries.size();

            J << "{"
              << "\"index\":\"DIndex\","
              << "\"dataset\":\"" << dataset << "\","
              << "\"category\":\"D\","
              << "\"num_levels\":" << numLevels << ","
              << "\"rho\":" << rho << ","
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
        cerr << "[DONE] Output generado: " << jsonOut << "\n";
    }

    return 0;
}
