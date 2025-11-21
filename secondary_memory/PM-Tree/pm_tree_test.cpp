#include <bits/stdc++.h>
#include <sys/stat.h>

#include "DSACLX.h"
#include "obj.h"

#include "../../datasets/paths.hpp"   // path_dataset, path_queries, path_radii
using namespace std;

// ============================================================
// Global DSACL vars required by DSACLX.cpp
// ============================================================
double numDistances = 0.0;
double IOread       = 0.0;
double IOwrite      = 0.0;

int    MaxArity     = 0;
int    ClusterSize  = 0;
double CurrentTime  = 0.0;
int    dimension    = 0;
double objNum       = 0.0;
int    func         = 0;
int    BlockSize    = 4096;

// ============================================================
// Configuración Chen2022
// ============================================================
static const vector<double> SELECTIVITIES = {0.02, 0.04, 0.08, 0.16, 0.32};
static const vector<int>    K_VALUES      = {5, 10, 20, 50, 100};
static const vector<int>    ARITY_VALUES  = {3, 5, 10, 15, 20};

// Solo datasets vectoriales (DSACL)
static const vector<string> DATASETS = {"LA", "Color", "Synthetic"};

// ============================================================
// Helpers
// ============================================================
bool parse_db_header(const string &dbPath, int &dim, int &num, int &fn) {
    ifstream f(dbPath);
    if (!f.is_open()) return false;
    f >> dim >> num >> fn;
    return f.good();
}

void load_queries_vec(const string &path, int dim, int maxQ, vector<vector<float>> &Q) {
    Q.clear();
    ifstream f(path);
    if (!f.is_open()) return;
    Q.reserve(maxQ);

    for (int i=0;i<maxQ;i++) {
        vector<float> q(dim);
        for (int j=0;j<dim;j++) {
            if (!(f >> q[j])) return;
        }
        Q.push_back(q);
    }
}

