#ifndef BKT_HPP
#define BKT_HPP

#include "../objectdb.hpp"
#include <cmath>
#include <queue>


struct BKNode 
{
    bool isLeaf;
    int pivot; 
    std::vector<int> bucket; // leaf's objects
    std::vector<std::pair<double, BKNode*>> children; // [dist, child] for non-leaf nodes
    BKNode() : isLeaf(true), pivot(-1) {}
};

struct ResultElem 
{
    int id;
    double dist;
};

class BKT 
{
    const ObjectDB *db;
    BKNode *root;
    int bucketSize;
    double step;

public:
    BKT(const ObjectDB *db_, int bsize = 10, double step_ = 1.0);
    ~BKT();

    void build();
    void insert(int objId);
    void countPivots();

    void rangeSearch(int qId, double r, std::vector<int> &res) const;
    std::vector<std::pair<double,int>> knnQuery(int qId, int k) const;
    void knnSearch(int qId, int k, std::vector<ResultElem> &out) const;

private:
    int height(const BKNode *node) const;
    int countPivots(const BKNode *node) const; 

    void addBKT(BKNode *node, int objId);
    void freeNode(BKNode *node);

    void searchRange(BKNode *node, int qId, double r, std::vector<int> &res) const;
    void searchKNN(BKNode *node, int qId, int k, std::priority_queue<std::pair<double,int>> &pq) const;
};

void BKT::countPivots()
{
    cout << "Altura del BKT: " << height(root) << endl;
    cout << "Pivots: " << countPivots(root) << endl;
}

int BKT::countPivots(const BKNode *node) const 
{
    if (node->isLeaf) return 0;
    int total = 1; // este nodo es pivote
    for (auto &[_, child] : node->children)
        total += countPivots(child);
    return total;
}


int BKT::height(const BKNode *node) const 
{
    if (node->isLeaf) return 1;
    int maxH = 0;
    for (auto &[d, child] : node->children)
        maxH = std::max(maxH, height(child));
    return 1 + maxH;
}


BKT::BKT(const ObjectDB *db_, int bsize, double step_): db(db_), bucketSize(bsize), step(step_) 
{
    root = new BKNode();
}

BKT::~BKT() 
{ 
    freeNode(root); 
}

void BKT::build() 
{
    for (int i = 0; i < db->size(); i++)
        insert(i);
}

void BKT::insert(int objId) 
{ 
    addBKT(root, objId); 
}

void BKT::rangeSearch(int qId, double r, std::vector<int> &res) const 
{
    searchRange(root, qId, r, res);
}

std::vector<std::pair<double,int>> BKT::knnQuery(int qId, int k) const 
{
    std::priority_queue<std::pair<double,int>> pq; // max-heap
    searchKNN(root, qId, k, pq);
    std::vector<std::pair<double,int>> res;
    while (!pq.empty()) { res.push_back(pq.top()); pq.pop(); }
    std::reverse(res.begin(), res.end());
    return res;
}

void BKT::knnSearch(int qId, int k, std::vector<ResultElem> &out) const 
{
    std::priority_queue<std::pair<double,int>> pq; // (dist,id)
    searchKNN(root, qId, k, pq);
    while (!pq.empty()) {
        auto [d,id] = pq.top(); pq.pop();
        out.push_back({id, d});
    }
    std::reverse(out.begin(), out.end());
}

void BKT::addBKT(BKNode *node, int objId) 
{
    if (node->isLeaf) 
    {
        if ((int)node->bucket.size() < bucketSize)  
        { // can be insert in a leaf's bucket
            node->bucket.push_back(objId);
            return;
        }

        // split 
        std::vector<int> oldBucket = node->bucket;
        node->bucket.clear();
        node->isLeaf = false;
        node->pivot = oldBucket[0]; // pivot = first element in the old bucket

        // move objects from node->bucket to node->children 
        for (size_t i = 1; i < oldBucket.size(); i++) addBKT(node, oldBucket[i]);
        addBKT(node, objId);
        return;
    }

    // non-leaf node find the associated ring (interval) for the object
    double dist = db->distance(objId, node->pivot);
    double dBucket = std::floor(dist / step) * step;

    // search for child node
    BKNode *child = nullptr;
    for (auto &[d, c] : node->children)
    {
        if (fabs(d - dBucket) < 1e-9) { child = c; break; }
    }

    if (!child) // if not exists, create a new child node
    {
        child = new BKNode();
        node->children.push_back({dBucket, child});
    }

    addBKT(child, objId);
}

    
void BKT::searchRange(BKNode *node, int qId, double r, std::vector<int> &res) const 
{
    if (node->isLeaf) 
    {
        for (auto id : node->bucket)
            if (db->distance(id, qId) <= r)
                res.push_back(id);
        return;
    }

    double dist = db->distance(qId, node->pivot); // d(q,p)
    if (dist <= r) res.push_back(node->pivot); // add the pivot if dist<=r

    // search in each child pruning with Lemma 4.1
    for (auto &[d, child] : node->children) 
    {
        if ((d + step > dist - r) && (d <= dist + r))
            searchRange(child, qId, r, res);
    }
}


void BKT::searchKNN(BKNode *node, int qId, int k, std::priority_queue<std::pair<double,int>> &pq) const 
{
    if (node->isLeaf) 
    {
        for (auto id : node->bucket) 
        {
            double d = db->distance(id, qId);
            pq.push({d, id});
            if ((int)pq.size() > k) pq.pop();
        }
        return;
    }

    double distPivot = db->distance(qId, node->pivot);
    pq.push({distPivot, node->pivot});
    if ((int)pq.size() > k) pq.pop();

    // Dk
    double rk = pq.size() < (size_t)k ? std::numeric_limits<double>::infinity()
                                        : pq.top().first;

    // Lemma 4.1 (simmilar to range query)
    for (auto &[d, child] : node->children){
        if ((d + step > distPivot - rk) && (d <= distPivot + rk))
            searchKNN(child, qId, k, pq);
    }
}

void BKT::freeNode(BKNode *node) 
{
    if (!node) return;
    for (auto &[_, c] : node->children)
        freeNode(c);
    delete node;
}
#endif
