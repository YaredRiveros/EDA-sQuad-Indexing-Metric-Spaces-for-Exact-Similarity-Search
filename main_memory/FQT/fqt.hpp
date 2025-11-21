#ifndef FQT_HPP
#define FQT_HPP

#include "objectdb.hpp"
#include <vector>
#include <queue>
#include <algorithm>
#include <cmath>
#include <memory>

// ============================================================
// FQT - Fixed Queries Tree (Wrapper C++ sobre ObjectDB)
// ============================================================
// Paper: "FQT utilizes the same pivot at the same level"
// Paper: "FQT is an unbalanced tree"
// ============================================================

class FQT {
private:
    struct FQTNode {
        bool is_leaf;
        std::vector<int> bucket;  // objetos en nodo hoja
        
        struct Child {
            double dist_threshold;  // límite inferior del rango
            std::unique_ptr<FQTNode> node;
        };
        std::vector<Child> children;  // hijos en nodo interno
        
        FQTNode() : is_leaf(true) {}
    };

    ObjectDB* db;
    int bucket_size;
    int arity;
    int height;
    long long compdists;
    
    std::vector<int> pivots;  // pivotes por nivel (uno por nivel)
    std::unique_ptr<FQTNode> root;

    // Construcción recursiva del árbol
    std::unique_ptr<FQTNode> buildRecursive(std::vector<int>& objects, int depth) {
        auto node = std::make_unique<FQTNode>();
        
        // Crear hoja si pocos objetos
        if ((int)objects.size() <= bucket_size) {
            node->is_leaf = true;
            node->bucket = objects;
            return node;
        }
        
        node->is_leaf = false;
        
        // Seleccionar pivote para este nivel
        if (depth >= height) {
            // Añadir nuevo pivote
            int pivot_idx = rand() % objects.size();
            int pivot = objects[pivot_idx];
            objects.erase(objects.begin() + pivot_idx);
            pivots.push_back(pivot);
            height++;
        }
        
        if (objects.empty()) return node;
        
        int pivot = pivots[depth];
        
        // Calcular distancias al pivote
        std::vector<std::pair<double, int>> dists;
        for (int obj : objects) {
            double d = db->distance(pivot, obj);
            compdists++;
            dists.push_back({d, obj});
        }
        
        // Ordenar por distancia
        std::sort(dists.begin(), dists.end());
        
        // Dividir en arity rangos
        double min_d = dists[0].first;
        double max_d = dists.back().first;
        double step = (max_d - min_d) / arity;
        
        if (step < 1e-9) step = 1.0;  // evitar división por 0
        
        // Particionar objetos
        std::vector<std::vector<int>> partitions(arity);
        for (auto& [d, obj] : dists) {
            int bucket_idx = std::min((int)((d - min_d) / step), arity - 1);
            partitions[bucket_idx].push_back(obj);
        }
        
        // Construir hijos recursivamente
        double threshold = min_d;
        for (int i = 0; i < arity; i++) {
            if (!partitions[i].empty()) {
                typename FQTNode::Child child;
                child.dist_threshold = threshold;
                child.node = buildRecursive(partitions[i], depth + 1);
                node->children.push_back(std::move(child));
            }
            threshold += step;
        }
        
        return node;
    }

    // Búsqueda por rango recursiva
    int rangeRecursive(FQTNode* node, int query, double radius, int depth) {
        if (node->is_leaf) {
            int count = 0;
            for (int obj : node->bucket) {
                double d = db->distance(query, obj);
                compdists++;
                if (d <= radius) count++;
            }
            return count;
        }
        
        // Nodo interno: calcular distancia al pivote del nivel
        double d_pivot = db->distance(query, pivots[depth]);
        compdists++;
        
        int count = 0;
        
        // Visitar hijos relevantes según poda triangular
        for (auto& child : node->children) {
            // Verificar si el rango del hijo puede contener resultados
            bool may_contain = true;
            
            // Poda simple: si hay siguiente hijo, verificar límite superior
            auto it = &child;
            auto next_it = it + 1;
            bool has_next = (next_it < &node->children.back() + 1);
            
            double lower = child.dist_threshold;
            double upper = has_next ? next_it->dist_threshold : d_pivot + radius + 1;
            
            // Poda por desigualdad triangular
            if (d_pivot + radius < lower || d_pivot - radius > upper) {
                may_contain = false;
            }
            
            if (may_contain) {
                count += rangeRecursive(child.node.get(), query, radius, depth + 1);
            }
        }
        
        return count;
    }

