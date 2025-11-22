// dindex.hpp
// Implementación del D-index (familia D-index) para benchmarking
// Siguiendo la descripción de Chen et al. (2022), sección 5.6 (D-index family) 
// y el diseño original de Dohnal et al. (D-Index: Distance searching index...) 
//
// Características principales:
//
//  - Objetos almacenados en un Random Access File (RAF) en disco.
//  - Pivotes seleccionados del conjunto de datos (numPivots = L).
//  - Construcción multi-nivel basada en ρ-split (bandas de exclusión de ancho rho).
//  - Cada objeto se almacena en EXACTAMENTE un bucket:
//        * En el primer nivel donde cae fuera de la banda [d_med - ρ, d_med + ρ].
//        * Si nunca sale de la banda, va al bucket de exclusión global (Eh).
//  - Cada bucket mantiene, para cada nivel, un intervalo [a_i, b_i] de distancias a su pivote,
//    lo que permite aplicar el pruning del D-index usando ||·||_∞ sobre el espacio de pivotes.
//
//  - Se ha eliminado el almacenamiento permanente de distancias por objeto (`mappedCache`),
//    reduciendo significativamente el consumo máximo de RAM.
//
// NOTA: Este código está pensado para ser usado en modo "de investigación / benchmarking" como en Chen (2022):
//       * constructor: DIndex(rafFile, db, numPivots, rho)
//       * build(objects)
//       * MRQ(queryId, radius) -> candidatos
//       * MkNN(queryId, k) -> k vecinos más cercanos (id, distancia)
//
// Requisitos mínimos de ObjectDB:
//    struct ObjectDB {
//        double distance(uint64_t a, uint64_t b) const;
//    };
//

#pragma once
#include <bits/stdc++.h>
#include "../../objectdb.hpp"

using namespace std;

/*** ---------- Data object & RAF ---------- ***/

struct DataObject {
    uint64_t id;
    vector<double> payload;
};

class RAF {
    string filename;
    unordered_map<uint64_t, streampos> offsets;
    mutable long long pageReads = 0;  // contador lógico de lecturas de página

public:
    RAF(const string &fname) : filename(fname) {
        ofstream ofs(filename, ios::binary | ios::trunc);
    }

    streampos append(const DataObject &o) {
        ofstream ofs(filename, ios::binary | ios::app);
        streampos pos = ofs.tellp();
        ofs.write((const char*)&o.id, sizeof(o.id));
        uint64_t len = o.payload.size();
        ofs.write((const char*)&len, sizeof(len));
        ofs.write((const char*)o.payload.data(), len * sizeof(double));
        ofs.close();
        offsets[o.id] = pos;
        return pos;
    }

    DataObject read(uint64_t id) const {
        auto it = offsets.find(id);
        if (it == offsets.end()) throw runtime_error("RAF: id not found");

        pageReads++;  // contar lectura lógica

        ifstream ifs(filename, ios::binary);
        ifs.seekg(it->second);
        DataObject o;
        ifs.read((char*)&o.id, sizeof(o.id));
        uint64_t len;
        ifs.read((char*)&len, sizeof(len));
        o.payload.assign(len, 0.0);
        ifs.read((char*)o.payload.data(), len * sizeof(double));
        ifs.close();
        return o;
    }

    bool has(uint64_t id) const {
        return offsets.find(id) != offsets.end();
    }

    long long get_pageReads() const { return pageReads; }
    void clear_pageReads() { pageReads = 0; }
};

/*** ---------- Pivot table & mapping ---------- ***/

struct PivotTable {
    vector<DataObject> pivots;
    ObjectDB* db = nullptr;
    mutable long long compDist = 0;  // contador de cálculos de distancia

    PivotTable() = default;
    PivotTable(ObjectDB *database) : db(database) {}

    void setDB(ObjectDB *database) { db = database; }

    void selectRandomPivots(const vector<DataObject>& objs,
                            size_t l,
                            uint64_t seed = 42) {
        pivots.clear();
        if (l == 0) return;
        mt19937_64 rng(seed);
        vector<size_t> idx(objs.size());
        iota(idx.begin(), idx.end(), 0);
        shuffle(idx.begin(), idx.end(), rng);
        for (size_t i = 0; i < l && i < idx.size(); ++i)
            pivots.push_back(objs[idx[i]]);
    }

    // Calcula distancias de un objeto a TODOS los pivotes
    vector<double> mapObject(uint64_t objId) const {
        vector<double> m;
        m.reserve(pivots.size());
        for (const auto &p : pivots) {
            double d = db->distance(objId, p.id);
            compDist++;
            m.push_back(d);
        }
        return m;
    }

    long long get_compDist() const { return compDist; }
    void clear_compDist() { compDist = 0; }
};

/*** ---------- D-index: estructuras internas ---------- ***/

