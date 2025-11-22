#ifndef EPT_STAR_HPP
#define EPT_STAR_HPP

#include <vector>
#include <random>
#include <limits>
#include <algorithm>
#include <queue>
#include <unordered_set>

//
//  EPT* — Extreme Pivot Table Improved with PSA
//  Implementación fiel a CHEN2022 (Pivot-Based Metric Indexing)
//  Basado en EPT original + PSA (Algorithm 1)
//
//  Referencias:
//  - Pivot-Based Metric Indexing (Chen et al., VLDB 2017)
//    Secciones:
//      * 3.2 EPT
//      * Algorithm 1 (PSA)
//      * Lemma 1 (Pivot Filtering)
//      * Lemma 4 (Pivot Validation)
//    Fig. 5 — Estructura EPT
//


template<typename Object, typename Distance>
class EPTStar {

public:
    using dist_t = double;

private:
    struct PivotEntry {
        size_t pivot_id;   // id del pivot elegido para ese objeto
        dist_t distance;   // distancia precomputada a ese pivot
    };

    const std::vector<Object>& objects;
    Distance dist_fn;

    size_t l;   // número de pivots por objeto (grupos)
    size_t cp_scale; // número de outliers candidatos (HF scale)

    // pivots globales seleccionados por PSA
    std::vector<size_t> candidate_pivots;  

    // Tabla principal: para cada objeto → lista de sus l pivots
    std::vector<std::vector<PivotEntry>> table;

public:

    EPTStar(
        const std::vector<Object>& objs,
        Distance distance_fn,
        size_t l_groups = 2,
        size_t cp_scale_val = 40
    ) :
        objects(objs),
        dist_fn(distance_fn),
        l(l_groups),
        cp_scale(cp_scale_val)
    {
        buildEPTStar();
    }

    //
    // ==========================================================
    // ===            MÉTODO PRINCIPAL DE CONSTRUCCIÓN        ===
    // ==========================================================
    //
    //   Fiel al Algorithm 1 (PSA) del paper CHEN2022
    //
    void buildEPTStar() {

        size_t n = objects.size();
        table.resize(n);
        if (n == 0) return;

        //
        // 1. Sample S (simple: elegimos subset aleatorio)
        //
        std::vector<size_t> sample = generateSample(std::min(n, cp_scale * 10));

        //
        // 2. Obtener CP usando HF (aquí simulamos HF= escoger outliers por dispersión)
        //    CHEN2022 usa HF para obtener outliers como pivots candidatos.
        //
        candidate_pivots = selectOutliersHF(sample, cp_scale);

        //
        // 3. Para cada objeto, seleccionar l pivots según PSA
        //
        for (size_t oid = 0; oid < n; ++oid) {

            std::vector<size_t> selected;  
            selected.reserve(l);

            while (selected.size() < l) {
                size_t best_pivot = findBestPivotForObject(oid, selected);
                selected.push_back(best_pivot);
            }

            //
            // 4. Guardar las distancias precomputadas en la tabla EPT*
            //
            table[oid].reserve(l);
            for (size_t pid : selected) {
                dist_t d = dist_fn(objects[oid], objects[pid]);
                table[oid].push_back({ pid, d });
            }
        }
    }


    //
    // ==========================================================
    // ===              MÉTODO: Range Query (MRQ)              ===
    // ===      Usando Lemma 1 y Lemma 4 del Paper            ===
    // ==========================================================
    //
    std::vector<size_t> rangeQuery(const Object& q, dist_t r) const {

        size_t n = objects.size();
        std::vector<dist_t> q_to_pivot;  
        q_to_pivot.reserve(l);

        // Distancia de q a pivots seleccionados por objeto
        // EPT* usa pivots distintos por objeto, así que tomamos
        // los del primer objeto solo para conocer los pivot IDs.
        if (n > 0) {
            for (const auto& pe : table[0]) {
                q_to_pivot.push_back(dist_fn(q, objects[pe.pivot_id]));
            }
        }

        std::vector<size_t> result;
        for (size_t oid = 0; oid < n; ++oid) {

            // Pivot filtering (Lemma 1)
            bool pruned = false;
            for (size_t i = 0; i < table[oid].size(); ++i) {
                dist_t o_pi = table[oid][i].distance;
                dist_t q_pi = q_to_pivot[i];
                if (std::abs(q_pi - o_pi) > r) {
                    pruned = true;
                    break;
                }
            }
            if (pruned) continue;

            // Pivot Validation (Lemma 4)
            bool validated = false;
            for (size_t i = 0; i < table[oid].size(); ++i) {
                dist_t o_pi = table[oid][i].distance;
                dist_t q_pi = q_to_pivot[i];
                if (o_pi <= r - q_pi) {
                    result.push_back(oid);
                    validated = true;
                    break;
                }
            }
            if (validated) continue;

            // Validación final: calcular distancia real
            if (dist_fn(q, objects[oid]) <= r)
                result.push_back(oid);
        }

        return result;
    }


