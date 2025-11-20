// DIndex.cpp
// Implementación prototipo del D-index (familia D-index) basada en el fragmento del artículo.
// - RAF: Random Access File (simulado como archivo binario + índice en memoria)
// - PivotTable: pivots y mapeo (distancias precomputadas)
// - ρ-split multilevel buckets (D-index build)
// - MRQ and MkNN processing for the D-index
//
// Compilar: g++ -std=c++17 -O2 DIndex.cpp -o DIndex
// Ejecutar: ./DIndex
//
// Referencia: fragmento "5.6 D-index Family" en Chen et al. (2022). :contentReference[oaicite:4]{index=4}

#include <bits/stdc++.h>
using namespace std;

/*** ---------- Metric interface (re-usable) ---------- ***/
struct Metric {
    virtual double distance(const vector<double>& a, const vector<double>& b) const = 0;
    virtual ~Metric() = default;
};

struct Euclidean : Metric {
    double distance(const vector<double>& a, const vector<double>& b) const override {
        double s=0;
        size_t n=a.size();
        for(size_t i=0;i<n;i++){ double d=a[i]-b[i]; s+=d*d; }
        return sqrt(s);
    }
};

/*** ---------- Data object & RAF ---------- ***/
struct DataObject {
    uint64_t id;
    vector<double> payload;
};

class RAF {
    string filename;
    unordered_map<uint64_t, streampos> offsets;
public:
    RAF(const string &fname): filename(fname) {
        ofstream ofs(filename, ios::binary | ios::trunc);
        (void)ofs;
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
        if(it==offsets.end()) throw runtime_error("RAF: id not found");
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
    bool has(uint64_t id) const { return offsets.find(id) != offsets.end(); }
};

/*** ---------- Pivot table & mapping ---------- ***/
struct PivotTable {
    vector<DataObject> pivots;
    const Metric* metric = nullptr;
    PivotTable() {}
    PivotTable(const Metric *m): metric(m) {}
    void setMetric(const Metric *m) { metric = m; }

    void selectRandomPivots(const vector<DataObject>& objs, size_t l, uint64_t seed=42) {
        pivots.clear();
        if(l==0) return;
        mt19937_64 rng(seed);
        vector<size_t> idx(objs.size());
        iota(idx.begin(), idx.end(), 0);
        shuffle(idx.begin(), idx.end(), rng);
        for(size_t i=0;i<l && i<idx.size(); ++i) pivots.push_back(objs[idx[i]]);
    }

    // map object o to vector of distances to pivots (full mapping)
    vector<double> mapObject(const DataObject &o) const {
        vector<double> m; m.reserve(pivots.size());
        for(const auto &p: pivots) m.push_back(metric->distance(o.payload, p.payload));
        return m;
    }
};

/*** ---------- D-index construction ---------- ***/

/*
 We implement a multilevel ρ-split:
 For level i we have pivot p_i and computed d_med_i (median of distances of objects considered for that node).
 Given rho (same for all levels), a distance dk to pivot p_i is assigned:
   - 'L' if dk < d_med_i - rho
   - '-' if d_med_i - rho <= dk <= d_med_i + rho
   - 'R' if dk > d_med_i + rho
 The bucket key is string of chars per level (e.g., "L- R -").
*/
struct LevelSpec {
    uint64_t pivotId;   // id of pivot (referenced via RAF / pivot table)
    double d_med;       // median distance at this level (over objects considered)
};

struct BucketInfo {
    // For each level, store interval [a,b] of possible d(o,pivot)
    // If level decision was 'L' -> [0, d_med - rho)
    // If 'R' -> (d_med + rho, +inf)
    // If '-' -> [d_med - rho, d_med + rho]
    vector<pair<double,double>> perLevelInterval; // length = numLevels
    vector<uint64_t> objectIds; // list of object ids in this bucket (stored in RAF overall)
};

class DIndex {
    const Metric* metric;
    RAF raf;
    PivotTable pt;
    size_t numLevels;
    double rho;
    // levels specs (one pivot per level)
    vector<LevelSpec> levels;

    // buckets: map bucket key -> BucketInfo
    unordered_map<string, BucketInfo> buckets;

