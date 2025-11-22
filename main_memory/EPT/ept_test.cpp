#include <bits/stdc++.h>
#include "ept.hpp"      // Tu implementación de EPT*
#include "../../datasets/paths.hpp"

using namespace std;
using namespace chrono;

// ============================================================
// CONFIGURACIÓN DEL PAPER (igual a FQT)
// ============================================================

static const vector<double> SELECTIVITIES = {0.02, 0.04, 0.08, 0.16, 0.32};
static const vector<int>    K_VALUES      = {5, 10, 20, 50, 100};
static const vector<string> DATASETS      = {"LA", "Words", "Color", "Synthetic"};

// Parámetros EPT* (como en Chen2022)
// {l, cp_scale}
struct EPT_Params {
    int l;         // pivots por objeto
    int cp_scale;  // tamaño HF candidate pool
};

// Parámetros por dataset — ajustables
static const vector<EPT_Params> PARAMS_LA = {
    {2, 40},
    {3, 40},
    {4, 40},
    {5, 40},
    {6, 40}
};

static const vector<EPT_Params> PARAMS_WORDS = {
    {2, 40},
    {3, 40},
    {4, 40},
    {5, 40},
    {6, 40}
};

static const vector<EPT_Params> PARAMS_SYNTH = {
    {2, 40},
    {3, 40},
    {4, 40},
    {5, 40},
    {6, 40}
};

static const vector<EPT_Params> PARAMS_COLOR = {
    {2, 40},
    {3, 40},
    {4, 40},
    {5, 40},
    {6, 40}
};


