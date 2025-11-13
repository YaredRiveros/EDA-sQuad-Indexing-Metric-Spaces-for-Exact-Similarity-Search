#ifndef BKT_HPP
#define BKT_HPP

#include "../../objectdb.hpp"
#include <cmath>
#include <queue>
#include <vector>
#include <utility>
#include <algorithm>
#include <limits>
#include <chrono>

struct BKNode 
{
    bool isLeaf;
    int pivot; 
    std::vector<int> bucket;                        // objects if leaf
    std::vector<std::pair<double, BKNode*>> children; // [dist, child] if internal

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

    // === Counters (metricas para el benchmark) ===
    mutable long long compDist = 0;   // # distance computations
    mutable long long queryTime = 0;  // μs acumulados

public:
    BKT(const ObjectDB *db_, int bsize = 10, double step_ = 1.0);
    ~BKT();

    void build();
    void insert(int objId);


    int get_height() const { return height(root); }
    int get_num_pivots() const { return countPivots(root); }

    // Counters API (mismo estilo que BST)
    void clear_counters() const { compDist = 0; queryTime = 0; }
    long long get_compDist() const { return compDist; }
    long long get_queryTime() const { return queryTime; }

    // Util para debug
    void printPivotsInfo() const;

    // Queries
    void rangeSearch(int qId, double r, std::vector<int> &res) const;
    std::vector<std::pair<double,int>> knnQuery(int qId, int k) const;
    void knnSearch(int qId, int k, std::vector<ResultElem> &out) const;

private:
    int  height(const BKNode *node) const;
    int  countPivots(const BKNode *node) const; 

    void addBKT(BKNode *node, int objId);
    void freeNode(BKNode *node);

    void searchRange(BKNode *node, int qId, double r, std::vector<int> &res) const;
    void searchKNN(BKNode *node, int qId, int k,
                   std::priority_queue<std::pair<double,int>> &pq) const;

    // Wrapper de distancia que incrementa compDist
    double dist(int a, int b) const {
        compDist++;
        return db->distance(a, b);
    }
};

// ============================================================
//  IMPLEMENTACIÓN
// ============================================================

BKT::BKT(const ObjectDB *db_, int bsize, double step_)
    : db(db_), root(new BKNode), bucketSize(bsize), step(step_) {}

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

// ------------ info de pivotes / altura ------------

int BKT::height(const BKNode *node) const 
{
    if (!node) return 0;
    if (node->isLeaf) return 1;
    int maxH = 0;
    for (const auto &p : node->children)
        maxH = std::max(maxH, height(p.second));
    return 1 + maxH;
}

int BKT::countPivots(const BKNode *node) const 
{
    if (!node || node->isLeaf) return 0;
    int total = 1; // este nodo tiene un pivot
    for (const auto &p : node->children)
        total += countPivots(p.second);
    return total;
}

void BKT::printPivotsInfo() const
{
    std::cout << "Altura del BKT: " << height(root) << "\n";
    std::cout << "Pivots (nodos internos): " << countPivots(root) << "\n";
}

// ============================================================
//  RANGE SEARCH (MRQ)
// ============================================================

void BKT::rangeSearch(int qId, double r, std::vector<int> &res) const
{
    auto start = std::chrono::high_resolution_clock::now();
    searchRange(root, qId, r, res);
    queryTime += std::chrono::duration_cast<std::chrono::microseconds>(
        std::chrono::high_resolution_clock::now() - start
    ).count();
}

void BKT::searchRange(BKNode *node, int qId, double r, std::vector<int> &res) const
{
    if (!node) return;

    if (node->isLeaf)
    {
        for (int id : node->bucket)
        {
            if (dist(id, qId) <= r)
                res.push_back(id);
        }
        return;
    }

    // nodo interno: revisar pivot
    double dqp = dist(qId, node->pivot);  // d(q,p)
    if (dqp <= r)
        res.push_back(node->pivot);

    // Lemma 4.1 con anillos [d, d+step)
    for (auto &[ringDist, child] : node->children)
    {
        // si el anillo {o: ringDist <= d(p,o) < ringDist+step} puede contener objetos
        // dentro de B(q,r), entonces exploramos
        if (ringDist + step > dqp - r && ringDist <= dqp + r)
            searchRange(child, qId, r, res);
    }
}

