// gnat.hpp
#ifndef GNAT_HPP
#define GNAT_HPP

#include <bits/stdc++.h>
#include "../../objectdb.hpp"

using namespace std;

extern int MaxHeight;

struct GNAT_node_t {
    vector<int> pivot;                
    vector<GNAT_node_t> children;     
    vector<vector<double>> min_dist, max_dist; // [i][j]: min/max dist pivote i -> subárbol j
    vector<int> bucket; 
    int num = 0; // number of objects inside the bucket
};

class GNAT_t {
    const ObjectDB* db;           
    GNAT_node_t root;             

    size_t max_pivot_cnt;
    size_t min_pivot_cnt;
    size_t avg_pivot_cnt;

    long long dist_call_cnt = 0;   

    // Wrapper de distancia 
    double dist(int x, int y) {
        ++dist_call_cnt;
        return db->distance(x, y);
    }

    void select(size_t& pivot_cnt, vector<int>& objects, GNAT_node_t* root);
    void _build(GNAT_node_t* root, vector<int> objects, size_t pivot_size, int h);
    void _rangeSearch(const GNAT_node_t* root, int query, double range, int& res_size);
    void _knnSearch(const GNAT_node_t* root, int query, int k,
                    priority_queue<double>& result, double& ave_r);

public:
    GNAT_t(const ObjectDB* db, size_t avg_pivot_cnt);

    void build();
    void rangeSearch(const vector<int>& queries, double range, int& result_size);
    void knnSearch(const vector<int>& queries, int k, double& ave_r);

    long long get_compDist() const { return dist_call_cnt; }
    void reset_compDist() { dist_call_cnt = 0; }
};


GNAT_t::GNAT_t(const ObjectDB* db_, size_t avg_pivot_cnt_)
    : db(db_),
      max_pivot_cnt(min(4 * avg_pivot_cnt_, (size_t)256)),
      min_pivot_cnt(2),
      avg_pivot_cnt(avg_pivot_cnt_) {}

void GNAT_t::build() {
    vector<int> objects;
    objects.reserve(db->size());
    for (int i = 0; i < db->size(); ++i) {
        objects.push_back(i);
    }

    // Shuffle reproducible
    std::mt19937 rng(42);
    std::shuffle(objects.begin(), objects.end(), rng);

    cout << "database size: " << objects.size() << endl;
    _build(&root, objects, avg_pivot_cnt, 1);
}

void GNAT_t::select(size_t& pivot_cnt, vector<int>& objects, GNAT_node_t* root) {
    size_t sample_cnt = min(pivot_cnt * 3, objects.size());
    vector<int> sample(sample_cnt);
    copy(objects.end() - sample_cnt, objects.end(), sample.begin());
    objects.resize(objects.size() - sample_cnt);

    vector<vector<double>> d(sample_cnt, vector<double>(sample_cnt));
    for (size_t i = 0; i < sample_cnt; ++i) {
        for (size_t j = i + 1; j < sample_cnt; ++j) {
            d[i][j] = d[j][i] = dist(sample[i], sample[j]);
        }
    }

    vector<size_t> pivot_pos(pivot_cnt);
    vector<bool> is_pivot(sample_cnt, false);
    for (size_t i = 0; i < sample_cnt; ++i) {
        d[i][i] = DBL_MAX;
    }

    // dist_other[i] = distancia mínima de sample[i] al resto (farthest-first)
    vector<double> dist_other(sample_cnt);
    for (size_t i = 0; i < sample_cnt; ++i) {
        dist_other[i] = *min_element(d[i].begin(), d[i].end());
    }
    auto p = max_element(dist_other.begin(), dist_other.end()) - dist_other.begin();
    pivot_pos[0] = p;
    is_pivot[p] = true;

    vector<double> dist_pivot(sample_cnt, DBL_MAX);
    for (size_t i = 0; i < sample_cnt; ++i) {
        d[i][i] = 0.0;
    }
    for (size_t i = 1; i < pivot_cnt; ++i) {
        for (size_t j = 0; j < sample_cnt; ++j) {
            dist_pivot[j] = min(dist_pivot[j], d[j][pivot_pos[i - 1]]);
        }
        for (p = 0; is_pivot[p]; ++p)
            /* avanzar hasta un no pivote */;
        for (auto j = p + 1; j < (int)sample_cnt; ++j) {
            if (dist_pivot[j] > dist_pivot[p] && !is_pivot[j]) {
                p = j;
            }
        }
        pivot_pos[i] = p;
        is_pivot[p] = true;
    }

    for (auto i : pivot_pos) {
        root->pivot.push_back(sample[i]);
    }
    for (size_t i = 0; i < sample_cnt; ++i) {
        if (!is_pivot[i]) {
            objects.push_back(sample[i]);
        }
    }
}

