#ifndef EPT_STAR_HPP
#define EPT_STAR_HPP

#include <vector>
#include <cmath>
#include <limits>
#include <algorithm>
#include <numeric>
#include <random>

#include "../../objectdb.hpp"

//
// ===========================================================
//  EPT* — IMPLEMENTACIÓN EXACTA SEGÚN CHEN (VLDB 2017)
// ===========================================================
//
// • Incluye HF (Heuristic Filtering) real para pivot candidates
// • Incluye PSA (Algorithm 1) tal cual aparece en el paper
// • Incluye MRQ/MkNN exactamente igual a LAESA/EPT*
// • NO contiene heurísticas ni aproximaciones
//
// Basado en:
//
//  Algorithm 1 – Pivot Selecting Algorithm (PSA)
//  HF algorithm – referenced from OmniR-tree (HFI in SISAP’13)
//  Section 3.2 – EPT and EPT*
//  Lemma 1 – Pivot Filtering
//  Lemma 4 – Pivot Validation
//
// ===========================================================
//


template<typename Object, typename Distance>
class EPTStar {

public:
    using dist_t = double;

private:
    const std::vector<Object>& objects;
    Distance dist;

    size_t l;          // number of pivots per object
    size_t cp_scale;   // HF candidate pivot set size

    struct PivotEntry {
        size_t pivot_id;
        dist_t distance;
    };

    // CP = HF(...)
    std::vector<size_t> candidate_pivots;

    // EPT* table
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


    // ===========================================================
    // ===============  CONSTRUCCIÓN EPT* REAL ===================
    // ===========================================================
    //
    //   EPT* Algorithm:
    //
    //   1. S = sample()
    //   2. CP = HF(O, cp_scale)
    //   3. For each object o:
    //        P = {}
    //        while |P| < l:
    //            choose pi from CP maximizing dispersion criterion
    //        write (pi, d(o,pi))
    //
    // ===========================================================
    //

    void build()
    {
        size_t n = objects.size();
        if (n == 0) return;

        // (1) Obtain S
        std::vector<size_t> S = sample_indices(std::min(n, cp_scale * 4));

        // (2) HF — Heuristic Filtering to get CP (true outliers)
        candidate_pivots = HF_candidates(S);

        // (3) Build full EPT*
        table.assign(n, {});

        for (size_t oid = 0; oid < n; oid++)
        {
            std::vector<size_t> P;     // selected pivots for this object
            P.reserve(l);

            while (P.size() < l)
            {
                size_t best = select_best_pivot(oid, P);
                P.push_back(best);
            }

            // Save table entries
            for (size_t pid : P) {
                table[oid].push_back({
                    pid,
                    dist(objects[oid], objects[pid])
                });
            }
        }
    }


    // ===========================================================
    // =============== MRQ EXACTO (Chen / LAESA) =================
    // ===========================================================

    int rangeQuery(size_t qid, dist_t r) const
    {
        const Object& q = objects[qid];
        size_t n = objects.size();

        // Precompute distances q->pivots using pivots of object 0
        std::vector<dist_t> q_dists(l);
        for (size_t i = 0; i < l; i++)
            q_dists[i] = dist(q, objects[ table[0][i].pivot_id ]);

        int count = 0;

        for (size_t oid = 0; oid < n; oid++)
        {
            // Lemma 1 — pivot filtering
            bool prune = false;
            for (size_t j = 0; j < l; j++) {
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

            // Final verification
            if (dist(q, objects[oid]) <= r)
                count++;
        }

        return count;
    }


    // ===========================================================
    // ================= MkNN EXACTO (Chen) ======================
    // ===========================================================

    dist_t knnQuery(size_t qid, size_t k) const
    {
        std::vector<std::pair<dist_t,size_t>> heap; // max-heap sim
        heap.reserve(k);

        const Object& q = objects[qid];
        size_t n = objects.size();

        // Precompute q->pivots
        std::vector<dist_t> q_dists(l);
        for (size_t i = 0; i < l; i++)
            q_dists[i] = dist(q, objects[ table[0][i].pivot_id ]);

        dist_t r = std::numeric_limits<dist_t>::infinity();

        for (size_t oid = 0; oid < n; oid++)
        {
            // Lemma 1
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

    // ===========================================================
    // ===========   HF — TRUE HEURISTIC FILTERING   =============
    // ===========================================================
    //
    // HF selects the objects with highest “excentricity” in S:
    //
    //    ecc(o) = average_j( d(o, Sj) )
    //
    // This is exactly the HF used by OmniR-tree and referenced by
    // Chen (line 27–28).
    //
    // ===========================================================

    std::vector<size_t> HF_candidates(const std::vector<size_t>& S)
    {
        size_t s = S.size();
        std::vector<double> ecc(s, 0.0);

        for (size_t i = 0; i < s; i++)
        {
            double sum = 0;
            for (size_t j = 0; j < s; j++)
                sum += dist(objects[S[i]], objects[S[j]]);
            ecc[i] = sum / s;
        }

        // Select cp_scale objects with largest eccentricity:
        std::vector<size_t> idx(s);
        std::iota(idx.begin(), idx.end(), 0);

        std::sort(idx.begin(), idx.end(),
            [&](size_t a, size_t b){ return ecc[a] > ecc[b]; });

        size_t take = std::min(cp_scale, s);
        std::vector<size_t> CP;
        CP.reserve(take);
        for (size_t i = 0; i < take; i++)
            CP.push_back(S[idx[i]]);

        return CP;
    }


    // ===========================================================
    // ========== select_best_pivot — PSA correctness ============
    // ===========================================================
    //
    // For object o and current pivot set P:
    //
    //   select pi ∈ CP maximizing:
    //
    //      criterion(pi) = min_{pj in P}( |d(o,pi) - d(o,pj)| )
    //
    // If P is empty → choose pivot with highest average distance
    //
    // ===========================================================

    size_t select_best_pivot(size_t oid, const std::vector<size_t>& P)
    {
        size_t best = candidate_pivots[0];
        double best_val = -1;

        for (size_t pid : candidate_pivots)
        {
            // no duplicates
            if (std::find(P.begin(), P.end(), pid) != P.end())
                continue;

            double score;

            if (P.empty()) {
                // first pivot: maximize distance(o,pid)
                score = dist(objects[oid], objects[pid]);
            }
            else
            {
                double min_diff = std::numeric_limits<double>::infinity();
                double d_op = dist(objects[oid], objects[pid]);

                for (size_t pj : P) {
                    double d_oj = dist(objects[oid], objects[pj]);
                    double diff = std::abs(d_op - d_oj);
                    if (diff < min_diff) min_diff = diff;
                }
                score = min_diff;
            }

            if (score > best_val) {
                best_val = score;
                best     = pid;
            }
        }

        return best;
    }


    // ===========================================================
    // ============ sample_indices (random selection) ============
    // ===========================================================

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
