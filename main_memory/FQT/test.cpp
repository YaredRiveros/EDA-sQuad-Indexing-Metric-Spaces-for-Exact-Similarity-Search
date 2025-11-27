#include <bits/stdc++.h>
#include "fqt.hpp"
#include "../../datasets/paths.hpp"
#include "../../objectdb.hpp"
#include <filesystem>
using namespace std;

// ============================================================
// CONFIGURACIÓN (igual a MVPT y LAESA)
// ============================================================

static const vector<double> SELECTIVITIES = {0.02, 0.04, 0.08, 0.16, 0.32};
static const vector<int>    K_VALUES      = {5, 10, 20, 50, 100};
static const vector<int>    L_VALUES      = {3, 5, 10, 15, 20};

// static const vector<string> DATASETS = {"LA","Words","Color","Synthetic"};
static const vector<string> DATASETS = {"LA"};

// ============================================================
// PARÁMETROS DE FQT POR DATASET (del paper Chen2022)
// ============================================================

struct FQT_Params {
    int bucket;
    int arity;
};

static const vector<FQT_Params> PARAMS_LA = {
    {100, 5},  // target height 3
    { 50, 5},  // target height 5
    { 20, 5},  // target height 10
    { 10, 5},  // target height 15
    {  5, 5}   // target height 20
};

static const vector<FQT_Params> PARAMS_WORDS = {
    {200,4},{100,4},{50,4},{20,4},{10,4}
};

static const vector<FQT_Params> PARAMS_COLOR = {
    {100,5},{50,5},{20,5},{10,5},{5,5}
};

static const vector<FQT_Params> PARAMS_SYNTH = {
    {100,5},{50,5},{20,5},{10,5},{5,5}
};

// ============================================================
// Función auxiliar para cargar pivots2k/*.json
// (idéntica a MVPT test)
// ============================================================

