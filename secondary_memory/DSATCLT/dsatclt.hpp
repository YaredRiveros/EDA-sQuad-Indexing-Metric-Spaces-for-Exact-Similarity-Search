#ifndef DSACLT_HPP
#define DSACLT_HPP

#include <bits/stdc++.h>
#include "../../objectdb.hpp"

using namespace std;

// Resultado para kNN específico de DSACLT
struct DSACLTResultElem {
    int id;
    double dist;
};

class DSACLT {
    // ------------------------
    //  Nodo interno DSACL-tree
    // ------------------------
    struct Node {
        int center;                   // id del centro (objeto de ObjectDB)
        double R;                     // covering radius del subárbol
        vector<int> neighbors;        // índices de vecinos N(a) (en 'nodes')
        vector<int> cluster;          // ids de objetos en cluster(a)
        vector<double> clusterDist;   // d'(xi) = d(center(a), xi)
        int time;                     // time(a): momento de creación del nodo

        Node(int c = -1)
            : center(c), R(0.0), time(0) {}
    };

    ObjectDB* db;
    vector<Node> nodes;        // pool de nodos
    int rootIdx;               // índice del nodo raíz
    int maxArity;              // MaxArity
    int kCluster;              // k = tamaño máximo de cluster
    int currentTime;           // CurrentTime global
    long long compDist;        // contador de distancias
    long long pageReads;       // contador aproximado de accesos de página
    vector<int> objTimestamp;  // timestamp de objetos (si luego lo necesitas)

    // ------------------------
    //  Utilidades internas
    // ------------------------

    double dist_obj(int a, int b) {
        compDist++;
        return db->distance(a, b);
    }

    int newNode(int centerId) {
        Node n(centerId);
        n.R = 0.0;
        n.time = ++currentTime;
        n.neighbors.clear();
        n.cluster.clear();
        n.clusterDist.clear();
        int idx = (int)nodes.size();
        nodes.push_back(n);
        return idx;
    }

    double rc(const Node& a) const {
        if (a.clusterDist.empty())
            return 0.0;
        return a.clusterDist.back();
    }

    void insertIntoClusterSorted(int aIdx, int xId, double d_ax) {
        Node& a = nodes[aIdx];
        auto it = lower_bound(a.clusterDist.begin(), a.clusterDist.end(), d_ax);
        size_t pos = (size_t)distance(a.clusterDist.begin(), it);
        a.clusterDist.insert(a.clusterDist.begin() + (long long)pos, d_ax);
        a.cluster.insert(a.cluster.begin() + (long long)pos, xId);
        objTimestamp[xId] = ++currentTime;
    }

    int argminNeighborCenterDist(int aIdx, int xId) {
        Node& a = nodes[aIdx];
        double best = numeric_limits<double>::infinity();
        int bestIdx = -1;
        for (int nbIdx : a.neighbors) {
            double d = dist_obj(nodes[nbIdx].center, xId);
            if (d < best) {
                best = d;
                bestIdx = nbIdx;
            }
        }
        return bestIdx;
    }

    // ----------------------------------------------------
    // InsertCl (Algorithm 3) versión ITERATIVA
    // ----------------------------------------------------
    void insertCl(int startIdx, int xId) {
        int aIdx = startIdx;
        int obj  = xId;

        while (true) {
            Node& a = nodes[aIdx];

            double d_ax = dist_obj(a.center, obj);
            if (d_ax > a.R) a.R = d_ax;

            double rc_a = rc(a);

            // Caso 1: entra al cluster(a)
            if ((int)a.cluster.size() < kCluster || d_ax < rc_a) {
                insertIntoClusterSorted(aIdx, obj, d_ax);

                if ((int)a.cluster.size() == kCluster + 1) {
                    // y = objeto más lejano del cluster(a)
                    int yId = a.cluster.back();
                    a.cluster.pop_back();
                    a.clusterDist.pop_back();

                    // repetir el proceso con y en el mismo nodo a
                    obj = yId;
                    continue;  // volvemos al while con mismo aIdx, obj=y
                }
                // no hay overflow de cluster -> terminamos inserción
                break;
            }

            // Caso 2: no entra al cluster -> ir a vecinos
            if (a.neighbors.empty()) {
                if (maxArity > 0 && (int)a.neighbors.size() < maxArity) {
                    int bIdx = newNode(obj);
                    // recuperar referencia por si nodes realoca (aunque hemos reservado)
                    nodes[aIdx].neighbors.push_back(bIdx);
                }
                // si maxArity==0 o lleno, no insertamos más (quedaría fuera del índice)
                break;
            }

            int cIdx = argminNeighborCenterDist(aIdx, obj);
            double d_cx = dist_obj(nodes[cIdx].center, obj);

            if (d_ax < d_cx && (int)nodes[aIdx].neighbors.size() < maxArity) {
                // Creamos nuevo nodo b con center=obj
                int bIdx = newNode(obj);
                nodes[aIdx].neighbors.push_back(bIdx);
                break;
            } else {
                // Continuar bajando por el vecino c (tail recursion -> loop)
                aIdx = cIdx;
                // obj se mantiene
                continue;
            }
        }
    }

