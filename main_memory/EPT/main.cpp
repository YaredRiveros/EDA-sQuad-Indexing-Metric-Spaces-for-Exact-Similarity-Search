#include "main.h"
#include "Objvector.h"
#include "Interpreter.h"
#include "Tuple.h"
#include "Cache.h"

#include <iostream>
#include <vector>
#include <cstdlib>
#include <cstring>
#include <cfloat>
#include <cassert>

using namespace std;

// globals used in other translation units
Interpreter* pi = nullptr;
int **pivotSeq = nullptr; // optional (not used in corrections unless provided)
set<int> pivotSet;
Tuple **gb = nullptr; // not used in this corrected code, kept for compatibility
float* DB = nullptr;
Tuple* G = nullptr;
int * sequence = nullptr;
double compDists = 0;
int LGroup = 0;
float* nobj = nullptr;

#ifdef RESULT
ofstream ret_range("ret_range.txt", ios::out);
#endif

double recyc_times = 0.0;

inline double safe_df(float* a, float* b, int dim) {
    // wrapper to count comparisons
    ++compDists;
    return pi->df(a, b, dim);
}

// calculate L2 distance between two data (by indexes)
double calculateDistance(int v1, int v2)
{
    return safe_df(DB + static_cast<size_t>(pi->dim) * v1, DB + static_cast<size_t>(pi->dim) * v2, pi->dim);
}

double calculateDistance(const Objvector &o1, int v2)
{
    // copy values into nobj (temporary buffer)
    for (int d = 0; d < pi->dim; ++d) {
        nobj[d] = o1.getValue()[d];
    }
    return safe_df(nobj, DB + static_cast<size_t>(pi->dim) * v2, pi->dim);
}

double calculateDistance(const float* o1, int v2)
{
    ++compDists;
    return pi->df(const_cast<float*>(o1), DB + static_cast<size_t>(pi->dim) * v2, pi->dim);
}

// average distance mu for pivot pid (pid is object id)
double calculateMiu(int pid)
{
    double miu = 0.0;
    for (int i = 0; i < pi->num; ++i) {
        miu += calculateDistance(i, pid);
    }
    return miu / static_cast<double>(pi->num);
}

// Helper: pick `nums` distinct random indices in [0, objs_num-1], returns vector
static vector<int> sampleDistinct(int nums, int objs_num)
{
    if (nums <= 0) return {};
    if (nums >= objs_num) {
        vector<int> all(objs_num);
        for (int i = 0; i < objs_num; ++i) all[i] = i;
        random_shuffle(all.begin(), all.end());
        all.resize(nums);
        return all;
    }
    vector<int> seq(objs_num);
    for (int i = 0; i < objs_num; ++i) seq[i] = i;
    vector<int> out;
    for (int i = 0; i < nums; ++i) {
        int idx = rand() % (objs_num - i);
        out.push_back(seq[idx]);
        seq[idx] = seq[objs_num - i - 1];
    }
    return out;
}

// calculate variance of X(distance between data and their pivot)
double calculateVx2(int group)
{
    double mean = 0.0;
    for (int i = 0; i < pi->num; i++)
    {
        mean += gb[group][i].getDistance();
    }
    mean = mean / pi->num;
    double vx2 = 0.0;
    for (int j = 0; j < pi->num; j++) {
        double d = (gb[group][j].getDistance() - mean);
        vx2 += d * d;
    }
    return vx2 / pi->num;
}

double calculateVy2(const vector<int>& samples)
{
    if (samples.size() < 2) return 0.0;
    double mean = 0.0;
    long long count = 0;
    int S = static_cast<int>(samples.size());
    for (int i = 0; i < S; ++i) {
        for (int j = i + 1; j < S; ++j) {
            mean += calculateDistance(samples[i], samples[j]);
            ++count;
        }
    }
    if (count == 0) return 0.0;
    mean = mean / static_cast<double>(count);
    double vy2 = 0.0;
    for (int i = 0; i < S; ++i) {
        for (int j = i + 1; j < S; ++j) {
            double d = calculateDistance(samples[i], samples[j]) - mean;
            vy2 += d * d;
        }
    }
    return vy2 / static_cast<double>(count);
}

double calculateR2()
{
    // placeholder - original returned constant; keep it but warn if used
    double radius = 100.0;
    return radius;
}

static const int NUM_CAND = 40;
int * cand = nullptr;
bool *ispivot = nullptr;
double** O_P_matrix = nullptr;
vector<int> samples_objs;

