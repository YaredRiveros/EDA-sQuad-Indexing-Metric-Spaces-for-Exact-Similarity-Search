#include <bits/stdc++.h>
#include <sys/stat.h>
#include "cpt.hpp"
#include "../../datasets/paths.hpp"

using namespace std;

int main(int argc, char **argv) {
    if (argc < 6) {
        cerr << "Usage: " << argv[0]
             << " <database file> <size> <mtree base> <num_pivots> <mode> [radius|k]\n";
        cerr << "  mode: build | range | knn\n";
        cerr << "  Example (range): " << argv[0]
             << " ../data/LA.txt 100000 LA 10 range 0.01\n";
        cerr << "  Example (kNN):   " << argv[0]
             << " ../data/LA.txt 100000 LA 10 knn 20\n";
        return 1;
    }

    string dbName    = argv[1];
    int    nObjects  = stoi(argv[2]);
    string mtBase    = argv[3];      // e.g., "LA" -> LA.mtree_index
    int    nPivots   = stoi(argv[4]);
    string mode      = argv[5];

    cerr << "[INFO] Database file: " << dbName   << "\n";
    cerr << "[INFO] Size (max N):  " << nObjects << "\n";
    cerr << "[INFO] M-tree base:   " << mtBase   << "\n";
    cerr << "[INFO] #pivots (l):   " << nPivots  << "\n";
    cerr << "[INFO] Mode:          " << mode     << "\n";

    // === Determinar tipo de base (igual que en otros mains) ===
    unique_ptr<ObjectDB> db;

    // Heurística simple: si el nombre contiene "Words" o "string", usamos StringDB;
    // si no, VectorDB con L2 (dimension=2 por defecto).
    if (dbName.find("Words") != string::npos || dbName.find("string") != string::npos) {
        db = make_unique<StringDB>(dbName);
    } else {
        // Ajusta la dimensión si hace falta; aquí usamos 2 como ejemplo
        db = make_unique<VectorDB>(dbName, 2);
    }

    if (nObjects > db->size())
        nObjects = db->size();

    cerr << "[INFO] Loaded " << db->size() << " objects from DB.\n";

    // === Construir CPT ===
    CPT cpt(db.get(), nPivots);

    // Si quieres usar pivots HFI reales (desde prepared_experiment):
    //   - debes saber el nombre lógico del dataset ("LA", "Color", etc.)
    //   - aquí, a modo de ejemplo, no lo inferimos automáticamente.
    //
    // string dataset = "LA"; // por ejemplo
    // vector<int> pivots = load_pivots_file(path_pivots(dataset, nPivots));
    // if (!pivots.empty()) cpt.overridePivots(pivots);

    cpt.buildFromMTree(mtBase);

    cout << fixed << setprecision(6);

    if (mode == "range") {
        if (argc < 7) {
            cerr << "Error: need <radius> for range mode\n";
            return 1;
        }
        double radius = stod(argv[6]);
        int qid = 0; // query object id (ajusta si quieres otro)

        vector<int> results;
        cpt.clear_counters();
        cpt.rangeSearch(qid, radius, results);

        cout << "=== CPT Range Search (r=" << radius << ") ===\n";
        cout << "[stats] compDist=" << cpt.get_compDist()
             << "  time_us="        << cpt.get_queryTime()
             << "  pageReads="      << cpt.get_pageReads() << "\n";

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

        vector<CPTResultElem> knn;
        cpt.clear_counters();
        cpt.knnSearch(qid, k, knn);

        cout << "=== CPT kNN Search (k=" << k << ") ===\n";
        cout << "[stats] compDist=" << cpt.get_compDist()
             << "  time_us="        << cpt.get_queryTime()
             << "  pageReads="      << cpt.get_pageReads() << "\n";

        cout << "Neighbors:\n";
        for (auto &e : knn) {
            cout << "id=" << e.id << " dist=" << e.dist << " -> ";
            db->print(e.id);
        }
        cout << "Returned " << knn.size() << " neighbors.\n";
    }
    else if (mode == "build") {
        cerr << "[CPT] Build-only mode complete (pivot table + pages).\n";
    }
    else {
        cerr << "Unknown mode: " << mode << "\n";
        return 1;
    }

    return 0;
}
