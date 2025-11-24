#include <bits/stdc++.h>
#include "dindex.hpp"
#include "../../objectdb.hpp"
#include "../../datasets/paths.hpp"

using namespace std;

static const vector<double> SELECTIVITIES = {
    0.02, 0.04, 0.08, 0.16, 0.32
};

static const vector<int> K_VALUES = {
    5, 10, 20, 50, 100
};

// static const vector<string> DATASETS = {
//     "LA", "Words", "Color", "Synthetic"
// };

static const vector<string> DATASETS = {"LA"};

int main(int argc, char** argv) {
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


    std::filesystem::create_directories("results");
    std::filesystem::create_directories("dindex_indexes");

    for (const string &dataset : datasets) {

        string dbfile = path_dataset(dataset);
        if (dbfile == "") {
            cerr << "[WARN] Dataset no encontrado: " << dataset << "\n";
            continue;
        }

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

        cerr << "\n==========================================\n";
        cerr << "[D-INDEX] Dataset: " << dataset
             << "   N=" << db->size() << "\n";
        cerr << "==========================================\n";

        vector<int> queries = load_queries_file(path_queries(dataset));
        auto radii          = load_radii_file(path_radii(dataset));

        if (queries.empty()) {
            cerr << "[WARN] No queries for " << dataset << "\n";
            continue;
        }

        string jsonOut = "results/results_DIndex_" + dataset + ".json";
        ofstream J(jsonOut);
        J << "[\n";
        bool firstOutput = true;

        const int    numLevels = 5;
        const double rho       = 5.0;

        cerr << "[BUILD] Construyendo D-index (l=" << numLevels
             << ", rho=" << rho << ") con pivotes HFI...\n";

        string rafFile = "dindex_indexes/" + dataset + "_raf.bin";
        string hfiFile = path_pivots(dataset, numLevels);

        DIndex dindex(rafFile, db.get(), numLevels, rho);

        vector<DataObject> allObjects;
        allObjects.reserve(db->size());

        cerr << "[BUILD] Cargando " << db->size() << " objetos...\n";
        for (int i = 0; i < db->size(); i++) {
            DataObject obj;
            obj.id = i;       // 0-based
            allObjects.push_back(obj);
        }

        cerr << "[BUILD] Iniciando construcción de D-index...\n";
        dindex.build(allObjects, 42, hfiFile);
        cerr << "[BUILD] OK.\n";

        // ============================ MRQ ===================================
        cerr << "\n[MRQ] Ejecutando selectividades...\n";

        for (double sel : SELECTIVITIES) {
            if (!radii.count(sel)) continue;

            double R = radii[sel];
            cerr << "  [MRQ] sel=" << sel << "  R=" << R << "\n";

            long long totalD = 0;
            long long totalT = 0;
            long long totalP = 0;

            for (int q : queries) {
                dindex.clear_counters();

                auto start = chrono::high_resolution_clock::now();
                auto results = dindex.MRQ(q, R);   // ya verificado dentro
                auto end   = chrono::high_resolution_clock::now();

                long long elapsed =
                    chrono::duration_cast<chrono::microseconds>(end - start).count();

                (void)results; // no hace falta usarlos aquí

                totalD += dindex.get_compDist();
                totalT += elapsed;
                totalP += dindex.get_pageReads(); // seguirá en 0 mientras no modelemos páginas
            }

            double avgD   = double(totalD) / queries.size();
            double avgTms = double(totalT) / (1000.0 * queries.size());
            double avgPg  = double(totalP) / queries.size();

            if (!firstOutput) J << ",\n";
            firstOutput = false;

            J << std::fixed << std::setprecision(6)
              << "{"
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
              << "\"pages\":" << avgPg << ","
              << "\"n_queries\":" << queries.size() << ","
              << "\"run_id\":1"
              << "}";
        }

        // ============================ MkNN ==================================
        cerr << "\n[MkNN] Ejecutando valores de k...\n";

        for (int k : K_VALUES) {
            cerr << "  [MkNN] k=" << k << "\n";

            long long totalD = 0;
            long long totalT = 0;
            long long totalP = 0;

            for (int q : queries) {
                dindex.clear_counters();

                auto start = chrono::high_resolution_clock::now();
                auto knn   = dindex.MkNN(q, k);
                auto end   = chrono::high_resolution_clock::now();

                long long elapsed =
                    chrono::duration_cast<chrono::microseconds>(end - start).count();

                (void)knn;

                totalD += dindex.get_compDist();
                totalT += elapsed;
                totalP += dindex.get_pageReads();
            }

            double avgD   = double(totalD) / queries.size();
            double avgTms = double(totalT) / (1000.0 * queries.size());
            double avgPg  = double(totalP) / queries.size();

            if (!firstOutput) J << ",\n";
            firstOutput = false;

            J << std::fixed << std::setprecision(6)
              << "{"
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
              << "\"pages\":" << avgPg << ","
              << "\"n_queries\":" << queries.size() << ","
              << "\"run_id\":1"
              << "}";
        }

        J << "\n]\n";
        cerr << "[DONE] Output generado: " << jsonOut << "\n";
    }

    return 0;
}
