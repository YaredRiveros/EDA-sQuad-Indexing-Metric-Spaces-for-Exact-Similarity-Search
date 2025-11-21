#include <bits/stdc++.h>
#include <sys/stat.h>

#include "DSACLX.h"
#include "obj.h"

#include "../../datasets/paths.hpp"
using namespace std;

// ============================================================
// Globals usados por DSACLX.cpp
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

// Solo datasets vectoriales
static const vector<string> DATASETS = {"LA", "Color", "Synthetic"};

// ============================================================
// Función: leer header DB DSACL
// ============================================================
bool parse_db_header(const string &dbPath,
                     int &dim, int &num, int &fn)
{
    ifstream f(dbPath);
    if (!f.is_open()) return false;
    f >> dim >> num >> fn;
    return f.good();
}

// ============================================================
// Leer queries JSON (lista simple de enteros)
// ============================================================
vector<int> load_query_ids_json(const string &path) {
    vector<int> ids;
    if (!file_exists(path)) return ids;

    ifstream f(path);
    string tok;

    while (f >> tok) {
        // remover símbolos JSON
        tok.erase(remove_if(tok.begin(), tok.end(),
                [](char c){
                    return c=='[' || c==']' || c==','; }), tok.end());

        if (!tok.empty() && all_of(tok.begin(), tok.end(), ::isdigit))
            ids.push_back(stoi(tok));
    }
    return ids;
}

// ============================================================
// MAIN — SATCTL benchmark Chen2022
// ============================================================
int main() {
    ios::sync_with_stdio(false);
    cin.tie(nullptr);

    std::filesystem::create_directories("results");

    const int MAX_Q = 100;

    for (const string &dataset : DATASETS) {

        // -----------------------------------------
        // Rutas DSACL + JSON desde paths.hpp
        // -----------------------------------------
        string dbPath    = path_dataset(dataset);   // datasets/LA.txt ...
        string qjsonPath = path_queries(dataset);   // queries JSON IDs
        string radiiPath = path_radii(dataset);

        if (dbPath=="" || !file_exists(dbPath)) {
            cerr << "[WARN] DB no encontrada: " << dbPath << "\n";
            continue;
        }
        if (qjsonPath=="" || !file_exists(qjsonPath)) {
            cerr << "[WARN] Query JSON no encontrado: " << qjsonPath << "\n";
            continue;
        }

        // -----------------------------------------
        // Leer header del dataset DSACL
        // -----------------------------------------
        int dim=0, num=0, fn=0;
        if (!parse_db_header(dbPath, dim, num, fn)) {
            cerr << "[ERROR] Header inválido: " << dbPath << "\n";
            continue;
        }

        dimension = dim;
        objNum    = num;
        func      = fn;

        cerr << "\n==========================================\n";
        cerr << "[INFO] Dataset: " << dataset
             << "  dim=" << dim
             << "  N=" << num << "\n";
        cerr << "==========================================\n";

        // -----------------------------------------
        // Cargar dataset completo en RAM
        // -----------------------------------------
        vector<vector<float>> DB(num, vector<float>(dim));
        {
            ifstream f(dbPath);
            int dummy_dim, dummy_num, dummy_func;
            f >> dummy_dim >> dummy_num >> dummy_func;

            for (int i=0; i<num; i++) {
                for (int j=0; j<dim; j++)
                    f >> DB[i][j];
            }
        }

        // -----------------------------------------
        // Cargar queries: JSON → IDs
        // -----------------------------------------
        vector<int> qIDs = load_query_ids_json(qjsonPath);
        if (qIDs.empty()) {
            cerr << "[WARN] No query IDs in " << qjsonPath << "\n";
            continue;
        }
        if ((int)qIDs.size() > MAX_Q)
            qIDs.resize(MAX_Q);

        cerr << "[INFO] Loaded " << qIDs.size() << " queries.\n";

        // -----------------------------------------
        // Cargar radios para MRQ
        // -----------------------------------------
        auto radii = load_radii_file(radiiPath);

        // -----------------------------------------
        // Archivo JSON salida
        // -----------------------------------------
        string out = "results/results_SATCTL_" + dataset + ".json";
        ofstream J(out);
        bool first=true;
        J << "[\n";

        // -----------------------------------------
        // Loop sobre MaxArity
        // -----------------------------------------
        for (int arity : ARITY_VALUES) {

            MaxArity  = arity;
            BlockSize = (dataset=="LA") ? 4096 : 40960; // 4KB / 40KB

            string idxPath = dataset + ".satctl_index";

            // -----------------------------------------
            // BUILD
            // -----------------------------------------
            numDistances=0.0; IOread=IOwrite=0.0;

            auto tb0 = chrono::high_resolution_clock::now();
            buildSecondaryMemory(const_cast<char*>(idxPath.c_str()),
                                 const_cast<char*>(dbPath.c_str()));
            auto tb1 = chrono::high_resolution_clock::now();

            cerr << "[BUILD] Arity=" << arity
                 << "  time=" << chrono::duration<double>(tb1-tb0).count()
                 << "  pages=" << IOread
                 << "  dists=" << numDistances << "\n";

            // ====================================================
            //                    MRQ
            // ====================================================
            for (double sel : SELECTIVITIES) {
                if (!radii.count(sel)) continue;

                double R = radii[sel];
                numDistances=0; IOread=IOwrite=0;

                auto t0 = chrono::high_resolution_clock::now();

                for (int qid : qIDs) {

                    Objvector q;
                    q.dim   = dim;
                    q.value = new float[dim];

                    for (int j=0;j<dim;j++)
                        q.value[j] = DB[qid][j];

                    rangeSearch(q, R);
                    q.freeResources();
                }

                auto t1 = chrono::high_resolution_clock::now();

                double total_ms = chrono::duration<double,milli>(t1-t0).count();
                double avgT     = total_ms / qIDs.size();
                double avgD     = numDistances / qIDs.size();
                double avgPages = IOread / qIDs.size();

                if(!first) J<<",\n";
                first=false;

                J<< fixed << setprecision(6)
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
                  << "\"pages\":"<<avgPages<<","
                  << "\"n_queries\":"<<qIDs.size()<<","
                  << "\"run_id\":1"
                  << "}";
            }

            // ====================================================
            //                    MkNN
            // ====================================================
            for (int k : K_VALUES) {

                numDistances=0; IOread=IOwrite=0;

                auto t0 = chrono::high_resolution_clock::now();

                for (int qid : qIDs) {
                    Objvector q;
                    q.dim   = dim;
                    q.value = new float[dim];
                    for (int j=0;j<dim;j++)
                        q.value[j] = DB[qid][j];

                    knnSearch(q, k);
                    q.freeResources();
                }

                auto t1 = chrono::high_resolution_clock::now();

                double total_ms = chrono::duration<double,milli>(t1-t0).count();
                double avgT     = total_ms / qIDs.size();
                double avgD     = numDistances / qIDs.size();
                double avgPages = IOread / qIDs.size();

                J<<",\n";
                J<< fixed << setprecision(6)
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
                  << "\"pages\":"<<avgPages<<","
                  << "\"n_queries\":"<<qIDs.size()<<","
                  << "\"run_id\":1"
                  << "}";
            }
        }

        J<<"\n]\n";
        J.close();
        cerr << "[DONE] " << out << "\n";
    }

    return 0;
}
