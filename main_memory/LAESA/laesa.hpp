#ifndef LAESA_HPP
#define LAESA_HPP

#include "../../objectdb.hpp"
#include <vector>
#include <queue>
#include <algorithm>
#include <cmath>
#include <limits>
using namespace std;

int compdists = 0;
int compdistsBuild = 0;

struct ResultElem {
    int id;
    double dist;
    bool operator<(const ResultElem &o) const { return dist < o.dist; }
};

class LAESA {
    ObjectDB *db;
    int nPivots;            // Number of pivots
    vector<int> pivots;     // IDs of the pivots (first nPivots objects)
    vector<vector<double>> distMatrix;  // Precomputed distance matrix [nObjects x nPivots]
                                        // distMatrix[i][j] = distance from object i to pivot j

public:
    LAESA(ObjectDB *db, int nPivots);
    
    void overridePivots(const vector<int>& newPivots) {
        if ((int)newPivots.size() != nPivots) return;

        pivots = newPivots;

        int n = db->size();
        distMatrix.assign(n, vector<double>(nPivots));

        for (int i = 0; i < n; i++) {
            for (int j = 0; j < nPivots; j++) {
                distMatrix[i][j] = db->distance(i, pivots[j]);
                compdistsBuild++;
            }
        }
    }

    void rangeSearch(int queryId, double radius, vector<int> &result) const;

    void knnSearch(int queryId, int k, vector<ResultElem> &out) const;

private:
    double lowerBound(const vector<double> &queryDists, int objectIdx) const;
};

LAESA::LAESA(ObjectDB *db, int nPivots) 
    : db(db), nPivots(nPivots) {
    int n = db->size();
    if (nPivots > n) nPivots = n;
    this->nPivots = nPivots;

    pivots.resize(nPivots);
    for (int i = 0; i < nPivots; i++) {
        pivots[i] = i;
    }

    distMatrix.resize(n, vector<double>(nPivots));
    for (int i = 0; i < n; i++) {
        for (int j = 0; j < nPivots; j++) {
            distMatrix[i][j] = db->distance(i, pivots[j]);
            compdistsBuild++;
        }
    }

    cerr << "[LAESA] Index built with " << nPivots << " pivots ("
         << (n * nPivots * sizeof(double)) / (1024.0 * 1024.0)
         << " MB precalculated distances)\n";
}

double LAESA::lowerBound(const vector<double> &queryDists, int objectIdx) const 
{
    double maxDiff = 0.0;
    for (int j = 0; j < nPivots; j++) {
        double diff = fabs(queryDists[j] - distMatrix[objectIdx][j]);
        maxDiff = max(maxDiff, diff);
    }
    return maxDiff;
}

void LAESA::rangeSearch(int queryId, double radius, vector<int> &result) const 
{
    int n = db->size();
    //compdists = compdistsBuild;  // Start counting from precomputed distances
    compdists = 0;

    vector<double> queryDists(nPivots);
    for (int j = 0; j < nPivots; j++) {
        queryDists[j] = db->distance(queryId, pivots[j]);
        compdists++;
        // Check if the pivot is within range
        if (queryDists[j] <= radius) {
            result.push_back(pivots[j]);
        }
    }

    for (int i = 0; i < n; i++) {
        // Skip if it's a pivot (already checked)
        bool isPivot = false;
        for (int p : pivots) {
            if (i == p) {
                isPivot = true;
                break;
            }
        }
        if (isPivot) continue;

        // Use lower bound to filter
        double lb = lowerBound(queryDists, i);
        if (lb <= radius) {
            // The object may be in range, calculate actual distance
            double d = db->distance(queryId, i);
            compdists++;
            if (d <= radius) {
                result.push_back(i);
            }
        }
        // If lb > radius, the object is pruned
    }
}

void LAESA::knnSearch(int queryId, int k, vector<ResultElem> &out) const 
{
    int n = db->size();
    priority_queue<ResultElem> pq;  // Max-heap to maintain top k results
    
    //compdists = compdistsBuild;  // Start counting from precomputed distances
    compdists = 0;

    vector<double> queryDists(nPivots);
    for (int j = 0; j < nPivots; j++) {
        queryDists[j] = db->distance(queryId, pivots[j]);
        compdists++;
        if ((int)pq.size() < k) {
            pq.push({pivots[j], queryDists[j]});
        } else if (queryDists[j] < pq.top().dist) {
            pq.pop();
            pq.push({pivots[j], queryDists[j]});
        }
    }

    double tau = pq.size() == k ? pq.top().dist : numeric_limits<double>::infinity();

    vector<pair<double, int>> candidates;  // (L1 distance, object ID)

    for (int i = 0; i < n; i++) {
        bool isPivot = false;
        for (int p : pivots) {
            if (i == p) {
                isPivot = true;
                break;
            }
        }
        if (isPivot) continue;

        // Calculate L1 distance as proximity heuristic
        double dist1 = 0.0;
        for (int j = 0; j < nPivots; j++) {
            dist1 += fabs(queryDists[j] - distMatrix[i][j]);
        }
        candidates.push_back({dist1, i});
    }

    sort(candidates.begin(), candidates.end());

    for (const auto &cand : candidates) {
        int i = cand.second;
        
        // Use lower bound to filter
        double lb = lowerBound(queryDists, i);
        if (lb <= tau || (int)pq.size() < k) {
            // Calculate actual distance
            double d = db->distance(queryId, i);
            compdists++;
            if ((int)pq.size() < k) {
                pq.push({i, d});
                if ((int)pq.size() == k) tau = pq.top().dist;
            } else if (d < pq.top().dist) {
                pq.pop();
                pq.push({i, d});
                tau = pq.top().dist;
            }
        }
        // If lb > tau, the object is pruned
    }

    while (!pq.empty()) {
        out.push_back(pq.top());
        pq.pop();
    }
    reverse(out.begin(), out.end());
}

int getCompDists() {
    return compdists;
}

#endif
