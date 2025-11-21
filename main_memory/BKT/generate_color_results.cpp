#include <bits/stdc++.h>
#include "bkt.hpp"
#include "../../datasets/paths.hpp"

using namespace std;

// ============================================================
// CONFIGURACIÓN
// ============================================================

static const vector<double> SELECTIVITIES = {0.02, 0.04, 0.08, 0.16, 0.32};
static const vector<int> K_VALUES = {5, 10, 20, 50, 100};

struct BKT_Params {
    int bucket;
    double stepMultiplier;
};

static const vector<BKT_Params> PARAMS_COLOR = {
    {50, 16},
    {30,  8},
    {20,  4},
    {10,  2},
    { 5,  1}
};

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
// MAIN
// ============================================================
int main()
{
    srand(12345);

    string dataset = "Color";
    
    // 1. Resolver dataset físico
    string dbfile = path_dataset(dataset);
    if (dbfile == "") {
        cerr << "[ERROR] Dataset no encontrado: " << dataset << "\n";
        return 1;
    }

    // 2. Cargar dataset con métrica L1
    cout << "[INFO] Cargando " << dataset << "...\n";
    unique_ptr<ObjectDB> db = make_unique<VectorDB>(dbfile, 1); // L1 metric

    int nObjects = db->size();
    cout << "[INFO] Dataset: " << dataset << "   N=" << nObjects << "\n";

    // 3. Cargar queries + radios
    vector<int> queries = load_queries_file(path_queries(dataset));
    auto radii = load_radii_file(path_radii(dataset));

    if (queries.empty()) {
        cerr << "[ERROR] No hay queries para " << dataset << "\n";
        return 1;
    }

    cout << "[INFO] Queries: " << queries.size() << "\n";
    cout << "[INFO] Radios: " << radii.size() << " selectividades\n";

    // 4. JSON Output
    filesystem::create_directories("results");
    string jsonOut = "results/results_BKT_Color.json";
    ofstream J(jsonOut);
    J << "[\n";
    bool firstOutput = true;

    // 5. Ejecutar experimentos
    for (auto P : PARAMS_COLOR)
    {
        int bucketSize = P.bucket;
        double step = P.stepMultiplier;

        cout << "\n------------------------------------------\n";
        cout << "[BUILD] BKT: bucket=" << bucketSize << "  step=" << step << "\n";
        cout << "------------------------------------------\n";

        // Construcción del BKT
        BKT bkt(db.get(), bucketSize, step);
        bkt.build();

        int realHeight = bkt.get_height();
        int numPivots  = bkt.get_num_pivots();

        cout << "[INFO] Altura = " << realHeight << "   Pivots = " << numPivots << "\n";

        // MRQ (range queries)
        for (double sel : SELECTIVITIES)
        {
            if (radii.find(sel) == radii.end())
                continue;

            double R = radii[sel];
            long long totalD = 0, totalT = 0;

            cout << "  [MRQ] sel=" << sel << " R=" << R << " ... " << flush;

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

            cout << "OK (avgD=" << avgD << ")\n";

            if (!firstOutput) J << ",\n";
            firstOutput = false;

            J << fixed << setprecision(6);
            J << "{"
              << "\"index\":\"BKT\","
              << "\"dataset\":\"" << dataset << "\","
              << "\"category\":\"FQ\","
              << "\"num_pivots\":" << numPivots << ","
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

        // MkNN
        for (int k : K_VALUES)
        {
            long long totalD = 0, totalT = 0;

            cout << "  [MkNN] k=" << k << " ... " << flush;

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

            cout << "OK (avgD=" << avgD << ")\n";

            if (!firstOutput) J << ",\n";
            firstOutput = false;

            J << fixed << setprecision(6);
            J << "{"
              << "\"index\":\"BKT\","
              << "\"dataset\":\"" << dataset << "\","
              << "\"category\":\"FQ\","
              << "\"num_pivots\":" << numPivots << ","
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

    cout << "\n[DONE] Archivo generado: " << jsonOut << "\n";
    
    return 0;
}
