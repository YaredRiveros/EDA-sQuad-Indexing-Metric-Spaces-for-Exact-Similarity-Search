// test.cpp - MB+-tree benchmark con demos (similar a M-index_star)
#include "mbpt.hpp"
#include "../../objectdb.hpp"
#include <bits/stdc++.h>

using namespace std;

struct TestResult {
    string index_name;
    string dataset;
    string category;  // "Range" o "kNN"
    int num_pivots;
    string query_type;
    double selectivity_or_k;
    long long comp_dists;
    double time_ms;
    long long pages;
    int n_queries;
    int run_id;
};

void writeJSON(const vector<TestResult>& results, const string& filename) {
    ofstream out(filename);
    out << "[\n";
    for (size_t i = 0; i < results.size(); i++) {
        const auto& r = results[i];
        out << "  {\n";
        out << "    \"index\": \"" << r.index_name << "\",\n";
        out << "    \"dataset\": \"" << r.dataset << "\",\n";
        out << "    \"category\": \"" << r.category << "\",\n";
        out << "    \"num_pivots\": " << r.num_pivots << ",\n";
        out << "    \"query_type\": \"" << r.query_type << "\",\n";
        out << "    \"" << (r.category == "Range" ? "selectivity" : "k") << "\": " << r.selectivity_or_k << ",\n";
        out << "    \"compDist\": " << r.comp_dists << ",\n";
        out << "    \"time_ms\": " << r.time_ms << ",\n";
        out << "    \"pages\": " << r.pages << ",\n";
        out << "    \"n_queries\": " << r.n_queries << ",\n";
        out << "    \"run_id\": " << r.run_id << "\n";
        out << "  }" << (i + 1 < results.size() ? "," : "") << "\n";
    }
    out << "]\n";
}

int main() {
    cout << "====================================\n";
    cout << "MB+-tree Benchmark (Chen et al.)\n";
    cout << "====================================\n\n";

    const int NUM_QUERIES = 100;

    // Dataset LA
    {
        cout << "[1] Loading LA dataset...\n";
        vector<TestResult> results;
        
        VectorDB dbLA("../../datasets/LA.txt", 2);
        cout << "    LA: n=" << dbLA.size() << " (2D vectors, L2)\n";

        MBPT_Disk mbpt(&dbLA, 0.1);  // rho = 0.1
        mbpt.build("index_mbpt_la");
        
        vector<double> selectivities = {0.0001, 0.001, 0.01, 0.02, 0.05};

        // Range queries
        cout << "\n    Range queries:\n";
        for (double sel : selectivities) {
            double R = 408.82 * sqrt(sel / 0.02);
            mbpt.clear_counters();
            
            mt19937_64 rng(42);
            uniform_int_distribution<int> qdist(0, dbLA.size() - 1);
            
            for (int i = 0; i < NUM_QUERIES; i++) {
                int qId = qdist(rng);
                vector<int> res;
                mbpt.rangeSearch(qId, R, res);
            }
            
            TestResult tr;
            tr.index_name = "MB+-tree";
            tr.dataset = "LA";
            tr.category = "Range";
            tr.num_pivots = 0;
            tr.query_type = "MRQ";
            tr.selectivity_or_k = sel;
            tr.comp_dists = mbpt.get_compDist() / NUM_QUERIES;
            tr.time_ms = mbpt.get_queryTime() / (1000.0 * NUM_QUERIES);
            tr.pages = mbpt.get_pageReads() / NUM_QUERIES;
            tr.n_queries = NUM_QUERIES;
            tr.run_id = 1;
            results.push_back(tr);
            
            cout << "      sel=" << sel << " -> compDist=" << tr.comp_dists << " time=" << tr.time_ms << "ms\n";
        }
        
        writeJSON(results, "results/results_MBPT_LA.json");
        cout << "    LA results written!\n";
    }

    // Dataset Synthetic
    {
        cout << "\n[2] Loading Synthetic dataset...\n";
        vector<TestResult> results;
        
        VectorDB dbSyn("../../datasets/Synthetic.txt", 2);
        cout << "    Synthetic: n=" << dbSyn.size() << " (2D vectors, L2)\n";

        MBPT_Disk mbpt(&dbSyn, 0.1);
        mbpt.build("index_mbpt_synthetic");
        
        vector<double> selectivities = {0.0001, 0.001, 0.01, 0.02, 0.05};

        cout << "\n    Range queries:\n";
        for (double sel : selectivities) {
            double R = 2740.21 * sqrt(sel / 0.02);
            mbpt.clear_counters();
            
            mt19937_64 rng(42);
            uniform_int_distribution<int> qdist(0, dbSyn.size() - 1);
            
            for (int i = 0; i < NUM_QUERIES; i++) {
                int qId = qdist(rng);
                vector<int> res;
                mbpt.rangeSearch(qId, R, res);
            }
            
            TestResult tr;
            tr.index_name = "MB+-tree";
            tr.dataset = "Synthetic";
            tr.category = "Range";
            tr.num_pivots = 0;
            tr.query_type = "MRQ";
            tr.selectivity_or_k = sel;
            tr.comp_dists = mbpt.get_compDist() / NUM_QUERIES;
            tr.time_ms = mbpt.get_queryTime() / (1000.0 * NUM_QUERIES);
            tr.pages = mbpt.get_pageReads() / NUM_QUERIES;
            tr.n_queries = NUM_QUERIES;
            tr.run_id = 1;
            results.push_back(tr);
            
            cout << "      sel=" << sel << " -> compDist=" << tr.comp_dists << " time=" << tr.time_ms << "ms\n";
        }
        
        writeJSON(results, "results/results_MBPT_Synthetic.json");
        cout << "    Synthetic results written!\n";
    }

    // Dataset Words
    {
        cout << "\n[3] Loading Words dataset...\n";
        vector<TestResult> results;
        
        StringDB dbWords("../../datasets/Words.txt");
        cout << "    Words: n=" << dbWords.size() << " (strings, Levenshtein)\n";

        MBPT_Disk mbpt(&dbWords, 0.1);
        mbpt.build("index_mbpt_words");
        
        vector<double> selectivities = {0.0001, 0.001, 0.01, 0.02, 0.05};

        cout << "\n    Range queries:\n";
        for (double sel : selectivities) {
            double R = 7.37 * sqrt(sel / 0.02);
            mbpt.clear_counters();
            
            mt19937_64 rng(42);
            uniform_int_distribution<int> qdist(0, dbWords.size() - 1);
            
            for (int i = 0; i < NUM_QUERIES; i++) {
                int qId = qdist(rng);
                vector<int> res;
                mbpt.rangeSearch(qId, R, res);
            }
            
            TestResult tr;
            tr.index_name = "MB+-tree";
            tr.dataset = "Words";
            tr.category = "Range";
            tr.num_pivots = 0;
            tr.query_type = "MRQ";
            tr.selectivity_or_k = sel;
            tr.comp_dists = mbpt.get_compDist() / NUM_QUERIES;
            tr.time_ms = mbpt.get_queryTime() / (1000.0 * NUM_QUERIES);
            tr.pages = mbpt.get_pageReads() / NUM_QUERIES;
            tr.n_queries = NUM_QUERIES;
            tr.run_id = 1;
            results.push_back(tr);
            
            cout << "      sel=" << sel << " -> compDist=" << tr.comp_dists << " time=" << tr.time_ms << "ms\n";
        }
        
        writeJSON(results, "results/results_MBPT_Words.json");
        cout << "    Words results written!\n";
    }
    
    cout << "\n====================================\n";
    cout << "All results written to results/ directory\n";
    cout << "====================================\n";

    return 0;
}
