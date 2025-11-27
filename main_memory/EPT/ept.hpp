#ifndef EPT_STAR_HPP
#define EPT_STAR_HPP

#include <vector>
#include <cmath>
#include <limits>
#include <algorithm>
#include <numeric>
#include <random>

#include "../../objectdb.hpp"


template<typename Object, typename Distance>
class EPTStar {

public:
    using dist_t = double;

private:
    const std::vector<Object>& objects;
    Distance dist;

    size_t l;          // number of pivots (GLOBAL, shared by all objects)
    size_t cp_scale;   // PSA candidate pivot set size

    struct PivotEntry {
        size_t pivot_id;   // índice global del pivot en 'objects'
        dist_t distance;   // d(o, pivot_id)
    };

    // Conjunto de candidatos (PSA, paso 1)
    std::vector<size_t> candidate_pivots;

    // Pivotes globales definitivos (PSA, paso 2)
    std::vector<size_t> global_pivots;

    // Tabla: para cada objeto oid, sus distancias a los l pivotes globales
    // table[oid][j] => distancia a global_pivots[j]
    std::vector<std::vector<PivotEntry>> table;


public:

    EPTStar(
        const std::vector<Object>& objs,
        Distance dist_fn,
        size_t l_pivots = 5,
        size_t cp_scale_val = 40
    )
        : objects(objs),
          dist(dist_fn),
          l(l_pivots),
          cp_scale(cp_scale_val)
    {
        build();
    }

    void build()
    {
        size_t n = objects.size();
        if (n == 0) return;

        // ---------------------------------------------------------------------
        // PSA paso 1: elegir candidatos a pivote desde una muestra S
        // ---------------------------------------------------------------------
        std::vector<size_t> S = sample_indices(std::min(n, cp_scale * 4));
        candidate_pivots = HF_candidates(S);   // CP en el paper

        if (candidate_pivots.empty()) return;

        // Por seguridad, ajustar l si hay menos candidatos que pivotes deseados
        if (candidate_pivots.size() < l)
            l = candidate_pivots.size();

        // ---------------------------------------------------------------------
        // PSA paso 2: seleccionar l pivotes globales bien dispersos (greedy)
        // ---------------------------------------------------------------------
        global_pivots = select_global_pivots_PSA(candidate_pivots);

        // Aseguramos coherencia: global_pivots.size() == l
        if (global_pivots.size() < l)
            l = global_pivots.size();

        // ---------------------------------------------------------------------
        // Construir tabla: para cada objeto, distancias a TODOS los pivotes globales
        // ---------------------------------------------------------------------
        table.assign(n, std::vector<PivotEntry>());
        for (size_t oid = 0; oid < n; oid++)
        {
            table[oid].reserve(l);
            for (size_t j = 0; j < l; j++) {
                size_t pid = global_pivots[j];
                dist_t d = dist(objects[oid], objects[pid]);
                table[oid].push_back({ pid, d });
            }
        }
    }

    int rangeQuery(size_t qid, dist_t r) const
    {
        const Object& q = objects[qid];
        size_t n = objects.size();
        if (n == 0 || l == 0 || table.empty()) return 0;

        // Distancias query → pivotes GLOBALes (mismos para todos los objetos)
        std::vector<dist_t> q_dists(l);
        for (size_t i = 0; i < l; i++) {
            size_t pid = table[0][i].pivot_id;       // pivot i-ésimo global
            q_dists[i] = dist(q, objects[pid]);
        }

        int count = 0;

        for (size_t oid = 0; oid < n; oid++)
        {
            // Lemma 1 — pivot filtering
            bool prune = false;
            for (size_t j = 0; j < l; j++) {
                // table[oid][j].distance = d(o, p_j global)
                if (std::abs(q_dists[j] - table[oid][j].distance) > r) {
                    prune = true;
                    break;
                }
            }
            if (prune) continue;

            // Lemma 4 — pivot validation
            bool valid = false;
            for (size_t j = 0; j < l; j++) {
                if (table[oid][j].distance <= r - q_dists[j]) {
                    valid = true;
                    break;
                }
            }
            if (valid) {
                count++;
                continue;
            }

            // Verificación final con distancia real
            if (dist(q, objects[oid]) <= r)
                count++;
        }

        return count;
    }

