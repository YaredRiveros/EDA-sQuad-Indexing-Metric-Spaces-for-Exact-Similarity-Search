// test_silent.cpp - MB+-tree benchmark silencioso (EGNAT-style)
#include "mbpt.hpp"
#include "../../objectdb.hpp"
#include <bits/stdc++.h>

using namespace std;

struct TestResult {
    string index_name;
    string dataset;
    string category;
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
    vector<TestResult> allResults;
    const int NUM_QUERIES = 100;
    const double SELECTIVITY = 0.02;

    // LA
    {
        VectorDB dbLA("../../datasets/LA.txt", 2);
        MBPT_Disk mbpt(&dbLA, 0.1);
        mbpt.build("index_mbpt_la_silent");
        
        double R = 408.82;  // Radio para selectivity 0.02
        mbpt.clear_counters();
        
        mt19937_64 rng(42);
        uniform_int_distribution<int> qdist(0, dbLA.size() - 1);
        
        for (int i = 0; i < NUM_QUERIES; i++) {
            int qId = qdist(rng);
            vector<int> res;
            mbpt.rangeSearch(qId, R, res);
        }
        
        TestResult tr;
        tr.index_name = "MB+-tree_Silent";
        tr.dataset = "LA";
        tr.category = "Range";
        tr.num_pivots = 0;
        tr.query_type = "MRQ";
        tr.selectivity_or_k = SELECTIVITY;
        tr.comp_dists = mbpt.get_compDist() / NUM_QUERIES;
        tr.time_ms = mbpt.get_queryTime() / (1000.0 * NUM_QUERIES);
        tr.pages = mbpt.get_pageReads() / NUM_QUERIES;
        tr.n_queries = NUM_QUERIES;
        tr.run_id = 1;
        allResults.push_back(tr);
    }

    // Synthetic
    {
        VectorDB dbSyn("../../datasets/Synthetic.txt", 2);
        MBPT_Disk mbpt(&dbSyn, 0.1);
        mbpt.build("index_mbpt_synthetic_silent");
        
        double R = 2740.21;
        mbpt.clear_counters();
        
        mt19937_64 rng(42);
        uniform_int_distribution<int> qdist(0, dbSyn.size() - 1);
        
        for (int i = 0; i < NUM_QUERIES; i++) {
            int qId = qdist(rng);
            vector<int> res;
            mbpt.rangeSearch(qId, R, res);
        }
        
        TestResult tr;
        tr.index_name = "MB+-tree_Silent";
        tr.dataset = "Synthetic";
        tr.category = "Range";
        tr.num_pivots = 0;
        tr.query_type = "MRQ";
        tr.selectivity_or_k = SELECTIVITY;
        tr.comp_dists = mbpt.get_compDist() / NUM_QUERIES;
        tr.time_ms = mbpt.get_queryTime() / (1000.0 * NUM_QUERIES);
        tr.pages = mbpt.get_pageReads() / NUM_QUERIES;
        tr.n_queries = NUM_QUERIES;
        tr.run_id = 1;
        allResults.push_back(tr);
    }

    // Words
    {
        StringDB dbWords("../../datasets/Words.txt");
        MBPT_Disk mbpt(&dbWords, 0.1);
        mbpt.build("index_mbpt_words_silent");
        
        double R = 7.37;
        mbpt.clear_counters();
        
        mt19937_64 rng(42);
        uniform_int_distribution<int> qdist(0, dbWords.size() - 1);
        
        for (int i = 0; i < NUM_QUERIES; i++) {
            int qId = qdist(rng);
            vector<int> res;
            mbpt.rangeSearch(qId, R, res);
        }
        
        TestResult tr;
        tr.index_name = "MB+-tree_Silent";
        tr.dataset = "Words";
        tr.category = "Range";
        tr.num_pivots = 0;
        tr.query_type = "MRQ";
        tr.selectivity_or_k = SELECTIVITY;
        tr.comp_dists = mbpt.get_compDist() / NUM_QUERIES;
        tr.time_ms = mbpt.get_queryTime() / (1000.0 * NUM_QUERIES);
        tr.pages = mbpt.get_pageReads() / NUM_QUERIES;
        tr.n_queries = NUM_QUERIES;
        tr.run_id = 1;
        allResults.push_back(tr);
    }

    writeJSON(allResults, "results/results_MBPT_Silent_All.json");

    return 0;
}
