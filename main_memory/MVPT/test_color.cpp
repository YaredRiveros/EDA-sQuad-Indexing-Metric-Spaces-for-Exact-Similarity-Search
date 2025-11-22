#include <bits/stdc++.h>
#include "mvpt.hpp"
#include "../../objectdb.hpp"
#include "../../datasets/paths.hpp"
#include <filesystem>
using namespace std;

// Experiment protocol igual al test principal
static const vector<double> SELECTIVITIES = {0.02, 0.04, 0.08, 0.16, 0.32};
static const vector<int>    K_VALUES       = {5, 10, 20, 50, 100};
static const vector<int>    PIVOT_COUNTS   = {3, 5, 10, 15, 20};

// Load pivots JSON (precomputed by HFI)
vector<int> load_pivots_json(const string& path) {
    vector<int> piv;
    if (path == "" || !file_exists(path)) {
        cerr << "[WARN] Pivot JSON missing: " << path << "\n";
        return piv;
    }
    ifstream f(path);
    string tok;
    while (f >> tok) {
        tok.erase(remove_if(tok.begin(), tok.end(),
                            [](char c){
                                return c=='['||c==']'||c==','||c=='"'||c==':';
                            }),
                  tok.end());
        if (tok == "pivots") continue;
        if (!tok.empty() && all_of(tok.begin(), tok.end(), ::isdigit))
            piv.push_back(stoi(tok));
    }
    return piv;
}