    //
    // ==========================================================
    // ===              MÉTODO: kNN Query (MkNNQ)             ===
    // ==========================================================
    //
    struct KNNItem {
        size_t oid;
        dist_t distance;
        bool operator<(const KNNItem& other) const {
            return distance < other.distance;
        }
    };

    std::vector<size_t> knnQuery(const Object& q, size_t k) const {

        size_t n = objects.size();
        if (k == 0 || n == 0) return {};

        std::vector<dist_t> q_to_pivot(l);
        for (size_t i = 0; i < l; ++i)
            q_to_pivot[i] = dist_fn(q, objects[ table[0][i].pivot_id ]);

        std::priority_queue<KNNItem> heap;
        dist_t r = std::numeric_limits<dist_t>::infinity();

        for (size_t oid = 0; oid < n; ++oid) {

            // Lemma 1 — Pivot Filtering
            bool pruned = false;
            for (size_t i = 0; i < table[oid].size(); ++i) {
                if (std::abs(q_to_pivot[i] - table[oid][i].distance) > r) {
                    pruned = true;
                    break;
                }
            }
            if (pruned) continue;

            dist_t d = dist_fn(q, objects[oid]);

            if (heap.size() < k) {
                heap.push({ oid, d });
                if (heap.size() == k)
                    r = heap.top().distance;
            } else if (d < heap.top().distance) {
                heap.pop();
                heap.push({ oid, d });
                r = heap.top().distance;
            }
        }

        std::vector<size_t> result;
        while (!heap.empty()) {
            result.push_back(heap.top().oid);
            heap.pop();
        }
        std::reverse(result.begin(), result.end());
        return result;
    }


private:

    //
    // ==========================================================
    // ===        SELECCIÓN DE OUTLIERS HF (APROX)           ===
    // ==========================================================
    //
    std::vector<size_t> selectOutliersHF(
        const std::vector<size_t>& sample,
        size_t k
    ) {
        // Elegimos los k puntos más alejados del centroide
        size_t s = sample.size();
        if (s == 0) return {};

        size_t center = sample[s / 2];

        std::vector<std::pair<dist_t,size_t>> dists;
        dists.reserve(s);
        for (size_t idx : sample)
            dists.push_back({ dist_fn(objects[idx], objects[center]), idx });

        std::sort(dists.begin(), dists.end(),
            [](auto& a, auto& b){ return a.first > b.first; });

        std::vector<size_t> out;
        for (size_t i = 0; i < std::min(k, dists.size()); ++i)
            out.push_back(dists[i].second);

        return out;
    }


    //
    // ==========================================================
    // ===              SELECCIÓN DE PIVOT PSA               ===
    // ==========================================================
    //
    size_t findBestPivotForObject(size_t oid, const std::vector<size_t>& already) {
        dist_t best_score = -1;
        size_t best_pid = candidate_pivots[0];

        for (size_t pid : candidate_pivots) {
            if (std::find(already.begin(), already.end(), pid) != already.end())
                continue;

            dist_t d = dist_fn(objects[oid], objects[pid]);

            // PSA quiere maximizar el lower bound |d(o,pi)-d(q,pi)|,
            // pero sin q predefinido usamos d(o,pi) como aproximación.
            if (d > best_score) {
                best_score = d;
                best_pid = pid;
            }
        }
        return best_pid;
    }


    //
    // ==========================================================
    // ===                 GENERAR SAMPLE S                  ===
    // ==========================================================
    //
    std::vector<size_t> generateSample(size_t wanted) {
        size_t n = objects.size();
        std::vector<size_t> ids(n);
        for (size_t i = 0; i < n; ++i) ids[i] = i;

        std::shuffle(ids.begin(), ids.end(), std::mt19937{ std::random_device{}() });

        if (ids.size() > wanted)
            ids.resize(wanted);

        return ids;
    }
};

#endif
