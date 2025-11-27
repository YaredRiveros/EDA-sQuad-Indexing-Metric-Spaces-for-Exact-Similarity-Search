#ifndef SAT_HPP
#define SAT_HPP

#include "../../objectdb.hpp"
#include <vector>
#include <queue>
#include <limits>
#include <algorithm>
#include <chrono>

// Resultado para kNN específico de SAT
struct SATResultElem
{
    int id;
    double dist;
};

class SAT
{
    struct Node
    {
        int center;                // id del objeto centro
        double maxDist;            // radio: max dist a objetos en su subárbol
        std::vector<int> children; // índices de hijos en "nodes"

        Node(int c = -1) : center(c), maxDist(0.0) {}
    };

    // Elementos usados solo durante la construcción (cola/queue del SAT original)
    struct BuildQueueElem
    {
        int objId;     // objeto
        double dist;   // mejor distancia al centro actual
        int bestChild; // índice dentro de "children" del mejor vecino (-1 si ninguno)
    };

    // Elemento del heap para el kNN
    struct NodeHeapElem
    {
        int nodeId;     // índice del nodo
        double dist;    // d(q, center)
        double lbound;  // lower bound del subárbol
        double mind;    // mejor distancia a vecinos (mínimo dd)
    };

    struct NodeHeapCmp
    {
        bool operator()(const NodeHeapElem &a, const NodeHeapElem &b) const
        {
            // Queremos un min-heap por lbound → el de menor lbound debe salir primero
            return a.lbound > b.lbound;
        }
    };

    const ObjectDB *db;

    std::vector<Node> nodes;                         // nodos del SAT
    std::vector<std::vector<BuildQueueElem>> queues; // colas por nodo (solo build)
    int rootId;                                      // índice del nodo raíz

    mutable long long compDist = 0;   // #distance computations (solo en queries)
    mutable long long queryTime = 0;  // tiempo acumulado en μs

    using Clock = std::chrono::high_resolution_clock;

public:

    SAT(const ObjectDB *db_)
        : db(db_), rootId(-1) {}

    void build()
    {
        if (!db) return;
        int n = db->size();
        if (n <= 0) return;

        nodes.clear();
        queues.clear();
        nodes.reserve(n);
        queues.reserve(n);
        rootId = newNode(0);

        for (int i = 1; i < n; ++i)
        {
            double d = distBuild(0, i);
            queues[rootId].push_back({i, d, -1});
        }

        distribute(rootId);
    }

    int get_height() const
    {
        if (rootId < 0) return 0;
        return heightRec(rootId);
    }
    int get_num_pivots() const
    {
        return static_cast<int>(nodes.size());
    }

    void clear_counters() const { compDist = 0; queryTime = 0; }
    long long get_compDist() const { return compDist; }
    long long get_queryTime() const { return queryTime; }

    void rangeSearch(int qId, double r, std::vector<int> &res) const
    {
        if (rootId < 0 || !db) return;

        auto start = Clock::now();

        double d0 = distQuery(qId, nodes[rootId].center);
        double mind = d0;   // mínima distancia a cualquier centro en el camino
        double s    = 0.0;  // digresión acumulada (Navarro)

        searchRangeRec(rootId, qId, r, d0, mind, s, res);

        queryTime += std::chrono::duration_cast<std::chrono::microseconds>(
                         Clock::now() - start)
                         .count();
    }

    // Versión que devuelve pares (dist, id) por comodidad (similar a BKT::knnQuery)
    std::vector<std::pair<double, int>> knnQuery(int qId, int k) const
    {
        std::vector<SATResultElem> tmp;
        knnSearch(qId, k, tmp);

        std::vector<std::pair<double, int>> res;
        res.reserve(tmp.size());
        for (auto &e : tmp)
            res.push_back({e.dist, e.id});

        return res;
    }

    // Versión “oficial” para el benchmark (como BKT::knnSearch)
    void knnSearch(int qId, int k, std::vector<SATResultElem> &out) const
    {
        out.clear();
        if (rootId < 0 || !db || k <= 0) return;

        auto start = Clock::now();

        // Heap de nodos ordenado por lbound (min-heap)
        std::priority_queue<NodeHeapElem,
                            std::vector<NodeHeapElem>,
                            NodeHeapCmp>
            heap;

        // k mejores vecinos (max-heap por distancia)
        std::priority_queue<std::pair<double, int>> best;
        double currentRadius = std::numeric_limits<double>::infinity();

        // vector temporal de distancias a hijos (tamaño máximo: nº de nodos)
        std::vector<double> dd(nodes.size());

        // Inicializar con la raíz (mismo esquema que searchNN en sat.c)
        double dist0 = distQuery(qId, nodes[rootId].center);
        double lbound = dist0 - nodes[rootId].maxDist;
        if (lbound < 0.0) lbound = 0.0;

        heap.push({rootId, dist0, lbound, dist0});

        while (!heap.empty())
        {
            NodeHeapElem hel = heap.top();
            heap.pop();

            // Condición de corte (equivalente a outCelem(&res, hel.lbound))
            if (!best.empty() && (int)best.size() == k && hel.lbound >= currentRadius)
                break;

            const Node &N = nodes[hel.nodeId];

            // Añadir centro actual a los candidatos
            best.push({hel.dist, N.center});
            if ((int)best.size() > k)
                best.pop();

            if ((int)best.size() == k)
                currentRadius = best.top().first;

            int m = static_cast<int>(N.children.size());
            if (m == 0) continue;

            // Calcular distancias a hijos y actualizar mind (hel.mind)
            for (int j = 0; j < m; ++j)
            {
                int childId = N.children[j];
                dd[j] = distQuery(qId, nodes[childId].center);
                if (dd[j] < hel.mind) hel.mind = dd[j];
            }

            // Insertar hijos en el heap de nodos con sus lower bounds
            for (int j = 0; j < m; ++j)
            {
                int childId = N.children[j];
                double dchild = dd[j];

                double lb = hel.lbound;
                double tmp = (dchild - hel.mind) / 2.0;
                if (lb < tmp) lb = tmp;

                double tmp2 = dchild - nodes[childId].maxDist;
                if (lb < tmp2) lb = tmp2;

                heap.push({childId, dchild, lb, hel.mind});
            }
        }

        // Pasamos del heap (max por dist) a vector ordenado ascendente
        while (!best.empty())
        {
            auto [d, id] = best.top();
            best.pop();
            out.push_back({id, d});
        }
        std::reverse(out.begin(), out.end());

        queryTime += std::chrono::duration_cast<std::chrono::microseconds>(
                         Clock::now() - start)
                         .count();
    }

private:

    // Para construcción: NO incrementa contador
    double distBuild(int a, int b) const
    {
        return db->distance(a, b);
    }

    // Para consultas: SÍ incrementa contador
    double distQuery(int a, int b) const
    {
        compDist++;
        return db->distance(a, b);
    }

    int newNode(int objId)
    {
        int id = static_cast<int>(nodes.size());
        nodes.emplace_back(objId);
        queues.emplace_back(); // cola vacía para este nodo
        return id;
    }

    void distribute(int nodeId)
    {
        Node &node = nodes[nodeId];
        auto &Q = queues[nodeId];

        if (Q.empty())
        {
            node.maxDist = 0.0;
            return;
        }

        // Ordenar cola por distancia creciente 
        std::sort(Q.begin(), Q.end(),
                  [](const BuildQueueElem &a, const BuildQueueElem &b)
                  { return a.dist < b.dist; });

        node.maxDist = Q.back().dist;

        int ni = 0;

        // Primera pasada: decidir qué elementos crean nuevos vecinos
        for (size_t i = 0; i < Q.size(); ++i)
        {
            BuildQueueElem q = Q[i];
            q.bestChild = -1;

            // comparar contra vecinos ya existentes
            for (int j = 0; j < (int)node.children.size(); ++j)
            {
                int childId = node.children[j];
                double d = distBuild(q.objId, nodes[childId].center);
                if (d <= q.dist)
                {
                    q.dist = d;
                    q.bestChild = j;
                }
            }

            if (q.bestChild == -1)
            {
                // se convierte en nuevo vecino/centro
                int newChildId = newNode(q.objId);
                node.children.push_back(newChildId);
            }
            else
            {
                // se queda en la cola para ser reenviado luego
                Q[ni++] = q;
            }
        }

        // Segunda pasada: mandar los demás al mejor vecino
        for (int i = 0; i < ni; ++i)
        {
            BuildQueueElem q = Q[i];

            // Puede haber nuevos vecinos añadidos después de q.bestChild
            for (int j = q.bestChild + 1; j < (int)node.children.size(); ++j)
            {
                int childId = node.children[j];
                double d = distBuild(q.objId, nodes[childId].center);
                if (d <= q.dist)
                {
                    q.dist = d;
                    q.bestChild = j;
                }
            }

            int targetChildId = node.children[q.bestChild];
            queues[targetChildId].push_back({q.objId, q.dist, -1});
        }

        // La cola ya no se usa en este nodo
        Q.clear();

        // Recursión sobre los hijos
        for (int childId : node.children)
            distribute(childId);
    }

    void searchRangeRec(int nodeId, int qId, double r,
                        double d0, double mind, double s,
                        std::vector<int> &res) const
    {
        const Node &N = nodes[nodeId];

        // Poda por digresión de ancestros: si s > 2r, no puede haber resultados
        if (s > 2.0 * r) return;

        // Poda por bola: si d0 - r > maxDist, no hay nada en este subárbol
        if (d0 - r > N.maxDist) return;

        // El centro del nodo puede ser respuesta
        if (d0 <= r)
            res.push_back(N.center);

        int m = static_cast<int>(N.children.size());
        if (m == 0) return;

        std::vector<double> dd(m);
        double newMind = mind;

        // distancias a hijos + actualizar newMind (mínima dist a centros)
        for (int j = 0; j < m; ++j)
        {
            int childId = N.children[j];
            dd[j] = distQuery(qId, nodes[childId].center);
            if (dd[j] < newMind) newMind = dd[j];
        }

        // Poda tipo SAT usando ancestros:
        //   - condición dd[j] <= newMind + 2r
        //   - digresión actualizada newS = max(0, s + (dd[j] - d0))
        for (int j = 0; j < m; ++j)
        {
            int childId = N.children[j];
            if (dd[j] <= newMind + 2.0 * r)
            {
                double newS = std::max(0.0, s + (dd[j] - d0));
                searchRangeRec(childId, qId, r, dd[j], newMind, newS, res);
            }
        }
    }

    // ------------------------
    //  Altura
    // ------------------------
    int heightRec(int nodeId) const
    {
        const Node &N = nodes[nodeId];
        int maxH = 0;
        for (int childId : N.children)
            maxH = std::max(maxH, heightRec(childId));
        return 1 + maxH;
    }
};

#endif // SAT_HPP
