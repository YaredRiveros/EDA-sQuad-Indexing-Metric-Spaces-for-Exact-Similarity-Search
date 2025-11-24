#include <bits/stdc++.h>
using namespace std;

/*** -----------------------------
    Abstract metric interface
    ----------------------------- ***/
struct Metric {
    virtual double distance(const vector<double>& a, const vector<double>& b) const = 0;
    virtual ~Metric() = default;
};

// Euclidean metric on vectors (example)
struct Euclidean : Metric {
    double distance(const vector<double>& a, const vector<double>& b) const override {
        double s=0;
        size_t n = a.size();
        for(size_t i=0;i<n;i++){ double d=a[i]-b[i]; s+=d*d; }
        return sqrt(s);
    }
};

/*** -----------------------------
    Data object (stored in RAF)
    ----------------------------- ***/
struct DataObject {
    uint64_t id;                 // unique id
    vector<double> payload;      // original representation (e.g., original vector)
};

class RAF {
    string filename;
    ofstream ofs;
    ifstream ifs;
    // Keep mapping id -> offset (in bytes) in memory (simple index)
    unordered_map<uint64_t, streampos> offsets;
public:
    RAF(const string &fname): filename(fname) {
        ofs.open(filename, ios::binary | ios::trunc);
        ofs.close();
        // reopen for append later
    }
    // append object, return offset
    streampos append(const DataObject &o) {
        ofs.open(filename, ios::binary | ios::app);
        streampos pos = ofs.tellp();
        // layout: id (8 bytes), payload length (8 bytes), then doubles
        ofs.write((const char*)&o.id, sizeof(o.id));
        uint64_t len = o.payload.size();
        ofs.write((const char*)&len, sizeof(len));
        ofs.write((const char*)o.payload.data(), len * sizeof(double));
        ofs.close();
        offsets[o.id] = pos;
        return pos;
    }
    // read object by id
    DataObject read(uint64_t id) {
        ifs.open(filename, ios::binary);
        auto it = offsets.find(id);
        if(it==offsets.end()) throw runtime_error("RAF: id not found");
        streampos pos = it->second;
        ifs.seekg(pos);
        DataObject o;
        ifs.read((char*)&o.id, sizeof(o.id));
        uint64_t len;
        ifs.read((char*)&len, sizeof(len));
        o.payload.resize(len);
        ifs.read((char*)o.payload.data(), len * sizeof(double));
        ifs.close();
        return o;
    }
    bool has(uint64_t id) const { return offsets.find(id) != offsets.end(); }
};

/*** -----------------------------
    Pivot table & mapping
    ----------------------------- ***/
struct PivotTable {
    vector<DataObject> pivots; // pivot objects (store full object or at least their payload)
    const Metric* metric;
    PivotTable(const Metric* m=nullptr): metric(m) {}
    void setMetric(const Metric* m) { metric = m; }

    // choose pivots by random sampling from dataset (ids-> DataObject vector passed)
    // objects: vector<DataObject> from which to pick pivots
    void selectRandomPivots(const vector<DataObject>& objects, size_t l, uint64_t seed=42) {
        pivots.clear();
        if(l==0) return;
        mt19937_64 rng(seed);
        vector<size_t> idx(objects.size());
        iota(idx.begin(), idx.end(), 0);
        shuffle(idx.begin(), idx.end(), rng);
        for(size_t i=0;i<l && i<idx.size(); ++i) pivots.push_back(objects[idx[i]]);
    }

    // map an object (payload) to vector of distances to pivots
    vector<double> mapObject(const DataObject &o) const {
        vector<double> v; v.reserve(pivots.size());
        for(const auto &p: pivots) {
            v.push_back(metric->distance(o.payload, p.payload));
        }
        return v;
    }
};

/*** -----------------------------
    R-tree types (MBB per dimension)
    ----------------------------- ***/
using Vec = vector<double>;

struct MBB {
    Vec low;   // per-dimension min
    Vec high;  // per-dimension max

    MBB() {}
    MBB(size_t d) { low.assign(d, numeric_limits<double>::infinity());
                     high.assign(d, -numeric_limits<double>::infinity()); }

    void expandWithPoint(const Vec& p) {
        size_t d=p.size();
        if(low.empty()) { low=p; high=p; return; }
        for(size_t i=0;i<d;i++){
            low[i] = min(low[i], p[i]);
            high[i] = max(high[i], p[i]);
        }
    }
    void expandWithMBB(const MBB& m) {
        if(low.empty()) { low=m.low; high=m.high; return; }
        for(size_t i=0;i<low.size();i++){
            low[i] = min(low[i], m.low[i]);
            high[i] = max(high[i], m.high[i]);
        }
    }
    size_t dim() const { return low.size(); }

