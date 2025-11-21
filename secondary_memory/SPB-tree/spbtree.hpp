// spbtree.hpp
// Implementación del SPB-tree (Space-filling curve + Pivot + B+-tree) para benchmarking
// Basado en Chen et al. (2022) - SPB-tree con Morton Z-order y B+ bulk-load
// Adaptado para trabajar con ObjectDB y métricas de rendimiento

#pragma once
#include <bits/stdc++.h>
#include "objectdb.hpp"

using namespace std;

/* -------------------------
   Data object and RAF
   ------------------------- */
struct DataObject { 
    uint64_t id; 
    vector<double> payload; 
};

class RAF {
    string filename;
    unordered_map<uint64_t, streampos> offsets;
    mutable long long pageReads = 0;
    
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
        if(it==offsets.end()) throw runtime_error("RAF: id not found");
        
        pageReads++;
        
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
    
    long long get_pageReads() const { return pageReads; }
    void clear_pageReads() { pageReads = 0; }
};

/* -------------------------
   Pivot table & mapping
   ------------------------- */
struct PivotTable {
    vector<DataObject> pivots;
    ObjectDB* db;
    mutable long long compDist = 0;
    
    PivotTable() : db(nullptr) {}
    PivotTable(ObjectDB* database) : db(database) {}
    
    void selectRandomPivots(const vector<DataObject>& objs, size_t l, uint64_t seed=42) {
        pivots.clear();
        if(l==0) return;
        mt19937_64 rng(seed);
        vector<size_t> idx(objs.size());
        iota(idx.begin(), idx.end(), 0);
        shuffle(idx.begin(), idx.end(), rng);
        for(size_t i=0;i<l && i<idx.size(); ++i) 
            pivots.push_back(objs[idx[i]]);
    }
    
    vector<double> mapObject(uint64_t objId) const {
        vector<double> v; 
        v.reserve(pivots.size());
        for(auto &p: pivots) {
            double d = db->distance(objId, p.id);
            compDist++;
            v.push_back(d);
        }
        return v;
    }
    
    long long get_compDist() const { return compDist; }
    void clear_compDist() { compDist = 0; }
};

/* -------------------------
   SFC mapping: Morton (Z-order)
   ------------------------- */
struct SFCMapper {
    size_t dims;
    unsigned bits_per_dim;
    vector<double> minv, maxv;

    SFCMapper(size_t dims_=0): dims(dims_), bits_per_dim(0) {}

    void configure(const vector<vector<double>>& mappedVectors) {
        if(mappedVectors.empty()) return;
        dims = mappedVectors[0].size();
        minv.assign(dims, numeric_limits<double>::infinity());
        maxv.assign(dims, -numeric_limits<double>::infinity());
        for(auto &vec : mappedVectors) {
            for(size_t i=0;i<dims;i++){
                minv[i] = min(minv[i], vec[i]);
                maxv[i] = max(maxv[i], vec[i]);
            }
        }
        bits_per_dim = max(1u, (unsigned)(64 / max<size_t>(1,dims)));
        if(bits_per_dim * dims > 64) { 
            bits_per_dim = 64 / dims; 
            if(bits_per_dim==0) bits_per_dim=1; 
        }
    }

    vector<uint64_t> scalarize(const vector<double>& v) const {
        vector<uint64_t> res(dims);
        for(size_t i=0;i<dims;i++){
            double lo = minv[i], hi = maxv[i];
            double x = v[i];
            if(hi - lo < 1e-12) { res[i] = 0; continue; }
            double t = (x - lo) / (hi - lo);
            if(t<0) t=0; if(t>1) t=1;
            uint64_t maxq = (bits_per_dim==64 ? (uint64_t)-1 : ((1ULL << bits_per_dim) - 1ULL));
            uint64_t q = (uint64_t) floor(t * (double)maxq + 0.5);
            res[i] = q;
        }
        return res;
    }

    uint64_t mortonKey(const vector<uint64_t>& coords) const {
        uint64_t key = 0;
        for(unsigned b=0; b < bits_per_dim; ++b) {
            for(size_t d=0; d<dims; ++d) {
                uint64_t bit = (coords[d] >> b) & 1ULL;
                key <<= 1;
                key |= bit;
            }
        }
        return key;
    }

    uint64_t map(const vector<double>& v) const {
        auto sc = scalarize(v);
        return mortonKey(sc);
    }
};

/* -------------------------
   MBB for pruning
   ------------------------- */
struct MBB {
    vector<double> low, high;
    
    MBB() {}
    MBB(size_t d) : low(d, numeric_limits<double>::infinity()), 
                     high(d, -numeric_limits<double>::infinity()) {}
    
    void expandWithPoint(const vector<double>& p) {
        if(low.empty()){ low=p; high=p; return; }
        for(size_t i=0;i<p.size();++i){ 
            low[i]=min(low[i], p[i]); 
            high[i]=max(high[i], p[i]); 
        }
    }
    
    void expandWithMBB(const MBB& m) {
        if(low.empty()){ low=m.low; high=m.high; return; }
        for(size_t i=0;i<low.size();++i){ 
            low[i]=min(low[i], m.low[i]); 
            high[i]=max(high[i], m.high[i]); 
        }
    }
    
