#ifndef CPT_HPP
#define CPT_HPP

#include "../../objectdb.hpp"

#include <vector>
#include <queue>
#include <algorithm>
#include <cmath>
#include <limits>
#include <chrono>
#include <cstdint>
#include <cstdio>
#include <iostream>

struct CPTResultElem {
    int id;
    double dist;

    // For std::priority_queue as max-heap (top = largest dist)
    bool operator<(const CPTResultElem& other) const {
        return dist < other.dist;
    }
};


class CPT {
public:
    mutable long long compDistQuery  = 0;  // # distance computations in queries
    mutable long long compDistBuild  = 0;  // # distance computations to build pivot table
    mutable long long pageReads      = 0;  // # data pages read
    mutable long long queryTime      = 0;  // accumulated time in µs (only queries)

    explicit CPT(const ObjectDB* db_, int nPivots_)
        : db(db_),
          n(db_ ? db_->size() : 0),
          nPivots(nPivots_)
    {
        if (!db) {
            nPivots = 0;
            return;
        }
        if (nPivots > n) nPivots = n;

        pivots.resize(nPivots);
        isPivot.assign(n, false);

        for (int i = 0; i < nPivots; ++i) {
            pivots[i] = i;
            isPivot[i] = true;
        }

        buildDefaultPages();

        buildDistanceTable();
    }

    void overridePivots(const std::vector<int>& newPivots) {
        if (!db) return;
        if ((int)newPivots.size() != nPivots) {
            std::cerr << "[CPT] overridePivots: invalid size ("
                      << newPivots.size() << " != " << nPivots << ")\n";
            return;
        }

        std::fill(isPivot.begin(), isPivot.end(), false);
        pivots = newPivots;

        for (int p : pivots) {
            if (p < 0 || p >= n) {
                std::cerr << "[CPT] overridePivots: pivot id out of range: " << p << "\n";
                return;
            }
            isPivot[p] = true;
        }

        buildDistanceTable();
    }

    int get_num_pivots() const { return nPivots; }

    void buildFromMTree(const std::string& basePath) {
        pages.clear();

        std::string indexPath = basePath + ".mtree_index";
        FILE* fp = std::fopen(indexPath.c_str(), "rb");
        if (!fp) {
            std::cerr << "[CPT] buildFromMTree: cannot open " << indexPath << "\n";
            buildDefaultPages();
            return;
        }

        int64_t rootOffset;
        if (std::fread(&rootOffset, sizeof(int64_t), 1, fp) != 1) {
            std::cerr << "[CPT] buildFromMTree: corrupted file (no rootOffset)\n";
            std::fclose(fp);
            buildDefaultPages();
            return;
        }

        // Scan all nodes sequentially
        while (true) {
            bool isLeaf;
            int32_t count;

            size_t r1 = std::fread(&isLeaf, sizeof(bool), 1, fp);
            if (r1 != 1) {
                // EOF or error
                if (!std::feof(fp)) {
                    std::cerr << "[CPT] buildFromMTree: fread(isLeaf) failed\n";
                }
                break;
            }
            if (std::fread(&count, sizeof(int32_t), 1, fp) != 1) {
                std::cerr << "[CPT] buildFromMTree: fread(count) failed\n";
                break;
            }

            std::vector<int> leafObjs;
            leafObjs.reserve(count);

            for (int i = 0; i < count; ++i) {
                int32_t objId;
                double radius;
                double parentDist;
                int64_t childOffset;

                if (std::fread(&objId,      sizeof(int32_t), 1, fp) != 1 ||
                    std::fread(&radius,     sizeof(double),  1, fp) != 1 ||
                    std::fread(&parentDist, sizeof(double),  1, fp) != 1 ||
                    std::fread(&childOffset,sizeof(int64_t), 1, fp) != 1) {
                    std::cerr << "[CPT] buildFromMTree: fread(entry) failed\n";
                    leafObjs.clear();
                    break;
                }

                if (isLeaf) {
                    leafObjs.push_back(objId);
                }
            }

            if (isLeaf && !leafObjs.empty()) {
                pages.push_back(std::move(leafObjs));
            }
        }

        std::fclose(fp);

        if (pages.empty()) {
            std::cerr << "[CPT] buildFromMTree: no leaf pages found, "
                         "falling back to default single page.\n";
            buildDefaultPages();
        } else {
            std::cerr << "[CPT] buildFromMTree: built " << pages.size()
                      << " pages clustered from M-tree.\n";
        }
    }

