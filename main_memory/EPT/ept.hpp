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

    size_t l;          // number of pivots per object
    size_t cp_scale;   // HF candidate pivot set size

    struct PivotEntry {
        size_t pivot_id;
        dist_t distance;
    };

    std::vector<size_t> candidate_pivots;

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

        std::vector<size_t> S = sample_indices(std::min(n, cp_scale * 4));

        candidate_pivots = HF_candidates(S);

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

            for (size_t pid : P) {
                table[oid].push_back({
                    pid,
                    dist(objects[oid], objects[pid])
                });
            }
        }
    }

    int rangeQuery(size_t qid, dist_t r) const
    {
        const Object& q = objects[qid];
        size_t n = objects.size();

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

            if (dist(q, objects[oid]) <= r)
                count++;
        }

        return count;
    }

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