// Información por nivel del índice (pivote + mediana de distancias)
struct LevelSpec {
    uint64_t pivotId;
    double d_med;
};

// Info de cada bucket: intervalos por nivel + lista de IDs
struct BucketInfo {
    // Para cada nivel i, intervalo [a_i, b_i] de distancias al pivot_i
    // (b_i puede ser +inf para zonas "derecha").
    vector<pair<double,double>> perLevelInterval;
    vector<uint64_t> objectIds;
};

class DIndex {
    ObjectDB* db;
    RAF raf;
    PivotTable pt;
    size_t numLevels; // = número de pivotes usados
    double rho;

    // Un LevelSpec por nivel (pivote + mediana)
    vector<LevelSpec> levels;

    // key: string de longitud numLevels, con caracteres en {'L','R','-'}
    //      '-': objeto estuvo en banda de exclusión en ese nivel
    //      'L': objeto cayó en "left" (d < d_med - rho) y se separó en ese nivel
    //      'R': objeto cayó en "right" (d > d_med + rho) y se separó en ese nivel
    //
    // El bucket de exclusión global Eh usa key = string(numLevels, '-')
    unordered_map<string, BucketInfo> buckets;

    // ---------- Helpers internos ----------

    // Devuelve índice del primer nivel donde key tiene 'L' o 'R'; si no hay, numLevels.
    size_t firstSeparatingLevel(const string &key) const {
        for (size_t i = 0; i < key.size(); ++i) {
            if (key[i] == 'L' || key[i] == 'R') return i;
        }
        return key.size();
    }

    // Construye los intervalos de un bucket dado su key.
    // Sigue la idea de Dohnal/Chen:
    //  - Para niveles anteriores al de separación: banda [d_med - rho, d_med + rho]
    //  - En el nivel de separación: [0, d_med - rho] o [d_med + rho, +inf)
    //  - En niveles posteriores: sin restricción [0, +inf)
    BucketInfo makeBucketIntervalsForKey(const string &key) const {
        BucketInfo b;
        b.perLevelInterval.assign(numLevels,
                                  {0.0, numeric_limits<double>::infinity()});

        size_t sepLevel = firstSeparatingLevel(key);

        if (sepLevel == numLevels) {
            // Bucket de exclusión global Eh: en TODOS los niveles estuvo en la banda
            for (size_t lvl = 0; lvl < numLevels; ++lvl) {
                double dmed = levels[lvl].d_med;
                b.perLevelInterval[lvl].first  = max(0.0, dmed - rho);
                b.perLevelInterval[lvl].second = dmed + rho;
            }
            return b;
        }

        // Niveles anteriores al de separación → zona de exclusión (banda)
        for (size_t lvl = 0; lvl < sepLevel; ++lvl) {
            double dmed = levels[lvl].d_med;
            b.perLevelInterval[lvl].first  = max(0.0, dmed - rho);
            b.perLevelInterval[lvl].second = dmed + rho;
        }

        // Nivel de separación
        {
            double dmed = levels[sepLevel].d_med;
            if (key[sepLevel] == 'L') {
                b.perLevelInterval[sepLevel].first  = 0.0;
                b.perLevelInterval[sepLevel].second = max(0.0, dmed - rho);
            } else { // 'R'
                b.perLevelInterval[sepLevel].first  = dmed + rho;
                b.perLevelInterval[sepLevel].second =
                    numeric_limits<double>::infinity();
            }
        }

        // Niveles posteriores -> quedan en [0,+inf) (sin restricción adicional)
        return b;
    }

    // Distancia mínima de un escalar x al intervalo [a,b]
    static double minDistToInterval(double x, pair<double,double> interval) {
        double a = interval.first, b = interval.second;
        if (!isfinite(b)) {
            // [a, +inf)
            if (x < a) return a - x;
            return 0.0;
        }
        if (x < a) return a - x;
        if (x > b) return x - b;
        return 0.0;
    }

public:
    DIndex(const string &rafFile,
           ObjectDB *database,
           size_t L,       // número de pivotes / niveles
           double rho_)
        : db(database),
          raf(rafFile),
          pt(database),
          numLevels(L),
          rho(rho_) {}

    // ---------- Construcción ----------

