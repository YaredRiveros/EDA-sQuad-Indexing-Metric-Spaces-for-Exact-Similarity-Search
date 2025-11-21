// dindex.hpp
// Implementación del D-index (familia D-index) para benchmarking
// Basado en Chen et al. (2022) - Section 5.6 D-index Family
// Adaptado para trabajar con ObjectDB y métricas de rendimiento

#pragma once
#include <bits/stdc++.h>
#include "objectdb.hpp"

using namespace std;

/*** ---------- Data object & RAF ---------- ***/
struct DataObject {
    uint64_t id;
    vector<double> payload;
};

class RAF {
    string filename;
    unordered_map<uint64_t, streampos> offsets;
    mutable long long pageReads = 0;  // contador de lecturas de página
    
public:
    RAF(const string &fname): filename(fname) {
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
        if(it == offsets.end()) throw runtime_error("RAF: id not found");
        
        pageReads++;  // contar lectura
        
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
    ObjectDB* db;
    mutable long long compDist = 0;  // contador de cálculos de distancia
    
    PivotTable() : db(nullptr) {}
    PivotTable(ObjectDB *database) : db(database) {}
    
    void setDB(ObjectDB *database) { db = database; }

    void selectRandomPivots(const vector<DataObject>& objs, size_t l, uint64_t seed=42) {
        pivots.clear();
        if(l == 0) return;
        mt19937_64 rng(seed);
        vector<size_t> idx(objs.size());
        iota(idx.begin(), idx.end(), 0);
        shuffle(idx.begin(), idx.end(), rng);
        for(size_t i = 0; i < l && i < idx.size(); ++i) 
            pivots.push_back(objs[idx[i]]);
    }

    // map object o to vector of distances to pivots
    vector<double> mapObject(uint64_t objId) const {
        vector<double> m; 
        m.reserve(pivots.size());
        for(const auto &p: pivots) {
            double d = db->distance(objId, p.id);
            compDist++;
            m.push_back(d);
        }
        return m;
    }
    
    long long get_compDist() const { return compDist; }
    void clear_compDist() { compDist = 0; }
};

/*** ---------- D-index construction ---------- ***/

struct LevelSpec {
    uint64_t pivotId;
    double d_med;
};

struct BucketInfo {
    vector<pair<double, double>> perLevelInterval;
    vector<uint64_t> objectIds;
};

class DIndex {
    ObjectDB* db;
    RAF raf;
    PivotTable pt;
    size_t numLevels;
    double rho;
    
    vector<LevelSpec> levels;
    unordered_map<string, BucketInfo> buckets;
    unordered_map<uint64_t, vector<double>> mappedCache;

public:
    DIndex(const string &rafFile, ObjectDB *database, size_t L, double rho_)
        : db(database), raf(rafFile), pt(database), numLevels(L), rho(rho_) {}

    void build(vector<DataObject> &objects, uint64_t pivotSeed=12345) {
        // Append all objects to RAF
        for(auto &o: objects) raf.append(o);

        // Select pivots (one per level)
        pt.selectRandomPivots(objects, numLevels, pivotSeed);
        if(pt.pivots.size() < numLevels) {
            throw runtime_error("Not enough objects to select pivots");
        }

        // Precompute distance matrix (n x L)
        size_t n = objects.size();
        mappedCache.clear();
        for(auto &o: objects) {
            vector<double> dv; 
            dv.reserve(numLevels);
            for(size_t i = 0; i < numLevels; i++) {
                double d = db->distance(o.id, pt.pivots[i].id);
                dv.push_back(d);
            }
            mappedCache[o.id] = dv;
        }

        // Initialize with all object ids
        vector<uint64_t> currentIds;
        currentIds.reserve(n);
        for(auto &o: objects) currentIds.push_back(o.id);

        levels.clear();
        buckets.clear();

        // Compute median for each level
        for(size_t lvl = 0; lvl < numLevels; ++lvl) {
            vector<double> dists; 
            dists.reserve(currentIds.size());
            for(auto id: currentIds) dists.push_back(mappedCache[id][lvl]);
            
            double d_med = 0.0;
            if(!dists.empty()) {
                size_t m = dists.size() / 2;
                nth_element(dists.begin(), dists.begin() + m, dists.end());
                d_med = dists[m];
            }
            
            LevelSpec Ls; 
            Ls.pivotId = pt.pivots[lvl].id; 
            Ls.d_med = d_med;
            levels.push_back(Ls);

            // Create next level's ids (exclusion zone)
            vector<uint64_t> nextIds;
            nextIds.reserve(currentIds.size());
            for(auto id: currentIds) {
                double dk = mappedCache[id][lvl];
                if(dk >= (d_med - rho) && dk <= (d_med + rho)) {
                    nextIds.push_back(id);
                }
            }
            currentIds.swap(nextIds);
        }

        // Assign every object to exactly one bucket
        for(auto &entry : mappedCache) {
            uint64_t id = entry.first;
            const vector<double> &dv = entry.second;
            string key; 
            key.reserve(numLevels);
            BucketInfo info;
            info.perLevelInterval.resize(numLevels);
            
            for(size_t lvl = 0; lvl < numLevels; ++lvl) {
                double dk = dv[lvl];
                double dmed = levels[lvl].d_med;
                
                if(dk < dmed - rho) {
                    key.push_back('L');
                    info.perLevelInterval[lvl] = {0.0, max(0.0, dmed - rho)};
                } else if(dk > dmed + rho) {
                    key.push_back('R');
                    info.perLevelInterval[lvl] = {dmed + rho, numeric_limits<double>::infinity()};
                } else {
                    key.push_back('-');
                    info.perLevelInterval[lvl] = {max(0.0, dmed - rho), dmed + rho};
                }
            }
            
            auto &b = buckets[key];
            if(b.objectIds.empty() && b.perLevelInterval.empty()) {
                b.perLevelInterval = info.perLevelInterval;
            }
            b.objectIds.push_back(id);
        }
    }