    // mapping cache: id -> vector of distances to pivots (size = numLevels)
    unordered_map<uint64_t, vector<double>> mappedCache;

public:
    DIndex(const string &rafFile, const Metric *m, size_t L, double rho_)
        : metric(m), raf(rafFile), pt(m), numLevels(L), rho(rho_) {}

    // build: objects: dataset, choose pivots randomly from objects
    void build(vector<DataObject> &objects, uint64_t pivotSeed=12345) {
        // append all objects to RAF
        for(auto &o: objects) raf.append(o);

        // choose pivots (one per level) randomly
        pt.selectRandomPivots(objects, numLevels, pivotSeed);
        if(pt.pivots.size() < numLevels) {
            throw runtime_error("Not enough objects to select pivots");
        }

        // set up levels: compute d_med per level relative to objects that will be considered
        // We will simulate top-down: initially all objects considered at level 1,
        // then compute which fall into exclusion '-' to pass to level 2, etc.

        // Precompute full distance matrix (n x L) where entry is d(o,pivot_i)
        size_t n = objects.size();
        mappedCache.clear();
        for(auto &o: objects) {
            vector<double> dv; dv.reserve(numLevels);
            for(size_t i=0;i<numLevels;i++){
                dv.push_back(metric->distance(o.payload, pt.pivots[i].payload));
            }
            mappedCache[o.id] = dv;
        }

        // start with all object ids considered for level 1
        vector<uint64_t> currentIds;
        currentIds.reserve(n);
        for(auto &o: objects) currentIds.push_back(o.id);

        levels.clear();
        buckets.clear();

        // For each level, compute median distance on currentIds relative to pivot i
        for(size_t lvl=0; lvl < numLevels; ++lvl) {
            // gather distances for currentIds to pivot lvl
            vector<double> dists; dists.reserve(currentIds.size());
            for(auto id: currentIds) dists.push_back(mappedCache[id][lvl]);
            // compute median
            double d_med = 0.0;
            if(!dists.empty()) {
                size_t m = dists.size()/2;
                nth_element(dists.begin(), dists.begin()+m, dists.end());
                d_med = dists[m];
                // for even count, fine to pick this m
            }
            LevelSpec Ls; Ls.pivotId = pt.pivots[lvl].id; Ls.d_med = d_med;
            levels.push_back(Ls);

            // create next level's currentIds as those that fall in exclusion '-'
            vector<uint64_t> nextIds;
            nextIds.reserve(currentIds.size());
            for(auto id: currentIds) {
                double dk = mappedCache[id][lvl];
                if( dk >= (d_med - rho) && dk <= (d_med + rho) ) {
                    nextIds.push_back(id);
                }
            }
            currentIds.swap(nextIds);
            // continue
        }

        // Having computed levels and their medians, now assign every object to exactly one bucket key
        // Bucket key is for each level char: 'L', '-', 'R' (no spaces)
        for(auto &entry : mappedCache) {
            uint64_t id = entry.first;
            const vector<double> &dv = entry.second;
            string key; key.reserve(numLevels);
            BucketInfo info;
            info.perLevelInterval.resize(numLevels);
            for(size_t lvl=0; lvl<numLevels; ++lvl) {
                double dk = dv[lvl];
                double dmed = levels[lvl].d_med;
                if(dk < dmed - rho) {
                    key.push_back('L');
                    info.perLevelInterval[lvl] = {0.0, max(0.0, dmed - rho)}; // left interval [0, d_med - rho)
                } else if(dk > dmed + rho) {
                    key.push_back('R');
                    info.perLevelInterval[lvl] = {dmed + rho, numeric_limits<double>::infinity()}; // right (d_med+rho, inf)
                } else {
                    key.push_back('-');
                    info.perLevelInterval[lvl] = {max(0.0, dmed - rho), dmed + rho}; // exclusion interval
                }
            }
            // append object id to bucket
            auto &b = buckets[key];
            if(b.objectIds.empty() && b.perLevelInterval.empty()) {
                b.perLevelInterval = info.perLevelInterval; // set intervals template
            }
            b.objectIds.push_back(id);
        }

        // Done: buckets map contains all buckets and their member ids
    }

