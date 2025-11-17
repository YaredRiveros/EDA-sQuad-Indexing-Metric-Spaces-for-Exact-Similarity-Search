#include <bits/stdc++.h>
#include "mindex_disk.hpp"
#include "../../objectdb.hpp"
#include "../../datasets/paths.hpp"

using namespace std;

int main() {
    srand(12345);

    string dataset = "LA";
    string dbfile = path_dataset(dataset);
    
    auto db = make_unique<VectorDB>(dbfile, 2);
    
    cout << "\n==========================================\n";
    cout << "[M-Index*] Dataset: " << dataset
         << "   N=" << db->size() << "\n";
    cout << "==========================================\n";

    // Parámetros
    int numPivots = 5;  // mismo que EGNAT

    // Crear M-Index
    MIndex_Disk midx(db.get(), numPivots);
    string base = "midx_indexes/" + dataset;

    // Crear directorio si no existe
    filesystem::create_directories("midx_indexes");

    // Build
    auto t0 = chrono::high_resolution_clock::now();
    midx.build(base);
    auto t1 = chrono::high_resolution_clock::now();
    auto buildTime = chrono::duration_cast<chrono::milliseconds>(t1 - t0).count();
    
    cout << "\n[BUILD] Tiempo: " << buildTime << " ms\n";
    cout << "[BUILD] Índice guardado en: " << base << ".midx_raf\n";

    // Cargar queries
    vector<int> queries = load_queries_file(path_queries(dataset));
    auto radii = load_radii_file(path_radii(dataset));
    
    cout << "\n[QUERIES] Cargadas " << queries.size() << " queries\n";
    cout << "[QUERIES] Radii para " << radii.size() << " selectividades\n";

    // ===== Range Query Demo (1 query, selectividad 0.02) =====
    cout << "\n=== RANGE QUERY DEMO ===\n";
    double sel = 0.02;
    double R = radii[sel];
    int qid = queries[0];
    
    vector<int> results;
    midx.clear_counters();
    
    t0 = chrono::high_resolution_clock::now();
    midx.rangeSearch(qid, R, results);
    t1 = chrono::high_resolution_clock::now();
    auto queryTime = chrono::duration_cast<chrono::microseconds>(t1 - t0).count() / 1000.0;
    
    cout << "Query ID: " << qid << "\n";
    cout << "Selectividad: " << sel << " (radius=" << R << ")\n";
    cout << "Resultados encontrados: " << results.size() << "\n";
    cout << "Cómputos de distancia: " << midx.get_compDist() << "\n";
    cout << "Lecturas de página: " << midx.get_pageReads() << "\n";
    cout << "Tiempo: " << queryTime << " ms\n";

    // ===== k-NN Query Demo (1 query, k=10) =====
    cout << "\n=== k-NN QUERY DEMO ===\n";
    int k = 10;
    vector<pair<double,int>> knn;
    
    midx.clear_counters();
    
    t0 = chrono::high_resolution_clock::now();
    midx.knnSearch(qid, k, knn);
    t1 = chrono::high_resolution_clock::now();
    queryTime = chrono::duration_cast<chrono::microseconds>(t1 - t0).count() / 1000.0;
    
    cout << "Query ID: " << qid << "\n";
    cout << "k: " << k << "\n";
    cout << "Resultados encontrados: " << knn.size() << "\n";
    cout << "Cómputos de distancia: " << midx.get_compDist() << "\n";
    cout << "Lecturas de página: " << midx.get_pageReads() << "\n";
    cout << "Tiempo: " << queryTime << " ms\n";
    
    cout << "\nPrimeros " << min((int)knn.size(), 5) << " vecinos:\n";
    for (int i = 0; i < min((int)knn.size(), 5); i++) {
        cout << "  " << (i+1) << ". ID=" << knn[i].second 
             << ", dist=" << fixed << setprecision(2) << knn[i].first << "\n";
    }

    // ===== Benchmark reducido (10 queries) =====
    cout << "\n=== BENCHMARK REDUCIDO (10 queries) ===\n";
    
    int numQueries = min(10, (int)queries.size());
    
    // Range queries
    cout << "\nRange Queries (selectividad " << sel << "):\n";
    long long totalD = 0, totalPages = 0;
    double totalTime = 0;
    
    for (int i = 0; i < numQueries; i++) {
        vector<int> out;
        midx.clear_counters();
        
        t0 = chrono::high_resolution_clock::now();
        midx.rangeSearch(queries[i], R, out);
        t1 = chrono::high_resolution_clock::now();
        
        totalD += midx.get_compDist();
        totalPages += midx.get_pageReads();
        totalTime += chrono::duration_cast<chrono::microseconds>(t1 - t0).count() / 1000.0;
    }
    
    cout << "  Promedio cómputos distancia: " << totalD / numQueries << "\n";
    cout << "  Promedio lecturas página: " << totalPages / numQueries << "\n";
    cout << "  Promedio tiempo: " << totalTime / numQueries << " ms\n";

    // k-NN queries
    cout << "\nk-NN Queries (k=" << k << "):\n";
    totalD = totalPages = 0;
    totalTime = 0;
    
    for (int i = 0; i < numQueries; i++) {
        vector<pair<double,int>> out;
        midx.clear_counters();
        
        t0 = chrono::high_resolution_clock::now();
        midx.knnSearch(queries[i], k, out);
        t1 = chrono::high_resolution_clock::now();
        
        totalD += midx.get_compDist();
        totalPages += midx.get_pageReads();
        totalTime += chrono::duration_cast<chrono::microseconds>(t1 - t0).count() / 1000.0;
    }
    
    cout << "  Promedio cómputos distancia: " << totalD / numQueries << "\n";
    cout << "  Promedio lecturas página: " << totalPages / numQueries << "\n";
    cout << "  Promedio tiempo: " << totalTime / numQueries << " ms\n";

    cout << "\n==========================================\n";
    cout << "[M-Index*] Demo completado exitosamente\n";
    cout << "==========================================\n";

    return 0;
}