    void build(vector<DataObject> &objects, uint64_t pivotSeed = 12345) {
        // 1) Volcar todos los objetos al RAF (disco)
        for (auto &o : objects) raf.append(o);

        // 2) Seleccionar pivotes (como en Chen: nº pivotes = numLevels) :contentReference[oaicite:4]{index=4}
        pt.selectRandomPivots(objects, numLevels, pivotSeed);
        if (pt.pivots.size() < numLevels) {
            throw runtime_error("DIndex: Not enough objects to select pivots");
        }

        // 3) Precalcular distancias a TODOS los pivotes (solo mientras construimos)
        //
        //    mapping[id][lvl] = d( obj_id , pivot_lvl )
        //
        //    OJO: este mapa se libera al final de build para no ocupar RAM
        unordered_map<uint64_t, vector<double>> mapping;
        mapping.reserve(objects.size() * 2);

        for (auto &o : objects) {
            vector<double> dv;
            dv.reserve(numLevels);
            for (size_t i = 0; i < numLevels; ++i) {
                double d = db->distance(o.id, pt.pivots[i].id);
                pt.compDist++;
                dv.push_back(d);
            }
            mapping[o.id] = std::move(dv);
        }

        // 4) Calcular medianas por nivel, usando SOLO los objetos que permanecen
        //    en la banda de exclusión (multilevel hashing como en Dohnal/Chen).
        levels.clear();
        buckets.clear();

        vector<uint64_t> candidates;
        candidates.reserve(objects.size());
        for (auto &o : objects) candidates.push_back(o.id);

        // Para almacenar qué objetos se separan en cada nivel
        vector<vector<uint64_t>> leftByLevel(numLevels);
        vector<vector<uint64_t>> rightByLevel(numLevels);

        for (size_t lvl = 0; lvl < numLevels; ++lvl) {
            if (candidates.empty()) {
                // ya no quedan objetos sin asignar; niveles restantes serán irrelevantes
                levels.push_back({pt.pivots[lvl].id, 0.0});
                continue;
            }

            vector<double> dists;
            dists.reserve(candidates.size());
            for (auto id : candidates) {
                dists.push_back(mapping[id][lvl]);
            }

            double d_med = 0.0;
            if (!dists.empty()) {
                size_t m = dists.size() / 2;
                nth_element(dists.begin(), dists.begin() + m, dists.end());
                d_med = dists[m];
            }

            levels.push_back({pt.pivots[lvl].id, d_med});

            // Particionar candidatos en L, R, y "middle" (exclusión)
            vector<uint64_t> nextCandidates;
            nextCandidates.reserve(candidates.size());

            for (auto id : candidates) {
                double dk = mapping[id][lvl];
                if (dk < d_med - rho) {
                    leftByLevel[lvl].push_back(id);
                } else if (dk > d_med + rho) {
                    rightByLevel[lvl].push_back(id);
                } else {
                    // permanece en la banda de exclusión -> pasa al siguiente nivel
                    nextCandidates.push_back(id);
                }
            }

            candidates.swap(nextCandidates);
        }

        // Lo que queda en candidates al final es el bucket de exclusión global Eh
        vector<uint64_t> exclusionIds = std::move(candidates);

        // 5) Crear buckets para cada nivel (L/R) y el bucket de exclusión Eh.

        // 5.1 Buckets de niveles L/R
        for (size_t lvl = 0; lvl < numLevels; ++lvl) {
            // LEFT bucket en este nivel
            if (!leftByLevel[lvl].empty()) {
                string key(numLevels, '-');
                key[lvl] = 'L';

                auto it = buckets.find(key);
                if (it == buckets.end()) {
                    BucketInfo b = makeBucketIntervalsForKey(key);
                    auto &ref = buckets[key];
                    ref.perLevelInterval = std::move(b.perLevelInterval);
                    ref.objectIds = std::move(leftByLevel[lvl]);
                } else {
                    auto &ref = it->second;
                    ref.objectIds.insert(ref.objectIds.end(),
                                         leftByLevel[lvl].begin(),
                                         leftByLevel[lvl].end());
                }
            }

            // RIGHT bucket en este nivel
            if (!rightByLevel[lvl].empty()) {
                string key(numLevels, '-');
                key[lvl] = 'R';

                auto it = buckets.find(key);
                if (it == buckets.end()) {
                    BucketInfo b = makeBucketIntervalsForKey(key);
                    auto &ref = buckets[key];
                    ref.perLevelInterval = std::move(b.perLevelInterval);
                    ref.objectIds = std::move(rightByLevel[lvl]);
                } else {
                    auto &ref = it->second;
                    ref.objectIds.insert(ref.objectIds.end(),
                                         rightByLevel[lvl].begin(),
                                         rightByLevel[lvl].end());
                }
            }
        }

        // 5.2 Bucket de exclusión global Eh (si no está vacío)
        if (!exclusionIds.empty()) {
            string key(numLevels, '-'); // todo '-' representa Eh

            auto it = buckets.find(key);
            if (it == buckets.end()) {
                BucketInfo b = makeBucketIntervalsForKey(key);
                auto &ref = buckets[key];
                ref.perLevelInterval = std::move(b.perLevelInterval);
                ref.objectIds = std::move(exclusionIds);
            } else {
                auto &ref = it->second;
                ref.objectIds.insert(ref.objectIds.end(),
                                     exclusionIds.begin(),
                                     exclusionIds.end());
            }
        }

        // 6) Liberar mapping para ahorrar memoria
        mapping.clear();
        mapping.rehash(0);
    }