    // Helper: compute minDist from scalar x to interval [a,b]
    static double minDistToInterval(double x, pair<double,double> interval) {
        double a = interval.first, b = interval.second;
        if(!isfinite(b)) { // interval [a, +inf)
            if(x < a) return a - x;
            else return 0.0;
        }
        if(x < a) return a - x;
        if(x > b) return x - b;
        return 0.0;
    }

    // MRQ: returns candidate ids that must be verified by exact metric.
    // Implementation: traverse (i.e., iterate) buckets; use LB (max over levels of minDistToInterval(d(q,p_i), interval_i)).
    vector<uint64_t> MRQ(const vector<double> &qPayload, double r) const {
        // precompute d(q,p_i) for all pivots (levels)
        vector<double> qmap(levels.size(), 0.0);
        for(size_t i=0;i<levels.size();++i) {
            qmap[i] = metric->distance(qPayload, pt.pivots[i].payload); // need pivot payload; but pt not stored publicly here
            // Note: we have pivot payloads in pt, but pt is private member. We'll create a helper to expose pivot payloads.
        }
        // But the code above uses pt; ensure pt exists as built. To avoid making pt public, we'll store pivot payloads locally.
        // For now in this method we assume pt.pivots accessible. (They are private; we'll make them accessible by adding a small accessor.)
        // In this file I will directly use levels' pivotId and a small pivotMap - but we have pt stored in object; adjust accordingly.
        // (Implementation detail handled below.)
        vector<uint64_t> candidates;
        candidates.reserve(1024);

        // For each bucket, compute LB
        for(const auto &kv : buckets) {
            const string &key = kv.first;
            const BucketInfo &binf = kv.second;
            double LB = 0.0;
            for(size_t lvl=0; lvl<binf.perLevelInterval.size(); ++lvl) {
                // need d(q, pivot lvl). Let's compute on demand and cache
                // We'll compute qmap lazily outside loop — above we tried but needed pt; so compute here:
                // (will compute qmap_local)
            }
        }

        // To avoid repeated distance calculations to pivots, compute qmap now using pivot payloads from pt.
        vector<double> qmap_local;
        qmap_local.resize(numLevels);
        for(size_t i=0;i<numLevels;i++){
            qmap_local[i] = metric->distance(qPayload, getPivotPayload(i));
        }

        for(const auto &kv : buckets) {
            const BucketInfo &binf = kv.second;
            double LB = 0.0;
            for(size_t lvl=0; lvl<binf.perLevelInterval.size(); ++lvl) {
                double md = minDistToInterval(qmap_local[lvl], binf.perLevelInterval[lvl]);
                if(md > LB) LB = md;
                if(LB > r) break; // early prune
            }
            if(LB > r) continue; // prune bucket
            // not pruned: add all its objects as candidates (we could do further pivot-filtering inside bucket, but for now return them)
            for(auto id : binf.objectIds) candidates.push_back(id);
        }

        return candidates;
    }

    // MkNN algorithm as described in fragment:
    // 1) set radius = rho; do MRQ(q, rho) -> candidates; compute kNN among them (exact distances)
    // 2) if k-th distance > rho, set radius = k-th distance and repeat MRQ(q, radius)
    vector<pair<uint64_t,double>> MkNN(const vector<double> &qPayload, size_t k, const vector<DataObject> &inMemoryDataset) const {
        double radius = rho;
        vector<pair<uint64_t,double>> finalResults;
        for(int iter=0; iter<3; ++iter) { // limit iterations to avoid infinite loop; typically 1 or 2 iterations suffice
            vector<uint64_t> cand = MRQ(qPayload, radius);
            // verify exact distances (use dataset provided in memory or using RAF)
            vector<pair<uint64_t,double>> dists; dists.reserve(cand.size());
            for(uint64_t id : cand) {
                // for demo: we use inMemoryDataset (ids start at 1..N)
                double dist = 0.0;
                if(id >=1 && id <= inMemoryDataset.size()) {
                    dist = metric->distance(qPayload, inMemoryDataset[id-1].payload);
                } else {
                    // fallback: read from RAF (slower)
                    DataObject o = raf.read(id);
                    dist = metric->distance(qPayload, o.payload);
                }
                dists.push_back({id, dist});
            }
            // sort by distance
            sort(dists.begin(), dists.end(), [](auto &a, auto &b){ return a.second < b.second; });
            if(dists.size() >= k) {
                double dk = dists[k-1].second;
                // if dk > radius we need to repeat with radius = dk
                if(dk > radius + 1e-12) {
                    radius = dk;
                    finalResults = vector<pair<uint64_t,double>>(dists.begin(), dists.begin()+min(k, dists.size()));
                    continue; // repeat
                } else {
                    // dk <= radius: we can accept top-k
                    finalResults = vector<pair<uint64_t,double>>(dists.begin(), dists.begin()+k);
                    break;
                }
            } else {
                // fewer candidates than k; expand radius generously (set to +inf) and recompute (in practice, we'd fallback to global search)
                // For safety, set radius to very large (max found distance or +inf). Here set to max distance found * 2
                double newrad = radius;
                if(!dists.empty()) newrad = max(radius, dists.back().second*2.0);
                else newrad = radius*2.0 + 1.0;
                if(newrad <= radius + 1e-12) break;
                radius = newrad;
                finalResults = dists;
                continue;
            }
        }
        // return finalResults (may have fewer than k if dataset small)
        return finalResults;
    }

