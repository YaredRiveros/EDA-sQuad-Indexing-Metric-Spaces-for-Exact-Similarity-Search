#include <bits/stdc++.h>
#include "../../objectdb.hpp"
#include "mindex.hpp"

using namespace std;

// Función para guardar JSON
void save_json(const string& filename, const vector<map<string, string>>& data) {
    filesystem::create_directories("results");
    ofstream file("results/" + filename);
    file << "[\n";
    for (size_t i = 0; i < data.size(); i++) {
        file << "{";
        bool first = true;
        for (const auto& [key, val] : data[i]) {
            if (!first) file << ",";
            file << "\"" << key << "\":";
            if (val == "null" || val.find_first_not_of("0123456789.-") == string::npos) {
                file << val;
            } else {
                file << "\"" << val << "\"";
            }
            first = false;
        }
        file << "}";
        if (i < data.size() - 1) file << ",";
        file << "\n";
    }
    file << "]\n";
}

void test_dataset(const string& dataset_name, ObjectDB* db, int num_pivots) {
    MIndex_Improved idx(db, num_pivots);
    idx.build("midx_indexes/" + dataset_name + "_silent.midx");
    
    vector<map<string, string>> results;
    
    // Generate random query IDs
    vector<int> query_ids;
    int n_queries = min(100, (int)db->size());
    for (int i = 0; i < n_queries; i++) {
        query_ids.push_back(rand() % db->size());
    }
    
    // Range queries
    vector<double> selectivities = {0.0001, 0.001, 0.01, 0.02, 0.05};
    for (double sel : selectivities) {
        // Calculate selectivity radius
        double r = 0.0;
        if (dataset_name == "LA") {
            if (sel == 0.02) r = 408.82;
            else continue; // Skip other selectivities
        } else if (dataset_name == "Synthetic") {
            if (sel == 0.02) r = 2740.21;
            else continue;
        } else if (dataset_name == "Words") {
            if (sel == 0.02) r = 7.37;
            else continue;
        } else {
            continue;
        }
        
        double total_compdists = 0, total_time = 0, total_pages = 0;
        
        for (int qid : query_ids) {
            idx.clear_counters();
            vector<int> res;
            auto start = chrono::high_resolution_clock::now();
            idx.rangeSearch(qid, r, res);
            auto end = chrono::high_resolution_clock::now();
            
            total_compdists += idx.get_compDist();
            total_time += chrono::duration<double, milli>(end - start).count();
            total_pages += idx.get_pageReads();
        }
        
        map<string, string> entry;
        entry["index"] = "MIndex*_Silent";
        entry["dataset"] = dataset_name;
        entry["category"] = "DM";
        entry["num_pivots"] = to_string(num_pivots);
        entry["num_centers_path"] = "null";
        entry["arity"] = "null";
        entry["query_type"] = "MRQ";
        entry["selectivity"] = to_string(sel);
        entry["radius"] = to_string(r);
        entry["k"] = "null";
        entry["compdists"] = to_string(total_compdists / query_ids.size());
        entry["time_ms"] = to_string(total_time / query_ids.size());
        entry["pages"] = to_string(total_pages / query_ids.size());
        entry["n_queries"] = to_string(query_ids.size());
        entry["run_id"] = "1";
        
        results.push_back(entry);
    }
    
    // k-NN queries
    vector<int> k_values = {1, 5, 10, 20, 50};
    for (int k : k_values) {
        double total_compdists = 0, total_time = 0, total_pages = 0;
        
        for (int qid : query_ids) {
            idx.clear_counters();
            vector<pair<double,int>> res;
            auto start = chrono::high_resolution_clock::now();
            idx.knnSearch(qid, k, res);
            auto end = chrono::high_resolution_clock::now();
            
            total_compdists += idx.get_compDist();
            total_time += chrono::duration<double, milli>(end - start).count();
            total_pages += idx.get_pageReads();
        }
        
        map<string, string> entry;
        entry["index"] = "MIndex*_Silent";
        entry["dataset"] = dataset_name;
        entry["category"] = "DM";
        entry["num_pivots"] = to_string(num_pivots);
        entry["num_centers_path"] = "null";
        entry["arity"] = "null";
        entry["query_type"] = "MkNN";
        entry["selectivity"] = "null";
        entry["radius"] = "null";
        entry["k"] = to_string(k);
        entry["compdists"] = to_string(total_compdists / query_ids.size());
        entry["time_ms"] = to_string(total_time / query_ids.size());
        entry["pages"] = to_string(total_pages / query_ids.size());
        entry["n_queries"] = to_string(query_ids.size());
        entry["run_id"] = "1";
        
        results.push_back(entry);
    }
    
    save_json("results_MIndex_Silent_" + dataset_name + ".json", results);
}

int main() {
    srand(42);
    
    try {
        // Test LA
        cerr << "Loading LA dataset..." << endl;
        VectorDB db_la("../../datasets/LA.txt", 2);
        cerr << "Testing LA..." << endl;
        test_dataset("LA", &db_la, 8);
        cerr << "LA completed." << endl;
        
        // Test Synthetic
        cerr << "Loading Synthetic dataset..." << endl;
        VectorDB db_syn("../../datasets/Synthetic.txt", 2);
        cerr << "Testing Synthetic..." << endl;
        test_dataset("Synthetic", &db_syn, 5);
        cerr << "Synthetic completed." << endl;
        
        // Test Words
        cerr << "Loading Words dataset..." << endl;
        StringDB db_words("../../datasets/Words.txt");
        cerr << "Testing Words..." << endl;
        test_dataset("Words", &db_words, 8);
        cerr << "Words completed." << endl;
        
        cerr << "All tests completed successfully!" << endl;
    } catch (exception& e) {
        cerr << "ERROR: " << e.what() << endl;
        return 1;
    }
    
    return 0;
}
