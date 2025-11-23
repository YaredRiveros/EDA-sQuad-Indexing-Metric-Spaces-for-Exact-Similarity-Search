#include <bits/stdc++.h>
#include "ept.hpp"
#include "../../datasets/paths.hpp"

using namespace std;
using namespace chrono;

// ============================================================
// CONFIGURACIÓN IGUAL CHEN
// ============================================================

static const vector<double> SELECTIVITIES = {0.02, 0.04, 0.08, 0.16, 0.32};
static const vector<int>    K_VALUES      = {5, 10, 20, 50, 100};
static const vector<string> DATASETS      = {"LA", "Words", "Color"};

// Parámetros EPT* (l pivots por objeto, cp_scale candidatos PSA)
// Parámetros EPT* (l pivots por objeto, cp_scale candidatos PSA)
struct EPT_Params {
    int l;
    int cp_scale;
};

// Experimentos en memoria principal al estilo Chen:
// l ∈ {3, 5, 10, 15, 20}, cp_scale = 40 (fijo)
static const vector<EPT_Params> PARAMS_LA = {
    { 3, 40 },
    { 5, 40 },
    {10, 40 },
    {15, 40 },
    {20, 40 }
};

static const vector<EPT_Params> PARAMS_WORDS = {
    { 3, 40 },
    { 5, 40 },
    {10, 40 },
    {15, 40 },
    {20, 40 }
};

static const vector<EPT_Params> PARAMS_COLOR = {
    { 3, 40 },
    { 5, 40 },
    {10, 40 },
    {15, 40 },
    {20, 40 }
};

static const vector<EPT_Params> PARAMS_SYNTH = {
    { 3, 40 },
    { 5, 40 },
    {10, 40 },
    {15, 40 },
    {20, 40 }
};

// ============================================================
// DistanceAdapter: wrapper con contador COMPARTIDO
//  • Cuenta distance() tanto en build como en consultas
//  • Podemos separar build vs query reseteando el contador
// ============================================================

struct DistanceAdapter {
    ObjectDB* db;
    shared_ptr<long long> counter;

    DistanceAdapter(ObjectDB* dbptr)
        : db(dbptr), counter(make_shared<long long>(0)) {}

    // copia: comparte el mismo contador
    DistanceAdapter(const DistanceAdapter& other)
        : db(other.db), counter(other.counter) {}

    double operator()(int a, int b) const {
        ++(*counter);
        return db->distance(a, b);
    }

    void reset() const { *counter = 0; }
    long long get() const { return *counter; }
};


// ============================================================
// MAIN
// ============================================================