void GNAT_t::_build(GNAT_node_t* root, vector<int> objects, size_t pivot_cnt, int h) {
    if (objects.empty()) {
        return;
    }

    if (h < MaxHeight) {
        root->num = -1;
        auto& pivot    = root->pivot;
        auto& children = root->children;
        auto& max_dist = root->max_dist;
        auto& min_dist = root->min_dist;

        min_dist.resize(pivot_cnt, vector<double>(pivot_cnt, DBL_MAX));
        max_dist.resize(pivot_cnt, vector<double>(pivot_cnt, 0.0));
        select(pivot_cnt, objects, root);

        if (objects.empty()) {
            return;
        }

        vector<vector<int>> objs_children(pivot_cnt);
        for (int obj : objects) {
            vector<double> dist_pivot(pivot_cnt);
            for (size_t i = 0; i < pivot_cnt; ++i) {
                dist_pivot[i] = dist(obj, pivot[i]);
            }
            size_t closest_pivot =
                min_element(dist_pivot.begin(), dist_pivot.end()) - dist_pivot.begin();
            objs_children[closest_pivot].push_back(obj);
            for (size_t i = 0; i < pivot_cnt; ++i) {
                max_dist[i][closest_pivot] = max(max_dist[i][closest_pivot], dist_pivot[i]);
                min_dist[i][closest_pivot] = min(min_dist[i][closest_pivot], dist_pivot[i]);
            }
        }

        children.resize(pivot_cnt);
        for (size_t i = 0; i < pivot_cnt; ++i) {
            size_t next_pivot_cnt =
                objs_children[i].empty()
                    ? 0
                    : objs_children[i].size() * avg_pivot_cnt * pivot_cnt / objects.size();
            next_pivot_cnt = max(min_pivot_cnt, next_pivot_cnt);
            next_pivot_cnt = min(max_pivot_cnt, next_pivot_cnt);
            _build(&children[i],
                   objs_children[i],
                   min(next_pivot_cnt, objs_children[i].size()),
                   h + 1);
        }
    } else {
        root->num = (int)objects.size();
        root->bucket.assign(objects.begin(), objects.end());
    }
}

void GNAT_t::_rangeSearch(const GNAT_node_t* root, int query, double range, int& res_size) {
    if (root->num < 0) {
        auto& pivot    = root->pivot;
        auto& children = root->children;
        auto& max_dist = root->max_dist;
        auto& min_dist = root->min_dist;
        size_t n = pivot.size();

        vector<double> d(n);
        for (size_t i = 0; i < n; ++i) {
            d[i] = dist(pivot[i], query);
            if (d[i] <= range) {
                ++res_size;
            }
        }

        for (size_t i = 0; i < n; ++i) {
            bool ok = true;
            for (size_t j = 0; ok && j < n; ++j) {
                ok &= (max_dist[j][i] >= d[j] - range);
                ok &= (min_dist[j][i] <= d[j] + range);
            }
            if (ok) {
                _rangeSearch(&children[i], query, range, res_size);
            }
        }
    } else {
        for (int id : root->bucket) {
            double d = dist(query, id);
            if (d <= range) ++res_size;
        }
    }
}

void GNAT_t::rangeSearch(const vector<int>& queries, double range, int& res_size) {
    for (int q : queries) {
        _rangeSearch(&root, q, range, res_size);
    }
}

static void addResult(int k, double d,
                      priority_queue<double>& result, double& r) {
    if ((int)result.size() < k || d < result.top()) {
        result.push(d);
    }
    if ((int)result.size() > k) {
        result.pop();
    }
    r = result.top();
}

void GNAT_t::_knnSearch(const GNAT_node_t* root, int query, int k,
                        priority_queue<double>& result, double& ave_r) {
    if (root->num < 0) {
        auto& pivot    = root->pivot;
        auto& children = root->children;
        auto& max_dist = root->max_dist;

        vector<pair<double, int>> od(pivot.size());
        for (int i = 0; i < (int)pivot.size(); i++) {
            od[i].first  = dist(query, pivot[i]);
            od[i].second = i;
            addResult(k, od[i].first, result, ave_r);
        }
        if (children.empty())
            return;

        sort(od.begin(), od.end());
        for (int i = 0; i < (int)pivot.size(); i++) {
            if ((int)result.size() == k &&
                ((od[i].first - od[0].first) / 2.0) > result.top())
                break;
            if (result.empty() ||
                (od[i].first - result.top() <= max_dist[od[i].second][od[i].second])) {
                _knnSearch(&children[od[i].second], query, k, result, ave_r);
            }
        }
    } else {
        for (int id : root->bucket) {
            double d = dist(query, id);
            addResult(k, d, result, ave_r);
        }
    }
}

void GNAT_t::knnSearch(const vector<int>& queries, int k, double& ave_r) {
    double r = 0.0;
    for (int q : queries) {
        priority_queue<double> result;
        _knnSearch(&root, q, k, result, r);
        ave_r += r;
    }
}

#endif // GNAT_HPP