// ============================================================
// MAIN
// ============================================================
int main() {
    ios::sync_with_stdio(false);
    cin.tie(nullptr);

    std::filesystem::create_directories("results");
    const int MAX_Q = 100;

    for (const string &dataset : DATASETS) {

        // -----------------------------------------
        // Obtener rutas desde paths.hpp
        // -----------------------------------------
        string dbPath     = path_dataset(dataset);
        string queriesPath= path_queries(dataset);
        string radiiPath  = path_radii(dataset);

        if (dbPath=="" || queriesPath=="") {
            cerr << "[WARN] Missing dataset paths for " << dataset << "\n";
            continue;
        }

        if (!file_exists(dbPath) || !file_exists(queriesPath)) {
            cerr << "[WARN] Missing files for " << dataset << "\n";
            continue;
        }

        // -----------------------------------------
        // Header DB
        // -----------------------------------------
        int dim=0, num=0, fn=0;
        if (!parse_db_header(dbPath, dim, num, fn)) {
            cerr << "[WARN] Invalid DB header\n";
            continue;
        }

        dimension = dim;
        objNum    = num;
        func      = fn;

        // -----------------------------------------
        // Cargar queries
        // -----------------------------------------
        vector<vector<float>> Q;
        load_queries_vec(queriesPath, dim, MAX_Q, Q);
        if (Q.empty()) {
            cerr << "[WARN] No queries for " << dataset << "\n";
            continue;
        }

        // -----------------------------------------
        // Cargar radios (viene de paths.hpp)
        // -----------------------------------------
        auto radii = load_radii_file(radiiPath);
        if (radii.empty()) {
            cerr << "[WARN] No radii JSON\n";
        }

        // -----------------------------------------
        // Archivo JSON output
        // -----------------------------------------
        string out = "results/results_SATCTL_" + dataset + ".json";
        ofstream J(out);
        bool first=true;
        J<<"[\n";

        // -----------------------------------------
        // Loop aridades
        // -----------------------------------------
        for (int arity : ARITY_VALUES) {

            MaxArity = arity;
            BlockSize = (dataset=="LA") ? 4096 : 40960;

            string idxPath = dataset + ".satctl_index";

            // BUILD
            numDistances = 0.0;
            IOread=IOwrite=0.0;

            auto t0 = chrono::high_resolution_clock::now();
            buildSecondaryMemory(const_cast<char*>(idxPath.c_str()),
                                 const_cast<char*>(dbPath.c_str()));
            auto t1 = chrono::high_resolution_clock::now();

            double build_s = chrono::duration<double>(t1-t0).count();
            cerr<<"[BUILD "<<dataset<<" a="<<arity<<"] "<<build_s<<"s pages="<<IOread<<" dists="<<numDistances<<"\n";

            // ==========================
            // MRQ
            // ==========================
            for (auto sel : SELECTIVITIES) {
                if (!radii.count(sel)) continue;
                double R = radii[sel];

                numDistances=0; IOread=IOwrite=0;
                auto t0 = chrono::high_resolution_clock::now();

                for (auto &v : Q) {
                    Objvector q;
                    q.dim=dim;
                    q.value=new float[dim];
                    for (int i=0;i<dim;i++) q.value[i]=v[i];
                    rangeSearch(q, R);
                    q.freeResources();
                }

                auto t1 = chrono::high_resolution_clock::now();

                double total_ms=chrono::duration<double,milli>(t1-t0).count();
                double avgT = total_ms / Q.size();
                double avgD = numDistances / Q.size();
                double avgP = IOread / Q.size();

                if(!first) J<<",\n";
                first=false;

                J << fixed << setprecision(6)
                  << "{"
                  << "\"index\":\"SATCTL\","
                  << "\"dataset\":\""<<dataset<<"\","
                  << "\"category\":\"DM\","
                  << "\"num_pivots\":null,"
                  << "\"num_centers_path\":null,"
                  << "\"arity\":"<<arity<<","
                  << "\"query_type\":\"MRQ\","
                  << "\"selectivity\":"<<sel<<","
                  << "\"radius\":"<<R<<","
                  << "\"k\":null,"
                  << "\"compdists\":"<<avgD<<","
                  << "\"time_ms\":"<<avgT<<","
                  << "\"pages\":"<<avgP<<","
                  << "\"n_queries\":"<<Q.size()<<","
                  << "\"run_id\":1"
                  << "}";
            }

            // ==========================
            // MkNN
            // ==========================
            for (int k : K_VALUES) {

                numDistances=0; IOread=IOwrite=0;
                auto t0 = chrono::high_resolution_clock::now();

                for (auto &v : Q) {
                    Objvector q;
                    q.dim=dim;
                    q.value=new float[dim];
                    for (int i=0;i<dim;i++) q.value[i]=v[i];
                    knnSearch(q, k);
                    q.freeResources();
                }

                auto t1 = chrono::high_resolution_clock::now();

                double total_ms=chrono::duration<double,milli>(t1-t0).count();
                double avgT = total_ms / Q.size();
                double avgD = numDistances / Q.size();
                double avgP = IOread / Q.size();

                J<<",\n";
                J << fixed << setprecision(6)
                  << "{"
                  << "\"index\":\"SATCTL\","
                  << "\"dataset\":\""<<dataset<<"\","
                  << "\"category\":\"DM\","
                  << "\"num_pivots\":null,"
                  << "\"num_centers_path\":null,"
                  << "\"arity\":"<<arity<<","
                  << "\"query_type\":\"MkNN\","
                  << "\"selectivity\":null,"
                  << "\"radius\":null,"
                  << "\"k\":"<<k<<","
                  << "\"compdists\":"<<avgD<<","
                  << "\"time_ms\":"<<avgT<<","
                  << "\"pages\":"<<avgP<<","
                  << "\"n_queries\":"<<Q.size()<<","
                  << "\"run_id\":1"
                  << "}";
            }
        }

        J<<"\n]\n";
        J.close();

        cerr<<"[DONE] "<<out<<"\n";
    }

    return 0;
}
