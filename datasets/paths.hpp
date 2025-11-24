#ifndef CONFIG_PATHS_HPP
#define CONFIG_PATHS_HPP

#include <bits/stdc++.h>
#include <sys/stat.h>
using namespace std;

// ------------------------------------------------------------
// Check if file exists
// ------------------------------------------------------------
inline bool file_exists(const string &path) {
    struct stat buffer;
    return (stat(path.c_str(), &buffer) == 0);
}

// ------------------------------------------------------------
// Try resolve relative paths in multiple common locations
// ------------------------------------------------------------
inline string resolve_path(const string &rel) {
    vector<string> candidates = {
        rel,
        "../" + rel,
        "../../" + rel,
        "../../../" + rel,
        "../../../../" + rel
    };

    for (const string &c : candidates)
        if (file_exists(c)) return c;

    return "";
}

// ------------------------------------------------------------
// Base prepared_experiment directory
// ------------------------------------------------------------
// root from each struct
static const string BASE_EXPERIMENT_DIR =
    "../../datasets/dataset_processing/prepared_experiment/";

// Subfolders
static const string QUERIES_DIR = BASE_EXPERIMENT_DIR + string("queries2k/");
static const string RADII_DIR   = BASE_EXPERIMENT_DIR + string("radii2k/");
static const string PIVOTS_DIR  = BASE_EXPERIMENT_DIR + string("pivots2k/");

// ------------------------------------------------------------
// DATASETS DIRECTORY (for LA.txt, Words.txt, etc.)
// ------------------------------------------------------------
// root from each struct
static const string DATASET_DIR = "../../datasets/";

// dataset file = datasets/<dataset>.txt
inline string path_dataset(const string &dataset) {
    string p = resolve_path(DATASET_DIR + dataset +"_2k"+ ".txt");
    if (p == "")
        cerr << "[WARN] Dataset file not found: " 
             << DATASET_DIR + dataset + ".txt" << "\n";
    return p;
}

// ------------------------------------------------------------
// Build paths (return empty string if missing)
// ------------------------------------------------------------
inline string path_queries(const string &dataset) {
    return resolve_path(QUERIES_DIR + dataset + "_queries.json");
}

inline string path_radii(const string &dataset) {
    return resolve_path(RADII_DIR + dataset + "_radii.json");
}

inline string path_pivots(const string &dataset, int centers) {
    return resolve_path(PIVOTS_DIR + dataset +
                        "_pivots_" + to_string(centers) + ".json");
}

// ------------------------------------------------------------
// JSON Loaders (safe)
// ------------------------------------------------------------
static vector<int> load_queries_file(const string& path) {
    vector<int> Q;

    if (path == "" || !file_exists(path)) {
        cerr << "[WARN] Queries file missing: " << path << "\n";
        return Q;
    }

    ifstream f(path);
    string token;

    while (f >> token) {
        token.erase(remove_if(token.begin(), token.end(),
                              [](char c){ return c=='['||c==']'||c==','; }),
                    token.end());
        if (!token.empty() && all_of(token.begin(), token.end(), ::isdigit))
            Q.push_back(stoi(token));
    }

    return Q;
}

static unordered_map<double,double> load_radii_file(const string& path) {
    unordered_map<double,double> R;

    if (path == "" || !file_exists(path)) {
        cerr << "[WARN] Radii file missing: " << path << "\n";
        return R;
    }

    ifstream f(path);
    string key, token;
    double val;

    while (f >> token) {
        if (token[0] == '"') {
            token.erase(remove(token.begin(), token.end(), '"'), token.end());
            key = token;
        }
        if (token.find(":") != string::npos) {
            f >> val;
            R[stod(key)] = val;
        }
    }

    return R;
}

#endif // CONFIG_PATHS_HPP
