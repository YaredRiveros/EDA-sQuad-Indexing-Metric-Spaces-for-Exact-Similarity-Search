#include <bits/stdc++.h>
#include <nlohmann/json.hpp>
#include <filesystem>
using namespace std;
using json = nlohmann::json;


double L2(const vector<double>& a, const vector<double>& b) {
    double sum = 0;
    for (size_t i = 0; i < a.size(); i++) {
        double d = a[i] - b[i];
        sum += d * d;
    }
    return sqrt(sum);
}

double L1(const vector<double>& a, const vector<double>& b) {
    double sum = 0;
    for (size_t i = 0; i < a.size(); i++) sum += fabs(a[i] - b[i]);
    return sum;
}

double L_inf(const vector<double>& a, const vector<double>& b) {
    double best = 0;
    for (size_t i = 0; i < a.size(); i++)
        best = max(best, fabs(a[i] - b[i]));
    return best;
}

int edit_distance(const string& a, const string& b) {
    int n = a.size(), m = b.size();
    vector<vector<int>> dp(n+1, vector<int>(m+1));

    for (int i = 0; i <= n; i++) dp[i][0] = i;
    for (int j = 0; j <= m; j++) dp[0][j] = j;

    for (int i = 1; i <= n; i++) {
        for (int j = 1; j <= m; j++) {
            int cost = (a[i-1] == b[j-1] ? 0 : 1);
            dp[i][j] = min({
                dp[i-1][j] + 1,
                dp[i][j-1] + 1,
                dp[i-1][j-1] + cost
            });
        }
    }
    return dp[n][m];
}


vector<vector<double>> load_LA_or_Synthetic(const string& path) {
    ifstream fin(path);
    string head;
    getline(fin, head);
    stringstream ss(head);
    int dim;
    ss >> dim;

    vector<vector<double>> data;
    string line;
    while (getline(fin, line)) {
        stringstream s(line);
        vector<double> row(dim);
        for (int i = 0; i < dim; i++) {
            if (!(s >> row[i])) goto skip_line;
        }
        data.push_back(row);
        continue;

        skip_line: ;
    }
    return data;
}

vector<vector<double>> load_Color(const string& path) {
    ifstream fin(path);
    string head;
    getline(fin, head);

    stringstream ss(head);
    int dim;
    ss >> dim;

    vector<vector<double>> rows;
    string line;
    while (getline(fin, line)) {
        stringstream s(line);
        vector<double> row;
        double x;
        while (s >> x) row.push_back(x);
        if ((int)row.size() == dim) rows.push_back(row);
    }
    return rows;
}

vector<string> load_Words(const string& path) {
    vector<string> words;
    ifstream fin(path);
    string line;
    while (getline(fin, line)) {
        if (!line.empty())
            words.push_back(line);
    }
    return words;
}


 
template<typename T>
vector<int> select_pivots_HFI( // HFI: selección de pivotes
    const vector<T>& data,
    int num_pivots,
    function<double(const T&, const T&)> metric
) {
    int N = data.size();
    vector<int> pivots;
    pivots.push_back(0);

    vector<double> score(N, 0.0);

    for (int k = 1; k < num_pivots; k++) {
        int last = pivots.back();

        vector<double> d(N);
        for (int i = 0; i < N; i++)
            d[i] = metric(data[last], data[i]);

        for (int i = 0; i < N; i++)
            score[i] += d[i];

        int next = max_element(score.begin(), score.end()) - score.begin();

        if (find(pivots.begin(), pivots.end(), next) != pivots.end())
            next = max_element(d.begin(), d.end()) - d.begin();

        pivots.push_back(next);
    }

    return pivots;
}

vector<int> select_queries(int N, int count) {
    vector<int> all(N);
    iota(all.begin(), all.end(), 0);

    std::mt19937 rng(12345);
    std::shuffle(all.begin(), all.end(), rng);

    all.resize(count);
    return all;
}


template<typename T>
json compute_radii(
    const vector<T>& data,
    const vector<int>& queries,
    function<double(const T&, const T&)> metric,
    const vector<double>& selectivities
) {
    int N = data.size();
    json radii;

    for (double s : selectivities) {
        vector<double> rad_list;

        for (int q : queries) {
            vector<double> dist(N);
            for (int i = 0; i < N; i++)
                dist[i] = metric(data[q], data[i]);

            sort(dist.begin(), dist.end());
            int idx = int(s * 100);
            double rad = dist[(idx * N) / 100];
            rad_list.push_back(rad);
        }

        double avg = accumulate(rad_list.begin(), rad_list.end(), 0.0)
                   / rad_list.size();

        radii[to_string(s)] = avg;
    }

    return radii;
}


void process_dataset(const string& name, const string& path) {
    cout << "\n=== Procesando " << name << " ===\n";

    vector<double> selectivities = {0.02, 0.04, 0.08, 0.16, 0.32};

    filesystem::create_directories("prepared_experiment/pivots2k");
    filesystem::create_directories("prepared_experiment/queries2k");
    filesystem::create_directories("prepared_experiment/radii2k");

    if (name == "Words") {

        auto data = load_Words(path);
        int N = data.size();

        std::function<double(const string&, const string&)> metric =
            [&](const string& a, const string& b){
                return double(edit_distance(a, b));
            };

        auto queries = select_queries(N, 100);
        json jq = queries;
        ofstream("prepared_experiment/queries2k/" + name + "_queries.json")
            << jq.dump(2);

        auto jr = compute_radii(data, queries, metric, selectivities);
        ofstream("prepared_experiment/radii2k/" + name + "_radii.json")
            << jr.dump(2);

        for (int p : {3,5,10,15,20}) {
            auto piv = select_pivots_HFI<string>(data, p, metric);
            json jp = piv;
            ofstream("prepared_experiment/pivots2k/" + name + "_pivots_" +
                     to_string(p) + ".json")
                << jp.dump(2);
        }

        return;
    }


    vector<vector<double>> data;

    if (name == "LA" || name == "Synthetic") 
        data = load_LA_or_Synthetic(path);
    else if (name == "Color")
        data = load_Color(path);
    else { 
        cout << "Dataset desconocido!\n"; 
        return; 
    }

    int N = data.size();

    std::function<double(const vector<double>&, const vector<double>&)> metric;

    if (name == "LA")          metric = L2;
    else if (name == "Color")  metric = L1;
    else if (name == "Synthetic") metric = L_inf;

    auto queries = select_queries(N, 100);
    json jq = queries;
    ofstream("prepared_experiment/queries2k/" + name + "_queries.json")
        << jq.dump(2);

    auto jr = compute_radii(data, queries, metric, selectivities);
    ofstream("prepared_experiment/radii2k/" + name + "_radii.json")
        << jr.dump(2);

    for (int p : {3,5,10,15,20}) {
        auto piv = select_pivots_HFI<vector<double>>(data, p, metric);
        json jp = piv;
        ofstream("prepared_experiment/pivots2k/" + name + "_pivots_" +
                 to_string(p) + ".json")
            << jp.dump(2);
    }

    cout << "✔ Dataset " << name << " procesado.\n";
}


int main() {
    unordered_map<string,string> PATHS = {
        {"LA", "../LA_2k.txt"},
        {"Words", "../Words_2k.txt"},
        {"Synthetic", "../Synthetic_2k.txt"},
        {"Color", "../Color_2k.txt"}
    };

    for (auto& kv : PATHS) {
        if (filesystem::exists(kv.second))
            process_dataset(kv.first, kv.second);
        else
            cout << "⚠ Dataset " << kv.first 
                 << " no encontrado → skipping (" << kv.second << ")\n";
    }

    return 0;
}