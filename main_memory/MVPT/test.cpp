#include <bits/stdc++.h>
#include "mvpt.hpp"
#include "../../objectdb.hpp"
#include "../../datasets/paths.hpp"
#include <filesystem>
using namespace std;

// Experiment protocol equal to LAESA test
static const vector<double> SELECTIVITIES = {0.02, 0.04, 0.08, 0.16, 0.32};
static const vector<int>    K_VALUES       = {5, 10, 20, 50, 100};
static const vector<int>    PIVOT_COUNTS   = {3, 5, 10, 15, 20};
static const vector<string> DATASETS       = {"LA", "Words", "Color", "Synthetic"};

// Load pivots JSON (precomputed by HFI) — same parser used in LAESA test
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

    for (const string& dataset : DATASETS)
    {
        string dbfile = path_dataset(dataset);
        if (dbfile == "") { cerr << "[WARN] Dataset not found: " << dataset << "\n"; continue; }

        unique_ptr<ObjectDB> db;
        if (dataset == "LA") db = make_unique<VectorDB>(dbfile, 2);
        else if (dataset == "Color") db = make_unique<VectorDB>(dbfile, 1);
        else if (dataset == "Synthetic") db = make_unique<VectorDB>(dbfile, 999999);
        else if (dataset == "Words") db = make_unique<StringDB>(dbfile);
        else { cerr << "[WARN] Unknown dataset: " << dataset << "\n"; continue; }

        int nObjects = db->size();
        cerr << "\n[INFO] Dataset: " << dataset << " N=" << nObjects << " File=" << dbfile << "\n";

        if (nObjects == 0) { cerr << "[WARN] Empty dataset\n"; continue; }

        vector<int> queries = load_queries_file(path_queries(dataset));
        auto radii = load_radii_file(path_radii(dataset));
        if (queries.empty()) { cerr << "[WARN] No queries, skipping\n"; continue; }

        string jsonOut = "results/results_MVPT_" + dataset + ".json";
        ofstream J(jsonOut);
        J << "[\n";
        bool firstOutput = true;

        // Experiment: try multiple pivot counts (precomputed pivots via HFI)
        for (int nPivots : PIVOT_COUNTS)
        {
            cerr << "\n[INFO] Building MVPT with nPivots=" << nPivots << " (arity fixed = 5)\n";

            // load pivots JSON prepared by HFI
            string pivFile = path_pivots(dataset, nPivots);
            vector<int> pivots = load_pivots_json(pivFile);

            if (pivots.empty()) {
                cerr << "[WARN] Pivots missing for l=" << nPivots << " (file: " << pivFile << ")\n";
                continue;
            }

            // Build MVPT: configuredHeight = nPivots, arity fixed to 5 (to mirror paper)
            const int MVPT_BUCKET_SIZE = 20;
            const int MVPT_ARITY = 5;

            auto t0 = chrono::high_resolution_clock::now();
            MVPT index(db.get(), MVPT_BUCKET_SIZE, MVPT_ARITY, nPivots, pivots);
            auto t1 = chrono::high_resolution_clock::now();
            double buildTimeMs = chrono::duration_cast<chrono::milliseconds>(t1 - t0).count();

            cerr << "[BUILD] time_ms=" << buildTimeMs << " configuredHeight=" << nPivots << "\n";

            // MRQ (range) experiments
            for (double sel : SELECTIVITIES) {
                if (!radii.count(sel)) continue;
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

                if (!firstOutput) J << ",\n"; firstOutput = false;
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
                cerr << "  MRQ sel=" << sel << " -> avgD=" << avgD << "\n";
            }

            // MkNN experiments
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

                if (!firstOutput) J << ",\n"; firstOutput = false;
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
                cerr << "  MkNN k=" << k << " -> avgD=" << avgD << "\n";
            }

        } // end PIVOT_COUNTS loop

        J << "\n]\n";
        J.close();
        cerr << "[DONE] results_MVPT_" << dataset << ".json\n";
    }

    return 0;
}
