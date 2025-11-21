#ifndef MVP_HPP
#define MVP_HPP

#include "objectdb.hpp"
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

// Helper struct to asociate object-pivot distance
struct ObjDist {
    int id;
    double dist;
    bool operator<(const ObjDist &o) const { return dist < o.dist; }
};

// M-ary VP-Tree node
struct VPNode {
    bool isLeaf;

    // Leaf nodes
    vector<int> bucket;

    // Internal nodes
    int pivot;                  // pivot ID
    vector<double> radii;       // m radius values defining the ranges
    vector<VPNode*> children;   // m children

    VPNode() : isLeaf(false), pivot(-1) {}
    ~VPNode() {
        for (auto child : children)
            delete child;
    }
};

class MVPT {
    ObjectDB *db;           // Object database
    VPNode *root;           // Root of the tree
    int bucketSize;        // Maximum bucket size in leaf nodes
    int arity;             // Branching factor (number of children per internal node)

public:
    /**
     * Builds the M-ary VP-Tree index
     * 
     * @param db Pointer to the object database
     * @param bucketSize Maximum bucket size in leaf nodes
     * @param arity Branching factor (m children per internal node)
     *              Recommendation: 2-8 for balance between depth and branching
     */
    MVPT(ObjectDB *db, int bucketSize = 10, int arity = 2);
    
    ~MVPT() { delete root; }

    /**
     * Range search: finds all objects within distance <= radius from the query
     * 
     * @param queryId ID of the query object
     * @param radius Search radius
     * @param result Vector to store the IDs of found objects
     */
    void rangeSearch(int queryId, double radius, vector<int> &result) const;

    /**
     * k-NN search: finds the k nearest neighbors of the query object
     * 
     * @param queryId ID of the query object
     * @param k NNumber of neighbors to find
     * @param out Vector to store the k neighbors (sorted by distance)
     */
    void knnSearch(int queryId, int k, vector<ResultElem> &out) const;

private:
    /**
     * Recursive tree construction
     * 
     * @param ids Vector of object IDs to insert into this subtree
     * @return Constructed node (leaf or internal)
     */
    VPNode* build(vector<int> ids);

    /**
     * Recursive range search
     */
    void rangeSearch(VPNode *node, int queryId, double radius, vector<int> &result) const;

    /**
     * Recursive k-NN search
     */
    void knnSearch(VPNode *node, int queryId, int k, priority_queue<ResultElem> &pq, double &tau) const;
};

MVPT::MVPT(ObjectDB *db, int bucketSize, int arity)
    : db(db), root(nullptr), bucketSize(bucketSize), arity(arity)
{
    if (arity < 2) arity = 2;
    this->arity = arity;

    // Create vector with all IDs
    vector<int> allIds(db->size());
    iota(allIds.begin(), allIds.end(), 0);
    
    // Build tree recursively
    root = build(allIds);
    
    cerr << "[MVPT] Index built with bucketSize=" << bucketSize 
         << ", arity=" << arity << "\n";
}

VPNode* MVPT::build(vector<int> ids)
{
    VPNode *node = new VPNode();
    
    // Base case: create leaf node if there are few objects
    if ((int)ids.size() <= bucketSize) {
        node->isLeaf = true;
        node->bucket = ids;
        return node;
    }

    // Recursive case: create internal node
    node->isLeaf = false;

    // 1: Select random pivot
    int pivotIdx = rand() % ids.size();
    node->pivot = ids[pivotIdx];

    // Remove pivot from the set
    ids.erase(ids.begin() + pivotIdx);

    // 2: Compute distances from all objects to the pivot
    vector<ObjDist> objDists;
    for (int id : ids) {
        double d = db->distance(id, node->pivot);
        compdistsBuild++;
        objDists.push_back({id, d});
    }
    
    // 3: Sort objects by distance to pivot
    sort(objDists.begin(), objDists.end());

    // 4: Compute the m-1 median (radius) values that define the ranges
    // Distribute objects evenly into m groups
    node->radii.resize(arity);
    node->children.resize(arity);
    
    int n = objDists.size();
    int perChild = n / arity;  // Objects per child
    int remainder = n % arity; // Remaining objects

    // Compute the radii; defines the boundaries of each partition
    node->radii[0] = 0.0;  // First child: [0, radius_1)
    for (int i = 1; i < arity; i++) {
        int idx = i * perChild + min(i, remainder) - 1;
        if (idx >= 0 && idx < n) {
            node->radii[i] = objDists[idx].dist;
        } else {
            node->radii[i] = numeric_limits<double>::infinity();
        }
    }

    // 5: Partition objects into m groups and build subtrees
    int startIdx = 0;
    for (int i = 0; i < arity; i++) {
        int count = perChild + (i < remainder ? 1 : 0);
        int endIdx = min(startIdx + count, n);

        // Extract IDs for this child
        vector<int> childIds;
        for (int j = startIdx; j < endIdx; j++) {
            childIds.push_back(objDists[j].id);
        }

        // Recursive construction of the subtree
        if (!childIds.empty()) {
            node->children[i] = build(childIds);
        } else {
            node->children[i] = nullptr;
        }
        
        startIdx = endIdx;
    }

    
    compdists = compdistsBuild;  // Start counting from precomputed distances
    
    return node;
}