    // ---------- Búsqueda de rango (MRQ) ----------
    //
    // Devuelve los IDs candidatos. El filtrado final (distancia real) lo hace el llamador
    // o MkNN. Esto imita el esquema de "traversal + filtro por triángulo" de Chen para D-index. :contentReference[oaicite:5]{index=5}
    vector<uint64_t> MRQ(uint64_t queryId, double r) {
        // Distancias de la query a TODOS los pivotes
        vector<double> qmap_local(numLevels, 0.0);
        for (size_t i = 0; i < numLevels; ++i) {
            qmap_local[i] = db->distance(queryId, pt.pivots[i].id);
            pt.compDist++;
        }

        vector<uint64_t> candidates;
        candidates.reserve(1024);

        // Para cada bucket, calculamos el lower bound usando los intervalos
        for (const auto &kv : buckets) {
            const BucketInfo &binf = kv.second;
            double LB = 0.0;

            // Lower bound en norma infinito sobre pivotes
            for (size_t lvl = 0; lvl < binf.perLevelInterval.size(); ++lvl) {
                double md = minDistToInterval(qmap_local[lvl],
                                              binf.perLevelInterval[lvl]);
                if (md > LB) LB = md;
                if (LB > r) break; // ya podemos podar el bucket
            }

            if (LB > r) continue;

            // Bucket potencialmente relevante → todos sus objetos son candidatos
            for (auto id : binf.objectIds) {
                candidates.push_back(id);
            }
        }

        return candidates;
    }

    // ---------- Búsqueda MkNN ----------
    //
    // Implementa el esquema descrito por Chen:
    //  - Primera búsqueda con radio = rho.
    //  - Si el k-ésimo vecino más cercano queda a distancia > radio,
    //    aumentamos radio y repetimos hasta estabilizar. :contentReference[oaicite:6]{index=6}
    vector<pair<uint64_t,double>> MkNN(uint64_t queryId, size_t k) {
        double radius = rho;
        vector<pair<uint64_t,double>> finalResults;

        for (int iter = 0; iter < 5; ++iter) { // limite duro de iteraciones
            vector<uint64_t> cand = MRQ(queryId, radius);

            // Verificación exacta (distancias reales)
            vector<pair<uint64_t,double>> dists;
            dists.reserve(cand.size());

            for (uint64_t id : cand) {
                double dist = db->distance(queryId, id);
                pt.compDist++;
                dists.push_back({id, dist});
            }

            sort(dists.begin(), dists.end(),
                 [](const auto &a, const auto &b) {
                     return a.second < b.second;
                 });

            if (dists.size() >= k) {
                double dk = dists[k - 1].second;
                // Si el radio fue subestimado, lo ajustamos y repetimos
                if (dk > radius + 1e-12) {
                    radius = dk;
                    finalResults.assign(dists.begin(),
                                        dists.begin() + min(k, dists.size()));
                    continue;
                } else {
                    finalResults.assign(dists.begin(),
                                        dists.begin() + k);
                    break;
                }
            } else {
                // Menos de k candidatos: expandimos radio de forma conservadora
                double newrad = radius;
                if (!dists.empty()) newrad = max(radius, dists.back().second * 2.0);
                else newrad = radius * 2.0 + 1.0;

                if (newrad <= radius + 1e-12) {
                    finalResults = dists;
                    break;
                }
                radius = newrad;
                finalResults = dists;
                continue;
            }
        }

        return finalResults;
    }

    // ---------- Accesores y estadísticas ----------

    const vector<DataObject>& getPivots() const { return pt.pivots; }

    const vector<double>& getPivotPayload(size_t idx) const {
        return pt.pivots[idx].payload;
    }

    void printStats() const {
        cout << "DIndex stats: levels=" << numLevels
             << " rho=" << rho << "\n";
        cout << "Number of buckets: " << buckets.size() << "\n";
        size_t total = 0;
        for (const auto &kv : buckets) total += kv.second.objectIds.size();
        cout << "Total indexed objects: " << total << "\n";

        vector<size_t> sizes;
        sizes.reserve(buckets.size());
        for (const auto &kv : buckets) sizes.push_back(kv.second.objectIds.size());
        sort(sizes.begin(), sizes.end(), greater<size_t>());

        for (size_t i = 0; i < min((size_t)5, sizes.size()); ++i)
            cout << " bucket " << i << " size=" << sizes[i] << "\n";
    }

    long long get_compDist() const { return pt.get_compDist(); }
    long long get_pageReads() const { return raf.get_pageReads(); }

    void clear_counters() {
        pt.clear_compDist();
        raf.clear_pageReads();
    }
};
