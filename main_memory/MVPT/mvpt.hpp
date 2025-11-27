#ifndef MVP_HPP
#define MVP_HPP

#include "../../objectdb.hpp"
#include <vector>
#include <queue>
#include <algorithm>
#include <cmath>
#include <limits>
#include <numeric>
#include <unordered_set>
using namespace std;

// NOTE: compdists is global to measure distance computations (kept as in your original setup)
int compdists = 0;
int compdistsBuild = 0;

struct ResultElem {
    int id;
    double dist;
    bool operator<(const ResultElem &o) const { return dist < o.dist; }
};

struct ObjDist {
    int id;
    double dist;
    bool operator<(const ObjDist &o) const { return dist < o.dist; }
};

struct VPNode {
    bool isLeaf;
    vector<int> bucket;
    int pivot;                    // pivot ID (if internal)
    vector<double> radii;         // partition boundaries (size = arity)
    vector<VPNode*> children;     // children pointers

    VPNode() : isLeaf(false), pivot(-1) {}
    ~VPNode() {
        for (auto c : children) delete c;
    }
};

class MVPT {
    ObjectDB *db;
    VPNode *root;
    int bucketSize;
    int arity;

    int configuredHeight;   // if >0, force tree height = configuredHeight (num pivots)
    vector<int> pivotsPerLevel;         // if non-empty, pivotsPerLevel[level-1] is pivot for that level (1-based)

public:
    MVPT(ObjectDB *db, int bucketSize = 10, int arity = 2, int configuredHeight = 0, const vector<int>& pivotsPerLevel = {});
    ~MVPT() { delete root; }

    // Searches
    void rangeSearch(int queryId, double radius, vector<int> &result) const;
    void knnSearch(int queryId, int k, vector<ResultElem> &out) const;

    // Returns the configured number of pivots (if configured >0, returns it; otherwise returns actual tree height)
    int getConfiguredNumPivots() const { return (configuredHeight > 0 ? configuredHeight : getTreeHeight()); }
    // number of unique pivot IDs actually stored (diagnostic)
    int getNumUniquePivots() const { unordered_set<int> s; collectPivots(root, s); return (int)s.size(); }
    // actual tree height computed
    int getTreeHeight() const { return treeHeight(root); }

private:
    VPNode* build(vector<int> ids, int depth);
    void rangeSearch(VPNode *node, int queryId, double radius, vector<int> &result) const;
    void knnSearch(VPNode *node, int queryId, int k, priority_queue<ResultElem> &pq, double &tau) const;

    // helpers for pivot reporting
    int treeHeight(VPNode* node) const;
    void collectPivots(VPNode* node, unordered_set<int>& s) const;
};

MVPT::MVPT(ObjectDB *db, int bucketSize, int arity, int configuredHeight, const vector<int>& pivotsPerLevel)
    : db(db), root(nullptr), bucketSize(bucketSize), arity(arity),
      configuredHeight(configuredHeight), pivotsPerLevel(pivotsPerLevel)
{
    if (arity < 2) arity = 2;
    this->arity = arity;

    // ensure pivotsPerLevel size is not larger than configuredHeight if configured
    if (configuredHeight > 0 && !pivotsPerLevel.empty() && (int)pivotsPerLevel.size() < configuredHeight) {
        cerr << "[MVPT] Warning: pivotsPerLevel size < configuredHeight; remaining pivots will be chosen randomly.\n";
    }

    // build using all object ids
    vector<int> allIds(db->size());
    iota(allIds.begin(), allIds.end(), 0);

    root = build(allIds, 1);

    cerr << "[MVPT] Index built (bucketSize=" << bucketSize << ", arity=" << arity
         << ", configuredHeight=" << configuredHeight
         << ", pivotsProvided=" << (pivotsPerLevel.empty() ? 0 : (int)pivotsPerLevel.size()) << ")\n";
}

VPNode* MVPT::build(vector<int> ids, int depth)
{
    VPNode *node = new VPNode();

    // stop splitting when depth >= configuredHeight (so height == configuredHeight)
    if (configuredHeight > 0 && depth >= configuredHeight) {
        node->isLeaf = true;
        node->bucket = ids;
        return node;
    }

    // base: bucket-size small -> leaf
    if ((int)ids.size() <= bucketSize) {
        node->isLeaf = true;
        node->bucket = ids;
        return node;
    }

    // internal node: choose pivot
    node->isLeaf = false;
    int pivotId = -1;

    // If pivots per level provided and has an entry for this depth -> use it (levels are 1-based)
    if (!pivotsPerLevel.empty() && (depth - 1) < (int)pivotsPerLevel.size()) {
        pivotId = pivotsPerLevel[depth - 1];
        // If pivot is not present in current ids, we still keep it (it's allowed; distances computed for all ids)
    } else {
        // fallback: choose pivot randomly among current ids
        int pivotIdx = rand() % ids.size();
        pivotId = ids[pivotIdx];
    }
    node->pivot = pivotId;

    // Remove pivot from ids if present (so pivot isn't duplicated in children)
    auto it = find(ids.begin(), ids.end(), pivotId);
    if (it != ids.end()) ids.erase(it);

    // compute distances to pivot
    vector<ObjDist> objDists;
    objDists.reserve(ids.size());
    for (int id : ids) {
        double d = db->distance(id, node->pivot);
        compdistsBuild++;
        objDists.push_back({id, d});
    }

    // sort distances
    sort(objDists.begin(), objDists.end());

    // prepare node radii and children
    node->radii.resize(arity);
    node->children.resize(arity, nullptr);

    int n = (int)objDists.size();
    int perChild = n / arity;
    int remainder = n % arity;

    node->radii[0] = 0.0;
    for (int i = 1; i < arity; i++) {
        int idx = i * perChild + min(i, remainder) - 1;
        if (idx >= 0 && idx < n) node->radii[i] = objDists[idx].dist;
        else node->radii[i] = numeric_limits<double>::infinity();
    }

    int startIdx = 0;
    for (int i = 0; i < arity; i++) {
        int count = perChild + (i < remainder ? 1 : 0);
        int endIdx = min(startIdx + count, n);

        vector<int> childIds;
        childIds.reserve(max(0, endIdx - startIdx));
        for (int j = startIdx; j < endIdx; j++) childIds.push_back(objDists[j].id);

        if (!childIds.empty()) node->children[i] = build(childIds, depth + 1);
        else node->children[i] = nullptr;

        startIdx = endIdx;
    }

    compdists = compdistsBuild;
    return node;
}