    void setPages(const std::vector<std::vector<int>>& newPages) {
        pages = newPages;
    }

    void buildSequentialPages(int objectsPerPage) {
        pages.clear();
        if (objectsPerPage <= 0 || n == 0) return;

        std::vector<int> current;
        current.reserve(objectsPerPage);

        for (int i = 0; i < n; ++i) {
            current.push_back(i);
            if ((int)current.size() == objectsPerPage) {
                pages.push_back(current);
                current.clear();
            }
        }
        if (!current.empty()) pages.push_back(current);
    }

    void clear_counters() const {
        compDistQuery = 0;
        pageReads     = 0;
        queryTime     = 0;
    }

    long long get_compDist()      const { return compDistQuery;  }
    long long get_compDistBuild() const { return compDistBuild;  }
    long long get_pageReads()     const { return pageReads;      }
    long long get_queryTime()     const { return queryTime;      }

    void rangeSearch(int queryId, double radius,
                     std::vector<int>& result) const
    {
        using clock = std::chrono::high_resolution_clock;
        auto t0 = clock::now();

        result.clear();
        if (!db || n == 0 || nPivots == 0 || pages.empty()) {
            auto t1 = clock::now();
            queryTime += std::chrono::duration_cast<std::chrono::microseconds>(t1 - t0).count();
            return;
        }

        // 1. Distances from query to pivots (in-memory).
        std::vector<double> queryDists(nPivots);
        for (int j = 0; j < nPivots; ++j) {
            queryDists[j] = db->distance(queryId, pivots[j]);
            compDistQuery++;
            // Pivots are in RAM; no pageReads.
            if (queryDists[j] <= radius) {
                result.push_back(pivots[j]);
            }
        }

        // 2. Scan pages in physical order.
        for (const auto& page : pages) {
            std::vector<int> candidates;
            candidates.reserve(page.size());

            // Check lower bound (Lemma 4.1) per object in this page.
            for (int objId : page) {
                // Skip pivots (already considered).
                if (isPivot[objId]) continue;

                double lb = lowerBound(queryDists, objId);
                if (lb <= radius) {
                    candidates.push_back(objId);
                }
            }

            if (candidates.empty()) {
                continue;
            }

            pageReads++;

            // Compute exact distances only for candidates.
            for (int objId : candidates) {
                double d = db->distance(queryId, objId);
                compDistQuery++;
                if (d <= radius) {
                    result.push_back(objId);
                }
            }
        }

        auto t1 = clock::now();
        queryTime += std::chrono::duration_cast<std::chrono::microseconds>(t1 - t0).count();
    }

