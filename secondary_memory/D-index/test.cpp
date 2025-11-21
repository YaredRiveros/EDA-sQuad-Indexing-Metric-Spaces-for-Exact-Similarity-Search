#include <bits/stdc++.h>
#include "dindex.hpp"
#include "../../objectdb.hpp"
#include "../../datasets/paths.hpp"

using namespace std;

// Selectividades del paper
static const vector<double> SELECTIVITIES = {0.02, 0.04, 0.08, 0.16, 0.32};

// Ks para MkNN
static const vector<int> K_VALUES = {5, 10, 20, 50, 100};

// Datasets del paper
static const vector<string> DATASETS = {"LA", "Words", "Color", "Synthetic"};

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
        else if (dataset == "Synthetic")  db = make_unique<VectorDB>(dbfile, 0);
        else if (dataset == "Words")      db = make_unique<StringDB>(dbfile);
        else continue;

        cerr << "\n==========================================\n";
        cerr << "[D-INDEX] Dataset: " << dataset
             << "   N=" << db->size() << "\n";
        cerr << "==========================================\n";

        vector<int> queries = load_queries_file(path_queries(dataset));
        auto radii = load_radii_file(path_radii(dataset));

        if (queries.empty()) {
            cerr << "[WARN] No hay queries para " << dataset << "\n";
            continue;
        }

        string jsonOut = "results/results_DIndex_" + dataset + ".json";
        ofstream J(jsonOut);
        J << "[\n";
        bool firstOutput = true;

        // Configuración fija para D-index (similar a EGNAT y OmniR-tree)
        int numLevels = 4;    // número de niveles/pivotes
        double rho = 5.0;     // parámetro rho para ρ-split

        cerr << "[BUILD] Construyendo D-index con " << numLevels 
             << " niveles y rho=" << rho << "...\n";

        // Crear D-index
        string rafFile = "dindex_indexes/" + dataset + "_raf.bin";
        DIndex dindex(rafFile, db.get(), numLevels, rho);

        // Build: preparar todos los objetos (solo ids, payload no necesario)
        vector<DataObject> allObjects;
        allObjects.reserve(db->size());
        
        cerr << "[BUILD] Cargando " << db->size() << " objetos...\n";
        for (int i = 1; i <= db->size(); i++) {
            if (i % 10000 == 0) {
                cerr << "  Cargados " << i << " objetos (" 
                     << (100 * i / db->size()) << "%)\n";
            }
            DataObject obj;
            obj.id = i;
            // El payload no se usa porque trabajamos con ObjectDB
            allObjects.push_back(obj);
        }

        cerr << "[BUILD] Iniciando construcción del índice...\n";
        dindex.build(allObjects, 42);
        cerr << "[BUILD] OK - D-index construido\n";
        dindex.printStats();

        // ===================================================================
        // MRQ
        // ===================================================================
        cerr << "\n[MRQ] Ejecutando experimentos con 5 selectividades...\n";
        for (double sel : SELECTIVITIES) {
            if (!radii.count(sel)) continue;

            double R = radii[sel];
            cerr << "  [MRQ] sel=" << sel << " R=" << R << " ... ";

            long long totalD = 0, totalT = 0, totalPages = 0;

            for (int q : queries) {
                dindex.clear_counters();
                auto start = chrono::high_resolution_clock::now();
                
                vector<uint64_t> candidates = dindex.MRQ(q, R);
                
                auto end = chrono::high_resolution_clock::now();
                auto elapsed = chrono::duration_cast<chrono::microseconds>(end - start).count();

                // Verificar candidatos (calcular distancias exactas)
                vector<int> results;
                for (uint64_t cid : candidates) {
                    double dist = db->distance(q, cid);
                    if (dist <= R) {
                        results.push_back(cid);
                    }
                }

                totalD     += dindex.get_compDist();
                totalT     += elapsed;
                totalPages += dindex.get_pageReads();
            }

            cerr << "OK\n";

            if (!firstOutput) J << ",\n";
            firstOutput = false;

            double avgD   = double(totalD)     / queries.size();
            double avgTms = double(totalT)     / (1000.0 * queries.size());
            double avgPA  = double(totalPages) / queries.size();

            J << fixed << setprecision(6)
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
              << "\"pages\":" << avgPA << ","
              << "\"n_queries\":" << queries.size() << ","
              << "\"run_id\":1"
              << "}";
        }

        // ===================================================================
        // MkNN
        // ===================================================================
        cerr << "\n[MkNN] Ejecutando experimentos con 5 valores de k...\n";
        for (int k : K_VALUES) {
            cerr << "  [MkNN] k=" << k << " ... ";

            long long totalD = 0, totalT = 0, totalPages = 0;

            for (int q : queries) {
                dindex.clear_counters();
                auto start = chrono::high_resolution_clock::now();
                
                vector<pair<uint64_t, double>> knn = dindex.MkNN(q, k);
                
                auto end = chrono::high_resolution_clock::now();
                auto elapsed = chrono::duration_cast<chrono::microseconds>(end - start).count();

                totalD     += dindex.get_compDist();
                totalT     += elapsed;
                totalPages += dindex.get_pageReads();
            }

            cerr << "OK\n";

            if (!firstOutput) J << ",\n";
            firstOutput = false;

            double avgD   = double(totalD)     / queries.size();
            double avgTms = double(totalT)     / (1000.0 * queries.size());
            double avgPA  = double(totalPages) / queries.size();

            J << fixed << setprecision(6)
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