// ============================================================
// MAIN — EXPERIMENTACIÓN COMPLETA (igual estilo FQT)
// ============================================================
int main()
{
    srand(12345);
    std::filesystem::create_directories("results");

    for (const string& dataset : DATASETS)
    {
        // ------------------------------------------------------------
        // 1. Resolver ruta del dataset
        // ------------------------------------------------------------
        string dbfile = path_dataset(dataset);
        if (dbfile == "") {
            cerr << "[WARN] Dataset no encontrado: " << dataset << "\n";
            continue;
        }

        // ------------------------------------------------------------
        // 2. Cargar dataset con su métrica (igual que FQT)
        // ------------------------------------------------------------
        unique_ptr<ObjectDB> db;

        if (dataset == "LA")              db = make_unique<VectorDB>(dbfile, 2);
        else if (dataset == "Color")      db = make_unique<VectorDB>(dbfile, 1);
        else if (dataset == "Synthetic")  db = make_unique<VectorDB>(dbfile, 999999);
        else if (dataset == "Words")      db = make_unique<StringDB>(dbfile);
        else continue;

        int nObjects = db->size();

        cerr << "\n==========================================\n";
        cerr << "[INFO] Dataset: " << dataset << "   N=" << nObjects << "\n";
        cerr << "==========================================\n";

        // ------------------------------------------------------------
        // 3. Cargar queries y radios
        // ------------------------------------------------------------
        vector<int> queries = load_queries_file(path_queries(dataset));
        auto radii = load_radii_file(path_radii(dataset));

        if (queries.empty()) {
            cerr << "[WARN] Queries ausentes, ignorando dataset " << dataset << "\n";
            continue;
        }

        cerr << "[INFO] " << queries.size() << " queries cargadas.\n";

        // ------------------------------------------------------------
        // 4. Parámetros por dataset
        // ------------------------------------------------------------
        vector<EPT_Params> params;
        if (dataset == "LA")              params = PARAMS_LA;
        else if (dataset == "Words")      params = PARAMS_WORDS;
        else if (dataset == "Color")      params = PARAMS_COLOR;
        else if (dataset == "Synthetic")  params = PARAMS_SYNTH;

        // ------------------------------------------------------------
        // 5. Archivo JSON
        // ------------------------------------------------------------
        string jsonOut = "results/results_EPT_" + dataset + ".json";
        ofstream J(jsonOut);
        if (!J.is_open()) {
            cerr << "[ERROR] No se pudo escribir JSON.\n";
            return 1;
        }

        J << "[\n";
        bool firstOutput = true;

        // ------------------------------------------------------------
        // 6. Pruebas con diferentes configuraciones
        // ------------------------------------------------------------
        for (size_t cfg = 0; cfg < params.size(); cfg++)
        {
            auto& P = params[cfg];

            cerr << "[INFO] ===== Config " << (cfg+1) << "/" << params.size()
                 << "  l=" << P.l << ", cp_scale=" << P.cp_scale << " =====\n";

            // ------------------------------------------------------------
            // Construcción del índice EPT*
            // ------------------------------------------------------------
            auto t1 = high_resolution_clock::now();
            EPTStar<ObjectWrapper, MetricWrapper> index(
                db->objects,
                db->metric,
                P.l,
                P.cp_scale
            );
            auto t2 = high_resolution_clock::now();

            double build_ms = duration_cast<milliseconds>(t2 - t1).count();
            long long build_dists = db->metric.getCompdists();

            cerr << "[INFO] Construcción: " << build_ms << " ms , "
                 << build_dists << " compdists\n";

            // ============================================================
            // MkNN experiments
            // ============================================================
            cerr << "[INFO] Ejecutando MkNN...\n";

            for (int k : K_VALUES)
            {
                db->metric.resetCompdists();
                auto start = high_resolution_clock::now();

                double sum_kth = 0;
                for (int q : queries) {
                    double kth = index.knn(q, k);
                    sum_kth += kth;
                }

                auto end = high_resolution_clock::now();
                double totalTime = duration_cast<milliseconds>(end - start).count();

                double avgTime  = totalTime / queries.size();
                double avgDists = (double)db->metric.getCompdists() / queries.size();
                double avgRadius = sum_kth / queries.size();

                if (!firstOutput) J << ",\n";
                firstOutput = false;

                J << "  {\n";
                J << "    \"index\": \"EPT*\",\n";
                J << "    \"dataset\": \"" << dataset << "\",\n";
                J << "    \"category\": \"" << (dataset == \"Words\" ? \"strings\" : \"vectors\") << "\",\n";
                J << "    \"num_pivots\": " << P.l << ",\n";
                J << "    \"arity\": null,\n";
                J << "    \"bucket_size\": null,\n";
                J << "    \"query_type\": \"MkNN\",\n";
                J << "    \"selectivity\": null,\n";
                J << "    \"radius\": " << avgRadius << ",\n";
                J << "    \"k\": " << k << ",\n";
                J << "    \"compdists\": " << avgDists << ",\n";
                J << "    \"time_ms\": " << avgTime << ",\n";
                J << "    \"n_queries\": " << queries.size() << ",\n";
                J << "    \"run_id\": \"EPT_" << dataset << "_l" << P.l << "_c" << P.cp_scale << "_k" << k << "\"\n";
                J << "  }";
            }

            // ============================================================
            // MRQ experiments
            // ============================================================
            cerr << "[INFO] Ejecutando MRQ...\n";

            for (auto& entry : radii)
            {
                double sel = entry.first;
                double radius = entry.second;

                db->metric.resetCompdists();
                auto start = high_resolution_clock::now();

                int totalResults = 0;
                for (int q : queries) {
                    int count = index.range(q, radius);
                    totalResults += count;
                }

                auto end = high_resolution_clock::now();
                double totalTime = duration_cast<milliseconds>(end - start).count();

                double avgTime  = totalTime / queries.size();
                double avgDists = (double)db->metric.getCompdists() / queries.size();

                J << ",\n";
                J << "  {\n";
                J << "    \"index\": \"EPT*\",\n";
                J << "    \"dataset\": \"" << dataset << "\",\n";
                J << "    \"category\": \"" << (dataset == \"Words\" ? \"strings\" : \"vectors\") << "\",\n";
                J << "    \"num_pivots\": " << P.l << ",\n";
                J << "    \"arity\": null,\n";
                J << "    \"bucket_size\": null,\n";
                J << "    \"query_type\": \"MRQ\",\n";
                J << "    \"selectivity\": " << sel << ",\n";
                J << "    \"radius\": " << radius << ",\n";
                J << "    \"k\": null,\n";
                J << "    \"compdists\": " << avgDists << ",\n";
                J << "    \"time_ms\": " << avgTime << ",\n";
                J << "    \"n_queries\": " << queries.size() << ",\n";
                J << "    \"run_id\": \"EPT_" << dataset << "_l" << P.l << "_c" << P.cp_scale << "_sel" << sel << "\"\n";
                J << "  }";
            }
        }

        J << "\n]\n";
        J.close();

        cerr << "[INFO] Resultados guardados en: " << jsonOut << "\n";
    }

    cerr << "\n[DONE] Benchmark EPT* completado.\n";
    return 0;
}
