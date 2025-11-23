#include <bits/stdc++.h>
#include "spbtree.hpp"
#include "../../objectdb.hpp"
#include "../../datasets/paths.hpp"

using namespace std;

// ============================================================
// Experimentos según Chen 2022
// ============================================================

static const vector<double> SELECTIVITIES = {0.02, 0.04, 0.08, 0.16, 0.32};
static const vector<int>    K_VALUES      = {5, 10, 20, 50, 100};
static const vector<string> DATASETS      = {"LA", "Words", "Color", "Synthetic"};

static const int NUM_PIVOTS = 5;
static const size_t LEAF_CAPACITY = 128;
static const size_t FANOUT        = 64;

// ============================================================
// DETECTOR AUTOMÁTICO: 0-based vs 1-based
// ============================================================

vector<int> auto_fix_ids(const vector<int> &ids, int nObjects) {
    if (ids.empty()) return ids;

    bool hasZero = false;
    bool hasOutOfRange = false;

    for (int v : ids) {
        if (v == 0) hasZero = true;
        if (v >= nObjects) hasOutOfRange = true;
    }

    // Caso A: ya son 0-based
    if (hasZero && !hasOutOfRange) {
        return ids;
    }

    // Caso B: parecen 1-based → convertir
    if (!hasZero) {
        vector<int> fixed;
        fixed.reserve(ids.size());
        for (int v : ids) fixed.push_back(v - 1);
        return fixed;
    }

    // Caso C: mezcla rara → corregir dentro de rango
    vector<int> fixed;
    fixed.reserve(ids.size());
    for (int v : ids) {
        if (v > 0 && v <= nObjects) fixed.push_back(v - 1);
        else fixed.push_back(v); // ya está 0-based
    }
    return fixed;
}

// ============================================================
// Construir dataset para SPB-tree
// ============================================================

vector<DataObject> build_dataset_for_spb(size_t nObjects) {
    vector<DataObject> dataset;
    dataset.reserve(nObjects);

    for (size_t i = 0; i < nObjects; ++i) {
        DataObject o;
        o.id = static_cast<uint64_t>(i);   // <-- 0-BASED
        dataset.push_back(std::move(o));
    }
    return dataset;
}

// ============================================================
// MRQ experiment
// ============================================================

void run_mrq_experiment(const string &datasetName) {
    cerr << "\n==========================================\n";
    cerr << "[MRQ] Dataset: " << datasetName << "\n";

    string dsPath = path_dataset(datasetName);
    string qPath  = path_queries(datasetName);
    string rPath  = path_radii(datasetName);

    if (dsPath.empty()) {
        cerr << "[ERROR] No se encontró dataset para " << datasetName << "\n";
        return;
    }

    vector<int> queries = load_queries_file(qPath);
    auto radii          = load_radii_file(rPath);

    if (queries.empty() || radii.empty()) {
        cerr << "[WARN] Sin datos preparados (queries o radii faltan). Se omite MRQ.\n";
        return;
    }

    // Crear DB
    ObjectDB *db;
    if (datasetName == "Words") db = new StringDB(dsPath);
    else                        db = new VectorDB(dsPath);

    size_t nObjects = db->size();

    // Detección automática 0/1-based
    queries = auto_fix_ids(queries, nObjects);

    // Dataset para SPB
    vector<DataObject> dataset = build_dataset_for_spb(nObjects);

    string rafFile = "spb_" + datasetName + "_mrq.raf";

    SPBTree spb(
        rafFile,
        db,
        NUM_PIVOTS,
        LEAF_CAPACITY,
        FANOUT,
        datasetName,
        true
    );

    cerr << "[MRQ] Construyendo SPB-tree...\n";
    spb.build(dataset);
    spb.stats();

    // Archivo JSON salida
    string jsonOut = "SPB_MRQ_" + datasetName + ".json";
    ofstream J(jsonOut);
    J << "[\n";
    bool first = true;

    for (double sel : SELECTIVITIES) {
        if (!radii.count(sel)) continue;

        double R = radii[sel];
        cerr << "  [sel=" << sel << ", R=" << R << "]\n";

        long long totalComp = 0;
        long long totalTime = 0;
        long long totalPages = 0;

        for (int q : queries) {
            spb.clear_counters();

            auto t0 = chrono::high_resolution_clock::now();
            auto cands = spb.MRQ(q, R);
            auto t1 = chrono::high_resolution_clock::now();

            long long us =
                chrono::duration_cast<chrono::microseconds>(t1 - t0).count();

            totalComp  += spb.get_compDist();
            totalTime  += us;
            totalPages += spb.get_pageReads();
        }

        if (!first) J << ",\n";
        first = false;

        double avgComp  = double(totalComp)  / queries.size();
        double avgTime  = double(totalTime)  / queries.size() / 1000.0;
        double avgPages = double(totalPages) / queries.size();

        J << "  {"
            << "\"index\":\"SPB\","
            << "\"dataset\":\"" << datasetName << "\","
            << "\"category\":\"DM\","
            << "\"num_pivots\":" << NUM_PIVOTS << ","
            << "\"num_centers_path\":null,"
            << "\"arity\":null,"
            << "\"query_type\":\"MRQ\","
            << "\"selectivity\":" << sel << ","
            << "\"radius\":" << R << ","
            << "\"k\":null,"
            << "\"compdists\":" << avgComp << ","
            << "\"time_ms\":" << avgTime << ","
            << "\"pages\":" << avgPages << ","
            << "\"n_queries\":" << queries.size() << ","
            << "\"run_id\":1"
            << "}";
    }

    J << "\n]\n";
    cerr << "[MRQ] Archivo generado: " << jsonOut << "\n";

    delete db;
}