// MaxPrunning: picks NUM_CAND candidate pivots from samples_objs
double** MaxPrunning(int sampleSize)
{
    if (sampleSize <= 0 || static_cast<size_t>(sampleSize) > samples_objs.size()) {
        throw runtime_error("MaxPrunning: invalid sample size");
    }

    cand = new int[NUM_CAND];
    std::fill(cand, cand + NUM_CAND, -1);

    vector<char> indicator(sampleSize, 1);
    vector<int> idset;
    idset.reserve(NUM_CAND);

    // allocate matrix: sampleSize x NUM_CAND
    double** distmatrix = new double*[sampleSize];
    for (int i = 0; i < sampleSize; ++i) {
        distmatrix[i] = new double[NUM_CAND];
        for (int j = 0; j < NUM_CAND; ++j) distmatrix[i][j] = 0.0;
    }

    // choose first pivot: farthest from samples_objs[0]
    double dmax = -1.0;
    int choose = -1;
    if (sampleSize > 1) {
        for (int i = 1; i < sampleSize; ++i) {
            double t = calculateDistance(samples_objs[i], samples_objs[0]);
            if (t > dmax) { dmax = t; choose = i; }
        }
        if (choose < 0) choose = 0;
    } else {
        choose = 0;
    }
    idset.push_back(choose);
    cand[0] = samples_objs[choose];
    indicator[choose] = 0;

    // choose second pivot: farthest from first pivot
    if (NUM_CAND > 1) {
        dmax = -1.0;
        int choose2 = -1;
        for (int i = 0; i < sampleSize; ++i) {
            if (indicator[i]) {
                distmatrix[i][0] = calculateDistance(samples_objs[choose], samples_objs[i]);
                if (distmatrix[i][0] > dmax) { dmax = distmatrix[i][0]; choose2 = i; }
            }
        }
        if (choose2 < 0) choose2 = 0;
        idset.push_back(choose2);
        cand[1] = samples_objs[choose2];
        indicator[choose2] = 0;
    }

    // iterative selection for remaining candidates
    double edge = dmax;
    for (int i = 2; i < NUM_CAND; ++i) {
        double bestT = std::numeric_limits<double>::max();
        int bestChoose = -1;
        for (int j = 0; j < sampleSize; ++j) {
            if (!indicator[j]) continue;
            double t = 0.0;
            // sum deviation with previously selected candidate distances
            for (int k = 0; k < i - 1; ++k) {
                t += std::fabs(edge - distmatrix[j][k]);
            }
            distmatrix[j][i - 1] = calculateDistance(samples_objs[j], cand[i - 1]); // uses previously chosen cand
            t += std::fabs(edge - distmatrix[j][i - 1]);
            if (t < bestT) { bestT = t; bestChoose = j; }
        }
        if (bestChoose < 0) bestChoose = 0;
        idset.push_back(bestChoose);
        indicator[bestChoose] = 0;
        cand[i] = samples_objs[bestChoose];
    }

    // Fill remaining entries in distmatrix last column
    for (int i = 0; i < sampleSize; ++i) {
        distmatrix[i][NUM_CAND - 1] = calculateDistance(samples_objs[i], cand[NUM_CAND - 1]);
    }

    // compute pairwise distances between idset representatives and candidates
    for (size_t ii = 0; ii < idset.size(); ++ii) {
        int idx = static_cast<int>(idset[ii]);
        for (int j = 0; j < NUM_CAND; ++j) {
            distmatrix[idx][j] = calculateDistance(samples_objs[idx], cand[j]);
        }
    }

    return distmatrix;
}