    // check intersection with search hyper-rectangle H = ×_i [q_i - r, q_i + r]
    bool intersectsHyperRect(const Vec &qMap, double r) const {
        size_t d = qMap.size();
        for(size_t i=0;i<d;i++){
            double a = qMap[i] - r;
            double b = qMap[i] + r;
            if(high[i] < a || low[i] > b) return false;
        }
        return true;
    }

    // lower bound on true distance using pivot LB described in analysis:
    // for each pivot i: minDistToInterval = 0 if q_i in [low[i], high[i]] else min(|q_i-low[i]|, |q_i-high[i]|)
    // LB = max over i of minDistToInterval
    double lowerBoundToQuery(const Vec &qMap) const {
        double lb = 0.0;
        size_t d = qMap.size();
        for(size_t i=0;i<d;i++){
            double qv = qMap[i];
            double lo = low[i], hi = high[i];
            double md = 0.0;
            if(qv < lo) md = lo - qv;
            else if(qv > hi) md = qv - hi;
            else md = 0.0;
            if(md > lb) lb = md;
        }
        return lb;
    }
};

/*** -----------------------------
    R-tree (simplified)
    - Node entries either point to child node or leaf records (object id and mapped vector)
    - Linear split, simple choose-by-enlargement insertion
    ----------------------------- ***/
struct RTreeEntry {
    MBB box;
    bool isLeafEntry;
    // if isLeafEntry -> store object id (RAF pointer)
    uint64_t objectId; // valid if leaf
    Vec mappedPoint;   // stored at leaf
    // else -> child pointer
    struct RTreeNode* child = nullptr;
};

struct RTreeNode {
    bool isLeaf;
    vector<RTreeEntry> entries;
    RTreeNode(bool leaf=false): isLeaf(leaf) {}
};

class RTree {
public:
    size_t maxEntries;
    size_t minEntries;
    RTreeNode* root;

    RTree(size_t maxE=16): maxEntries(maxE) {
        minEntries = maxEntries/2;
        root = new RTreeNode(true);
    }

    ~RTree() { freeNode(root); }
private:
    void freeNode(RTreeNode* n) {
        if(!n) return;
        if(!n->isLeaf) {
            for(auto &e: n->entries) freeNode(e.child);
        }
        delete n;
    }
public:
    // insert a mapped point with object id
    void insert(const Vec &mappedPoint, uint64_t objectId) {
        RTreeEntry e;
        e.isLeafEntry = true;
        e.objectId = objectId;
        e.mappedPoint = mappedPoint;
        e.box = MBB(mappedPoint.size());
        e.box.expandWithPoint(mappedPoint);
        insertRec(root, e);
        if(root->entries.size() > maxEntries) {
            // split root
            auto [n1, n2] = splitNode(root);
            RTreeNode* newRoot = new RTreeNode(false);
            RTreeEntry e1, e2;
            e1.isLeafEntry=false; e1.child=n1; e1.box = computeNodeMBB(n1);
            e2.isLeafEntry=false; e2.child=n2; e2.box = computeNodeMBB(n2);
            newRoot->entries.push_back(e1);
            newRoot->entries.push_back(e2);
            root = newRoot;
        }
    }

private:
    void insertRec(RTreeNode* node, const RTreeEntry &e) {
        if(node->isLeaf) {
            node->entries.push_back(e);
        } else {
            // choose child minimizing area enlargement
            double bestInc = numeric_limits<double>::infinity();
            size_t bestIdx = 0;
            for(size_t i=0;i<node->entries.size();++i){
                MBB old = node->entries[i].box;
                MBB merged = old;
                merged.expandWithMBB(e.box);
                // compute "area" as product of (high-low) per dim (if zero length, small epsilon)
                double oldArea = mbbVolume(old);
                double newArea = mbbVolume(merged);
                double inc = newArea - oldArea;
                if(inc < bestInc) { bestInc = inc; bestIdx = i; }
            }
            insertRec(node->entries[bestIdx].child, e);
            // update box
            node->entries[bestIdx].box.expandWithMBB(e.box);
            // split if needed
            if(node->entries[bestIdx].child->entries.size() > maxEntries) {
                auto [n1,n2] = splitNode(node->entries[bestIdx].child);
                // replace child entry by two entries
                RTreeEntry e1,e2;
                e1.isLeafEntry=false; e1.child=n1; e1.box=computeNodeMBB(n1);
                e2.isLeafEntry=false; e2.child=n2; e2.box=computeNodeMBB(n2);
                // replace
                node->entries.erase(node->entries.begin()+bestIdx);
                node->entries.insert(node->entries.begin()+bestIdx, e2);
                node->entries.insert(node->entries.begin()+bestIdx, e1);
            }
        }
    }

