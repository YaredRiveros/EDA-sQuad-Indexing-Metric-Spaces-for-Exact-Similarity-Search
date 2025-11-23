// test_mbpt.cpp - MB+-tree benchmark (escenario memoria secundaria, estilo Chen)
// - Usa los mismos datasets, selectividades y Ks que los otros índices
// - Formato JSON compatible con DIndex / OmniR-tree / EGNAT

#include "mbpt.hpp"
#include "../../objectdb.hpp"
#include "../../datasets/paths.hpp"

#include <bits/stdc++.h>
#include <filesystem>

using namespace std;

// Selectividades del paper (Chen 2022)
static const vector<double> SELECTIVITIES = {0.02, 0.04, 0.08, 0.16, 0.32};

// Ks para MkNN
static const vector<int> K_VALUES = {5, 10, 20, 50, 100};

// Datasets del entorno experimental
static const vector<string> DATASETS = {"LA", "Words", "Color", "Synthetic"};

// Parámetros por defecto según el survey (puedes ajustar rho si quieres afinar)
static constexpr double MBPT_RHO = 0.1;

int main() {
    srand(12345);

    std::filesystem::create_directories("results");

    for (const string& dataset : DATASETS) {

        // ---------------------------------------------------------------
        // Cargar dataset
        // ---------------------------------------------------------------
        string dbfile = path_dataset(dataset);
        if (dbfile == "") {
            cerr << "[WARN] Dataset no encontrado: " << dataset << "\n";
            continue;
        }

        unique_ptr<ObjectDB> db;

        if (dataset == "LA")
            db = make_unique<VectorDB>(dbfile, 2);
        else if (dataset == "Color")
            db = make_unique<VectorDB>(dbfile, 1);
        else if (dataset == "Synthetic")
            db = make_unique<VectorDB>(dbfile, 999999);
        else if (dataset == "Words")
            db = make_unique<StringDB>(dbfile);
        else {
            cerr << "[WARN] Dataset no soportado: " << dataset << "\n";
            continue;
        }

        cerr << "\n==========================================\n";
        cerr << "[MB+-tree] Dataset: " << dataset
             << "   N=" << db->size() << "\n";
        cerr << "==========================================\n";

        // ---------------------------------------------------------------
        // Cargar queries y radios
        // ---------------------------------------------------------------
        vector<int> queries = load_queries_file(path_queries(dataset));
        auto radii          = load_radii_file(path_radii(dataset));

        if (queries.empty()) {
            cerr << "[WARN] No hay queries para " << dataset << "\n";
            continue;
        }

        cerr << "[QUERIES] " << queries.size() << " queries cargadas\n";
        cerr << "[QUERIES] Radii para " << radii.size() << " selectividades\n";

        // ---------------------------------------------------------------
        // Construir MB+-tree (memoria secundaria)
        // ---------------------------------------------------------------
        cerr << "\n[BUILD] Construyendo MB+-tree con rho=" << MBPT_RHO << "...\n";

        MBPT_Disk mbpt(db.get(), MBPT_RHO);

        auto t0 = chrono::high_resolution_clock::now();
        mbpt.build("index_mbpt_" + dataset);
        auto t1 = chrono::high_resolution_clock::now();
        auto buildTime =
            chrono::duration_cast<chrono::milliseconds>(t1 - t0).count();

        cerr << "[BUILD] Tiempo: " << buildTime << " ms\n";

        // ---------------------------------------------------------------
        // Archivo JSON de salida
        // ---------------------------------------------------------------
        string jsonOut = "results/results_MBPT_" + dataset + ".json";
        ofstream J(jsonOut);
        J << "[\n";
        bool firstOutput = true;

        // ===========================================================
        // EXPERIMENTO 1: Variar selectividad en MRQ
        // ===========================================================
        cerr << "\n========================================\n";
        cerr << "[EXP 1] Variando SELECTIVIDAD en MRQ\n";
        cerr << "========================================\n";

        for (double sel : SELECTIVITIES) {
            if (!radii.count(sel)) {
                cerr << "  [SKIP] Selectividad " << sel
                     << " no disponible para " << dataset << "\n";
                continue;
            }

            double R = radii[sel];
            long long totalD = 0;
            long long totalT = 0;
            long long totalPages = 0;

            cerr << "  sel=" << sel << " (R=" << R << ")... " << flush;

            for (int q : queries) {
                vector<int> out;
                mbpt.clear_counters();

                auto qt0 = chrono::high_resolution_clock::now();
                mbpt.rangeSearch(q, R, out);
                auto qt1 = chrono::high_resolution_clock::now();

                totalD     += mbpt.get_compDist();   // incluye TODAS las distancias
                totalT     += chrono::duration_cast<
                                  chrono::microseconds>(qt1 - qt0).count();
                totalPages += mbpt.get_pageReads();
            }

            double avgD   = double(totalD)     / queries.size();
            double avgTms = double(totalT)     / (1000.0 * queries.size());
            double avgPA  = double(totalPages) / queries.size();

            if (!firstOutput) J << ",\n";
            firstOutput = false;

            J << fixed << setprecision(6)
              << "{"
              << "\"index\":\"MB+-tree\","
              << "\"dataset\":\"" << dataset << "\","
              << "\"category\":\"DM\","          // Distance Mapping
              << "\"num_pivots\":null,"
              << "\"num_centers_path\":null,"
              << "\"arity\":null,"
              << "\"query_type\":\"MRQ\","
              << "\"selectivity\":" << sel << ","
              << "\"radius\":" << R << ","
              << "\"k\":null,"
              << "\"compdists\":" << avgD << ","
              << "\"time_ms\":" << avgTms << ","
              << "\"pages\":" << avgPA << ","
              << "\"n_queries\":" << queries.size() << ","
              << "\"run_id\":1"
              << "}";

            cerr << "OK (avg " << (long long)avgD
                 << " compdists, " << avgPA << " páginas)\n";
        }

        // ===========================================================
        // EXPERIMENTO 2: Variar k en MkNN
        // ===========================================================
        cerr << "\n========================================\n";
        cerr << "[EXP 2] Variando K en MkNN\n";
        cerr << "========================================\n";

        for (int k : K_VALUES) {
            long long totalD = 0;
            long long totalT = 0;
            long long totalPages = 0;

            cerr << "  k=" << k << "... " << flush;

            for (int q : queries) {
                vector<pair<double,int>> out;
                mbpt.clear_counters();

                auto qt0 = chrono::high_resolution_clock::now();
                mbpt.knnSearch(q, k, out);
                auto qt1 = chrono::high_resolution_clock::now();

                totalD     += mbpt.get_compDist();
                totalT     += chrono::duration_cast<
                                  chrono::microseconds>(qt1 - qt0).count();
                totalPages += mbpt.get_pageReads();
            }

            double avgD   = double(totalD)     / queries.size();
            double avgTms = double(totalT)     / (1000.0 * queries.size());
            double avgPA  = double(totalPages) / queries.size();

            if (!firstOutput) J << ",\n";
            firstOutput = false;

            J << fixed << setprecision(6)
              << "{"
              << "\"index\":\"MB+-tree\","
              << "\"dataset\":\"" << dataset << "\","
              << "\"category\":\"DM\","
              << "\"num_pivots\":null,"
              << "\"num_centers_path\":null,"
              << "\"arity\":null,"
              << "\"query_type\":\"MkNN\","
              << "\"selectivity\":null,"
              << "\"radius\":null,"
              << "\"k\":" << k << ","
              << "\"compdists\":" << avgD << ","
              << "\"time_ms\":" << avgTms << ","
              << "\"pages\":" << avgPA << ","
              << "\"n_queries\":" << queries.size() << ","
              << "\"run_id\":1"
              << "}";

            cerr << "OK (avg " << (long long)avgD
                 << " compdists, " << avgPA << " páginas)\n";
        }

        J << "\n]\n";
        J.close();

        cerr << "\n[DONE] Archivo generado: " << jsonOut << "\n";
        cerr << "==========================================\n";
    }

    cerr << "\n\n##########################################\n";
    cerr << "### TODAS LAS PRUEBAS COMPLETADAS\n";
    cerr << "##########################################\n";

    return 0;
}
