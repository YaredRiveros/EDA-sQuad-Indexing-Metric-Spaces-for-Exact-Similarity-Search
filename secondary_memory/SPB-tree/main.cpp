// SPBTree.cpp
// Implementación prototipo del SPB-tree (Space-filling curve + Pivot + B+-tree)
// Basado en la descripción en Chen et al. (2022), sección SPB-tree.
// - Pivot mapping (pre-compute distances to pivots)
// - SFC mapping: Z-order (Morton) implemented; Hilbert hook left as TODO
// - B+-tree (in-memory bulk-load) where non-leaf entries store SFC min/max and MBB corners
// - MRQ (depth-first with pivot pruning Lemma 4.1 and pivot validation Lemma 4.5)
// - MkNN (best-first via LB over MBBs, verify via RAF)
// Compile: g++ -std=c++17 -O2 SPBTree.cpp -o SPBTree

#include <bits/stdc++.h>
using namespace std;

/* -------------------------
   Metric interface & Euclid
   ------------------------- */
struct Metric {
    virtual double distance(const vector<double>& a, const vector<double>& b) const = 0;
    virtual ~Metric() = default;
};
struct Euclidean : Metric {
    double distance(const vector<double>& a, const vector<double>& b) const override {
        double s=0;
        for(size_t i=0;i<a.size();++i){ double d=a[i]-b[i]; s+=d*d; }
        return sqrt(s);
    }
};

/* -------------------------
   Data object and RAF
   ------------------------- */
struct DataObject { uint64_t id; vector<double> payload; };

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
};

/* -------------------------
   Pivot table & mapping
   ------------------------- */
struct PivotTable {
    vector<DataObject> pivots;
    const Metric* metric=nullptr;
    PivotTable() {}
    PivotTable(const Metric* m): metric(m) {}
    void selectRandomPivots(const vector<DataObject>& objs, size_t l, uint64_t seed=42) {
        pivots.clear();
        if(l==0) return;
        mt19937_64 rng(seed);
        vector<size_t> idx(objs.size());
        iota(idx.begin(), idx.end(), 0);
        shuffle(idx.begin(), idx.end(), rng);
        for(size_t i=0;i<l && i<idx.size(); ++i) pivots.push_back(objs[idx[i]]);
    }
    vector<double> mapObject(const DataObject &o) const {
        vector<double> v; v.reserve(pivots.size());
        for(auto &p: pivots) v.push_back(metric->distance(o.payload, p.payload));
        return v;
    }
};

/* -------------------------
   SFC mapping: Morton (Z-order)
   - discretize each dimension into bits_per_dim bits
   - interleave bits to produce uint64_t key
   ------------------------- */
struct SFCMapper {
    size_t dims;
    unsigned bits_per_dim; // bits per pivot dimension
    vector<double> minv, maxv; // normalization per dim

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
        // choose bits_per_dim so total bits <= 64
        bits_per_dim = max(1u, (unsigned)(64 / max<size_t>(1,dims)));
        // if extremely many dims (e.g., dims>64) bits_per_dim becomes 0; guard above
        if(bits_per_dim * dims > 64) { bits_per_dim = 64 / dims; if(bits_per_dim==0) bits_per_dim=1; }
    }

    // map a real vector -> discretized integer per dim
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

    // Morton (Z-order) interleave bits across dims producing uint64_t key
    uint64_t mortonKey(const vector<uint64_t>& coords) const {
        // coords.size() == dims, each coord fits in bits_per_dim bits
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

    // convenience: real vector -> morton key
    uint64_t map(const vector<double>& v) const {
        auto sc = scalarize(v);
        return mortonKey(sc);
    }
};

/* -------------------------
   Simple B+ tree bulk-load in-memory
   - Leaf nodes contain (key, object id, mapped vector)
   - Non-leaf nodes contain child pointers + minKey/maxKey + MBB in mapped-space
   ------------------------- */
struct MBB {
    vector<double> low, high;
    MBB() {}
    MBB(size_t d) : low(d, numeric_limits<double>::infinity()), high(d, -numeric_limits<double>::infinity()) {}
    void expandWithPoint(const vector<double>& p) {
        if(low.empty()){ low=p; high=p; return; }
        for(size_t i=0;i<p.size();++i){ low[i]=min(low[i], p[i]); high[i]=max(high[i], p[i]); }
    }
    void expandWithMBB(const MBB& m) {
        if(low.empty()){ low=m.low; high=m.high; return; }
        for(size_t i=0;i<low.size();++i){ low[i]=min(low[i], m.low[i]); high[i]=max(high[i], m.high[i]); }
    }
    size_t dim() const { return low.size(); }
    // LB from query mapped vector q: same idea as Lemma 4.1 lower bound
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
    // max distance to pivot i (for validation): it's high[i]
};

struct BPlusEntry {
    uint64_t minKey, maxKey; // SFC key range for subtree / leaf range
    MBB box;                 // MBB of mapped vectors in subtree
    bool isLeaf;
    // if leaf: vectors of (key, id, mapped vector)
    vector< tuple<uint64_t,uint64_t,vector<double>> > records; // (key, id, mappedVec)
    // if not leaf: children nodes
    vector<BPlusEntry*> children;
    BPlusEntry(bool leaf=false): minKey(0), maxKey(0), isLeaf(leaf) {}
};