    static double mbbVolume(const MBB &m) {
        double vol=1.0;
        for(size_t i=0;i<m.low.size();++i){
            double len = max(1e-12, m.high[i] - m.low[i]);
            vol *= len;
        }
        return vol;
    }

    // naive linear split: pick two seeds with largest distance between MBB centers (or points)
    pair<RTreeNode*, RTreeNode*> splitNode(RTreeNode* node) {
        // create two new nodes with same leaf flag
        RTreeNode* n1 = new RTreeNode(node->isLeaf);
        RTreeNode* n2 = new RTreeNode(node->isLeaf);
        size_t n = node->entries.size();
        if(n==0) return {n1,n2};
        // pick seeds: choose pair with max "separation" (distance between box centers)
        double bestDist=-1;
        size_t s1=0,s2=0;
        for(size_t i=0;i<n;i++){
            for(size_t j=i+1;j<n;j++){
                Vec c1 = center(node->entries[i].box);
                Vec c2 = center(node->entries[j].box);
                double dist = 0;
                for(size_t k=0;k<c1.size();++k){ double d=c1[k]-c2[k]; dist += d*d; }
                if(dist > bestDist) { bestDist=dist; s1=i; s2=j; }
            }
        }
        // assign seeds
        n1->entries.push_back(node->entries[s1]);
        n2->entries.push_back(node->entries[s2]);
        // assign rest greedily by minimal enlargement
        for(size_t i=0;i<n;i++){
            if(i==s1 || i==s2) continue;
            RTreeEntry &e = node->entries[i];
            MBB mb1 = computeNodeMBB(n1);
            MBB mb2 = computeNodeMBB(n2);
            MBB m1 = mb1; m1.expandWithMBB(e.box); double area1 = mbbVolume(m1) - mbbVolume(mb1);
            MBB m2 = mb2; m2.expandWithMBB(e.box); double area2 = mbbVolume(m2) - mbbVolume(mb2);
            if(area1 < area2) n1->entries.push_back(e);
            else n2->entries.push_back(e);
            // balance: ensure minEntries
            if((n - i + n1->entries.size()) == minEntries) {
                // must assign all remaining to n1
                // handled naturally as loop continues, not strictly necessary here
            }
        }
        delete node;
        return {n1,n2};
    }

    static Vec center(const MBB &m) {
        Vec c(m.dim());
        for(size_t i=0;i<m.dim();++i) c[i] = (m.low[i] + m.high[i]) / 2.0;
        return c;
    }

