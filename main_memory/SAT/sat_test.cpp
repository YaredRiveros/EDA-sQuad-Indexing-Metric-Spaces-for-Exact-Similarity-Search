#include <bits/stdc++.h>
#include "sat.hpp"
#include "../../datasets/paths.hpp"

using namespace std;

// ============================================================
// CONFIGURACIÓN DEL PAPER
// ============================================================

// Selectividades (MRQ)
static const vector<double> SELECTIVITIES = {0.02, 0.04, 0.08, 0.16, 0.32};

// K para MkNN
static const vector<int> K_VALUES = {5, 10, 20, 50, 100};

// Datasets evaluados
static const vector<string> DATASETS = {"LA", "Words", "Color", "Synthetic"};


// ============================================================
// MAIN — EXPERIMENTACIÓN COMPLETA PARA SAT
// ============================================================
int main()
{
    ios::sync_with_stdio(false);
    cin.tie(nullptr);

    // Semilla para cualquier aleatoriedad futura (si hiciera falta)
    srand(42);

    // CREAR SUBCARPETA 'RESULTS'
    std::filesystem::create_directories("results");

    for (const string &dataset : DATASETS)
    {
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
        string jsonOut = "results/results_SAT_" + dataset + ".json";
        ofstream J(jsonOut);
        J << "[\n";
        bool firstOutput = true;

        // ------------------------------------------------------------
        // 5. Construir SAT (sin parámetros externos)
        // ------------------------------------------------------------
        cerr << "\n------------------------------------------\n";
        cerr << "[INFO] Construyendo SAT (sin parámetros)\n";
        cerr << "------------------------------------------\n";

        SAT sat(db.get());
        sat.build();

        int realHeight = sat.get_height();
        int numPivots  = sat.get_num_pivots();

        cerr << "[INFO] Altura real = " << realHeight
             << "   #Pivots=" << numPivots << "\n";

        // ============================================================
        //  MRQ (range queries)
        // ============================================================
        for (double sel : SELECTIVITIES)
        {
            if (radii.find(sel) == radii.end())
                continue;

            double R = radii[sel];
            long long totalD = 0, totalT = 0;

            for (int q : queries)
            {
                vector<int> out;
                sat.clear_counters();
                sat.rangeSearch(q, R, out);

                totalD += sat.get_compDist();
                totalT += sat.get_queryTime();
            }

            double avgD = double(totalD) / queries.size();
            double avgT = double(totalT) / queries.size();

            if (!firstOutput) J << ",\n";
            firstOutput = false;

            J << fixed << setprecision(6);
            J << "{"
              << "\"index\":\"SAT\","
              << "\"dataset\":\"" << dataset << "\","
              << "\"category\":\"FQ\","
              << "\"num_pivots\":" << numPivots << ","
              << "\"num_centers_path\":" << realHeight << ","
              << "\"arity\":null,"                 // si luego calculas aridad media, cámbialo aquí
              << "\"query_type\":\"MRQ\","
              << "\"selectivity\":" << sel << ","
              << "\"radius\":" << R << ","
              << "\"k\":null,"
              << "\"compdists\":" << avgD << ","
              << "\"time_ms\":" << (avgT / 1000.0) << ","
              << "\"n_queries\":" << queries.size() << ","
              << "\"run_id\":1"
              << "}";
        }

        // ============================================================
        //  MkNN
        // ============================================================
        for (int k : K_VALUES)
        {
            long long totalD = 0, totalT = 0;

            for (int q : queries)
            {
                // OJO:
                // Usa SATResultElem si así lo definiste en sat.hpp;
                // si prefieres reutilizar ResultElem como en BKT,
                // cambia también la firma de SAT::knnSearch.
                vector<SATResultElem> out;

                sat.clear_counters();
                sat.knnSearch(q, k, out);

                totalD += sat.get_compDist();
                totalT += sat.get_queryTime();
            }

            double avgD = double(totalD) / queries.size();
            double avgT = double(totalT) / queries.size();

            J << ",\n";
            J << fixed << setprecision(6);
            J << "{"
              << "\"index\":\"SAT\","
              << "\"dataset\":\"" << dataset << "\","
              << "\"category\":\"FQ\","
              << "\"num_pivots\":" << numPivots << ","
              << "\"num_centers_path\":" << realHeight << ","
              << "\"arity\":null,"                 // idem comentario anterior
              << "\"query_type\":\"MkNN\","
              << "\"selectivity\":null,"
              << "\"radius\":null,"
              << "\"k\":" << k << ","
              << "\"compdists\":" << avgD << ","
              << "\"time_ms\":" << (avgT / 1000.0) << ","
              << "\"n_queries\":" << queries.size() << ","
              << "\"run_id\":1"
              << "}";
        }

        J << "\n]\n";
        J.close();

        cerr << "[DONE] Archivo generado: " << jsonOut << "\n";
    }

    return 0;
}
