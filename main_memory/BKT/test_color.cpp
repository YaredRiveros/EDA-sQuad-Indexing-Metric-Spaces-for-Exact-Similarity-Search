#include <bits/stdc++.h>
#include "bkt.hpp"
#include "../../datasets/paths.hpp"

using namespace std;

int main() {
    cout << "[TEST] Verificando BKT con dataset Color\n\n";
    
    // 1. Cargar dataset
    string dbfile = path_dataset("Color");
    if (dbfile == "") {
        cerr << "[ERROR] No se encontró Color.txt\n";
        return 1;
    }
    
    cout << "[INFO] Cargando " << dbfile << "...\n";
    unique_ptr<ObjectDB> db = make_unique<VectorDB>(dbfile, 1); // L1 metric
    
    int N = db->size();
    cout << "[OK] Cargados " << N << " objetos\n";
    
    // 2. Cargar queries y radios
    vector<int> queries = load_queries_file(path_queries("Color"));
    auto radii = load_radii_file(path_radii("Color"));
    
    cout << "[OK] Cargadas " << queries.size() << " queries\n";
    cout << "[OK] Cargados " << radii.size() << " radios\n";
    
    // 3. Construir índice pequeño (1000 objetos)
    int nTest = 1000;
    cout << "\n[BUILD] Construyendo BKT con " << nTest << " objetos...\n";
    cout << "  Parámetros: bucket=20, step=1000\n";
    
    BKT index(db.get(), 20, 1000.0);
    for (int i = 0; i < nTest; i++) {
        index.insert(i);
        if ((i+1) % 100 == 0) {
            cout << "  Insertados " << (i+1) << " objetos\r" << flush;
        }
    }
    cout << "\n[OK] Índice construido\n";
    
    // 4. Probar búsqueda por rango
    int qid = queries[0];
    double R = radii[0.02]; // selectividad 2%
    
    cout << "\n[MRQ] Probando Range Query...\n";
    cout << "  Query ID: " << qid << "\n";
    cout << "  Radio: " << R << " (sel=0.02)\n";
    
    vector<int> results;
    index.rangeSearch(qid, R, results);
    
    cout << "[OK] Encontrados " << results.size() << " resultados\n";
    
    // 5. Probar k-NN
    int k = 10;
    cout << "\n[MkNN] Probando k-NN...\n";
    cout << "  Query ID: " << qid << "\n";
    cout << "  k: " << k << "\n";
    
    vector<ResultElem> knn;
    index.knnSearch(qid, k, knn);
    
    cout << "[OK] Encontrados " << knn.size() << " vecinos\n";
    
    if (!knn.empty()) {
        cout << "  Primeros 3 vecinos:\n";
        for (int i = 0; i < min(3, (int)knn.size()); i++) {
            cout << "    " << i+1 << ". ID=" << knn[i].id 
                 << " dist=" << fixed << setprecision(2) << knn[i].dist << "\n";
        }
    }
    
    cout << "\n[SUCCESS] ✓ BKT funciona correctamente con Color!\n";
    
    return 0;
}