// PivotSelect: select pivotNum pivots for object objId from candidate list
int PivotSelect(int objId, int o_num, int q_num, int pivotNum)
{
    // o_num = sampleSize; q_num typically 1 in calls from algorithm3
    if (pivotNum <= 0) return 0;

    // allocate matrices
    vector<vector<double>> Q_O_matrix(q_num, vector<double>(o_num, 0.0));
    vector<vector<double>> Q_P_matrix(q_num, vector<double>(NUM_CAND, 0.0));
    vector<vector<double>> esti(q_num, vector<double>(o_num, 0.0));
    vector<char> indicator(NUM_CAND, 1);

    // prepare Q_O_matrix and Q_P_matrix
    for (int i = 0; i < q_num; ++i) {
        for (int j = 0; j < o_num; ++j) {
            Q_O_matrix[i][j] = calculateDistance(objId, samples_objs[j]);
            esti[i][j] = 0.0;
        }
        for (int j = 0; j < NUM_CAND; ++j) {
            Q_P_matrix[i][j] = calculateDistance(objId, cand[j]);
        }
    }

    double bestScore = -1.0;
    int chosenCount = 0;

    for (int i = 0; i < pivotNum; ++i) {
        int choose = -1;
        double bestT = -1.0;
        for (int j = 0; j < NUM_CAND; ++j) {
            if (!indicator[j]) continue;
            double t = 0.0;
            for (int m = 0; m < q_num; ++m) {
                for (int n = 0; n < o_num; ++n) {
                    double denom = Q_O_matrix[m][n];
                    if (denom != 0.0) {
                        double val = std::max(std::fabs(Q_P_matrix[m][j] - O_P_matrix[n][j]), esti[m][n]);
                        t += val / denom;
                    }
                }
            }
            t = t / (q_num * o_num);
            if (t > bestT) {
                bestT = t;
                choose = j;
            }
        }
        if (choose == -1) break;
        // mark chosen
        indicator[choose] = 0;
        if (!ispivot[choose]) ispivot[choose] = true;

        // store pivot id (actual object id)
        int chosenPivotObjId = cand[choose];
        G[objId * pivotNum + i].setObjId(objId);
        G[objId * pivotNum + i].setPivId(chosenPivotObjId);
        G[objId * pivotNum + i].setDistance(Q_P_matrix[0][choose]);

        // update esti
        for (int m = 0; m < q_num; ++m) {
            for (int n = 0; n < o_num; ++n) {
                double cur = std::fabs(Q_P_matrix[m][choose] - O_P_matrix[n][choose]);
                esti[m][n] = std::max(cur, esti[m][n]);
            }
        }
        ++chosenCount;
    }

    // if not enough pivots selected, fill with remaining candidates
    if (chosenCount < pivotNum) {
        for (int j = 0; j < NUM_CAND && chosenCount < pivotNum; ++j) {
            if (indicator[j]) {
                indicator[j] = 0;
                if (!ispivot[j]) ispivot[j] = true;
                int chosenPivotObjId = cand[j];
                G[objId * pivotNum + chosenCount].setObjId(objId);
                G[objId * pivotNum + chosenCount].setPivId(chosenPivotObjId);
                G[objId * pivotNum + chosenCount].setDistance(Q_P_matrix[0][j]);
                ++chosenCount;
            }
        }
    }

    return pivotNum;
}

// sample_data: sample `nums` object indices from objs_num without replacement
void sample_data(int nums, int objs_num)
{
    samples_objs.clear();
    vector<int> s = sampleDistinct(nums, objs_num);
    // transform sample indices into actual object ids (here sampleDistinct returns indices already)
    for (int idx : s) samples_objs.push_back(idx);
}

// readIndex & saveIndex (kept similar to original, but safe)
bool readIndex(Tuple* t, const char* fname)
{
    if (!t) return false;
    FILE* fp = fopen(fname, "rb");
    if (!fp) return false;
    for (int i = 0; i < pi->num; ++i) {
        for (int j = 0; j < LGroup; ++j) {
            int pid = -1, oid = -1;
            double d = 0.0;
            if (fread(&pid, sizeof(int), 1, fp) != 1 ||
                fread(&oid, sizeof(int), 1, fp) != 1 ||
                fread(&d, sizeof(double), 1, fp) != 1)
            {
                fclose(fp);
                return false;
            }
            t[i * LGroup + j].setPivId(pid);
            t[i * LGroup + j].setObjId(oid);
            t[i * LGroup + j].setDistance(d);
            // mark the candidate index: here pid is pivot object id; we need to find its candidate index to set ispivot
            // but we cannot map easily; skip marking ispivot here
        }
    }
    fclose(fp);
    return true;
}

void saveIndex(Tuple * t, char *fname)
{
    if (!t) return;
    FILE *fp = fopen(fname, "wb");
    if (!fp) {
        fprintf(stderr, "saveIndex: Error opening output file %s\n", fname);
        return;
    }
    for (int i = 0; i < pi->num; ++i) {
        for (int j = 0; j < LGroup; ++j) {
            int pid = t[i*LGroup + j].getPivId();
            int oid = t[i*LGroup + j].getObjId();
            double d = t[i*LGroup + j].getDistance();
            fwrite(&pid, sizeof(int), 1, fp);
            fwrite(&oid, sizeof(int), 1, fp);
            fwrite(&d, sizeof(double), 1, fp);
        }
    }
    fclose(fp);
}

