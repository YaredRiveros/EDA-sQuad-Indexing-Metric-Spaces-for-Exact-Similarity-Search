#include <bits/stdc++.h>
#include <sys/stat.h>
#include "bst.hpp"  
#include "../objectdb.hpp"
using namespace std;

long long numDistances = 0;

int main(int argc, char **argv) {
    if (argc < 6) {
        cerr << "Usage: " << argv[0]
             << " <database name> <size> <index name> <bucket size> <mode> [param]\n"
             << "  mode = build | range | knn\n"
             << "  param = radius (for range) or k (for knn)\n";
        return 1;
    }

    int idx = 1;
    string dbName = argv[idx]; idx++;
    int nObjects = stoi(argv[idx]); idx++;
    string indexFile = argv[idx]; idx++;
    int bucketSize = stoi(argv[idx]); idx++;
    int maxHeight = stoi(argv[idx]); idx++;
    string mode = argv[idx]; idx++;

    // DB construction
    unique_ptr<ObjectDB> db;
    if (dbName.find("string") == string::npos)
        db = make_unique<VectorDB>(dbName, 2);  // vectors
    else
        db = make_unique<StringDB>(dbName);     // strings

    if (nObjects > db->size()) nObjects = db->size();
    cerr << "[BST] Loaded " << nObjects << " objects from " << dbName << "\n";

    
    cerr << "[BST] Building index (bucketSize=" << bucketSize << ")...\n";
    BST index(db.get(), bucketSize, maxHeight);
    cerr << "[BST] Index built.\n";

    /*
    // Save data
    ofstream out(indexFile, ios::binary);
    if (!out.is_open()) {
        cerr << "Error: cannot open " << indexFile << " for writing.\n";
        return 1;
    }
    out.write((char*)&bucketSize, sizeof(int));
    out.write((char*)&nObjects, sizeof(int));
    out.close();

    struct stat sdata;
    stat(indexFile.c_str(), &sdata);
    cerr << "[BST] Index metadata saved (" << sdata.st_size << " bytes)\n";
    */


    // search mode : 'range' |  'knn'
    cout << fixed << setprecision(2);
    if (mode == "range") {
        if (argc < idx+1) {
            cerr << "Error: need <radius>\n";
            return 1;
        }
        double radius = stod(argv[idx]);
        int qid = 0; // objeto de consulta (puedes cambiarlo)
        vector<int> results;
        index.rangeSearch(qid, radius, results);
        cout << "=== Range Search (r=" << radius << ") ===\n";
        for (int id : results) {
            cout << "ID " << id << " -> ";
            db->print(id);
        }
        cout << "Found " << results.size() << " objects.\n";
    }
    else if (mode == "knn") {
        if (argc < idx+1) {
            cerr << "Error: need <k>\n";
            return 1;
        }
        int k = stoi(argv[idx]);
        int qid = 0;
        vector<ResultElem> knn;
        index.knnSearch(qid, k, knn);
        cout << "=== kNN Search (k=" << k << ") ===\n";
        for (auto &e : knn) {
            cout << "id=" << e.id << " dist=" << e.dist << " -> ";
            db->print(e.id);
        }
        cout << "Returned " << knn.size() << " neighbors.\n";
    }
    else if (mode == "build") {
        cerr << "[BST] Build-only mode complete.\n";
    }
    else {
        cerr << "Unknown mode: " << mode << "\n";
        return 1;
    }

    return 0;
}