    size_t dim() const { return low.size(); }
    
    double lowerBoundToQuery(const vector<double>& q) const {
        double lb = 0.0;
        for(size_t i=0;i<q.size();++i){
            double qi=q[i], a=low[i], b=high[i];
            double md=0.0;
            if(qi < a) md = a - qi;
            else if(qi > b) md = qi - b;
            if(md > lb) lb = md;
        }
        return lb;
    }
};

/* -------------------------
   B+ tree structure
   ------------------------- */
struct BPlusEntry {
    uint64_t minKey, maxKey;
    MBB box;
    bool isLeaf;
    vector< tuple<uint64_t,uint64_t,vector<double>> > records;
    vector<BPlusEntry*> children;
    
    BPlusEntry(bool leaf=false): minKey(0), maxKey(0), isLeaf(leaf) {}
};

class BPlusTree {
public:
    BPlusEntry* root;
    size_t leafCapacity;
    size_t fanout;
    
    BPlusTree(size_t leafCap=64, size_t fanout_=64) 
        : root(nullptr), leafCapacity(leafCap), fanout(fanout_) {}

    ~BPlusTree(){ freeNode(root); }
    
    void freeNode(BPlusEntry* n) {
        if(!n) return;
        for(auto c: n->children) freeNode(c);
        delete n;
    }

    void bulkLoad(vector< tuple<uint64_t,uint64_t,vector<double>> > &sortedRecs) {
        vector<BPlusEntry*> leaves;
        size_t i=0, n=sortedRecs.size();
        
        while(i<n){
            BPlusEntry* leaf = new BPlusEntry(true);
            size_t end = min(n, i + leafCapacity);
            for(size_t j=i;j<end;++j) {
                leaf->records.push_back(sortedRecs[j]);
                auto &mv = get<2>(sortedRecs[j]);
                leaf->box.expandWithPoint(mv);
                uint64_t k = get<0>(sortedRecs[j]);
                if(leaf->records.size()==1){ 
                    leaf->minKey = leaf->maxKey = k; 
                }
                else { 
                    leaf->minKey = min(leaf->minKey, k); 
                    leaf->maxKey = max(leaf->maxKey, k); 
                }
            }
            leaves.push_back(leaf);
            i = end;
        }
        
        if(leaves.empty()) { root=nullptr; return; }
        
        vector<BPlusEntry*> cur = leaves;
        while(cur.size() > 1) {
            vector<BPlusEntry*> next;
            size_t j=0;
            while(j < cur.size()) {
                BPlusEntry* node = new BPlusEntry(false);
                size_t end = min(cur.size(), j + fanout);
                for(size_t t=j;t<end;++t) {
                    node->children.push_back(cur[t]);
                    node->box.expandWithMBB(cur[t]->box);
                    if(node->children.size()==1){ 
                        node->minKey = cur[t]->minKey; 
                        node->maxKey = cur[t]->maxKey; 
                    }
                    else { 
                        node->minKey = min(node->minKey, cur[t]->minKey); 
                        node->maxKey = max(node->maxKey, cur[t]->maxKey); 
                    }
                }
                next.push_back(node);
                j = end;
            }
            cur.swap(next);
        }
        root = cur[0];
    }

    vector<uint64_t> rangeQuery(const vector<double>& qmap, double r) const {
        vector<uint64_t> result;
        if(!root) return result;
        
        vector<BPlusEntry*> stack; 
        stack.push_back(root);
        
        while(!stack.empty()) {
            BPlusEntry* node = stack.back(); 
            stack.pop_back();
            
            double lb = node->box.lowerBoundToQuery(qmap);
            if(lb > r) continue;
            
            bool validated = false;
            for(size_t i=0;i<qmap.size();++i){
                double qi = qmap[i];
                double maxd = node->box.high[i];
                if(maxd <= r - qi) { validated = true; break; }
            }
            
            if(validated) {
                collectAll(node, result);
                continue;
            }
            
            if(node->isLeaf) {
                for(auto &rec : node->records) {
                    uint64_t id = get<1>(rec);
                    const vector<double>& mv = get<2>(rec);
                    double lbobj = 0.0;
                    for(size_t i=0;i<qmap.size();++i){
                        double md = fabs(qmap[i] - mv[i]);
                        if(md > lbobj) lbobj = md;
                        if(lbobj > r) break;
                    }
                    if(lbobj <= r) result.push_back(id);
                }
            } else {
                for(auto child : node->children) stack.push_back(child);
            }
        }
        return result;
    }

