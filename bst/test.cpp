#include <bits/stdc++.h>
#include <sys/stat.h>
#include "bst.hpp"
#include "../objectdb.hpp"
using namespace std;
using namespace std::chrono;

// ================================================================
// 🔹 Estima los radios que cubren las selectividades (%)
// ================================================================
vector<double> estimateRadius(ObjectDB *db, int nObjects, const vector<double> &selectivities) {
    vector<double> radii(selectivities.size(), 0.0);
    int samples = min(10, nObjects);
    vector<int> queryIds(samples);
    iota(queryIds.begin(), queryIds.end(), 0);
    std::shuffle(queryIds.begin(), queryIds.end(), std::mt19937(std::random_device{}()));

    for (int s = 0; s < samples; s++) {
        int q = queryIds[s];
        vector<double> dists;
        dists.reserve(nObjects - 1);
        for (int i = 0; i < nObjects; i++) {
            if (i == q) continue;
            dists.push_back(db->distance(q, i));
        }
        sort(dists.begin(), dists.end());

        for (size_t j = 0; j < selectivities.size(); j++) {
            int idx = int(selectivities[j] * dists.size());
            if (idx >= (int)dists.size()) idx = dists.size() - 1;
            radii[j] += dists[idx];
        }
    }

    for (double &r : radii) r /= samples;
    return radii;
}

// ================================================================
// 🔹 Programa principal con tiempo + comparaciones + encabezado CSV
// ================================================================
int main(int argc, char **argv) {
    if (argc < 6) {
        cerr << "Usage: " << argv[0]
             << " <dataset> <nObjects> <indexName> <bucketSize> <maxHeight>\n";
        return 1;
    }

    int idx = 1;
    string dbName = argv[idx++];
    int nObjects = stoi(argv[idx++]);
    string indexFile = argv[idx++];
    int bucketSize = stoi(argv[idx++]);
    int maxHeight = stoi(argv[idx++]);

    cerr << "[BST] Loading " << nObjects << " objects from " << dbName << "...\n";

    // === Crear base de datos ===
    unique_ptr<ObjectDB> db;
    if (dbName.find("string") == string::npos)
        db = make_unique<VectorDB>(dbName, 2);
    else
        db = make_unique<StringDB>(dbName);

    if (nObjects > db->size()) nObjects = db->size();
    cerr << "[BST] Loaded " << nObjects << " objects (" << db->size() << " total)\n";

    // === Construcción del índice ===
    cerr << "[BST] Building index (bucketSize=" << bucketSize << ", maxHeight=" << maxHeight << ")...\n";
    BST index(db.get(), nObjects, bucketSize, maxHeight);
    cerr << "[BST] Index built. Height=" << index.get_height() << "\n";

    // === Archivo de métricas ===
    ofstream fout("metrics.txt", ios::app);
    fout << fixed << setprecision(6);

    if (!fout.is_open()) {
        cerr << "Error: cannot open metrics.txt for writing.\n";
        return 1;
    }

    // Si el archivo está vacío, añadir encabezado
    struct stat st;
    if (stat("metrics.txt", &st) != 0 || st.st_size == 0) {
        fout << "#index, height, dataset, queryType, param, avgTime(us), avgDists\n";
    }

    // === Configuraciones ===
    vector<double> selectivities = {0.02, 0.04, 0.08, 0.16, 0.32};
    vector<int> knnValues = {3, 5, 10, 15, 20};
    const int ITER = 100;
    int qid = 0;

    
    cerr << "[BST] Estimating radii for range selectivities...\n";
    auto radii = estimateRadius(db.get(), nObjects, selectivities);

    cerr << "\n=== Estimated Radii (dataset-based) ===\n";
    for (size_t i = 0; i < selectivities.size(); i++)
        cerr << "Selectivity " << selectivities[i] * 100 << "% -> radius ≈ " << radii[i] << "\n";
    cerr << "======================================\n";

    // ================================================================
    // 🔹 RANGE SEARCH EXPERIMENTS
    // ================================================================
    for (size_t i = 0; i < selectivities.size(); i++) {
        double radius = radii[i];
        double totalTime = 0;
        double totalDists = 0;

        for (int t = 0; t < ITER; t++) {
            vector<int> results;
            index.rangeSearch(qid, radius, results);
            totalTime += index.get_queryTime();
            totalDists += index.get_compDist();
        }

        double avgTime = totalTime / ITER;
        double avgDists = totalDists / ITER;
        fout << "BST, " << index.get_height() << ", " << dbName
             << ", range, " << (selectivities[i] * 100) << "%, "
             << avgTime << ", " << avgDists << "\n";
        cout << "[RANGE] " << selectivities[i] * 100
             << "% -> avg=" << avgTime << " µs, dists=" << avgDists << "\n";
    }

    // ================================================================
    // 🔹 KNN SEARCH EXPERIMENTS
    // ================================================================
    for (int k : knnValues) {
        double totalTime = 0;
        double totalDists = 0;

        for (int t = 0; t < ITER; t++) {
            vector<ResultElem> knn;
            index.knnSearch(qid, k, knn);
            totalTime += index.get_queryTime();
            totalDists += index.get_compDist();
        }

        double avgTime = totalTime / ITER;
        double avgDists = totalDists / ITER;
        fout << "BST, " << index.get_height() << ", " << dbName
             << ", knn, " << k << ", " << avgTime << ", " << avgDists << "\n";
        cout << "[KNN] k=" << k << " -> avg=" << avgTime << " µs, dists=" << avgDists << "\n";
    }

    fout.close();
    cerr << "[BST] All results saved in metrics.txt ✅\n";
    return 0;
}