int main() {
    srand(12345);
    filesystem::create_directories("results");

    for (const string& dataset : DATASETS)
    {
        // -------------------------------------------------------
        // 1. Localizar dataset
        // -------------------------------------------------------
        string dbfile = path_dataset(dataset);
        if (dbfile == "") {
            cerr << "[WARN] Dataset no encontrado: " << dataset << "\n";
            continue;
        }

        // -------------------------------------------------------
        // 2. Cargar dataset
        // -------------------------------------------------------
        unique_ptr<ObjectDB> db;

        if (dataset == "LA")             db = make_unique<VectorDB>(dbfile, 2);
        else if (dataset == "Color")     db = make_unique<VectorDB>(dbfile, 1);
        else if (dataset == "Synthetic") db = make_unique<VectorDB>(dbfile, 999999);
        else if (dataset == "Words")     db = make_unique<StringDB>(dbfile);
        else continue;

        int N = db->size();

        cerr << "\n==============================================\n";
        cerr << "[INFO] Dataset: " << dataset << "   N=" << N << "\n";
        cerr << "==============================================\n";

        // -------------------------------------------------------
        // 3. Queries y radios (como en Chen)
        // -------------------------------------------------------
        vector<int> queries = load_queries_file(path_queries(dataset));
        auto radii          = load_radii_file(path_radii(dataset));

        if (queries.empty()) {
            cerr << "[WARN] No hay queries. Saltando dataset.\n";
            continue;
        }

        cerr << "[INFO] Queries: " << queries.size() << "\n";

        // -------------------------------------------------------
        // 4. Parámetros por dataset
        // -------------------------------------------------------
        vector<EPT_Params> params;
        if (dataset == "LA")         params = PARAMS_LA;
        else if (dataset == "Words") params = PARAMS_WORDS;
        else if (dataset == "Color") params = PARAMS_COLOR;
        else if (dataset == "Synthetic") params = PARAMS_SYNTH;

        // -------------------------------------------------------
        // 5. JSON output (mismo formato que FQT)
        // -------------------------------------------------------
        string jsonOut = "results/results_EPT_" + dataset + ".json";
        ofstream J(jsonOut);
        if (!J.is_open()) {
            cerr << "[ERROR] No se pudo abrir JSON: " << jsonOut << "\n";
            return 1;
        }

        J << "[\n";
        bool first = true;

        // -------------------------------------------------------
        // 6. Loop de configuraciones (como en FQT)
        // -------------------------------------------------------
        for (size_t c = 0; c < params.size(); c++)
        {
            auto P = params[c];

            cerr << "[INFO] Config " << (c+1) << "/" << params.size()
                 << "  l=" << P.l
                 << "  cp_scale=" << P.cp_scale << "\n";

            // Adaptador de distancia con contador compartido
            DistanceAdapter dist(db.get());

            // Vector de IDs [0..N-1]
            vector<int> ids(N);
            iota(ids.begin(), ids.end(), 0);

            // ---------------------------------------------------
            // BUILD — como Chen: medimos tiempo y compdists SOLO build
            // ---------------------------------------------------
            dist.reset();
            auto t1 = high_resolution_clock::now();
            EPTStar<int, DistanceAdapter> index(ids, dist, P.l, P.cp_scale);
            auto t2 = high_resolution_clock::now();

            double build_ms    = duration_cast<milliseconds>(t2 - t1).count();
            long long build_cd = dist.get();   // solo build
            cerr << "  Build: " << build_ms << " ms"
                 << "  compdists_build=" << build_cd << "\n";

            // A partir de aquí, las consultas tendrán su propio conteo
            // Reseteamos el contador ANTES de MkNN/MRQ
            // (esto separa build de consultas como hace Chen)
            // =======================================================

            // =======================================================
            // MkNN — exactamente al estilo Chen, promedio por query
            // =======================================================
            cerr << "  MkNN...\n";

            for (int k : K_VALUES)
            {
                dist.reset();  // solo compdists de este conjunto de consultas
                auto start = high_resolution_clock::now();

                double sumK = 0.0;
                for (int q : queries) {
                    double kth = index.knnQuery(q, k);
                    sumK += kth;
                }

                auto end = high_resolution_clock::now();
                double t = duration_cast<milliseconds>(end - start).count();

                double avgTime  = t / queries.size();
                double avgDists = static_cast<double>(dist.get()) / queries.size();
                double avgKth   = sumK / queries.size();

                if (!first) J << ",\n";
                first = false;

                J << "  {\n";
                J << "    \"index\": \"EPT*\",\n";
                J << "    \"dataset\": \"" << dataset << "\",\n";
                J << "    \"category\": \"" 
                  << (dataset == "Words" ? "strings" : "vectors") << "\",\n";
                J << "    \"num_pivots\": " << P.l << ",\n";
                J << "    \"num_centers_path\": null,\n";
                J << "    \"arity\": null,\n";
                J << "    \"bucket_size\": null,\n";
                J << "    \"query_type\": \"MkNN\",\n";
                J << "    \"selectivity\": null,\n";
                J << "    \"radius\": " << avgKth << ",\n";
                J << "    \"k\": " << k << ",\n";
                J << "    \"compdists\": " << avgDists << ",\n";
                J << "    \"time_ms\": " << avgTime << ",\n";
                J << "    \"n_queries\": " << queries.size() << ",\n";
                J << "    \"run_id\": \"EPT_" << dataset
                  << "_l" << P.l
                  << "_c" << P.cp_scale
                  << "_k" << k << "\"\n";
                J << "  }";
            }

            // =======================================================
            // MRQ — mismo formato y conteo que FQT / Chen
            // =======================================================
            cerr << "  MRQ...\n";

            for (auto& R : radii)
            {
                double sel    = R.first;
                double radius = R.second;

                dist.reset();  // solo este conjunto (este r / selectividad)
                auto start = high_resolution_clock::now();

                int total = 0;
                for (int q : queries) {
                    total += index.rangeQuery(q, radius);
                }

                auto end = high_resolution_clock::now();
                double t = duration_cast<milliseconds>(end - start).count();

                double avgTime  = t / queries.size();
                double avgDists = static_cast<double>(dist.get()) / queries.size();

                if (!first) J << ",\n";
                first = false;

                J << "  {\n";
                J << "    \"index\": \"EPT*\",\n";
                J << "    \"dataset\": \"" << dataset << "\",\n";
                J << "    \"category\": \"" 
                  << (dataset == "Words" ? "strings" : "vectors") << "\",\n";
                J << "    \"num_pivots\": " << P.l << ",\n";
                J << "    \"num_centers_path\": null,\n";
                J << "    \"arity\": null,\n";
                J << "    \"bucket_size\": null,\n";
                J << "    \"query_type\": \"MRQ\",\n";
                J << "    \"selectivity\": " << sel << ",\n";
                J << "    \"radius\": " << radius << ",\n";
                J << "    \"k\": null,\n";
                J << "    \"compdists\": " << avgDists << ",\n";
                J << "    \"time_ms\": " << avgTime << ",\n";
                J << "    \"n_queries\": " << queries.size() << ",\n";
                J << "    \"run_id\": \"EPT_" << dataset
                  << "_l" << P.l
                  << "_c" << P.cp_scale
                  << "_sel" << sel << "\"\n";
                J << "  }";
            }
        }

        J << "\n]\n";
        J.close();
        cerr << "[INFO] Guardado JSON: " << jsonOut << "\n";
    }

    cerr << "\n[OK] Benchmark EPT* completado.\n";
    return 0;
}
