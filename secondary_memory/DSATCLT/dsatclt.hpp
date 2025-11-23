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
        vector<int> neighbors;        // índices de vecinos N(a) (en 'nodes'), en orden creciente de timestamp
        vector<int> cluster;          // ids de objetos en cluster(a)
        vector<double> clusterDist;   // d'(xi) = d(center(a), xi), mismo orden que 'cluster'
        int time;                     // time(a): momento de creación del nodo

        Node(int c = -1)
            : center(c), R(0.0), time(0) {}
    };

    ObjectDB* db;
    vector<Node> nodes;        // pool de nodos
    int rootIdx;               // índice del nodo raíz en 'nodes'
    int maxArity;              // MaxArity (cota superior del grado)
    int kCluster;              // k = tamaño máximo de cluster
    int currentTime;           // CurrentTime global (timestamps)
    long long compDist;        // contador de evaluaciones de distancia
    long long pageReads;       // contador aproximado de accesos a página (nodos visitados)

    // Timestamps por objeto (no estrictamente necesarios para Algorithm 4,
    // pero útiles si luego quieres extender podas tipo DSA-tree).
    vector<int> objTimestamp;

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
        n.neighbors.clear();
        n.cluster.clear();
        n.clusterDist.clear();
        n.time = ++currentTime;           // tiempo de creación del nodo
        int idx = (int)nodes.size();
        nodes.push_back(n);
        return idx;
    }

    // rc(a) = radio de cluster(a) = distancia al elemento más lejano del cluster
    double rc(const Node& a) const {
        if (a.clusterDist.empty())
            return 0.0;
        return a.clusterDist.back();
    }

    // Inserta (x, d_ax) en el cluster(aIdx) manteniendo orden creciente por distancia
    void insertIntoClusterSorted(int aIdx, int xId, double d_ax) {
        Node& a = nodes[aIdx];
        auto it = lower_bound(a.clusterDist.begin(), a.clusterDist.end(), d_ax);
        size_t pos = (size_t)distance(a.clusterDist.begin(), it);
        a.clusterDist.insert(a.clusterDist.begin() + (long long)pos, d_ax);
        a.cluster.insert(a.cluster.begin() + (long long)pos, xId);
        objTimestamp[xId] = ++currentTime;    // momento de inserción en el cluster
    }

    // Devuelve índice del vecino c ∈ N(a) tal que d(center(c), x) es mínima.
    // Se asume que N(a) no está vacío.
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

    // ------------------------
    //  InsertCl (Algorithm 3)
    // ------------------------
    void insertCl(int aIdx, int xId) {
        Node& a = nodes[aIdx];

        // 1. R(a) ← max(R(a), d(center(a), x))
        double d_ax = dist_obj(a.center, xId);
        if (d_ax > a.R) a.R = d_ax;

        double rc_a = rc(a);

        // 2. If ((|cluster(a)| < k) ∨ (d(center(a), x) < rc(a))) Then ...
        if ((int)a.cluster.size() < kCluster || d_ax < rc_a) {
            // 3. cluster(a) ← cluster(a) ∪ {x}
            // 4. d'(x) ← d(center(a), x)
            // 5. timestamp(x) ← CurrentTime
            insertIntoClusterSorted(aIdx, xId, d_ax);

            // 6. If (|cluster(a)| = k + 1) Then
            if ((int)a.cluster.size() == kCluster + 1) {
                // 7. y ← argmax_z∈cluster(a) d'(z) -> último en la lista ordenada
                int yId = a.cluster.back();

                // 8. cluster(a) ← cluster(a) − {y}
                a.cluster.pop_back();
                a.clusterDist.pop_back();

                // 9. InsertCl(a, y)
                insertCl(aIdx, yId);
            }
        } else {
            // 10. Else
            // 11. c ← argmin_{b∈N(a)} d(center(b), x)

            // Caso especial: N(a) vacío -> creamos vecino nuevo si es posible
            if (a.neighbors.empty()) {
                if ((int)a.neighbors.size() < maxArity) {
                    int bIdx = newNode(xId);
                    a.neighbors.push_back(bIdx);   // más nuevo al final
                }
                // Si maxArity = 0, simplemente no insertamos más vecinos
                return;
            }

            int cIdx = argminNeighborCenterDist(aIdx, xId);
            double d_cx = dist_obj(nodes[cIdx].center, xId);

            // 12. If d(center(a), x) < d(center(c), x) ∧ |N(a)| < MaxArity Then ...
            if (d_ax < d_cx && (int)a.neighbors.size() < maxArity) {
                // 13-16. Crear nuevo nodo b con center(b) = x y vecinos vacíos
                int bIdx = newNode(xId);

                // 13. N(a) ← N(a) ∪ {b}
                a.neighbors.push_back(bIdx);

                // 15. N(b) ← ∅, 16. cluster(b) ← ∅ ya se hicieron en newNode
                // 17. timestamp(x) ← CurrentTime (ya se asignó en newNode)
                // 18. time(b) ← CurrentTime (ya se asignó en newNode)
            } else {
                // 19-20. Else InsertCl(c, x)
                insertCl(cIdx, xId);
            }
        }
    }

    // ------------------------
    //  RangeSearchCl (Algorithm 4)
    //  Se llama con t = CurrentTime inicial.
    // ------------------------
    void rangeSearchCl(
        int aIdx,
        int qId,
        double r,
        int t,
        vector<int>& out,
        vector<double>& centerDistCache
    ) {
        Node& a = nodes[aIdx];
        pageReads++;  // una visita a nodo ~ acceso de página

        // Cache d(center(a), q) para no evaluarla más de una vez por nodo
        if (centerDistCache[aIdx] < 0.0) {
            centerDistCache[aIdx] = dist_obj(a.center, qId);
        }
        double d_aq = centerDistCache[aIdx];

        // 1. If time(a) < t ∧ d(center(a), q) ≤ R(a) + r Then
        if (!(a.time < t && d_aq <= a.R + r)) return;

        // 2. If d(center(a), q) ≤ r Then Report a
        if (d_aq <= r) {
            out.push_back(a.center);
        }

        double rc_a = rc(a);

        // 3. If (d(center(a), q) − r ≤ rc(a)) ∨ (d(center(a), q) + r ≤ rc(a)) Then
        if ((d_aq - r <= rc_a) || (d_aq + r <= rc_a)) {
            // 4. For ci ∈ cluster(a) Do
            for (size_t i = 0; i < a.cluster.size(); ++i) {
                int ci = a.cluster[i];
                double dprime = a.clusterDist[i]; // d'(ci)

                // 5. If |d(center(a), q) − d'(ci)| ≤ r Then
                if (fabs(d_aq - dprime) <= r) {
                    // 6. If d(ci, q) ≤ r Then Report ci
                    double d_ciq = dist_obj(ci, qId);
                    if (d_ciq <= r) {
                        out.push_back(ci);
                    }
                }
            }
            // 7. If d(center(a), q) + r < rc(a) Then Return
            if (d_aq + r < rc_a) return;
        }

        // 8. dmin ← ∞
        double dmin = numeric_limits<double>::infinity();

        // 9. For bi ∈ N(a) in increasing order of timestamp Do
        size_t nb = a.neighbors.size();
        if (nb == 0) return;

        // Cache de distancias d(center(bi), q) para todos los vecinos una sola vez
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

            // 10. If d(center(bi), q) ≤ dmin + 2r Then
            if (d_bi_q <= dmin + 2.0 * r) {
                // 11. k ← min { j > i, d(center(bi), q) > d(center(bj), q) + 2r }
                // Adaptación de Algorithm 2 de DSA-tree:
                // t' = min{t} ∪ { time(bj), j>i ∧ d(bi,q) > d(bj,q) + 2r }
                int tNext = t;
                for (size_t j = i + 1; j < nb; ++j) {
                    double d_bj_q = d_nb[j];
                    if (d_bi_q > d_bj_q + 2.0 * r) {
                        int bjIdx = a.neighbors[j];
                        tNext = min(tNext, nodes[bjIdx].time);
                    }
                }

                // 12. RangeSearchCl (bi, q, r, time(bk))  -> usamos tNext
                rangeSearchCl(biIdx, qId, r, tNext, out, centerDistCache);

                // 13. dmin ← min{dmin, d(center(bi), q)}
                if (d_bi_q < dmin) dmin = d_bi_q;
            }
        }
    }