int main()
{
    filesystem::create_directories("results");

    const string dataset = "Color";
    
    cerr << "\n##########################################\n";
    cerr << "### TESTING MVPT - Dataset: " << dataset << "\n";
    cerr << "##########################################\n";

    string dbfile = path_dataset(dataset);
    if (dbfile == "") { 
        cerr << "[ERROR] Dataset file not found: " << dataset << ".txt\n";
        cerr << "\nPara generar Color.txt:\n";
        cerr << "  cd datasets\n";
        cerr << "  bash download_color.sh\n";
        cerr << "  python convert_color.py\n\n";
        return 1; 
    }

    cerr << "[INFO] Loading Color dataset from: " << dbfile << "\n";

    // Color usa VectorDB con p=1 (L1 distance)
    unique_ptr<ObjectDB> db = make_unique<VectorDB>(dbfile, 1);

    int nObjects = db->size();
    cerr << "\n[INFO] Dataset: " << dataset << " N=" << nObjects << " File=" << dbfile << "\n";

    if (nObjects == 0) { 
        cerr << "[ERROR] Empty dataset\n"; 
        return 1; 
    }

    vector<int> queries = load_queries_file(path_queries(dataset));
    auto radii = load_radii_file(path_radii(dataset));
    
    if (queries.empty()) { 
        cerr << "[ERROR] No queries available\n"; 
        return 1; 
    }

    cerr << "[QUERIES] Cargadas " << queries.size() << " queries\n";
    cerr << "[RADII] Disponibles " << radii.size() << " selectividades\n";

    string jsonOut = "results/results_MVPT_" + dataset + ".json";
    ofstream J(jsonOut);
    J << "[\n";
    bool firstOutput = true;

    // Experiment: try multiple pivot counts
    for (int nPivots : PIVOT_COUNTS)
    {
        cerr << "\n========================================\n";
        cerr << "[BUILD] MVPT with nPivots=" << nPivots << " (arity=5)\n";
        cerr << "========================================\n";

        // Load pivots JSON prepared by HFI
        string pivFile = path_pivots(dataset, nPivots);
        vector<int> pivots = load_pivots_json(pivFile);

        if (pivots.empty()) {
            cerr << "[WARN] Pivots missing for l=" << nPivots << " (file: " << pivFile << ")\n";
            continue;
        }

        cerr << "[PIVOTS] Loaded " << pivots.size() << " pivots from " << pivFile << "\n";

        // Build MVPT
        const int MVPT_BUCKET_SIZE = 20;
        const int MVPT_ARITY = 5;

        auto t0 = chrono::high_resolution_clock::now();
        MVPT index(db.get(), MVPT_BUCKET_SIZE, MVPT_ARITY, nPivots, pivots);
        auto t1 = chrono::high_resolution_clock::now();
        double buildTimeMs = chrono::duration_cast<chrono::milliseconds>(t1 - t0).count();

        cerr << "[BUILD] Completed in " << buildTimeMs << " ms\n";

        // MRQ experiments (varying selectivity)
        cerr << "\n[MRQ] Running range queries...\n";
        for (double sel : SELECTIVITIES) {
            if (!radii.count(sel)) {
                cerr << "  [SKIP] Selectivity " << sel << " not available\n";
                continue;
            }
            
            double R = radii[sel];
            long long totalD = 0;
            auto t1q = chrono::high_resolution_clock::now();
            
            for (int q : queries) {
                vector<int> out;
                compdists = 0;
                index.rangeSearch(q, R, out);
                totalD += compdists;
            }
            
            auto t2q = chrono::high_resolution_clock::now();
            double totalTimeUS = chrono::duration_cast<chrono::microseconds>(t2q - t1q).count();
            double avgD = double(totalD) / queries.size();
            double avgTms = totalTimeUS / (1000.0 * queries.size());

            if (!firstOutput) J << ",\n"; 
            firstOutput = false;
            
            J << fixed << setprecision(6);
            J << "{"
              << "\"index\":\"MVPT\","
              << "\"dataset\":\"" << dataset << "\","
              << "\"category\":\"HFI\","
              << "\"num_pivots\":" << nPivots << ","
              << "\"num_centers_path\":null,"
              << "\"arity\":" << MVPT_ARITY << ","
              << "\"query_type\":\"MRQ\","
              << "\"selectivity\":" << sel << ","
              << "\"radius\":" << R << ","
              << "\"k\":null,"
              << "\"compdists\":" << avgD << ","
              << "\"time_ms\":" << avgTms << ","
              << "\"n_queries\":" << queries.size() << ","
              << "\"run_id\":1"
              << "}";
              
            cerr << "  sel=" << sel << " R=" << R << " -> avgD=" << (long long)avgD << "\n";
        }

        // MkNN experiments (varying k)
        cerr << "\n[MkNN] Running k-NN queries...\n";
        for (int k : K_VALUES) {
            long long totalD = 0;
            auto t1q = chrono::high_resolution_clock::now();
            
            for (int q : queries) {
                vector<ResultElem> out;
                compdists = 0;
                index.knnSearch(q, k, out);
                totalD += compdists;
            }
            
            auto t2q = chrono::high_resolution_clock::now();
            double totalTimeUS = chrono::duration_cast<chrono::microseconds>(t2q - t1q).count();
            double avgD = double(totalD) / queries.size();
            double avgTms = totalTimeUS / (1000.0 * queries.size());

            if (!firstOutput) J << ",\n"; 
            firstOutput = false;
            
            J << fixed << setprecision(6);
            J << "{"
              << "\"index\":\"MVPT\","
              << "\"dataset\":\"" << dataset << "\","
              << "\"category\":\"HFI\","
              << "\"num_pivots\":" << nPivots << ","
              << "\"num_centers_path\":null,"
              << "\"arity\":" << MVPT_ARITY << ","
              << "\"query_type\":\"MkNN\","
              << "\"selectivity\":null,"
              << "\"radius\":null,"
              << "\"k\":" << k << ","
              << "\"compdists\":" << avgD << ","
              << "\"time_ms\":" << avgTms << ","
              << "\"n_queries\":" << queries.size() << ","
              << "\"run_id\":1"
              << "}";
              
            cerr << "  k=" << k << " -> avgD=" << (long long)avgD << "\n";
        }

    } // end PIVOT_COUNTS loop

    J << "\n]\n";
    J.close();
    
    cerr << "\n==========================================\n";
    cerr << "[DONE] " << jsonOut << "\n";
    cerr << "==========================================\n";

    return 0;
}