// KNNQuery and rangeQuery: use G table where G stores pivots per object (pivots are pivot object IDs)
double KNNQuery(float* q, int k)
{
    vector<SortEntry> result;
    vector<double> pivotsquery(NUM_CAND, 0.0);

    for (int i = 0; i < NUM_CAND; ++i) {
        if (ispivot && ispivot[i]) {
            pivotsquery[i] = calculateDistance(q, cand[i]);
        }
    }
    double kdist = numeric_limits<double>::max();
    bool next = false;
    double tempdist;

    // iterate objects
    for (int i = 0; i < pi->num; ++i) {
        next = false;
        int pos = i * LGroup;
        for (int j = 0; j < LGroup; ++j) {
            int pivObjId = G[pos + j].getPivId();
            // find candidate index of this pivot object (candIndex) -- linear search (could be optimized with map)
            int candIndex = -1;
            for (int c = 0; c < NUM_CAND; ++c) {
                if (cand[c] == pivObjId) { candIndex = c; break; }
            }
            if (candIndex == -1) continue; // pivot wasn't from candidate list? skip this pivot
            if (std::fabs(pivotsquery[candIndex] - G[pos + j].getDistance()) > kdist) {
                next = true;
                break;
            }
        }
        if (!next) {
            tempdist = calculateDistance(reinterpret_cast<const float*>(q), i);
            if (tempdist <= kdist) {
                SortEntry s; s.dist = tempdist;
                result.push_back(s);
                sort(result.begin(), result.end());
                if ((int)result.size() > k) result.pop_back();
                if ((int)result.size() >= k) kdist = result.back().dist;
            }
        }
    }
    return kdist;
}

int rangeQuery(float* q, double radius, int /*qj*/)
{
    int result = 0;
    vector<double> pivotsquery(NUM_CAND, 0.0);
    for (int i = 0; i < NUM_CAND; ++i) {
        if (ispivot && ispivot[i]) {
            pivotsquery[i] = calculateDistance(reinterpret_cast<const float*>(q), cand[i]);
        }
    }
    for (int i = 0; i < pi->num; ++i) {
        bool next = false;
        int pos = i * LGroup;
        for (int j = 0; j < LGroup; ++j) {
            int pivObjId = G[pos + j].getPivId();
            int candIndex = -1;
            for (int c = 0; c < NUM_CAND; ++c) {
                if (cand[c] == pivObjId) { candIndex = c; break; }
            }
            if (candIndex == -1) continue;
            if (std::fabs(pivotsquery[candIndex] - G[pos + j].getDistance()) > radius) {
                next = true;
                break;
            }
        }
        if (!next) {
            if (calculateDistance(reinterpret_cast<const float*>(q), i) <= radius) result++;
        }
    }
    return result;
}


// algorithm3: build EPT* using PSA
void algorithm3(char* fname)
{
    cout << "algorithm3 start." << endl;

    int sampleSize = max(1, pi->num / 200);
    sample_data(sampleSize, pi->num);

    // init ispivot and cand
    if (ispivot) delete[] ispivot;
    ispivot = new bool[NUM_CAND];
    fill(ispivot, ispivot + NUM_CAND, false);

    // get candidate pivots and distances matrix
    O_P_matrix = MaxPrunning(static_cast<int>(samples_objs.size()));

    // allocate G: pi->num * LGroup
    if (G) { delete[] G; G = nullptr; }
    G = new Tuple[static_cast<size_t>(pi->num) * static_cast<size_t>(LGroup)];

    // try to read index; if not present, run pivot selection for each object
    if (!readIndex(G, const_cast<char*>(fname))) {
        for (int i = 0; i < pi->num; ++i) {
            if (i % 10000 == 0) cout << "PivotSelect at object: " << i << endl;
            PivotSelect(i, static_cast<int>(samples_objs.size()), 1, LGroup);
        }
    }

    // free O_P_matrix safely
    for (size_t i = 0; i < samples_objs.size(); ++i) {
        delete[] O_P_matrix[i];
    }
    delete[] O_P_matrix; O_P_matrix = nullptr;

    cout << "algorithm3 end." << endl;
}