    dist_t knnQuery(size_t qid, size_t k) const
    {
        std::vector<std::pair<dist_t,size_t>> heap; // max-heap simulado
        heap.reserve(k);

        const Object& q = objects[qid];
        size_t n = objects.size();
        if (n == 0 || l == 0 || table.empty() || k == 0) return 0.0;

        // Precompute q->pivots GLOBALes
        std::vector<dist_t> q_dists(l);
        for (size_t i = 0; i < l; i++) {
            size_t pid = table[0][i].pivot_id;
            q_dists[i] = dist(q, objects[pid]);
        }

        dist_t r = std::numeric_limits<dist_t>::infinity();

        for (size_t oid = 0; oid < n; oid++)
        {
            // Lemma 1 (con radio actual r)
            bool prune = false;
            for (size_t j = 0; j < l; j++) {
                if (std::abs(q_dists[j] - table[oid][j].distance) > r) {
                    prune = true;
                    break;
                }
            }
            if (prune) continue;

            dist_t d = dist(q, objects[oid]);

            if (heap.size() < k)
            {
                heap.emplace_back(d, oid);
                if (heap.size() == k) {
                    std::make_heap(heap.begin(), heap.end());
                    r = heap.front().first;
                }
            }
            else if (d < heap.front().first)
            {
                std::pop_heap(heap.begin(), heap.end());
                heap.back() = {d, oid};
                std::push_heap(heap.begin(), heap.end());
                r = heap.front().first;
            }
        }

        return (heap.size() == k) ? heap.front().first : 0.0;
    }


private:
    // -------------------------------------------------------------------------
    // HF_candidates: PSA paso 1 — genera un conjunto de candidatos CP desde S
    //
    // S = muestra de índices del dataset.
    // Para cada s ∈ S, calcula excentricidad = media de distancias a S.
    // Devuelve los cp_scale más excéntricos como candidatos a pivote.
    // -------------------------------------------------------------------------
    std::vector<size_t> HF_candidates(const std::vector<size_t>& S)
    {
        size_t s = S.size();
        std::vector<double> ecc(s, 0.0);

        for (size_t i = 0; i < s; i++)
        {
            double sum = 0.0;
            for (size_t j = 0; j < s; j++)
                sum += dist(objects[S[i]], objects[S[j]]);
            ecc[i] = sum / static_cast<double>(s);
        }

        std::vector<size_t> idx(s);
        std::iota(idx.begin(), idx.end(), 0);

        std::sort(idx.begin(), idx.end(),
            [&](size_t a, size_t b){ return ecc[a] > ecc[b]; });

        size_t take = std::min(cp_scale, s);
        std::vector<size_t> CP;
        CP.reserve(take);
        for (size_t i = 0; i < take; i++)
            CP.push_back(S[idx[i]]);

        return CP; // CP = candidate pivots
    }

    // -------------------------------------------------------------------------
    // PSA paso 2: selección greedy de l pivotes globales desde CP
    //
    //  - Primer pivot: el de mayor excentricidad (media de distancias a CP).
    //  - Siguientes: farthest-first, maximizando la distancia mínima a pivotes ya elegidos.
    // -------------------------------------------------------------------------
    std::vector<size_t> select_global_pivots_PSA(const std::vector<size_t>& CP)
    {
        std::vector<size_t> P;
        if (CP.empty() || l == 0) return P;

        size_t s = CP.size();

        // Recalcular excentricidades dentro de CP (podríamos reutilizar, pero es barato)
        std::vector<double> ecc(s, 0.0);
        for (size_t i = 0; i < s; i++) {
            double sum = 0.0;
            for (size_t j = 0; j < s; j++) {
                sum += dist(objects[CP[i]], objects[CP[j]]);
            }
            ecc[i] = sum / static_cast<double>(s);
        }

        // 1) Primer pivot: el de máxima excentricidad
        size_t best_idx = 0;
        double best_val = ecc[0];
        for (size_t i = 1; i < s; i++) {
            if (ecc[i] > best_val) {
                best_val = ecc[i];
                best_idx = i;
            }
        }
        P.push_back(CP[best_idx]);

        // Marcar usados
        std::vector<bool> used(s,false);
        used[best_idx] = true;

        // 2) Siguientes pivotes: greedy farthest-first
        while (P.size() < l)
        {
            double best_score = -1.0;
            size_t best_cand  = 0;
            bool found = false;

            for (size_t i = 0; i < s; i++) {
                if (used[i]) continue;

                // Distancia mínima de CP[i] a los pivotes ya elegidos
                double min_d = std::numeric_limits<double>::infinity();
                for (size_t pj : P) {
                    double d = dist(objects[CP[i]], objects[pj]);
                    if (d < min_d) min_d = d;
                }

                if (min_d > best_score) {
                    best_score = min_d;
                    best_cand  = i;
                    found = true;
                }
            }

            if (!found) break; // no quedan candidatos útiles
            used[best_cand] = true;
            P.push_back(CP[best_cand]);
        }

        return P;
    }

    // Muestra aleatoria de índices
    std::vector<size_t> sample_indices(size_t k)
    {
        size_t n = objects.size();
        std::vector<size_t> ids(n);
        std::iota(ids.begin(), ids.end(), 0);

        std::shuffle(ids.begin(), ids.end(), std::mt19937{std::random_device{}()});

        ids.resize(std::min(k, n));
        return ids;
    }

};

#endif