    // Accessors to pivot payloads (we used pt internally but it's private member; so we need helper)
    const vector<DataObject>& getPivots() const { return pt.pivots; }

    // For MRQ internal pivot access
    const vector<double> getPivotPayloadAsVector(size_t idx) const {
        return pt.pivots[idx].payload;
    }

private:
    // helper to access pivot payload easily
    const vector<double>& getPivotPayload(size_t idx) const {
        return pt.pivots[idx].payload;
    }

public:
    // for debugging / analysis
    void printStats() const {
        cout << "DIndex stats: levels="<<numLevels<<" rho="<<rho<<"\n";
        cout << "Number of buckets: "<<buckets.size() << "\n";
        size_t total = 0;
        for(const auto &kv : buckets) total += kv.second.objectIds.size();
        cout << "Total indexed objects: "<< total << "\n";
        // show bucket sizes summary
        vector<size_t> sizes; sizes.reserve(buckets.size());
        for(const auto &kv: buckets) sizes.push_back(kv.second.objectIds.size());
        sort(sizes.begin(), sizes.end(), greater<size_t>());
        for(size_t i=0;i<min((size_t)10, sizes.size()); ++i) cout << " top bucket " << i << " size="<<sizes[i]<<"\n";
    }
};

/*** ---------- Demo main ---------- ***/
int main(){
    ios::sync_with_stdio(false);
    cin.tie(nullptr);

    // create a synthetic dataset in 3D
    Euclidean metric;
    vector<DataObject> dataset;
    const size_t N = 2000;
    const size_t dim = 3;
    mt19937_64 rng(2025);
    uniform_real_distribution<double> unif(0.0, 100.0);
    for(size_t i=0;i<N;i++){
        DataObject o; o.id = i+1;
        o.payload.resize(dim);
        for(size_t d=0; d<dim; ++d) o.payload[d] = unif(rng);
        dataset.push_back(o);
    }

    size_t L = 4;                // number of levels / pivots
    double rho = 5.0;            // chosen rho (tunable)
    DIndex dindex("dindex_raf.bin", &metric, L, rho);

    cout << "Building D-index with N="<<N<<" objects, L="<<L<<", rho="<<rho<<" ...\n";
    dindex.build(dataset, 42);
    dindex.printStats();

    // query
    vector<double> q(dim);
    for(size_t i=0;i<dim;i++) q[i] = unif(rng);

    double r = 10.0; // MRQ radius
    auto candidates = dindex.MRQ(q, r);
    cout << "MRQ(q,r="<<r<<") candidate count = "<<candidates.size()<<"\n";

    // verify exact distances and count true results
    size_t truecnt=0;
    for(auto id : candidates) {
        double dist = metric.distance(q, dataset[id-1].payload);
        if(dist <= r) truecnt++;
    }
    cout << "MRQ true results among candidates (verified): "<<truecnt<<"\n";

    // MkNN demo: find k nearest
    size_t k = 5;
    auto knn = dindex.MkNN(q, k, dataset);
    cout << "MkNN results (k="<<k<<") found " << knn.size() << " candidates:\n";
    for(auto &pr : knn) cout << " id="<<pr.first<<" d="<<pr.second<<"\n";

    cout << "Done demo.\n";
    return 0;
}