class BPlusTree {
public:
    BPlusEntry* root;
    size_t leafCapacity;
    size_t fanout;
    BPlusTree(size_t leafCap=64, size_t fanout_=64) : root(nullptr), leafCapacity(leafCap), fanout(fanout_) {}

    ~BPlusTree(){ freeNode(root); }
    void freeNode(BPlusEntry* n) {
        if(!n) return;
        for(auto c: n->children) freeNode(c);
        delete n;
    }

    // Bulk-load from sorted list of triples (key, id, mappedVec)
    void bulkLoad(vector< tuple<uint64_t,uint64_t,vector<double>> > &sortedRecs) {
        // create leaf nodes
        vector<BPlusEntry*> leaves;
        size_t i=0, n=sortedRecs.size();
        while(i<n){
            BPlusEntry* leaf = new BPlusEntry(true);
            size_t end = min(n, i + leafCapacity);
            for(size_t j=i;j<end;++j) {
                leaf->records.push_back(sortedRecs[j]);
                // update leaf MBB
                auto &mv = get<2>(sortedRecs[j]);
                leaf->box.expandWithPoint(mv);
                uint64_t k = get<0>(sortedRecs[j]);
                if(leaf->records.size()==1){ leaf->minKey = leaf->maxKey = k; }
                else { leaf->minKey = min(leaf->minKey, k); leaf->maxKey = max(leaf->maxKey, k); }
            }
            leaves.push_back(leaf);
            i = end;
        }
        if(leaves.empty()) { root=nullptr; return; }
        // build upper levels
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
                    if(node->children.size()==1){ node->minKey = cur[t]->minKey; node->maxKey = cur[t]->maxKey; }
                    else { node->minKey = min(node->minKey, cur[t]->minKey); node->maxKey = max(node->maxKey, cur[t]->maxKey); }
                }
                next.push_back(node);
                j = end;
            }
            cur.swap(next);
        }
        root = cur[0];
    }

    // MRQ traversal depth-first with pruning and validation
    // qmap: mapped query vector (pivot-distances), r: radius in original metric
    // metricUsed only for final verification if needed (but node-level pruning uses qmap)
    vector<uint64_t> rangeQuery(const vector<double>& qmap, double r) const {
        vector<uint64_t> result;
        if(!root) return result;
        // stack DFS
        vector<BPlusEntry*> stack; stack.push_back(root);
        while(!stack.empty()) {
            BPlusEntry* node = stack.back(); stack.pop_back();
            double lb = node->box.lowerBoundToQuery(qmap);
            if(lb > r) continue; // prune via Lemma 4.1 style LB
            // pivot validation (Lemma 4.5) check: if exists pivot i such that node.box.high[i] <= r - qmap[i],
            // then every object in node is guaranteed (3(o,pi) <= r - 3(q,pi)) => 3(q,o) <= r
            bool validated = false;
            for(size_t i=0;i<qmap.size();++i){
                double qi = qmap[i];
                double maxd = node->box.high[i];
                if(maxd <= r - qi) { validated = true; break; }
            }
            if(validated) {
                // collect all objects in subtree without reading/verification
                collectAll(node, result);
                continue;
            }
            if(node->isLeaf) {
                // check each record with pivot pruning (LB) and then mark as candidate (verification later)
                for(auto &rec : node->records) {
                    uint64_t id = get<1>(rec);
                    const vector<double>& mv = get<2>(rec);
                    // per-object pivot pruning: LB = max_i | qmap[i] - mv[i] |
                    double lbobj = 0.0;
                    for(size_t i=0;i<qmap.size();++i){
                        double md = fabs(qmap[i] - mv[i]);
                        if(md > lbobj) lbobj = md;
                        if(lbobj > r) break;
                    }
                    if(lbobj <= r) result.push_back(id); // candidate (still needs exact distance check by caller)
                }
            } else {
                // push children (depth-first)
                for(auto child : node->children) stack.push_back(child);
            }
        }
        return result;
    }

    // best-first knn: return candidate object ids in approximate best-first order by LB
    // We'll return candidates to verify (ID list), using LB computed from MBB and per-record for leaf.
    vector<uint64_t> knnCandidates(const vector<double>& qmap, size_t k) const {
        vector<uint64_t> result;
        if(!root) return result;
        // Node PQ by LB
        struct NodeItem { BPlusEntry* node; double lb; };
        struct CmpN { bool operator()(const NodeItem &a, const NodeItem &b) const { return a.lb > b.lb; } };
        priority_queue<NodeItem, vector<NodeItem>, CmpN> nodePQ;
        nodePQ.push({root, root->box.lowerBoundToQuery(qmap)});
        // leaf entry PQ for records
        struct RecItem { uint64_t id; double lb; vector<double> mv; };
        struct CmpR { bool operator()(const RecItem &a, const RecItem &b) const { return a.lb > b.lb; } };
        priority_queue<RecItem, vector<RecItem>, CmpR> recPQ;
        while((!nodePQ.empty() || !recPQ.empty()) && result.size() < k*10) {
            // Expand smallest LB among nodes
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
                        for(auto c: n->children) nodePQ.push({c, c->box.lowerBoundToQuery(qmap)});
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
                    for(auto c: n->children) nodePQ.push({c, c->box.lowerBoundToQuery(qmap)});
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
    const Metric* metric;
    RAF raf;
    PivotTable pt;
    SFCMapper sfc;
    BPlusTree bplus;
    size_t l; // number of pivots
    // store mapped vectors & keys
    vector< tuple<uint64_t,uint64_t,vector<double>> > records; // (key, id, mappedVec)
public:
    SPBTree(const string &rafFile, const Metric* m, size_t l_, size_t leafCap=128, size_t fanout=64)
        : metric(m), raf(rafFile), pt(m), bplus(leafCap, fanout), l(l_) {}

    // build from dataset: append to RAF, select pivots (random), compute mapped vectors, sfc keys, bulk-load B+
    void build(vector<DataObject>& dataset, uint64_t pivotSeed=42) {
        // append to RAF
        for(auto &o: dataset) raf.append(o);
        // choose pivots
        pt.selectRandomPivots(dataset, l, pivotSeed);
        // compute mapped vectors
        vector<vector<double>> mapped;
        mapped.reserve(dataset.size());
        for(auto &o: dataset) {
            auto mv = pt.mapObject(o);
            mapped.push_back(mv);
        }
        // configure SFC using all mapped vectors
        sfc.configure(mapped);
        // build records with keys
        records.clear();
        for(size_t i=0;i<dataset.size();++i){
            uint64_t id = dataset[i].id;
            auto &mv = mapped[i];
            uint64_t key = sfc.map(mv);
            records.push_back({key, id, mv});
        }
        // sort by key
        sort(records.begin(), records.end(), [](auto &a, auto &b){
            return get<0>(a) < get<0>(b);
        });
        // copy sorted to vector for bulkLoad
        bplus.bulkLoad(records);
    }

    // MRQ: returns candidates (ids) to verify, using B+ depth-first, Lemma 4.1 and 4.5
    vector<uint64_t> MRQ(const vector<double>& qPayload, double r) const {
        // compute qmap
        DataObject q; q.payload = qPayload; q.id = 0;
        vector<double> qmap = pt.mapObject(q);
        // query bplus
        auto cands = bplus.rangeQuery(qmap, r);
        return cands;
    }

    // MkNN: second strategy from Sec 2.2: best-first traversal + verify exact distances
    vector<pair<uint64_t,double>> MkNN(const vector<double>& qPayload, size_t k, const vector<DataObject>& inMemDataset) const {
        DataObject q; q.payload = qPayload; q.id=0;
        vector<double> qmap = pt.mapObject(q);
        // get candidate order from bplus (best-first approx)
        auto candIds = bplus.knnCandidates(qmap, k);
        // verify exact distances (use inMemDataset if available, else RAF)
        vector<pair<uint64_t,double>> dists; dists.reserve(candIds.size());
        for(auto id : candIds) {
            double dist;
            if(id>=1 && id <= inMemDataset.size()) dist = metric->distance(q.payload, inMemDataset[id-1].payload);
            else { DataObject o = raf.read(id); dist = metric->distance(q.payload, o.payload); }
            dists.push_back({id, dist});
        }
        sort(dists.begin(), dists.end(), [](auto &a, auto &b){ return a.second < b.second; });
        if(dists.size() > k) dists.resize(k);
        return dists;
    }

    // stats helper
    void stats() const {
        cout << "SPB-tree: pivots="<<l<<", records="<<records.size()<<", SFC bits/dim="<<sfc.bits_per_dim<<"\n";
    }
};

/* -------------------------
   Demo main
   ------------------------- */
int main(){
    ios::sync_with_stdio(false);
    cin.tie(nullptr);

    // synthetic dataset (3D)
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
        dataset.push_back(move(o));
    }

    size_t L = 4; // number of pivots
    SPBTree spb("spb_raf.bin", &metric, L, 128, 64);
    cout << "Building SPB-tree with N="<<N<<", L="<<L<<"...\n";
    spb.build(dataset, 123);
    spb.stats();

    // MRQ demo
    vector<double> q(dim);
    for(size_t i=0;i<dim;i++) q[i] = unif(rng);
    double r = 10.0;
    auto cands = spb.MRQ(q, r);
    cout << "MRQ candidates count (from index): "<<cands.size()<<"\n";
    size_t truecnt=0;
    for(auto id: cands) {
        double dist = metric.distance(q, dataset[id-1].payload);
        if(dist <= r) truecnt++;
    }
    cout << "Verified true results among candidates: "<<truecnt<<"\n";

    // MkNN demo
    size_t k=5;
    auto knn = spb.MkNN(q, k, dataset);
    cout << "MkNN (k="<<k<<") results:\n";
    for(auto &p: knn) cout << " id="<<p.first<<" d="<<p.second<<"\n";

    cout << "Demo finished.\n";
    return 0;
}