vector<int> load_pivots_json(const string& path) {
    vector<int> piv;
    if (path == "" || !file_exists(path)) {
        cerr << "[WARN] Missing pivot JSON: " << path << "\n";
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

// ============================================================
// MAIN
// ============================================================

int main(int argc, char** argv)
{
    vector<string> datasets;

    if (argc > 1) {
        for (int i = 1; i < argc; i++)
            datasets.push_back(argv[i]);
    }
    else {
        datasets = DATASETS;
    }

    filesystem::create_directories("results");

    // ========================================================
    // LOOP POR DATASETS
    // ========================================================
    for (const string& dataset : datasets)
    {
        cout << "\n==============================\n";
        cout << " DATASET: " << dataset << "\n";
        cout << "==============================\n";

        string dbfile = path_dataset(dataset);

        if (dbfile == "") {
            cerr << "[WARN] Dataset not found: " << dataset << "\n";
            continue;
        }

        unique_ptr<ObjectDB> db;

        if (dataset == "LA") db = make_unique<VectorDB>(dbfile, 2);
        else if (dataset == "Words") db = make_unique<StringDB>(dbfile);
        else if (dataset == "Color") db = make_unique<VectorDB>(dbfile, 1);
        else if (dataset == "Synthetic") db = make_unique<VectorDB>(dbfile, 999999);
        else continue;

        int N = db->size();
        if (N == 0) { cerr << "[WARN] Empty dataset\n"; continue; }

        // Cargar queries y radios
        vector<int> queries = load_queries_file(path_queries(dataset));
        auto radii = load_radii_file(path_radii(dataset));

        if (queries.empty()) {
            cerr << "[WARN] Missing queries for " << dataset << "\n";
            continue;
        }

        // Seleccionar parámetros por dataset
        vector<FQT_Params> params;
        if (dataset == "LA") params = PARAMS_LA;
        else if (dataset == "Words") params = PARAMS_WORDS;
        else if (dataset == "Color") params = PARAMS_COLOR;
        else if (dataset == "Synthetic") params = PARAMS_SYNTH;

        // Archivo JSON
        string jsonOut = "results/results_FQT_" + dataset + ".json";
        ofstream J(jsonOut);
        J << "[\n";
        bool first = true;

        // ========================================================
        // LOOP por l = {3,5,10,15,20}
        // ========================================================
        for (int i = 0; i < (int)L_VALUES.size(); i++)
        {
            int l = L_VALUES[i];
            FQT_Params P = params[i];

            cout << "\n[INFO] Building FQT: l=" << l
                 << " bucket=" << P.bucket
                 << " arity="  << P.arity << "\n";

            // Cargar pivots HFI desde pivots2k/*.json
            string pivFile = path_pivots(dataset, l);
            vector<int> pivots = load_pivots_json(pivFile);

            if (pivots.empty()) {
                cerr << "[WARN] Missing HFI pivots: " << pivFile << "\n";
                continue;
            }

            // Construir FQT
            auto t0 = chrono::high_resolution_clock::now();
            FQT index(db.get(), P.bucket, P.arity, pivots);
            index.build();
            auto t1 = chrono::high_resolution_clock::now();

            double build_ms = chrono::duration_cast<chrono::milliseconds>(t1 - t0).count();

            cout << "[BUILD] time_ms=" << build_ms << " height=" << l << "\n";

            // ========================
            // MRQ EXPERIMENTS
            // ========================
            for (double sel : SELECTIVITIES)
            {
                if (!radii.count(sel)) continue;
                double R = radii.at(sel);

                long long totalD = 0;
                auto q0 = chrono::high_resolution_clock::now();

                for (int q : queries) {
                    index.resetCompdists();
                    int cnt = index.range(q, R);
                    totalD += index.getCompdists();
                }

                auto q1 = chrono::high_resolution_clock::now();
                double q_us = chrono::duration_cast<chrono::microseconds>(q1 - q0).count();

                double avgD  = totalD / (double)queries.size();
                double avgMs = (q_us / 1000.0) / queries.size();

                if (!first) J << ",\n"; first = false;

                J << fixed << setprecision(6);
                J << "{"
                  << "\"index\":\"FQT\","
                  << "\"dataset\":\"" << dataset << "\","
                  << "\"num_pivots\":" << l << ","
                  << "\"arity\":" << P.arity << ","
                  << "\"bucket_size\":" << P.bucket << ","
                  << "\"query_type\":\"MRQ\","
                  << "\"selectivity\":" << sel << ","
                  << "\"radius\":" << R << ","
                  << "\"k\":null,"
                  << "\"compdists\":" << avgD << ","
                  << "\"time_ms\":" << avgMs << ","
                  << "\"n_queries\":" << queries.size() << ","
                  << "\"run_id\":1"
                  << "}";
            }

            // ========================
            // MkNN EXPERIMENTS
            // ========================
            for (int k : K_VALUES)
            {
                long long totalD = 0;
                auto q0 = chrono::high_resolution_clock::now();

                for (int q : queries) {
                    index.resetCompdists();
                    vector<pair<double,int>> dummy; // result ignored
                    double kth = index.knn(q, k); // counts compdists internally
                    totalD += index.getCompdists();
                }

                auto q1 = chrono::high_resolution_clock::now();
                double q_us = chrono::duration_cast<chrono::microseconds>(q1 - q0).count();

                double avgD  = totalD / (double)queries.size();
                double avgMs = (q_us / 1000.0) / queries.size();

                if (!first) J << ",\n"; first = false;

                J << fixed << setprecision(6);
                J << "{"
                  << "\"index\":\"FQT\","
                  << "\"dataset\":\"" << dataset << "\","
                  << "\"num_pivots\":" << l << ","
                  << "\"arity\":" << P.arity << ","
                  << "\"bucket_size\":" << P.bucket << ","
                  << "\"query_type\":\"MkNN\","
                  << "\"selectivity\":null,"
                  << "\"radius\":null,"
                  << "\"k\":" << k << ","
                  << "\"compdists\":" << avgD << ","
                  << "\"time_ms\":" << avgMs << ","
                  << "\"n_queries\":" << queries.size() << ","
                  << "\"run_id\":1"
                  << "}";
            }

        } // END loop l ∈ L_VALUES

        J << "\n]\n";
        J.close();

        cout << "[DONE] results_FQT_" << dataset << ".json\n";
    }

    return 0;
}
