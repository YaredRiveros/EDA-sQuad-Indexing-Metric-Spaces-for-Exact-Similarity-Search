#include <bits/stdc++.h>
#include <sys/stat.h>
#include "laesa.hpp"  
#include "../objectdb.hpp"
using namespace std;

long long numDistances = 0;

int main(int argc, char **argv) {
    if (argc < 6) {
        cerr << "Usage: " << argv[0]
             << " <database name> <size> <index name> <num_pivots> <mode> [param]\n"
             << "  mode = build | range | knn\n"
             << "  param = radius (for range) or k (for knn)\n"
             << "\nRecommendations for num_pivots:\n"
             << "  - For small datasets (< 10K): 10-50 pivots\n"
             << "  - For medium datasets (10K-100K): 50-200 pivots\n"
             << "  - For large datasets: sqrt(n) is a good starting point\n"
             << "  - Trade-off: more pivots = better filtering but more memory\n";
        return 1;
    }

    string dbName = argv[1];
    int nObjects = stoi(argv[2]);
    string indexFile = argv[3];
    int nPivots = stoi(argv[4]);
    string mode = argv[5];

    // DB construction
    unique_ptr<ObjectDB> db;
    if (dbName.find("string") == string::npos)
        db = make_unique<VectorDB>(dbName, 2);  // vectors
    else
        db = make_unique<StringDB>(dbName);     // strings

    if (nObjects > db->size()) nObjects = db->size();
    cerr << "[LAESA] Loaded " << nObjects << " objects from " << dbName << "\n";

    
    cerr << "[LAESA] Building index (nPivots=" << nPivots << ")...\n";
    LAESA index(db.get(), nPivots);
    cerr << "[LAESA] Index built.\n";

    /*
    // Save index metadata (opcional)
    ofstream out(indexFile, ios::binary);
    if (!out.is_open()) {
        cerr << "Error: cannot open " << indexFile << " for writing.\n";
        return 1;
    }
    out.write((char*)&nPivots, sizeof(int));
    out.write((char*)&nObjects, sizeof(int));
    out.close();

    struct stat sdata;
    stat(indexFile.c_str(), &sdata);
    cerr << "[LAESA] Index metadata saved (" << sdata.st_size << " bytes)\n";
    */

    // search mode : 'range' |  'knn'
    cout << fixed << setprecision(2);
    if (mode == "range") {
        if (argc < 7) {
            cerr << "Error: need <radius>\n";
            return 1;
        }
        double radius = stod(argv[6]);
        int qid = 0; // objeto de consulta (puedes cambiarlo)
        vector<int> results;
        
        auto start = chrono::high_resolution_clock::now();
        index.rangeSearch(qid, radius, results);
        auto end = chrono::high_resolution_clock::now();
        auto elapsed = chrono::duration_cast<chrono::microseconds>(end - start);
        
        cout << "=== Range Search (r=" << radius << ") ===\n";
        cout << "Query object: ";
        db->print(qid);
        cout << "\nResults:\n";
        for (int id : results) {
            cout << "ID " << id << " -> ";
            db->print(id);
        }
        cout << "\nFound " << results.size() << " objects.\n";
        cout << "Time: " << elapsed.count() / 1000.0 << " ms\n";
    }
    else if (mode == "knn") {
        if (argc < 7) {
            cerr << "Error: need <k>\n";
            return 1;
        }
        int k = stoi(argv[6]);
        int qid = 0;
        vector<ResultElem> knn;
        
        auto start = chrono::high_resolution_clock::now();
        index.knnSearch(qid, k, knn);
        auto end = chrono::high_resolution_clock::now();
        auto elapsed = chrono::duration_cast<chrono::microseconds>(end - start);
        
        cout << "=== kNN Search (k=" << k << ") ===\n";
        cout << "Query object: ";
        db->print(qid);
        cout << "\nNearest neighbors:\n";
        for (auto &e : knn) {
            cout << "id=" << e.id << " dist=" << e.dist << " -> ";
            db->print(e.id);
        }
        cout << "\nReturned " << knn.size() << " neighbors.\n";
        cout << "Time: " << elapsed.count() / 1000.0 << " ms\n";
    }
    else if (mode == "build") {
        cerr << "[LAESA] Build-only mode complete.\n";
    }
    else {
        cerr << "Unknown mode: " << mode << "\n";
        return 1;
    }

    return 0;
}
