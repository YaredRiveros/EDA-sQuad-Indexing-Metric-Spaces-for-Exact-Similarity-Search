// gnat_test.cpp
#include <bits/stdc++.h>
#include <filesystem>
#include <chrono>

#include "../../objectdb.hpp"
#include "../../datasets/paths.hpp"
#include "GNAT.hpp"

using namespace std;
using namespace chrono;
namespace fs = std::filesystem;

// Altura global del GNAT (usada por gnat.hpp)
int MaxHeight = 0;

// Config igual que antes
static const vector<double> SELECTIVITIES = {0.02, 0.04, 0.08, 0.16, 0.32};
static const vector<int>    K_VALUES      = {5, 10, 20, 50, 100};
// Puedes cambiar el set por defecto aquí
static const vector<string> DATASETS      = {"LA"};
static const vector<int>    HEIGHT_VALUES = {3, 5, 10, 15, 20};

string dataset_category(const string& dataset) {
    if (dataset == "LA")       return "vectors";
    if (dataset == "Color")    return "vectors";
    if (dataset == "Synthetic")return "vectors";
    if (dataset == "Words")    return "strings";
    return "unknown";
}

int main(int argc, char** argv) {
    vector<string> datasets;

    if (argc > 1) {
        for (int i = 1; i < argc; ++i)
            datasets.push_back(argv[i]);
    } else {
        datasets = DATASETS;
    }

    fs::create_directories("results");

    for (const string& dataset : datasets) {
        string dbfile = path_dataset(dataset);
        if (dbfile.empty() || !fs::exists(dbfile)) {
            cerr << "[WARN] Dataset no encontrado, omitido: " << dataset
                 << " (" << dbfile << ")\n";
            continue;
        }

        cerr << "\n==========================================\n";
        cerr << "[INFO] Dataset: " << dataset << "   File=" << dbfile << "\n";
        cerr << "==========================================\n";

        unique_ptr<ObjectDB> db;

        if (dataset == "LA" || dataset == "Color" || dataset == "Synthetic") {
            // VectorDB lee el header "dim n p" y ajusta la norma
            db = make_unique<VectorDB>(dbfile);
        } else if (dataset == "Words") {
            db = make_unique<StringDB>(dbfile);
        } else {
            cerr << "[WARN] Tipo de dataset no reconocido: " << dataset << "\n";
            continue;
        }

        int nObjects = db->size();
        if (nObjects == 0) {
            cerr << "[WARN] Dataset vacío, omitido: " << dataset << "\n";
            continue;
        }

        cerr << "[INFO] Objetos: " << nObjects << "\n";

        vector<int> queries = load_queries_file(path_queries(dataset));
        auto radii          = load_radii_file(path_radii(dataset));

        if (queries.empty()) {
            cerr << "[WARN] Queries ausentes, omitiendo dataset: " << dataset << "\n";
            continue;
        }

        cerr << "[INFO] Loaded " << queries.size() << " queries\n";
        cerr << "[INFO] Loaded " << radii.size()   << " radii\n";

        string jsonOut = "results/results_GNAT_" + dataset + ".json";
        ofstream J(jsonOut);
        if (!J.is_open()) {
            cerr << "[ERROR] No se pudo abrir JSON para escribir: " << jsonOut << "\n";
            return 1;
        }

        J << "[\n";
        bool firstConfig = true;

        for (int HEIGHT : HEIGHT_VALUES) {
            cerr << "[INFO] ============ HEIGHT=" << HEIGHT << " ============\n";

            MaxHeight = HEIGHT;   // límite global de altura
            int arity = 5;        // avg_pivot_cnt

            cerr << "[INFO] Construyendo índice GNAT con HEIGHT=" << HEIGHT
                 << ", avg_pivot_cnt=" << arity << "...\n";

            GNAT_t index(db.get(), arity);

            auto t1 = high_resolution_clock::now();
            long long prevDist = index.get_compDist();

            index.build();

            auto t2 = high_resolution_clock::now();
            double buildTime  = duration_cast<milliseconds>(t2 - t1).count();
            double buildDists = index.get_compDist() - prevDist;

            cerr << "[INFO] Construcción completada: "
                 << buildTime << " ms, "
                 << buildDists << " compdists\n";

            // ===================== MkNN =====================
            cerr << "[INFO] Ejecutando MkNN queries...\n";

            for (int k : K_VALUES) {
                cerr << "  k=" << k << "...\n";

                index.reset_compDist();
                auto start = high_resolution_clock::now();

                double avgRadius = 0.0;
                index.knnSearch(queries, k, avgRadius);

                auto end = high_resolution_clock::now();
                double totalTime = duration_cast<milliseconds>(end - start).count();

                double avgTime  = totalTime / queries.size();
                double avgDists = (double)index.get_compDist() / queries.size();

                if (!firstConfig) J << ",\n";
                firstConfig = false;

                J << "  {\n";
                J << "    \"index\": \"GNAT\",\n";
                J << "    \"dataset\": \"" << dataset << "\",\n";
                J << "    \"category\": \"" << dataset_category(dataset) << "\",\n";
                J << "    \"num_pivots\": " << HEIGHT << ",\n";
                J << "    \"num_centers_path\": null,\n";
                J << "    \"arity\": " << arity << ",\n";
                J << "    \"bucket_size\": null,\n";
                J << "    \"query_type\": \"MkNN\",\n";
                J << "    \"selectivity\": null,\n";
                J << "    \"radius\": " << avgRadius << ",\n";
                J << "    \"k\": " << k << ",\n";
                J << "    \"compdists\": " << avgDists << ",\n";
                J << "    \"time_ms\": " << avgTime << ",\n";
                J << "    \"n_queries\": " << queries.size() << ",\n";
                J << "    \"run_id\": \"GNAT_" << dataset
                                             << "_H" << HEIGHT
                                             << "_k" << k << "\"\n";
                J << "  }";
            }

            // ===================== MRQ =====================
            cerr << "[INFO] Ejecutando MRQ queries...\n";

            for (const auto& entry : radii) {
                double sel    = entry.first;
                double radius = entry.second;

                cerr << "  selectivity=" << sel
                     << ", radius=" << radius << "...\n";

                index.reset_compDist();
                auto start = high_resolution_clock::now();

                int totalResults = 0;
                index.rangeSearch(queries, radius, totalResults);

                auto end = high_resolution_clock::now();
                double totalTime = duration_cast<milliseconds>(end - start).count();

                double avgTime    = totalTime / queries.size();
                double avgDists   = (double)index.get_compDist() / queries.size();
                double avgResults = (double)totalResults / queries.size();

                (void)avgResults; // si no lo usas en JSON puedes quitar el warning

                J << ",\n";
                J << "  {\n";
                J << "    \"index\": \"GNAT\",\n";
                J << "    \"dataset\": \"" << dataset << "\",\n";
                J << "    \"category\": \"" << dataset_category(dataset) << "\",\n";
                J << "    \"num_pivots\": " << HEIGHT << ",\n";
                J << "    \"num_centers_path\": null,\n";
                J << "    \"arity\": " << arity << ",\n";
                J << "    \"bucket_size\": null,\n";
                J << "    \"query_type\": \"MRQ\",\n";
                J << "    \"selectivity\": " << sel << ",\n";
                J << "    \"radius\": " << radius << ",\n";
                J << "    \"k\": null,\n";
                J << "    \"compdists\": " << avgDists << ",\n";
                J << "    \"time_ms\": " << avgTime << ",\n";
                J << "    \"n_queries\": " << queries.size() << ",\n";
                J << "    \"run_id\": \"GNAT_" << dataset
                                             << "_H" << HEIGHT
                                             << "_sel" << sel << "\"\n";
                J << "  }";
            }
        }

        J << "\n]\n";
        J.close();

        cerr << "[INFO] Resultados guardados en: " << jsonOut << "\n";
    }

    cerr << "\n[DONE] GNAT benchmark completado.\n";
    return 0;
}