void MVPT::rangeSearch(int queryId, double radius, vector<int> &result) const {
    rangeSearch(root, queryId, radius, result);
}

void MVPT::rangeSearch(VPNode *node, int queryId, double radius, vector<int> &result) const {
    if (!node) return;

    if (node->isLeaf) {
        for (int id : node->bucket) {
            double d = db->distance(queryId, id);
            compdists++;
            if (d <= radius) result.push_back(id);
        }
        return;
    }

    double distToPivot = db->distance(queryId, node->pivot);
    compdists++;
    if (distToPivot <= radius) result.push_back(node->pivot);

    for (int i = 0; i < arity; i++) {
        if (!node->children[i]) continue;
        double lowerBound = node->radii[i];
        double upperBound = (i + 1 < arity) ? node->radii[i + 1] : numeric_limits<double>::infinity();

        // pruning: check intersection of [distToPivot - radius, distToPivot + radius] with [lowerBound, upperBound]
        if (distToPivot - radius <= upperBound && distToPivot + radius >= lowerBound) {
            rangeSearch(node->children[i], queryId, radius, result);
        }
    }
}

void MVPT::knnSearch(int queryId, int k, vector<ResultElem> &out) const {
    priority_queue<ResultElem> pq;
    double tau = numeric_limits<double>::infinity();
    knnSearch(root, queryId, k, pq, tau);
    while (!pq.empty()) { out.push_back(pq.top()); pq.pop(); }
    reverse(out.begin(), out.end());
}

void MVPT::knnSearch(VPNode *node, int queryId, int k, priority_queue<ResultElem> &pq, double &tau) const {
    if (!node) return;

    if (node->isLeaf) {
        for (int id : node->bucket) {
            double d = db->distance(queryId, id);
            compdists++;
            if ((int)pq.size() < k) { pq.push({id, d}); if ((int)pq.size() == k) tau = pq.top().dist; }
            else if (d < pq.top().dist) { pq.pop(); pq.push({id, d}); tau = pq.top().dist; }
        }
        return;
    }

    double distToPivot = db->distance(queryId, node->pivot);
    compdists++;
    if ((int)pq.size() < k) { pq.push({node->pivot, distToPivot}); if ((int)pq.size() == k) tau = pq.top().dist; }
    else if (distToPivot < pq.top().dist) { pq.pop(); pq.push({node->pivot, distToPivot}); tau = pq.top().dist; }

    int closestChild = 0;
    for (int i = 1; i < arity; i++) {
        if (node->radii[i] > distToPivot) { closestChild = i - 1; break; }
        if (i == arity - 1) closestChild = i;
    }

    vector<int> order;
    order.push_back(closestChild);
    for (int d = 1; d < arity; d++) {
        if (closestChild - d >= 0) order.push_back(closestChild - d);
        if (closestChild + d < arity) order.push_back(closestChild + d);
    }

    for (int i : order) {
        if (!node->children[i]) continue;
        double lowerBound = node->radii[i];
        double upperBound = (i + 1 < arity) ? node->radii[i + 1] : numeric_limits<double>::infinity();

        if ((int)pq.size() < k || (distToPivot - tau <= upperBound && distToPivot + tau >= lowerBound)) {
            knnSearch(node->children[i], queryId, k, pq, tau);
        }
    }
}

// helpers
int MVPT::treeHeight(VPNode* node) const {
    if (!node) return 0;
    if (node->isLeaf) return 1;
    int h = 0;
    for (auto c : node->children) if (c) h = max(h, treeHeight(c));
    return 1 + h;
}

void MVPT::collectPivots(VPNode* node, unordered_set<int>& s) const {
    if (!node) return;
    if (!node->isLeaf && node->pivot >= 0) s.insert(node->pivot);
    for (auto c : node->children) if (c) collectPivots(c, s);
}

int getCompDists() { return compdists; }

#endif