    vector<uint64_t> knnCandidates(const vector<double>& qmap, size_t k) const {
        vector<uint64_t> result;
        if(!root) return result;
        
        struct NodeItem { BPlusEntry* node; double lb; };
        struct CmpN { bool operator()(const NodeItem &a, const NodeItem &b) const { return a.lb > b.lb; } };
        priority_queue<NodeItem, vector<NodeItem>, CmpN> nodePQ;
        nodePQ.push({root, root->box.lowerBoundToQuery(qmap)});
        
        struct RecItem { uint64_t id; double lb; vector<double> mv; };
        struct CmpR { bool operator()(const RecItem &a, const RecItem &b) const { return a.lb > b.lb; } };
        priority_queue<RecItem, vector<RecItem>, CmpR> recPQ;
        
        while((!nodePQ.empty() || !recPQ.empty()) && result.size() < k*10) {
            if(!recPQ.empty()) {
                double bestRecLB = recPQ.top().lb;
                if(!nodePQ.empty() && nodePQ.top().lb < bestRecLB) {
                    auto it = nodePQ.top(); nodePQ.pop();
                    BPlusEntry* n = it.node;
                    if(n->isLeaf) {
                        for(auto &rec : n->records) {
                            const vector<double>& mv = get<2>(rec);
                            double lbobj = 0.0;
                            for(size_t i=0;i<mv.size();++i){
                                double md = fabs(qmap[i] - mv[i]);
                                if(md>lbobj) lbobj=md;
                            }
                            recPQ.push({ get<1>(rec), lbobj, mv });
                        }
                    } else {
                        for(auto c: n->children) 
                            nodePQ.push({c, c->box.lowerBoundToQuery(qmap)});
                    }
                    continue;
                }
            }
            if(!recPQ.empty()) {
                auto ritem = recPQ.top(); recPQ.pop();
                result.push_back(ritem.id);
                continue;
            }
            if(!nodePQ.empty()) {
                auto it = nodePQ.top(); nodePQ.pop();
                BPlusEntry* n = it.node;
                if(n->isLeaf) {
                    for(auto &rec : n->records) {
                        const vector<double>& mv = get<2>(rec);
                        double lbobj = 0.0;
                        for(size_t i=0;i<mv.size();++i){
                            double md = fabs(qmap[i] - mv[i]);
                            if(md>lbobj) lbobj=md;
                        }
                        recPQ.push({ get<1>(rec), lbobj, mv });
                    }
                } else {
                    for(auto c: n->children) 
                        nodePQ.push({c, c->box.lowerBoundToQuery(qmap)});
                }
                continue;
            }
        }
        return result;
    }

private:
    void collectAll(BPlusEntry* n, vector<uint64_t>& out) const {
        if(n->isLeaf) {
            for(auto &rec : n->records) out.push_back(get<1>(rec));
        } else {
            for(auto c: n->children) collectAll(c, out);
        }
    }
};

/* -------------------------
   SPB-tree wrapper
   ------------------------- */
class SPBTree {
    ObjectDB* db;
    RAF raf;
    PivotTable pt;
    SFCMapper sfc;
    BPlusTree bplus;
    size_t l;
    vector< tuple<uint64_t,uint64_t,vector<double>> > records;
    
public:
    SPBTree(const string &rafFile, ObjectDB* database, size_t l_, 
            size_t leafCap=128, size_t fanout=64)
        : db(database), raf(rafFile), pt(database), bplus(leafCap, fanout), l(l_) {}

    void build(vector<DataObject>& dataset, uint64_t pivotSeed=42) {
        for(auto &o: dataset) raf.append(o);
        
        pt.selectRandomPivots(dataset, l, pivotSeed);
        
        vector<vector<double>> mapped;
        mapped.reserve(dataset.size());
        for(auto &o: dataset) {
            auto mv = pt.mapObject(o.id);
            mapped.push_back(mv);
        }
        
        sfc.configure(mapped);
        
        records.clear();
        for(size_t i=0;i<dataset.size();++i){
            uint64_t id = dataset[i].id;
            auto &mv = mapped[i];
            uint64_t key = sfc.map(mv);
            records.push_back({key, id, mv});
        }
        
        sort(records.begin(), records.end(), [](auto &a, auto &b){
            return get<0>(a) < get<0>(b);
        });
        
        bplus.bulkLoad(records);
    }

    vector<uint64_t> MRQ(uint64_t queryId, double r) {
        vector<double> qmap = pt.mapObject(queryId);
        auto cands = bplus.rangeQuery(qmap, r);
        return cands;
    }

    vector<pair<uint64_t,double>> MkNN(uint64_t queryId, size_t k) {
        vector<double> qmap = pt.mapObject(queryId);
        auto candIds = bplus.knnCandidates(qmap, k);
        
        vector<pair<uint64_t,double>> dists; 
        dists.reserve(candIds.size());
        for(auto id : candIds) {
            double dist = db->distance(queryId, id);
            pt.compDist++;
            dists.push_back({id, dist});
        }
        
        sort(dists.begin(), dists.end(), 
             [](auto &a, auto &b){ return a.second < b.second; });
        if(dists.size() > k) dists.resize(k);
        return dists;
    }

    void stats() const {
        cout << "SPB-tree: pivots=" << l << ", records=" << records.size() 
             << ", SFC bits/dim=" << sfc.bits_per_dim << "\n";
    }
    
    long long get_compDist() const { return pt.get_compDist(); }
    long long get_pageReads() const { return raf.get_pageReads(); }
    
    void clear_counters() {
        pt.clear_compDist();
        raf.clear_pageReads();
    }
};