// ============================================================
// MkNN experiment
// ============================================================

void run_mknn_experiment(const string &datasetName) {
    cerr << "\n==========================================\n";
    cerr << "[MkNN] Dataset: " << datasetName << "\n";

    string dsPath = path_dataset(datasetName);
    string qPath  = path_queries(datasetName);

    if (dsPath.empty()) {
        cerr << "[ERROR] No dataset para " << datasetName << "\n";
        return;
    }

    vector<int> queries = load_queries_file(qPath);

    if (queries.empty()) {
        cerr << "[WARN] Sin queries, omitiendo MkNN.\n";
        return;
    }

    ObjectDB *db;
    if (datasetName == "Words") db = new StringDB(dsPath);
    else                        db = new VectorDB(dsPath);

    size_t nObjects = db->size();

    // Autodetección 0/1 based
    queries = auto_fix_ids(queries, nObjects);

    vector<DataObject> dataset = build_dataset_for_spb(nObjects);

    string rafFile = "spb_" + datasetName + "_mknn.raf";

    SPBTree spb(
        rafFile,
        db,
        NUM_PIVOTS,
        LEAF_CAPACITY,
        FANOUT,
        datasetName,
        true
    );

    cerr << "[MkNN] Construyendo SPB-tree...\n";
    spb.build(dataset);
    spb.stats();

    string jsonOut = "SPB_MkNN_" + datasetName + ".json";
    ofstream J(jsonOut);
    J << "[\n";
    bool first = true;

    for (int k : K_VALUES) {
        cerr << " k=" << k << "\n";

        long long totalComp = 0;
        long long totalTime = 0;
        long long totalPages = 0;

        for (int q : queries) {
            spb.clear_counters();

            auto t0 = chrono::high_resolution_clock::now();
            auto results = spb.MkNN(q, k);
            auto t1 = chrono::high_resolution_clock::now();

            long long us =
                chrono::duration_cast<chrono::microseconds>(t1 - t0).count();

            totalComp  += spb.get_compDist();
            totalTime  += us;
            totalPages += spb.get_pageReads();
        }

        if (!first) J << ",\n";
        first = false;

        double avgComp  = double(totalComp)  / queries.size();
        double avgTime  = double(totalTime)  / queries.size() / 1000.0;
        double avgPages = double(totalPages) / queries.size();

        J << "  {"
            << "\"index\":\"SPB\","
            << "\"dataset\":\"" << datasetName << "\","
            << "\"category\":\"DM\","
            << "\"num_pivots\":" << NUM_PIVOTS << ","
            << "\"num_centers_path\":null,"
            << "\"arity\":null,"
            << "\"query_type\":\"MkNN\","
            << "\"selectivity\":null,"
            << "\"radius\":null,"
            << "\"k\":" << k << ","
            << "\"compdists\":" << avgComp << ","
            << "\"time_ms\":" << avgTime << ","
            << "\"pages\":" << avgPages << ","
            << "\"n_queries\":" << queries.size() << ","
            << "\"run_id\":1"
            << "}";

    }

    J << "\n]\n";
    cerr << "[MkNN] Archivo generado: " << jsonOut << "\n";

    delete db;
}

// ============================================================
// main
// ============================================================

int main() {
    ios::sync_with_stdio(false);
    cin.tie(nullptr);

    cerr << "===== Experimentos SPB-tree (Chen 2022) =====\n";

    for (auto &ds : DATASETS) {
        run_mrq_experiment(ds);
        run_mknn_experiment(ds);
    }

    cerr << "===== FIN =====\n";
    return 0;
}