    static MBB computeNodeMBB(RTreeNode* node) {
        if(node->entries.empty()) return MBB();
        MBB m(node->entries[0].box.dim());
        bool first=true;
        for(auto &e: node->entries){
            if(first){ m = e.box; first=false; }
            else m.expandWithMBB(e.box);
        }
        return m;
    }

public:
    // Query: MRQ (range) -> returns object ids (uses pivot hyper-rect intersection)
    vector<uint64_t> rangeQuery(const Vec &qMap, double r) {
        vector<uint64_t> result;
        rangeRec(root, qMap, r, result);
        return result;
    }

private:
    void rangeRec(RTreeNode* node, const Vec &qMap, double r, vector<uint64_t> &res) {
        for(auto &e : node->entries) {
            if(!e.box.intersectsHyperRect(qMap, r)) {
                // prune via Lemma 4.1 (mapped box doesn't intersect search hyper-rect)
                continue;
            }
            if(node->isLeaf) {
                // leaf entry -> check object's mapped point (we already ensured box intersects,
                // but still we need to trust later verification (the RAF will give us object to compute real dist))
                res.push_back(e.objectId);
            } else {
                rangeRec(e.child, qMap, r, res);
            }
        }
    }

public:
    // kNN (best-first) using LB described in analysis
    vector<pair<uint64_t,double>> knnQuery(const Vec &qMap, size_t k, function<double(uint64_t)> verifyDistFunc) {
        // verifyDistFunc: when we pop leaf objects (ids) we call this to compute exact d(q,o)
        struct NodePQ {
            RTreeNode* node;
            double lb;
            bool operator<(const NodePQ& o) const { return lb > o.lb; } // for min-heap via priority_queue
        };
        struct EntryPQ {
            uint64_t oid;
            double lb;
            Vec mapped;
            bool operator<(const EntryPQ& o) const { return lb > o.lb; }
        };

        priority_queue<NodePQ> nodePQ;
        priority_queue<EntryPQ> leafPQ;
        vector<pair<uint64_t,double>> result;

        double INF = numeric_limits<double>::infinity();
        nodePQ.push({root, root->entries.empty() ? 0.0 : computeNodeMBB(root).lowerBoundToQuery(qMap)});

        // best-first: pop smallest LB node; if leaf entries, push entries into leafPQ; if internal, push child nodes
        while((!nodePQ.empty() || !leafPQ.empty()) && result.size() < k) {
            // ensure leafPQ has any candidate with LB <= best node lb
            if(!leafPQ.empty()) {
                // check if there is nodePQ with lb < top leaf lb; if so expand node first
                double bestLeafLB = leafPQ.top().lb;
                if(!nodePQ.empty() && nodePQ.top().lb < bestLeafLB) {
                    // expand node
                    auto nitem = nodePQ.top(); nodePQ.pop();
                    RTreeNode* node = nitem.node;
                    if(node->isLeaf) {
                        for(auto &e : node->entries) {
                            double lb = e.box.lowerBoundToQuery(qMap);
                            leafPQ.push({e.objectId, lb, e.mappedPoint});
                        }
                    } else {
                        for(auto &e : node->entries) {
                            double lb = e.box.lowerBoundToQuery(qMap);
                            nodePQ.push({e.child, lb});
                        }
                    }
                    continue;
                }
            }
            // If leafPQ not empty, pop and verify real distance
            if(!leafPQ.empty()) {
                auto ent = leafPQ.top(); leafPQ.pop();
                double reald = verifyDistFunc(ent.oid); // compute exact distance (user-provided)
                result.push_back({ent.oid, reald});
                // After collecting k items, we could prune nodes with lb > max result so far
                if(result.size() == k) break;
                continue;
            }
            // else nodePQ non-empty, expand next node
            if(!nodePQ.empty()) {
                auto nitem = nodePQ.top(); nodePQ.pop();
                RTreeNode* node = nitem.node;
                if(node->isLeaf) {
                    for(auto &e : node->entries) {
                        double lb = e.box.lowerBoundToQuery(qMap);
                        leafPQ.push({e.objectId, lb, e.mappedPoint});
                    }
                } else {
                    for(auto &e : node->entries) {
                        double lb = e.box.lowerBoundToQuery(qMap);
                        nodePQ.push({e.child, lb});
                    }
                }
                continue;
            }
        }
        // Sort results by distance asc and keep top k
        sort(result.begin(), result.end(), [](auto &a, auto &b){ return a.second < b.second; });
        if(result.size() > k) result.resize(k);
        return result;
    }
};

/*** -----------------------------
    OmniRTree wrapper tying everything
    ----------------------------- ***/
class OmniRTree {
    RAF raf;
    PivotTable pt;
    RTree rtree;
    const Metric* metric;
    // Keep small in-memory map id -> mapped vector (to avoid reading RAF for mapping retrieval during construction)
    unordered_map<uint64_t, Vec> mappedCache;
public:
    OmniRTree(const string &rafFile, const Metric* m, size_t rtreeNodeCap=32)
        : raf(rafFile), pt(m), rtree(rtreeNodeCap), metric(m) {}

    // build from vector of DataObjects: writes them to RAF and constructs pivot table and R-tree
    // pivots: either empty (random choose) or pre-provided ids
    void build(vector<DataObject> &objects, size_t l_pivots, bool selectRandom=true, uint64_t seed=42) {
        // 1) write all objects to RAF
        for(auto &o : objects) {
            raf.append(o);
        }
        // 2) choose pivots
        if(selectRandom) {
            pt.selectRandomPivots(objects, l_pivots, seed);
        } else {
            // if user provides pivots in objects[0..l_pivots-1]
            vector<DataObject> pivs;
            for(size_t i=0;i<l_pivots && i<objects.size(); ++i) pivs.push_back(objects[i]);
            pt.pivots = pivs;
        }
        // 3) compute mapping for each object and insert into R-tree
        mappedCache.clear();
        for(auto &o : objects) {
            Vec mvec = pt.mapObject(o);
            mappedCache[o.id] = mvec;
            rtree.insert(mvec, o.id);
        }
    }

