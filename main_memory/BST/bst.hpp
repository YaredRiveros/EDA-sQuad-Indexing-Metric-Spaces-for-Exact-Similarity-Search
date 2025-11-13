#ifndef BST_GENERIC_HPP
#define BST_GENERIC_HPP

#include "../../objectdb.hpp"
#include <vector>
#include <queue>
#include <cmath>
#include <chrono>
using namespace std;
using namespace std::chrono;

struct Node {
    int pl = -1, pr = -1;
    double lRadius = 0, rRadius = 0;
    bool leaf = false;
    vector<int> bucket;
    Node *lChild = nullptr, *rChild = nullptr;
};

struct ResultElem {
    int id;
    double dist;
    bool operator<(const ResultElem &o) const { return dist < o.dist; }
};

// hyperplane: 2 centers + ball partition: each center + radius
class BST {
    ObjectDB *db;
    Node *root;
    int bucketSize;
    int maxHeight;
    int compDist;
    double queryTime;
    
public:
    BST(ObjectDB *db, int nObjects, int bucketSize = 10, int maxHeight = 10);
    Node* build(const vector<int> &ids, int h);
    void rangeSearch(int queryId, double radius, vector<int> &result);
    void knnSearch(int queryId, int k, vector<ResultElem> &out);

    double get_queryTime() const;
    double get_compDist() const;
    int get_height() const;

    void clear_counters();

private:
    void rangeSearch(Node *node, int q, double radius, vector<int> &res);
    void knnSearch(Node *node, int q, int k, priority_queue<ResultElem> &pq, double &tau);

    int height(Node *node) const;
};

BST::BST(ObjectDB *db, int nObjects, int bucketSize, int maxHeight) 
    : db(db), root(nullptr), bucketSize(bucketSize), maxHeight(maxHeight)
{
    nObjects = min(nObjects, db->size());
    vector<int> ids(nObjects);
    iota(ids.begin(), ids.end(), 0);

    root = build(ids, 0);
    compDist = 0;
    cerr << "[BST] Height: " << height(root) << "\n";
}

double BST::get_queryTime() const
{
    return queryTime;
}

double BST::get_compDist() const
{
    return compDist;
}

int BST::get_height() const
{
    return height(root);
}
    

void BST::clear_counters() {
    compDist = 0;
    queryTime = 0;
}


Node* BST::build(const vector<int> &ids, int h) 
{
    Node *node = new Node();
    if ((int)ids.size() <= bucketSize || h+1>=maxHeight) 
    {
        node->leaf = true;
        node->bucket = ids;
        return node;
    }

    // farthest-point
    int i = ids[rand() % ids.size()];
    int j = i;
    double maxDist = -1;
    for (int id : ids) 
    {
        double d = db->distance(i, id); 
        if (d > maxDist) { maxDist = d; j = id; }
    }

    node->pl = i;
    node->pr = j;

    vector<int> left, right;
    double maxL=0, maxR=0;

    for (int id : ids) 
    {
        if (id==i || id==j) continue;
        double dL = db->distance(id, i);
        double dR = db->distance(id, j);
        if (dL < dR) { left.push_back(id); maxL = max(maxL, dL); }
        else { right.push_back(id); maxR = max(maxR, dR); }
    }

    node->lRadius = maxL;
    node->rRadius = maxR;
    node->lChild = build(left, h+1);
    node->rChild = build(right, h+1);

    return node;
}

void BST::rangeSearch(int queryId, double radius, vector<int> &result) 
{
    compDist = 0;
    auto t1 = chrono::high_resolution_clock::now();
    rangeSearch(root, queryId, radius, result);
    auto t2 = chrono::high_resolution_clock::now();
    queryTime = chrono::duration_cast<chrono::microseconds>(t2 - t1).count();

}

void BST::rangeSearch(Node *node, int q, double radius, vector<int> &res) 
{
    if (!node) return;
    if (node->leaf) {
    
        for (int id : node->bucket)
        {   
            double d = db->distance(q, id); 
            compDist++;
            if ( d <= radius ) res.push_back(id);
        }
        return;
    }

    double dl = db->distance(q, node->pl); compDist++;
    double dr = db->distance(q, node->pr); compDist++;
    
    if (dl <= radius) res.push_back(node->pl);
    if (dr <= radius) res.push_back(node->pr);

    // Lemma 4.2
    if (dl - node->lRadius <= radius) rangeSearch(node->lChild, q, radius, res);
    if (dr - node->rRadius <= radius) rangeSearch(node->rChild, q, radius, res);
}

void BST::knnSearch(int queryId, int k, vector<ResultElem> &out)  
{
    compDist = 0;
    
    auto t1 = chrono::high_resolution_clock::now();
    priority_queue<ResultElem> pq;
    double tau = 1e18;

    knnSearch(root, queryId, k, pq, tau);
    while (!pq.empty()) { out.push_back(pq.top()); pq.pop(); }
    reverse(out.begin(), out.end());
    
    auto t2 = chrono::high_resolution_clock::now();
     queryTime = chrono::duration_cast<chrono::microseconds>(t2 - t1).count();
}

void BST::knnSearch(Node *node, int q, int k, priority_queue<ResultElem> &pq, double &tau) 
{
    if (!node) return;
    if (node->leaf) {
        for (int id : node->bucket) {
            double d = db->distance(q, id);
            compDist++;
            if ((int)pq.size() < k) pq.push({id, d});
            else if (d < pq.top().dist) { pq.pop(); pq.push({id, d}); tau = pq.top().dist; }
        }
        return;
    }

    double dl = db->distance(q, node->pl); compDist++;
    double dr = db->distance(q, node->pr); compDist++;

    if (dl <= tau) pq.push({node->pl, dl});
    if (dr <= tau) pq.push({node->pr, dr});
    if ((int)pq.size() > k) { while ((int)pq.size() > k) pq.pop(); tau = pq.top().dist; }

    // Lemma 4.2
    if (dl - node->lRadius < tau) knnSearch(node->lChild, q, k, pq, tau);
    if (dr - node->rRadius < tau) knnSearch(node->rChild, q, k, pq, tau);
}

int BST::height(Node *node) const 
{
    if (!node) return 0;
    if (node->leaf) return 1;
    return 1 + max(height(node->lChild), height(node->rChild));
}

#endif
