#include <bits/stdc++.h>
#include "bkt.hpp"
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

// Conjuntos de parámetros BKT que vamos a probar
// (equivalentes a {3,5,10,15,20} alturas del paper)
struct BKT_Params {
    int bucket;
    double stepMultiplier;
};

static const vector<int> L_VALUES = {3, 5, 10, 15, 20};


static const vector<BKT_Params> PARAMS_LA = {
    {50, 16},
    {30,  8},
    {20,  4},
    {10,  2},
    { 5,  1}
};

static const vector<BKT_Params> PARAMS_WORDS = {
    {50, 4},
    {30, 3},
    {20, 2},
    {10, 2},
    { 5, 1}
};


static const vector<BKT_Params> PARAMS_SYNTH = {
    {50, 0.50},
    {30, 0.40},
    {20, 0.30},
    {10, 0.25},
    { 5, 0.20}
};

static const vector<BKT_Params> PARAMS_COLOR = PARAMS_LA;



// ============================================================
// Función auxiliar: estimar distancia promedio del dataset
// ============================================================
double estimate_avg_dist(const ObjectDB* db, int samples = 1000)
{
    double avg = 0;
    int n = db->size();
    for (int i = 0; i < samples; i++) {
        int a = rand() % n;
        int b = rand() % n;
        avg += db->distance(a, b);
    }
    return avg / samples;
}


// ============================================================
// MAIN — EXPERIMENTACIÓN COMPLETA
// ============================================================
int main(int argc, char** argv)
{
    srand(12345);

    // 1) Decidir qué datasets correr
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

    // CREAR SUBCARPETA 'RESULTS'
    std::filesystem::create_directories("results");
    for (const string& dataset : datasets)
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
        bool useAvgDist = false;
        if (dataset == "LA")          db = make_unique<VectorDB>(dbfile, 2);
        else if (dataset == "Color")  db = make_unique<VectorDB>(dbfile, 1);
        else if (dataset == "Synthetic") 
        {
            db = make_unique<VectorDB>(dbfile, 999999);
            useAvgDist = true;
        }
        else if (dataset == "Words")  db = make_unique<StringDB>(dbfile);
        else continue;

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
        string jsonOut = "results/results_BKT_" + dataset + ".json";
        ofstream J(jsonOut);
        J << "[\n";
        bool firstOutput = true;

        // ============================================================
        // 5. Parámetros del BKT
        // ============================================================

        const vector<BKT_Params>* paramSet;

        if (dataset == "Words")
            paramSet = &PARAMS_WORDS;
        else if (dataset == "Synthetic")
            paramSet = &PARAMS_SYNTH;
        else if (dataset == "Color")
            paramSet = &PARAMS_COLOR;
        else
            paramSet = &PARAMS_LA;

       for (int paramIndex = 0; paramIndex < paramSet->size(); paramIndex++)
{
            auto P = (*paramSet)[paramIndex];

            // este es L = {3,5,10,15,20}
            int l_value = L_VALUES[paramIndex];

            int bucketSize = P.bucket;
            double avgDist = estimate_avg_dist(db.get());
            double step = useAvgDist ? (avgDist * P.stepMultiplier)
                         : P.stepMultiplier;

            cerr << "\n------------------------------------------\n";
            cerr << "[INFO] Construyendo BKT: bucket=" << bucketSize
                 << "  step=" << step << "\n";
            cerr << "------------------------------------------\n";

            // Construcción del BKT
            BKT bkt(db.get(), bucketSize, step);
            bkt.build();

            int realHeight = bkt.get_height();
            int numPivots  = bkt.get_num_pivots();

            cerr << "[INFO] Altura real = " << realHeight
                 << "   #Pivots=" << numPivots << "\n";

            // ========================================================
            //  MRQ (range queries)
            // ========================================================
            for (double sel : SELECTIVITIES)
            {
                if (radii.find(sel) == radii.end())
                    continue;

                double R = radii[sel];
                long long totalD = 0, totalT = 0;

                for (int q : queries)
                {
                    vector<int> out;
                    bkt.clear_counters();
                    bkt.rangeSearch(q, R, out);

                    totalD += bkt.get_compDist();
                    totalT += bkt.get_queryTime();
                }

                double avgD = double(totalD) / queries.size();
                double avgT = double(totalT) / queries.size();

                if (!firstOutput) J << ",\n";
                firstOutput = false;

                J << fixed << setprecision(6);
                J << "{"
                  << "\"index\":\"BKT\","
                  << "\"dataset\":\"" << dataset << "\","
                  << "\"category\":\"FQ\","
                  << "\"num_pivots\":" << l_value << ","
                  << "\"num_centers_path\":" << realHeight << ","
                  << "\"arity\":null,"
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

            // ========================================================
            //  MkNN
            // ========================================================
            for (int k : K_VALUES)
            {
                long long totalD = 0, totalT = 0;

                for (int q : queries)
                {
                    vector<ResultElem> out;
                    bkt.clear_counters();
                    bkt.knnSearch(q, k, out);

                    totalD += bkt.get_compDist();
                    totalT += bkt.get_queryTime();
                }

                double avgD = double(totalD) / queries.size();
                double avgT = double(totalT) / queries.size();

                J << ",\n";
                J << fixed << setprecision(6);
                J << "{"
                  << "\"index\":\"BKT\","
                  << "\"dataset\":\"" << dataset << "\","
                  << "\"category\":\"FQ\","
                  << "\"num_pivots\":" << l_value << ","
                  << "\"num_centers_path\":" << realHeight << ","
                  << "\"arity\":null,"
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
        }

        J << "\n]\n";
        J.close();

        cerr << "[DONE] Archivo generado: " << jsonOut << "\n";
    }

    return 0;
}