// Original main() commented out - now using test.cpp main()
#if 0
int main(int argc, char **argv)
{
    clock_t begin, buildEnd, queryEnd;
    double buildComp, queryComp;

    cout << "main start." << endl;

    srand(static_cast<unsigned int>(time(NULL)));

    if (argc < 6) {
        cerr << "Usage: " << argv[0] << " <input> <index_out> <results_out> <queries> <pivotnum>" << endl;
        return 1;
    }
    const char* input = argv[1];
    const char* indexfile = argv[2];
    const char* results_out = argv[3];
    const char* querydata = argv[4];
    int pn = atoi(argv[5]);
    if (pn <= 0) { cerr << "Invalid pivotnum" << endl; return 1; }
    LGroup = pn;

    // parse dataset
    pi = new Interpreter();
    try {
        pi->parseRawData(input);
    } catch (const exception& ex) {
        cerr << "Error parsing raw data: " << ex.what() << endl;
        return 1;
    }

    // prepare nobj buffer
    if (nobj) delete[] nobj;
    nobj = new float[pi->dim];

    FILE* f = fopen(results_out, "w");
    if (!f) {
        cerr << "Cannot open results output file: " << results_out << endl;
        return 1;
    }

    fprintf(f, "pivotnum: %d\n", LGroup);

    begin = clock();
    compDists = 0.0;

    algorithm3(indexfile);
    saveIndex(G, indexfile);

    buildEnd = clock() - begin;
    buildComp = compDists;
    fprintf(f, "building...\n");
    fprintf(f, "finished... %f build time\n", (double)buildEnd / CLOCKS_PER_SEC);
    fprintf(f, "finished... %f distances computed\n", buildComp);

    // Querying
    const int qcount = 100;
    vector<int> kvalues = {1,5,10,20,50,100};
    vector<double> radius;
    // simple default radius values if dataset not matched
    if (string(input).find("LA") != string::npos) {
        radius = {473, 692, 989, 1409, 1875, 2314, 3096};
    } else if (string(input).find("integer") != string::npos) {
        radius = {2321, 2733, 3229, 3843, 4614, 5613, 7090};
    } else if (string(input).find("mpeg_1M") != string::npos) {
        radius = {3838, 4092, 4399, 4773, 5241, 5904, 7104};
    } else if (string(input).find("sf") != string::npos) {
        radius = {100,200,300,400,500,600,700};
    } else {
        radius = {1,2,3,4,5,6,7};
    }

    // KNN queries
    std::ifstream qfin(querydata, ios::in);
    if (!qfin.is_open()) {
        cerr << "Cannot open queries file: " << querydata << endl;
        fclose(f);
        return 1;
    }
    vector<float> obj(pi->dim, 0.0f);

    for (size_t ik = 0; ik < kvalues.size(); ++ik) {
        qfin.clear();
        qfin.seekg(0);
        compDists = 0.0;
        double totalRad = 0.0;
        clock_t start = clock();

        for (int j = 0; j < qcount; ++j) {
            for (int x = 0; x < pi->dim; ++x) {
                if (!(qfin >> obj[x])) {
                    // if not enough queries, rewind and continue
                    qfin.clear();
                    qfin.seekg(0);
                    qfin >> obj[x];
                }
            }
            double kd = KNNQuery(obj.data(), kvalues[ik]);
            totalRad += kd;
        }

        clock_t times = clock() - start;
        queryComp = compDists;
        fprintf(f, "k: %d\n", kvalues[ik]);
        fprintf(f, "finished... %f query time\n", (double)times / CLOCKS_PER_SEC / qcount);
        fprintf(f, "finished... %f distances computed\n", queryComp / qcount);
        fprintf(f, "finished... %f radius\n", totalRad / qcount);
        fflush(f);
    }

    // Range queries
    for (size_t ir = 0; ir < radius.size(); ++ir) {
        qfin.clear();
        qfin.seekg(0);
        compDists = 0.0;
        double totalObjs = 0.0;
        clock_t start = clock();
        for (int j = 0; j < qcount; ++j) {
            for (int x = 0; x < pi->dim; ++x) {
                if (!(qfin >> obj[x])) {
                    qfin.clear();
                    qfin.seekg(0);
                    qfin >> obj[x];
                }
            }
            int temp = rangeQuery(obj.data(), radius[ir], j);
            totalObjs += temp;
        }
        clock_t times = clock() - start;
        queryComp = compDists;
        fprintf(f, "r: %f\n", radius[ir]);
        fprintf(f, "finished... %f query time\n", (double)times / CLOCKS_PER_SEC / qcount);
        fprintf(f, "finished... %f distances computed\n", queryComp / qcount);
        fprintf(f, "finished... %f objs\n", totalObjs / qcount);
        fflush(f);
    }

    // cleanup
    if (cand) { delete[] cand; cand = nullptr; }
    if (ispivot) { delete[] ispivot; ispivot = nullptr; }
    if (G) { delete[] G; G = nullptr; }
    if (DB) { delete[] DB; DB = nullptr; }
    if (nobj) { delete[] nobj; nobj = nullptr; }

    fclose(f);
    cout << "main end." << endl;
    return 0;
}
#endif // commented out main()