    // ----------------------------------------------------
    // RangeSearchCl (Algorithm 4)
    // ----------------------------------------------------
    void rangeSearchCl(
        int aIdx,
        int qId,
        double r,
        int t,
        vector<int>& out,
        vector<double>& centerDistCache
    ) {
        Node& a = nodes[aIdx];
        pageReads++;

        if (centerDistCache[aIdx] < 0.0) {
            centerDistCache[aIdx] = dist_obj(a.center, qId);
        }
        double d_aq = centerDistCache[aIdx];

        if (!(a.time < t && d_aq <= a.R + r))
            return;

        if (d_aq <= r) {
            out.push_back(a.center);
        }

        double rc_a = rc(a);

        if ((d_aq - r <= rc_a) || (d_aq + r <= rc_a)) {
            for (size_t i = 0; i < a.cluster.size(); ++i) {
                int ci = a.cluster[i];
                double dprime = a.clusterDist[i];

                if (fabs(d_aq - dprime) <= r) {
                    double d_ciq = dist_obj(ci, qId);
                    if (d_ciq <= r) {
                        out.push_back(ci);
                    }
                }
            }
            if (d_aq + r < rc_a) return;
        }

        double dmin = numeric_limits<double>::infinity();

        size_t nb = a.neighbors.size();
        if (nb == 0) return;

        vector<double> d_nb(nb, -1.0);
        for (size_t i = 0; i < nb; ++i) {
            int biIdx = a.neighbors[i];
            if (centerDistCache[biIdx] < 0.0) {
                centerDistCache[biIdx] = dist_obj(nodes[biIdx].center, qId);
            }
            d_nb[i] = centerDistCache[biIdx];
        }

        for (size_t i = 0; i < nb; ++i) {
            int biIdx = a.neighbors[i];
            double d_bi_q = d_nb[i];

            if (d_bi_q <= dmin + 2.0 * r) {
                int tNext = t;
                for (size_t j = i + 1; j < nb; ++j) {
                    double d_bj_q = d_nb[j];
                    if (d_bi_q > d_bj_q + 2.0 * r) {
                        int bjIdx = a.neighbors[j];
                        tNext = min(tNext, nodes[bjIdx].time);
                    }
                }

                rangeSearchCl(biIdx, qId, r, tNext, out, centerDistCache);

                if (d_bi_q < dmin) dmin = d_bi_q;
            }
        }
    }

public:
    DSACLT(ObjectDB* database,
           int maxArity_ = 32,
           int kCluster_ = 10)
        : db(database),
          rootIdx(-1),
          maxArity(maxArity_),
          kCluster(kCluster_),
          currentTime(0),
          compDist(0),
          pageReads(0)
    {
        int n = db ? db->size() : 0;
        objTimestamp.assign(n, 0);
    }

    // Construcción incremental insertando 0..N-1
    void build() {
        nodes.clear();
        int n = db->size();
        nodes.reserve(n);          // ⚠️ IMPORTANTE: evita realocaciones
        rootIdx = -1;
        currentTime = 0;
        compDist = 0;
        pageReads = 0;
        objTimestamp.assign(n, 0);

        for (int x = 0; x < n; ++x) {
            if (rootIdx == -1) {
                rootIdx = newNode(x);
            } else {
                insertCl(rootIdx, x);
            }
        }
    }

    // MRQ
    vector<int> MRQ(int qId, double r) {
        vector<int> result;
        if (rootIdx == -1) return result;
        vector<double> centerDistCache(nodes.size(), -1.0);
        int t0 = numeric_limits<int>::max();
        rangeSearchCl(rootIdx, qId, r, t0, result, centerDistCache);
        return result;
    }