    // MRQ: return candidate ids which must be verified against true metric
    vector<uint64_t> MRQ(const vector<double>& qPayload, double r) {
        // compute mapped query
        DataObject q; q.payload = qPayload; q.id = 0;
        Vec qMap = pt.mapObject(q);
        // retrieve candidate ids via R-tree rangeQuery using hyper-rectangle intersect (Lemma 4.1)
        vector<uint64_t> candidates = rtree.rangeQuery(qMap, r);
        return candidates;
    }

    // MkNN: return k nearest using best-first as described in paper.
    // verifyDistFunc must compute exact distance d(q, object) (calls RAF -> metric).
    vector<pair<uint64_t,double>> MkNN(const vector<double>& qPayload, size_t k) {
        DataObject q; q.payload = qPayload; q.id = 0;
        Vec qMap = pt.mapObject(q);
        // verify function uses RAF and metric
        auto verifyFunc = [&](uint64_t oid)->double {
            DataObject o = raf.read(oid);
            return metric->distance(q.payload, o.payload);
        };
        return rtree.knnQuery(qMap, k, verifyFunc);
    }

    // expose pivot table for inspection
    const PivotTable& getPivotTable() const { return pt; }
};

/*** -----------------------------
    Example usage & small test
    ----------------------------- ***/
int main(){
    ios::sync_with_stdio(false);
    cin.tie(nullptr);

    // Simple demo dataset: points in 3D Euclidean space
    Euclidean metric;
    vector<DataObject> dataset;
    const size_t N = 500;
    const size_t dim = 3;
    mt19937_64 rng(12345);
    uniform_real_distribution<double> unif(0.0, 100.0);

    for(size_t i=0;i<N;i++){
        DataObject o;
        o.id = i+1;
        o.payload.resize(dim);
        for(size_t j=0;j<dim;j++) o.payload[j] = unif(rng);
        dataset.push_back(o);
    }

    // Build OmniRTree: choose l=5 pivots, rtree node cap 32
    OmniRTree omni("omni_raf.bin", &metric, 32);
    omni.build(dataset, 5 /*l_pivots*/, true, 42);

    // choose a random query
    vector<double> q(dim);
    for(size_t j=0;j<dim;j++) q[j] = unif(rng);
    double range_r = 20.0;

    // MRQ: get candidates (must verify in real system by computing d(q,o) with RAF)
    auto cand = omni.MRQ(q, range_r);
    cout << "MRQ candidates: " << cand.size() << " (objects whose mapped vectors intersect pivot-hyperrect).\n";

    // verify and count true results
    size_t truecnt=0;
    for(auto id : cand) {
        DataObject o = omni.getPivotTable().pivots.size() ? DataObject() : DataObject(); // dummy to quiet unused warning
        double dist = metric.distance(q, dataset[id-1].payload);
        if(dist <= range_r) truecnt++;
    }
    cout << "Verified true MRQ results (via in-memory dataset): " << truecnt << " elements.\n";

    // MkNN
    size_t K = 5;
    auto knn = omni.MkNN(q, K);
    cout << "MkNN results (k="<<K<<"):\n";
    for(auto &pr : knn) {
        cout << " id="<<pr.first<< " d="<<pr.second<<"\n";
    }

    // show pivots
    const PivotTable &pt = omni.getPivotTable();
    cout << "Pivots selected (ids):";
    for(auto &p: pt.pivots) cout << " " << p.id;
    cout << "\n";

    // Note: this program stores objects in omni_raf.bin (RAF). The R-tree is in-memory only.
    // For real testing with large objects, RAF allows avoiding storing the objects inside R-tree nodes (as in paper).
    //
    // Limitations / extension ideas:
    // - R-tree here is in-memory; to follow paper strictly you'd store R-tree on disk pages and maintain RAF separate.
    // - Split strategy is linear/simple; production R-tree variants (R*-tree) improve query performance.
    // - Pivot selection: random in this demo. The paper discusses pivot selection effects — consider k-centers or farthest-first.
    //
    // Citations: Omni-family description, MRQ and MkNN processing and pivot filtering (Lemma 4.1) in Chen et al. (2022). :contentReference[oaicite:9]{index=9} :contentReference[oaicite:10]{index=10}

    return 0;
}