void MVPT::rangeSearch(int queryId, double radius, vector<int> &result) const
{
    rangeSearch(root, queryId, radius, result);
}

void MVPT::rangeSearch(VPNode *node, int queryId, double radius, vector<int> &result) const
{
    if (!node) 
        return;
    
    if (node->isLeaf) {
        // Leaf node: sequential search in the bucket
        for (int id : node->bucket) {
            double d = db->distance(queryId, id);
            compdists++;
            if (d <= radius) {
                result.push_back(id);
            }
        }
        return;
    }
    
    // Internal node: compute distance to pivot
    double distToPivot = db->distance(queryId, node->pivot);
    compdists++;
    
    // Check if pivot is in range
    if (distToPivot <= radius) {
        result.push_back(node->pivot);
    }

    // Prune subtrees using triangle inequality
    // Only visit child i if its range [radii[i], radii[i+1]) can contain results
    for (int i = 0; i < arity; i++) {
        if (!node->children[i]) continue;
        
        double lowerBound = node->radii[i];
        double upperBound = (i + 1 < arity) ? node->radii[i + 1] : numeric_limits<double>::infinity();
        
        // Use triangle inequality to prune:
        // If distToPivot - radius > upperBound: all objects are too far away
        // If distToPivot + radius < lowerBound: all objects are too close
        if (distToPivot - radius <= upperBound && distToPivot + radius >= lowerBound) {
            rangeSearch(node->children[i], queryId, radius, result);
        }
    }
}

void MVPT::knnSearch(int queryId, int k, vector<ResultElem> &out) const
{
    priority_queue<ResultElem> pq;  // Max-heap to keep track of top k results
    double tau = numeric_limits<double>::infinity();
    
    knnSearch(root, queryId, k, pq, tau);

    // Extract results and sort from smallest to largest distance
    while (!pq.empty()) {
        out.push_back(pq.top());
        pq.pop();
    }
    reverse(out.begin(), out.end());
}

void MVPT::knnSearch(VPNode *node, int queryId, int k, priority_queue<ResultElem> &pq, double &tau) const
{
    if (!node) return;
    
    if (node->isLeaf) {
        // Leaf node: process all objects in the bucket
        for (int id : node->bucket) {
            double d = db->distance(queryId, id);
            compdists++;
            
            if ((int)pq.size() < k) {
                pq.push({id, d});
                if ((int)pq.size() == k) tau = pq.top().dist;
            } else if (d < pq.top().dist) {
                pq.pop();
                pq.push({id, d});
                tau = pq.top().dist;
            }
        }
        return;
    }

    // Internal node: compute distance to pivot
    double distToPivot = db->distance(queryId, node->pivot);
    compdists++;

    // Add pivot to candidates
    if ((int)pq.size() < k) {
        pq.push({node->pivot, distToPivot});
        if ((int)pq.size() == k) tau = pq.top().dist;
    } else if (distToPivot < pq.top().dist) {
        pq.pop();
        pq.push({node->pivot, distToPivot});
        tau = pq.top().dist;
    }

    // Get the closest child to the query
    int closestChild = 0;
    for (int i = 1; i < arity; i++) {
        if (node->radii[i] > distToPivot) {
            closestChild = i - 1;
            break;
        }
        if (i == arity - 1) closestChild = i;
    }

    // Explore children in proximity order
    vector<int> order;
    order.push_back(closestChild);
    for (int d = 1; d < arity; d++) {
        if (closestChild - d >= 0) order.push_back(closestChild - d);
        if (closestChild + d < arity) order.push_back(closestChild + d);
    }

    // Visit children in order with pruning
    for (int i : order) {
        if (!node->children[i]) continue;
        
        double lowerBound = node->radii[i];
        double upperBound = (i + 1 < arity) ? node->radii[i + 1] : numeric_limits<double>::infinity();
        
        // Prune: only explore if it can improve the current top k
        if ((int)pq.size() < k || distToPivot - tau <= upperBound && distToPivot + tau >= lowerBound) {
            knnSearch(node->children[i], queryId, k, pq, tau);
        }
    }
}

int getCompDists() {
    return compdists;
}

#endif
