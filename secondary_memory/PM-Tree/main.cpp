#include <bits/stdc++.h>
#include <sys/stat.h>
#include "pm_tree.hpp"

using namespace std;

int main(int argc, char **argv) {
    if (argc < 6) {
        cerr << "Usage: " << argv[0]
             << " <database file> <size> <mtree base> <num_pivots> <mode> [radius|k]\n";
        cerr << "  mode: build | range | knn\n";
        cerr << "Example (range): " << argv[0]
             << " ../data/LA.txt 100000 LA 10 range 0.01\n";
        cerr << "Example (kNN):   " << argv[0]
             << " ../data/LA.txt 100000 LA 10 knn 20\n";
        return 1;
    }

    string dbName   = argv[1];
    int    nObjects = stoi(argv[2]);
    string mtBase   = argv[3];      // e.g., "LA" -> LA.mtree_index
    int    nPivots  = stoi(argv[4]);
    string mode     = argv[5];

    cerr << "[INFO] Database file: " << dbName   << "\n";
    cerr << "[INFO] Size (max N):  " << nObjects << "\n";
    cerr << "[INFO] M-tree base:   " << mtBase   << "\n";
    cerr << "[INFO] #pivots (l):   " << nPivots  << "\n";
    cerr << "[INFO] Mode:          " << mode     << "\n";

    // === Determinar tipo de base (igual que en otros mains) ===
    unique_ptr<ObjectDB> db;

    if (dbName.find("Words") != string::npos ||
        dbName.find("string") != string::npos) {
        db = make_unique<StringDB>(dbName);
    } else {
        // Ajusta la dimensión según corresponda; aquí usamos 2 como ejemplo
        db = make_unique<VectorDB>(dbName, 2);
    }

    if (nObjects > db->size())
        nObjects = db->size();

    cerr << "[INFO] Loaded " << db->size() << " objects from DB.\n";

    // === Construir PM-tree sobre el M-tree existente ===
    PMTree pmt(db.get(), nPivots);
    pmt.buildFromMTree(mtBase);

    // Para este main simple usamos los primeros nPivots objetos como pivots.
    // En tus experimentos reales usarás HFI pivots desde JSON.
    vector<int> pivots;
    for (int i = 0; i < nPivots && i < nObjects; ++i)
        pivots.push_back(i);
    pmt.overridePivots(pivots);

    cout << fixed << setprecision(6);

    if (mode == "range") {
        if (argc < 7) {
            cerr << "Error: need <radius> for range mode\n";
            return 1;
        }
        double radius = stod(argv[6]);
        int qid = 0; // objeto de consulta (puedes cambiarlo)

        vector<int> results;
        pmt.clear_counters();
        pmt.rangeSearch(qid, radius, results);

        cout << "=== PM-tree Range Search (r=" << radius << ") ===\n";
        cout << "[stats] compDist=" << pmt.get_compDist()
             << "  time_us="       << pmt.get_queryTime()
             << "  pageReads="     << pmt.get_pageReads() << "\n";

        cout << "Results (IDs):\n";
        for (int id : results) {
            cout << "ID " << id << " -> ";
            db->print(id);
        }
        cout << "Found " << results.size() << " objects.\n";
    }
    else if (mode == "knn") {
        if (argc < 7) {
            cerr << "Error: need <k> for knn mode\n";
            return 1;
        }
        int k = stoi(argv[6]);
        int qid = 0;

        vector<pair<double,int>> knn;
        pmt.clear_counters();
        pmt.knnSearch(qid, k, knn);

        cout << "=== PM-tree kNN Search (k=" << k << ") ===\n";
        cout << "[stats] compDist=" << pmt.get_compDist()
             << "  time_us="       << pmt.get_queryTime()
             << "  pageReads="     << pmt.get_pageReads() << "\n";

        cout << "Neighbors:\n";
        for (auto &e : knn) {
            cout << "id=" << e.second << " dist=" << e.first << " -> ";
            db->print(e.second);
        }
        cout << "Returned " << knn.size() << " neighbors.\n";
    }
    else if (mode == "build") {
        cerr << "[PMTree] Build-only mode complete (loaded tree + pivots).\n";
    }
    else {
        cerr << "Unknown mode: " << mode << "\n";
        return 1;
    }

    return 0;
}