    // Helper: compute minDist from scalar x to interval [a,b]
    static double minDistToInterval(double x, pair<double, double> interval) {
        double a = interval.first, b = interval.second;
        if(!isfinite(b)) {
            if(x < a) return a - x;
            else return 0.0;
        }
        if(x < a) return a - x;
        if(x > b) return x - b;
        return 0.0;
    }

    // MRQ: returns candidate ids
    vector<uint64_t> MRQ(uint64_t queryId, double r) {
        // Compute d(q, pivot_i) for all pivots
        vector<double> qmap_local;
        qmap_local.resize(numLevels);
        for(size_t i = 0; i < numLevels; i++) {
            qmap_local[i] = db->distance(queryId, pt.pivots[i].id);
            pt.compDist++;
        }

        vector<uint64_t> candidates;
        candidates.reserve(1024);

        // For each bucket, compute LB
        for(const auto &kv : buckets) {
            const BucketInfo &binf = kv.second;
            double LB = 0.0;
            
            for(size_t lvl = 0; lvl < binf.perLevelInterval.size(); ++lvl) {
                double md = minDistToInterval(qmap_local[lvl], binf.perLevelInterval[lvl]);
                if(md > LB) LB = md;
                if(LB > r) break;
            }
            
            if(LB > r) continue;
            
            // Add all objects in bucket as candidates
            for(auto id : binf.objectIds) {
                candidates.push_back(id);
            }
        }

        return candidates;
    }

    // MkNN algorithm
    vector<pair<uint64_t, double>> MkNN(uint64_t queryId, size_t k) {
        double radius = rho;
        vector<pair<uint64_t, double>> finalResults;
        
        for(int iter = 0; iter < 3; ++iter) {
            vector<uint64_t> cand = MRQ(queryId, radius);
            
            // Verify exact distances
            vector<pair<uint64_t, double>> dists; 
            dists.reserve(cand.size());
            
            for(uint64_t id : cand) {
                double dist = db->distance(queryId, id);
                pt.compDist++;
                dists.push_back({id, dist});
            }
            
            // Sort by distance
            sort(dists.begin(), dists.end(), 
                 [](auto &a, auto &b){ return a.second < b.second; });
            
            if(dists.size() >= k) {
                double dk = dists[k - 1].second;
                if(dk > radius + 1e-12) {
                    radius = dk;
                    finalResults = vector<pair<uint64_t, double>>(
                        dists.begin(), dists.begin() + min(k, dists.size()));
                    continue;
                } else {
                    finalResults = vector<pair<uint64_t, double>>(
                        dists.begin(), dists.begin() + k);
                    break;
                }
            } else {
                double newrad = radius;
                if(!dists.empty()) newrad = max(radius, dists.back().second * 2.0);
                else newrad = radius * 2.0 + 1.0;
                if(newrad <= radius + 1e-12) break;
                radius = newrad;
                finalResults = dists;
                continue;
            }
        }
        
        return finalResults;
    }

    // Accessors
    const vector<DataObject>& getPivots() const { return pt.pivots; }
    
    const vector<double>& getPivotPayload(size_t idx) const {
        return pt.pivots[idx].payload;
    }

    // Stats and counters
    void printStats() const {
        cout << "DIndex stats: levels=" << numLevels << " rho=" << rho << "\n";
        cout << "Number of buckets: " << buckets.size() << "\n";
        size_t total = 0;
        for(const auto &kv : buckets) total += kv.second.objectIds.size();
        cout << "Total indexed objects: " << total << "\n";
        
        vector<size_t> sizes; 
        sizes.reserve(buckets.size());
        for(const auto &kv: buckets) sizes.push_back(kv.second.objectIds.size());
        sort(sizes.begin(), sizes.end(), greater<size_t>());
        
        for(size_t i = 0; i < min((size_t)5, sizes.size()); ++i) 
            cout << " bucket " << i << " size=" << sizes[i] << "\n";
    }
    
    long long get_compDist() const { return pt.get_compDist(); }
    long long get_pageReads() const { return raf.get_pageReads(); }
    
    void clear_counters() {
        pt.clear_compDist();
        raf.clear_pageReads();
    }
};