    void knnSearch(int queryId, int k,
                   std::vector<CPTResultElem>& out,
                   double preScanFraction = 0.02) const
    {
        using clock = std::chrono::high_resolution_clock;
        auto t0 = clock::now();

        out.clear();
        if (!db || n == 0 || nPivots == 0 || pages.empty() || k <= 0) {
            auto t1 = clock::now();
            queryTime += std::chrono::duration_cast<std::chrono::microseconds>(t1 - t0).count();
            return;
        }

        compDistQuery = 0;
        pageReads     = 0;

        // 1. Distances query–pivots (in-memory).
        std::vector<double> queryDists(nPivots);
        for (int j = 0; j < nPivots; ++j) {
            queryDists[j] = db->distance(queryId, pivots[j]);
            compDistQuery++;
        }

        // 2. Pre-scan a small prefix of objects (non-clustered region)
        int N0 = std::max(1, (int)std::round(preScanFraction * n));
        if (N0 > n) N0 = n;

        std::priority_queue<CPTResultElem> best; // max-heap by distance

        for (int objId = 0; objId < N0; ++objId) {
            double d = db->distance(queryId, objId);
            compDistQuery++;
            CPTResultElem e{objId, d};
            if ((int)best.size() < k) {
                best.push(e);
            } else if (e.dist < best.top().dist) {
                best.pop();
                best.push(e);
            }
        }

        double tau = best.size() == (size_t)k
                   ? best.top().dist
                   : std::numeric_limits<double>::infinity();

        // 3. Scan clustered pages in physical order.
        for (const auto& page : pages) {
            std::vector<int> candidates;
            candidates.reserve(page.size());

            // Check lower bound for objects in this page.
            for (int objId : page) {
                // Skip objects in pre-scan prefix.
                if (objId < N0) continue;

                double lb = lowerBound(queryDists, objId);
                if (best.size() < (size_t)k || lb < tau) {
                    candidates.push_back(objId);
                }
            }

            if (candidates.empty()) {
                // Page fully pruned; no I/O.
                continue;
            }

            pageReads++;

            // Compute real distances only for candidates.
            for (int objId : candidates) {
                double d = db->distance(queryId, objId);
                compDistQuery++;

                CPTResultElem e{objId, d};
                if ((int)best.size() < k) {
                    best.push(e);
                    if ((int)best.size() == k)
                        tau = best.top().dist;
                } else if (d < best.top().dist) {
                    best.pop();
                    best.push(e);
                    tau = best.top().dist;
                }
            }
        }

        // 4. Extract results sorted by distance ascending.
        while (!best.empty()) {
            out.push_back(best.top());
            best.pop();
        }
        std::reverse(out.begin(), out.end());

        auto t1 = clock::now();
        queryTime += std::chrono::duration_cast<std::chrono::microseconds>(t1 - t0).count();
    }

private:
    const ObjectDB* db = nullptr;
    int n        = 0;  // #objects
    int nPivots  = 0;  // #pivots

    std::vector<int>  pivots;                     // pivot IDs
    std::vector<bool> isPivot;                    // quick pivot check
    std::vector<std::vector<double>> distMatrix;  // [obj][pivot] = d(obj,pivot)

    std::vector<std::vector<int>> pages;

    // Default: single page [0..n-1]
    void buildDefaultPages() {
        pages.clear();
        if (n == 0) return;
        std::vector<int> all(n);
        for (int i = 0; i < n; ++i) all[i] = i;
        pages.push_back(std::move(all));
    }

    // Build full distance table object–pivots.
    void buildDistanceTable() {
        distMatrix.assign(n, std::vector<double>(nPivots));
        compDistBuild = 0;

        if (!db || nPivots == 0) return;

        for (int i = 0; i < n; ++i) {
            for (int j = 0; j < nPivots; ++j) {
                distMatrix[i][j] = db->distance(i, pivots[j]);
                compDistBuild++;
            }
        }

        std::cerr << "[CPT] Built distance table with "
                  << nPivots << " pivots ("
                  << (double(n) * double(nPivots) * sizeof(double)) / (1024.0 * 1024.0)
                  << " MB)\n";
    }

    // Lower bound via triangle inequality (Lemma 4.1)
    double lowerBound(const std::vector<double>& queryDists,
                      int objectIdx) const
    {
        double maxDiff = 0.0;
        for (int j = 0; j < nPivots; ++j) {
            double diff = std::fabs(queryDists[j] - distMatrix[objectIdx][j]);
            if (diff > maxDiff) maxDiff = diff;
        }
        return maxDiff;
    }
};

#endif // CPT_HPP