// ============================================================
//  KNN (MkNN)
// ============================================================

std::vector<std::pair<double,int>> BKT::knnQuery(int qId, int k) const
{
    std::priority_queue<std::pair<double,int>> pq;
    searchKNN(root, qId, k, pq);

    std::vector<std::pair<double,int>> res;
    while (!pq.empty())
    {
        res.push_back(pq.top());
        pq.pop();
    }
    std::reverse(res.begin(), res.end());
    return res;
}

void BKT::knnSearch(int qId, int k, std::vector<ResultElem> &out) const
{
    auto start = std::chrono::high_resolution_clock::now();

    std::priority_queue<std::pair<double,int>> pq;
    searchKNN(root, qId, k, pq);

    while (!pq.empty())
    {
        auto [d, id] = pq.top();
        pq.pop();
        out.push_back({id, d});
    }
    std::reverse(out.begin(), out.end());

    queryTime += std::chrono::duration_cast<std::chrono::microseconds>(
        std::chrono::high_resolution_clock::now() - start
    ).count();
}

void BKT::searchKNN(BKNode *node, int qId, int k,
                    std::priority_queue<std::pair<double,int>> &pq) const
{
    if (!node) return;

    if (node->isLeaf)
    {
        for (int id : node->bucket)
        {
            double d = dist(id, qId);
            pq.push({d, id});
            if ((int)pq.size() > k) pq.pop();
        }
        return;
    }

    // visitar el pivot
    double dqp = dist(qId, node->pivot);
    pq.push({dqp, node->pivot});
    if ((int)pq.size() > k) pq.pop();

    double rk = pq.size() < (size_t)k ?
                std::numeric_limits<double>::infinity() :
                pq.top().first;

    // Lemma 4.1 adaptado a kNN (igual que range pero con r = Dk)
    for (auto &[ringDist, child] : node->children)
    {
        if (ringDist + step > dqp - rk && ringDist <= dqp + rk)
            searchKNN(child, qId, k, pq);
    }
}

// ============================================================
//  FREE
// ============================================================

void BKT::freeNode(BKNode *node)
{
    if (!node) return;
    for (auto &[_, child] : node->children)
        freeNode(child);
    delete node;
}

void BKT::addBKT(BKNode *node, int objId)
{
    // ============================
    // 1. Nodo hoja → insertar o dividir
    // ============================
    if (node->isLeaf)
    {
        // Hay espacio en el bucket
        if ((int)node->bucket.size() < bucketSize)
        {
            node->bucket.push_back(objId);
            return;
        }

        // --- SPLIT EXACTO como el original ---
        std::vector<int> oldBucket = node->bucket;
        node->bucket.clear();
        node->isLeaf = false;

        // Primer objeto del bucket → pivot = query
        node->pivot = oldBucket[0];

        // Los demás se reinsertan
        for (size_t i = 1; i < oldBucket.size(); i++)
            addBKT(node, oldBucket[i]);

        // Insertamos el objeto nuevo
        addBKT(node, objId);
        return;
    }

    // ============================
    // 2. Nodo interno → seleccionar hijo según banda
    // ============================

    double d = dist(objId, node->pivot);  // distancia al pivot

    BKNode *child = nullptr;

    // Buscar banda CHILD que satisfaga:
    //    ringDist <= d < ringDist + step
    //
    // Esto es EXACTAMENTE lo que implementa el código original de C.
    //
    for (auto &pr : node->children)
    {
        double ringDist = pr.first;

        if (ringDist <= d && d < ringDist + step)
        {
            child = pr.second;
            break;
        }
    }

    // ============================
    // 2b. Si NO existe banda → crear hijo nuevo
    // ============================

    if (!child)
    {
        // ringDist = floor(d/step) * step   (igual al original)
        double ringDist = std::floor(d / step) * step;

        // crear hijo
        BKNode *newChild = new BKNode();

        node->children.push_back({ringDist, newChild});
        child = newChild;
    }

    // ============================
    // 3. Insertar recursivamente
    // ============================
    addBKT(child, objId);
}

#endif