public:
    DSACLT(
        ObjectDB* database,
        int maxArity_ = 32,
        int kCluster_ = 10
    )
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

    // Construye el árbol incrementalmente insertando todos los objetos 0..n-1
    void build() {
        nodes.clear();
        rootIdx = -1;
        currentTime = 0;
        compDist = 0;
        pageReads = 0;

        int n = db->size();
        objTimestamp.assign(n, 0);

        for (int x = 0; x < n; ++x) {
            if (rootIdx == -1) {
                // Primer elemento crea el nodo raíz:
                // center(a) = x, rc(a) = 0, cluster(a)=∅, N(a)=∅, R(a)=0
                rootIdx = newNode(x);
            } else {
                insertCl(rootIdx, x);
            }
        }
    }

    // ------------------------
    //  MRQ: Range Query (Algorithm 4)
    // ------------------------
    vector<int> MRQ(int qId, double r) {
        vector<int> result;
        if (rootIdx == -1) return result;
        vector<double> centerDistCache(nodes.size(), -1.0);
        int t0 = numeric_limits<int>::max(); // CurrentTime "infinito"
        rangeSearchCl(rootIdx, qId, r, t0, result, centerDistCache);
        return result;
    }

    // ------------------------
    //  MkNN: búsqueda k-NN
    //  (Extensión best-first sobre DSACL-tree usando R(a) como cota)
    // ------------------------
    vector<DSACLTResultElem> MkNN(int qId, int k) {
        vector<DSACLTResultElem> ans;
        if (k <= 0 || rootIdx == -1) return ans;

        // Distancias memoizadas a centros y a objetos
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

        // Best-first con elementos tipo (lb, tipo, nodeIdx, objId)
        struct Item {
            double lb;
            int nodeIdx;
            int objId;   // -1 si es nodo, >=0 si es objeto concreto
        };
        struct Cmp {
            bool operator()(const Item& a, const Item& b) const {
                return a.lb > b.lb;  // min-heap
            }
        };

        priority_queue<Item, vector<Item>, Cmp> H;

        // Inserta nodo con cota lb usando covering radius
        auto pushNode = [&](int nodeIdx) {
            double d_cq = getCenterDist(nodeIdx);
            double lb = max(0.0, d_cq - nodes[nodeIdx].R);
            H.push({lb, nodeIdx, -1});
        };

        // inicializamos con raíz
        pushNode(rootIdx);

        double tau = numeric_limits<double>::infinity();  // radio actual (distancia del peor vecino entre los k mejores)

        while (!H.empty()) {
            Item it = H.top(); H.pop();
            if (it.lb > tau) break;  // nada mejor puede encontrarse

            if (it.objId >= 0) {
                // objeto concreto
                double d = getObjDist(it.objId);
                if (d > tau) continue;
                ans.push_back({it.objId, d});
                if ((int)ans.size() > k) {
                    // mantenemos solo los k mejores
                    nth_element(ans.begin(), ans.begin() + k, ans.end(),
                                [](const DSACLTResultElem& a, const DSACLTResultElem& b) {
                                    return a.dist < b.dist;
                                });
                    ans.resize(k);
                }
                if ((int)ans.size() == k) {
                    // actualizamos tau
                    double worst = 0.0;
                    for (auto& e : ans) worst = max(worst, e.dist);
                    tau = worst;
                }
            } else {
                // nodo
                int aIdx = it.nodeIdx;
                Node& a = nodes[aIdx];
                pageReads++;  // visita de nodo

                double d_aq = getCenterDist(aIdx);

                // Centro como candidato
                double d_center = d_aq;
                if (d_center <= tau) {
                    ans.push_back({a.center, d_center});
                    if ((int)ans.size() > k) {
                        nth_element(ans.begin(), ans.begin() + k, ans.end(),
                                    [](const DSACLTResultElem& x, const DSACLTResultElem& y) {
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

                // Cluster(a): usamos d'(xi) y d_aq para podar con desigualdad triangular
                double rc_a = rc(a);
                if (!a.cluster.empty()) {
                    // si el cluster está completamente más allá de tau alrededor del centro, podemos podar
                    // pero como tau puede ser grande o infinito, hacemos poda por elemento
                    for (size_t i = 0; i < a.cluster.size(); ++i) {
                        int xi = a.cluster[i];
                        double dprime = a.clusterDist[i]; // d(center(a), xi)
                        // lower bound |d(center(a),q) - d(center(a),xi)|
                        double lb_obj = fabs(d_aq - dprime);
                        if (lb_obj > tau) continue;  // seguro peor que tau
                        double dxi = getObjDist(xi);
                        if (dxi <= tau) {
                            ans.push_back({xi, dxi});
                            if ((int)ans.size() > k) {
                                nth_element(ans.begin(), ans.begin() + k, ans.end(),
                                            [](const DSACLTResultElem& u, const DSACLTResultElem& v) {
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

                // Vecinos: encolamos sus subárboles con cota basada en covering radius
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

    // ------------------------
    //  Estadísticas
    // ------------------------
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
