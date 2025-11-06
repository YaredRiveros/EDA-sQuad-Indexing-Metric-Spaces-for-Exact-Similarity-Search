#ifndef LAESA_HPP
#define LAESA_HPP

#include "../objectdb.hpp"
#include <vector>
#include <queue>
#include <algorithm>
#include <cmath>
#include <limits>
using namespace std;

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
    /**
     * Builds the LAESA index
     * 
     * @param db Pointer to the object database
     * @param nPivots Number of pivots to use
     *                Recommendation: between sqrt(n) and 0.1*n, where n = number of objects
     */
    LAESA(ObjectDB *db, int nPivots);

    /**
     * Finds all objects within distance <= radius from the query
     * 
     * @param queryId ID of the query object
     * @param radius Search radius
     * @param result Vector to store the IDs of found objects
     */
    void rangeSearch(int queryId, double radius, vector<int> &result) const;

    /**
     * k-Nearest Neighbors search
     * 
     * @param queryId ID of the query object
     * @param k NNumber of neighbors to find
     * @param out Vector to store the k neighbors (sorted by distance)
     */
    void knnSearch(int queryId, int k, vector<ResultElem> &out) const;

private:
    /**
     * Calculates the lower bound distance using the triangle inequality
     * 
     * For each pivot p: |d(q,p) - d(o,p)| <= d(q,o)
     * Returns the maximum of all differences (tightest lower bound)
     *
     * @param queryDists Distances from the query to the pivots
     * @param objectIdx Index of the candidate object
     * @return Lower bound of the distance d(query, object)
     */
    double lowerBound(const vector<double> &queryDists, int objectIdx) const;
};

LAESA::LAESA(ObjectDB *db, int nPivots) 
    : db(db), nPivots(nPivots) {
    int n = db->size();
    if (nPivots > n) nPivots = n;
    this->nPivots = nPivots;

    // 1. Select the first nPivots objects
    pivots.resize(nPivots);
    for (int i = 0; i < nPivots; i++) {
        pivots[i] = i;
    }

    // 2. Precompute distance matrix (n × nPivots)
    distMatrix.resize(n, vector<double>(nPivots));
    for (int i = 0; i < n; i++) {
        for (int j = 0; j < nPivots; j++) {
            distMatrix[i][j] = db->distance(i, pivots[j]);
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

    // 1. Calculate distances from the query to the pivots
    vector<double> queryDists(nPivots);
    for (int j = 0; j < nPivots; j++) {
        queryDists[j] = db->distance(queryId, pivots[j]);
        // Check if the pivot is within range
        if (queryDists[j] <= radius) {
            result.push_back(pivots[j]);
        }
    }

    // 2. Filter and check non-pivot objects
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

    // 1. Calculate distances from the query to the pivots and add them
    vector<double> queryDists(nPivots);
    for (int j = 0; j < nPivots; j++) {
        queryDists[j] = db->distance(queryId, pivots[j]);
        if ((int)pq.size() < k) {
            pq.push({pivots[j], queryDists[j]});
        } else if (queryDists[j] < pq.top().dist) {
            pq.pop();
            pq.push({pivots[j], queryDists[j]});
        }
    }

    double tau = pq.size() == k ? pq.top().dist : numeric_limits<double>::infinity();

    // 2. Calculate L1 distances to order candidates (heuristic)
    vector<pair<double, int>> candidates;  // (L1 distance, object ID)

    for (int i = 0; i < n; i++) {
        // Skip if it's a pivot
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

    // 3. Sort candidates by L1 distance (process the most promising first)
    sort(candidates.begin(), candidates.end());

    // 4. Process candidates in order with filtering
    for (const auto &cand : candidates) {
        int i = cand.second;
        
        // Use lower bound to filter
        double lb = lowerBound(queryDists, i);
        if (lb <= tau || (int)pq.size() < k) {
            // Calculate actual distance
            double d = db->distance(queryId, i);
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

    // 5. Extract results and sort by distance
    while (!pq.empty()) {
        out.push_back(pq.top());
        pq.pop();
    }
    reverse(out.begin(), out.end());
}

#endif
