#include <bits/stdc++.h>
#include <sys/stat.h>
#include "mvpt.hpp"  
#include "../objectdb.hpp"
using namespace std;

long long numDistances = 0;

int main(int argc, char **argv) {
    if (argc < 7) {
        cerr << "Usage: " << argv[0]
             << " <database name> <size> <index name> <bucket_size> <arity> <mode> [param]\n"
             << "  mode = build | range | knn\n"
             << "  param = radius (for range) or k (for knn)\n"
             << "\nRecommendations:\n"
             << "  - bucket_size: 5-20 (smaller = deeper tree, more pivots)\n"
             << "  - arity: 2-8 (2=binary tree, higher=more compact but less pivots)\n"
             << "  - Trade-off: higher arity → fewer levels → fewer pivots for pruning\n";
        return 1;
    }

    string dbName = argv[1];
    int nObjects = stoi(argv[2]);
    string indexFile = argv[3];
    int bucketSize = stoi(argv[4]);

    // arity set to 5 by default
    // int arity = stoi(argv[5]);
    int arity = 5;
    string mode = argv[5];

    // DB construction
    unique_ptr<ObjectDB> db;
    if (dbName.find("string") == string::npos)
        db = make_unique<VectorDB>(dbName, 2);  // vectors
    else
        db = make_unique<StringDB>(dbName);     // strings

    if (nObjects > db->size()) nObjects = db->size();
    cerr << "[MVPT] Loaded " << nObjects << " objects from " << dbName << "\n";

    
    cerr << "[MVPT] Building index (bucketSize=" << bucketSize 
         << ", arity=" << arity << ")...\n";
    MVPT index(db.get(), bucketSize, arity);
    cerr << "[MVPT] Index built.\n";

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
        cerr << "[MVPT] Build-only mode complete.\n";
    }
    else {
        cerr << "Unknown mode: " << mode << "\n";
        return 1;
    }

    return 0;
}