    // MkNN (extensión best-first)
    vector<DSACLTResultElem> MkNN(int qId, int k) {
        vector<DSACLTResultElem> ans;
        if (k <= 0 || rootIdx == -1) return ans;

        vector<double> centerDistCache(nodes.size(), -1.0);
        int nObjs = db->size();
        vector<double> objDistCache(nObjs, -1.0);

        auto getCenterDist = [&](int nodeIdx) {
            if (centerDistCache[nodeIdx] < 0.0) {
                centerDistCache[nodeIdx] = dist_obj(nodes[nodeIdx].center, qId);
            }
            return centerDistCache[nodeIdx];
        };

        auto getObjDist = [&](int objId) {
            if (objDistCache[objId] < 0.0) {
                objDistCache[objId] = dist_obj(objId, qId);
            }
            return objDistCache[objId];
        };

        struct Item {
            double lb;
            int nodeIdx;
            int objId;   // -1 -> nodo; >=0 -> objeto
        };
        struct Cmp {
            bool operator()(const Item& a, const Item& b) const {
                return a.lb > b.lb;
            }
        };

        priority_queue<Item, vector<Item>, Cmp> H;

        auto pushNode = [&](int nodeIdx) {
            double d_cq = getCenterDist(nodeIdx);
            double lb = max(0.0, d_cq - nodes[nodeIdx].R);
            H.push({lb, nodeIdx, -1});
        };

        pushNode(rootIdx);
        double tau = numeric_limits<double>::infinity();

        while (!H.empty()) {
            Item it = H.top(); H.pop();
            if (it.lb > tau) break;

            if (it.objId >= 0) {
                double d = getObjDist(it.objId);
                if (d > tau) continue;
                ans.push_back({it.objId, d});
                if ((int)ans.size() > k) {
                    nth_element(ans.begin(), ans.begin() + k, ans.end(),
                                [](const DSACLTResultElem& a,
                                   const DSACLTResultElem& b) {
                                    return a.dist < b.dist;
                                });
                    ans.resize(k);
                }
                if ((int)ans.size() == k) {
                    double worst = 0.0;
                    for (auto& e : ans) worst = max(worst, e.dist);
                    tau = worst;
                }
            } else {
                int aIdx = it.nodeIdx;
                Node& a = nodes[aIdx];
                pageReads++;

                double d_aq = getCenterDist(aIdx);

                double d_center = d_aq;
                if (d_center <= tau) {
                    ans.push_back({a.center, d_center});
                    if ((int)ans.size() > k) {
                        nth_element(ans.begin(), ans.begin() + k, ans.end(),
                                    [](const DSACLTResultElem& x,
                                       const DSACLTResultElem& y) {
                                        return x.dist < y.dist;
                                    });
                        ans.resize(k);
                    }
                    if ((int)ans.size() == k) {
                        double worst = 0.0;
                        for (auto& e : ans) worst = max(worst, e.dist);
                        tau = worst;
                    }
                }

                double rc_a = rc(a);
                if (!a.cluster.empty()) {
                    for (size_t i = 0; i < a.cluster.size(); ++i) {
                        int xi = a.cluster[i];
                        double dprime = a.clusterDist[i];
                        double lb_obj = fabs(d_aq - dprime);
                        if (lb_obj > tau) continue;
                        double dxi = getObjDist(xi);
                        if (dxi <= tau) {
                            ans.push_back({xi, dxi});
                            if ((int)ans.size() > k) {
                                nth_element(ans.begin(), ans.begin() + k,
                                            ans.end(),
                                            [](const DSACLTResultElem& u,
                                               const DSACLTResultElem& v) {
                                                return u.dist < v.dist;
                                            });
                                ans.resize(k);
                            }
                            if ((int)ans.size() == k) {
                                double worst = 0.0;
                                for (auto& e : ans) worst = max(worst, e.dist);
                                tau = worst;
                            }
                        }
                    }
                }

                for (int nbIdx : a.neighbors) {
                    double d_nbq = getCenterDist(nbIdx);
                    double lb_nb = max(0.0, d_nbq - nodes[nbIdx].R);
                    if (lb_nb <= tau) {
                        H.push({lb_nb, nbIdx, -1});
                    }
                }
            }
        }

        sort(ans.begin(), ans.end(),
             [](const DSACLTResultElem& a, const DSACLTResultElem& b) {
                 return a.dist < b.dist;
             });
        if ((int)ans.size() > k) ans.resize(k);
        return ans;
    }

    // Estadísticas
    long long get_compDist() const { return compDist; }
    long long get_pageReads() const { return pageReads; }

    void clear_counters() {
        compDist = 0;
        pageReads = 0;
    }

    void stats() const {
        cout << "DSACLT: nodes=" << nodes.size()
             << ", maxArity=" << maxArity
             << ", kCluster=" << kCluster << "\n";
    }
};

#endif // DSACLT_HPP