    // k-NN recursivo (best-first con priority queue)
    void knnRecursive(int query, int k, std::vector<std::pair<double, int>>& results) {
        // Priority queue: (distancia_minima, node, depth)
        struct QueueEntry {
            double min_dist;
            FQTNode* node;
            int depth;
            
            bool operator>(const QueueEntry& other) const {
                return min_dist > other.min_dist;
            }
        };
        
        std::priority_queue<QueueEntry, std::vector<QueueEntry>, std::greater<QueueEntry>> pq;
        
        // Pre-calcular distancias a todos los pivotes
        std::vector<double> pivot_dists(height);
        for (int i = 0; i < height; i++) {
            pivot_dists[i] = db->distance(query, pivots[i]);
            compdists++;
        }
        
        // Iniciar con la raíz
        pq.push({0.0, root.get(), 0});
        
        while (!pq.empty()) {
            auto [min_dist, node, depth] = pq.top();
            pq.pop();
            
            // Poda: si ya tenemos k resultados y este nodo está más lejos
            if ((int)results.size() >= k && min_dist > results.back().first) {
                continue;
            }
            
            if (node->is_leaf) {
                // Examinar todos los objetos del bucket
                for (int obj : node->bucket) {
                    double d = db->distance(query, obj);
                    compdists++;
                    
                    results.push_back({d, obj});
                    std::sort(results.begin(), results.end());
                    if ((int)results.size() > k) {
                        results.pop_back();
                    }
                }
            } else {
                // Nodo interno: añadir hijos a la cola
                double d_pivot = pivot_dists[depth];
                
                for (auto& child : node->children) {
                    // Estimar distancia mínima al hijo usando desigualdad triangular
                    double child_min = std::max(0.0, std::abs(d_pivot - child.dist_threshold));
                    
                    // Podar si ya tenemos k resultados mejores
                    if ((int)results.size() >= k && child_min > results.back().first) {
                        continue;
                    }
                    
                    pq.push({child_min, child.node.get(), depth + 1});
                }
            }
        }
    }

public:
    FQT(ObjectDB* database, int bucket_sz, int ar)
        : db(database), bucket_size(bucket_sz), arity(ar), height(0), compdists(0) {}

    void build() {
        compdists = 0;
        height = 0;
        pivots.clear();
        
        // Crear lista de todos los objetos
        std::vector<int> all_objects;
        for (int i = 0; i < db->size(); i++) {
            all_objects.push_back(i);
        }
        
        // Construir árbol
        root = buildRecursive(all_objects, 0);
    }

    int range(int query, double radius) {
        long long old_compdists = compdists;
        
        // Contar distancias a pivotes
        for (int pivot : pivots) {
            double d = db->distance(query, pivot);
            compdists++;
            if (d <= radius) {
                // El pivote es resultado pero no lo contamos aquí 
                // (ya está en algún bucket)
            }
        }
        
        int count = rangeRecursive(root.get(), query, radius, 0);
        return count;
    }

    double knn(int query, int k) {
        std::vector<std::pair<double, int>> results;
        knnRecursive(query, k, results);
        
        if (results.empty()) return 0.0;
        return results.back().first;  // k-ésima distancia
    }

    int getHeight() const { return height; }
    long long getCompdists() const { return compdists; }
    void resetCompdists() { compdists = 0; }
};

#endif // FQT_HPP
