#include <bits/stdc++.h>
#include <sys/stat.h>
#include "sat.hpp"

using namespace std;

int main(int argc, char **argv) {
    if (argc < 5) {
        cerr << "Usage: " << argv[0]
             << " <database name> <size> <index name> <mode> [radius|k]\n";
        cerr << "  mode: build | range | knn\n";
        return 1;
    }

    string dbName    = argv[1];
    int    nObjects  = stoi(argv[2]);
    string indexFile = argv[3];
    string mode      = argv[4];

    cerr << "Indexing " << nObjects << " objects from " << dbName << "...\n";

    // === Determinar tipo de base (igual que en main de BKT) ===
    unique_ptr<ObjectDB> db;
    if (dbName.find("string") == string::npos)
        db = make_unique<VectorDB>(dbName, 2); // ejemplo: vectores L2
    else
        db = make_unique<StringDB>(dbName);

    if (nObjects > db->size()) nObjects = db->size();

    // === Construir índice SAT ===
    SAT index(db.get());
    index.build();

    cerr << "Finished building SAT index.\n";
    cerr << "[SAT] height = "     << index.get_height()
         << "  #centers = "       << index.get_num_pivots() << "\n";

    /*
    // Si quieres guardar algún metadato del índice:
    ofstream out(indexFile, ios::binary);
    if (!out.is_open()) {
        cerr << "Error: cannot open " << indexFile << " for writing.\n";
        return 1;
    }

    out.write((char*)&nObjects, sizeof(int));
    // Aquí podrías serializar tus nodos si más adelante implementas save/load
    out.close();

    struct stat sdata;
    stat(indexFile.c_str(), &sdata);
    cerr << "Saved index metadata (" << sdata.st_size << " bytes)\n";
    */

    // === Modo de búsqueda: 'range' | 'knn' | 'build' ===
    cout << fixed << setprecision(2);

    if (mode == "range") {
        if (argc < 6) {
            cerr << "Error: need <radius>\n";
            return 1;
        }
        double radius = stod(argv[5]);
        int qid = 0; // ID del objeto de consulta (ajusta si quieres otro)

        vector<int> results;
        index.clear_counters();
        index.rangeSearch(qid, radius, results);

        cout << "=== SAT Range Search (r=" << radius << ") ===\n";
        cout << "[stats] compDist=" << index.get_compDist()
             << "  time_us="        << index.get_queryTime() << "\n";

        for (int id : results) {
            cout << "ID " << id << " -> ";
            db->print(id);
        }
        cout << "Found " << results.size() << " objects.\n";
    }
    else if (mode == "knn") {
        if (argc < 6) {
            cerr << "Error: need <k>\n";
            return 1;
        }
        int k = stoi(argv[5]);
        int qid = 0;

        vector<SATResultElem> knn;
        index.clear_counters();
        index.knnSearch(qid, k, knn);

        cout << "=== SAT kNN Search (k=" << k << ") ===\n";
        cout << "[stats] compDist=" << index.get_compDist()
             << "  time_us="        << index.get_queryTime() << "\n";

        for (auto &e : knn) {
            cout << "id=" << e.id << " dist=" << e.dist << " -> ";
            db->print(e.id);
        }
        cout << "Returned " << knn.size() << " neighbors.\n";
    }
    else if (mode == "build") {
        cerr << "[SAT] Build-only mode complete.\n";
    }
    else {
        cerr << "Unknown mode: " << mode << "\n";
        return 1;
    }

    return 0;
}
