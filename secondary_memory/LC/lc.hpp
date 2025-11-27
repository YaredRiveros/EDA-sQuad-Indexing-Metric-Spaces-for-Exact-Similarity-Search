#ifndef LC_DISK_HPP
#define LC_DISK_HPP

#include "../../objectdb.hpp"
#include <vector>
#include <set>
#include <queue>
#include <cmath>
#include <chrono>
#include <algorithm>
#include <limits>
#include <iostream>
#include <fstream>
#include <cstdint>
#include <cstdio>

class LC_Disk {
public:
    // Info de cluster en RAM
    struct ClusterInfo {
        int      centerId;  // ID del centro
        double   radius;    // radio (máx dist centro–miembro)
        int32_t  count;     // # miembros
        int64_t  offset;    // desplazamiento en base.lc_node (en IDs, no bytes)
    };

    // Formato en disco (por si se quiere leer directamente desde archivo)
    struct ClusterInfoDisk {
        int32_t  centerId;
        double   radius;
        int32_t  count;
        int64_t  offset;   // desplazamiento en unidades de int
    };

private:
    const ObjectDB* db;
    int             n;              // número de objetos
    int             pageBytes;      // tamaño lógico de cluster
    int             bucketSize;     // capacidad máx de miembros por cluster
    int             pagesPerCluster;// pageBytes / 4096, usado para PA

    std::vector<ClusterInfo> clusters;

    // Métricas
    mutable long long compDist   = 0;  // # distancias
    mutable long long pageReads  = 0;  // Páginas de 4KB leídas en queries
    mutable long long pageWrites = 0;  // Páginas de 4KB escritas en build
    mutable long long queryTime  = 0;  // tiempo acumulado

    // Archivos de disco
    std::string indexPath;
    std::string nodePath;
    mutable FILE* nodeFp = nullptr;    // solo se usa en restore + queries

public:
    LC_Disk(const ObjectDB* db_, int pageBytes_ = 4096)
        : db(db_), n(db_->size()), pageBytes(pageBytes_)
    {
        if (pageBytes <= 0) pageBytes = 4096;

        // Capacidad del cluster: cuántos IDs caben en una "página lógica" del índice
        int approxObjBytes = sizeof(int32_t);   // guardamos solo IDs
        bucketSize = std::max(1, pageBytes / approxObjBytes);

        pagesPerCluster = std::max(1, pageBytes / 4096);

        std::cerr << "[LC_Disk] n=" << n
                  << " pageBytes=" << pageBytes
                  << " bucketSize=" << bucketSize
                  << " pagesPerCluster=" << pagesPerCluster << "\n";
    }

    ~LC_Disk() {
        if (nodeFp) {
            std::fclose(nodeFp);
            nodeFp = nullptr;
        }
    }

    void clear_counters() const {
        compDist   = 0;
        pageReads  = 0;
        queryTime  = 0;
        // pageWrites se mantiene como métrica de build si quieres inspeccionarla
    }
    long long get_compDist()   const { return compDist;   }
    long long get_pageReads()  const { return pageReads;  }
    long long get_pageWrites() const { return pageWrites; }
    long long get_queryTime()  const { return queryTime;  }

    int get_num_clusters() const { return (int)clusters.size(); }
    int get_pageBytes()    const { return pageBytes; }
    int get_bucketSize()   const { return bucketSize; }

    void build(const std::string& basePath) {
        std::cerr << "[LC_Disk] Build start...\n";
        clusters.clear();
        compDist   = 0;
        pageReads  = 0;
        pageWrites = 0;
        queryTime  = 0;

        indexPath = basePath + ".lc_index";
        nodePath  = basePath + ".lc_node";

        // Abrimos salida binaria
        std::ofstream idxOut(indexPath, std::ios::binary | std::ios::trunc);
        if (!idxOut.is_open()) {
            throw std::runtime_error("[LC_Disk] No se pudo crear " + indexPath);
        }
        std::ofstream nodeOut(nodePath, std::ios::binary | std::ios::trunc);
        if (!nodeOut.is_open()) {
            throw std::runtime_error("[LC_Disk] No se pudo crear " + nodePath);
        }

        struct ObjInfo {
            int  id;
            double tdist; // suma de distancias a centros previos
        };

        std::vector<ObjInfo> rem(n);
        for (int i = 0; i < n; ++i)
            rem[i] = {i, 0.0};

        int64_t currentOffset = 0;

        while (!rem.empty()) {
            // 1) centro = objeto con mayor tdist
            auto it = std::max_element(
                rem.begin(), rem.end(),
                [](const ObjInfo& a, const ObjInfo& b){
                    return a.tdist < b.tdist;
                }
            );
            int centerId = it->id;
            rem.erase(it);

            // 2) distancias a centro para todos los remanentes
            std::vector<std::pair<double,int>> dlist;
            dlist.reserve(rem.size());

            for (auto &o : rem) {
                double d = dist(centerId, o.id);
                o.tdist += d;
                dlist.emplace_back(d, o.id);
            }

            std::sort(dlist.begin(), dlist.end(),
                      [](auto &a, auto &b){ return a.first < b.first; });

            int K = std::min((int)dlist.size(), bucketSize);

            ClusterInfo c;
            c.centerId = centerId;
            c.radius   = (K > 0 ? dlist[K-1].first : 0.0);
            c.count    = K;
            c.offset   = currentOffset;

            clusters.push_back(c);

            // 3) escribir miembros del cluster en nodeOut
            for (int i = 0; i < K; ++i) {
                int32_t oid = dlist[i].second;
                nodeOut.write(reinterpret_cast<const char*>(&oid), sizeof(int32_t));
            }
            currentOffset += K;

            // 4) escribir cluster info
            ClusterInfoDisk cDisk;
            cDisk.centerId = c.centerId;
            cDisk.radius   = c.radius;
            cDisk.count    = c.count;
            cDisk.offset   = c.offset;
            idxOut.write(reinterpret_cast<const char*>(&cDisk), sizeof(ClusterInfoDisk));

            // 5) eliminar miembros usados de rem
            std::set<int> used;
            for (int i = 0; i < K; ++i)
                used.insert(dlist[i].second);

            std::vector<ObjInfo> newRem;
            newRem.reserve(rem.size());
            for (auto &o : rem)
                if (!used.count(o.id))
                    newRem.push_back(o);

            rem.swap(newRem);

            if (clusters.size() % 100 == 0) {
                std::cerr << "\r[LC_Disk] Clusters=" << clusters.size()
                          << " Remaining=" << rem.size() << std::flush;
            }
        }

        idxOut.close();
        nodeOut.close();

        // Estimación de páginas escritas: una página lógica por cluster
        pageWrites = (long long)clusters.size() * (long long)pagesPerCluster;

        std::cerr << "\n[LC_Disk] Build done. #clusters = " << clusters.size()
                  << "  (pageWrites≈" << pageWrites << " páginas de 4KB)\n";
    }

    void restore(const std::string& basePath) {
        indexPath = basePath + ".lc_index";
        nodePath  = basePath + ".lc_node";

        clusters.clear();

        // Leer todos los clusters a RAM
        std::ifstream idxIn(indexPath, std::ios::binary);
        if (!idxIn.is_open()) {
            throw std::runtime_error("[LC_Disk] No se pudo abrir " + indexPath);
        }

        ClusterInfoDisk cd;
        while (idxIn.read(reinterpret_cast<char*>(&cd), sizeof(ClusterInfoDisk))) {
            ClusterInfo c;
            c.centerId = cd.centerId;
            c.radius   = cd.radius;
            c.count    = cd.count;
            c.offset   = cd.offset;
            clusters.push_back(c);
        }
        idxIn.close();

        // Abrir archivo de nodos para lecturas
        if (nodeFp) {
            std::fclose(nodeFp);
            nodeFp = nullptr;
        }
        nodeFp = std::fopen(nodePath.c_str(), "rb");
        if (!nodeFp) {
            throw std::runtime_error("[LC_Disk] No se pudo abrir " + nodePath);
        }

        std::cerr << "[LC_Disk] restore: #clusters=" << clusters.size()
                  << "  nodeFile=" << nodePath << "\n";
    }

    void rangeSearch(int qId, double R, std::vector<int>& out) const {
        using clock = std::chrono::high_resolution_clock;
        auto t0 = clock::now();

        out.clear();
        if (!nodeFp) {
            throw std::runtime_error("[LC_Disk] rangeSearch: nodeFp==nullptr (¿faltó restore?)");
        }

        std::vector<int32_t> buffer;

        for (const auto& c : clusters) {
            double dqc = dist(qId, c.centerId);

            // poda: si la bola del cluster no intersecta B(q,R)
            if (dqc > c.radius + R)
                continue;

            // accedemos el cluster -> cuenta como pagesPerCluster páginas (paper)
            pageReads += pagesPerCluster;

            // centro
            if (dqc <= R)
                out.push_back(c.centerId);

            if (c.count <= 0)
                continue;

            // leer miembros desde disco
            buffer.resize(c.count);
            std::int64_t byteOffset = (std::int64_t)c.offset * (std::int64_t)sizeof(int32_t);

            if (std::fseek(nodeFp, byteOffset, SEEK_SET) != 0) {
                throw std::runtime_error("[LC_Disk] fseek falló en rangeSearch");
            }
            size_t nRead = std::fread(buffer.data(), sizeof(int32_t), c.count, nodeFp);
            if (nRead != (size_t)c.count) {
                throw std::runtime_error("[LC_Disk] fread incompleto en rangeSearch");
            }

            // chequear miembros
            for (int i = 0; i < c.count; ++i) {
                int id = buffer[i];
                if (dist(qId, id) <= R)
                    out.push_back(id);
            }
        }

        auto t1 = clock::now();
        queryTime += std::chrono::duration_cast<std::chrono::microseconds>(t1 - t0).count();
    }

    void knnSearch(int qId, int k, std::vector<std::pair<double,int>>& out) const {
        using clock = std::chrono::high_resolution_clock;
        auto t0 = clock::now();

        if (!nodeFp) {
            throw std::runtime_error("[LC_Disk] knnSearch: nodeFp==nullptr (¿faltó restore?)");
        }

        std::priority_queue<std::pair<double,int>> pq;
        std::vector<int32_t> buffer;

        for (const auto& c : clusters) {
            double dqc = dist(qId, c.centerId);

            double rk = pq.size() < (size_t)k ?
                        std::numeric_limits<double>::infinity() :
                        pq.top().first;

            // poda: si el cluster está completamente fuera de B(q, rk)
            if (pq.size() >= (size_t)k && dqc - c.radius >= rk)
                continue;

            // tocamos cluster -> sumamos pagesPerCluster páginas
            pageReads += pagesPerCluster;

            // centro
            pq.emplace(dqc, c.centerId);
            if ((int)pq.size() > k) pq.pop();

            if (c.count > 0) {
                buffer.resize(c.count);
                std::int64_t byteOffset = (std::int64_t)c.offset * (std::int64_t)sizeof(int32_t);

                if (std::fseek(nodeFp, byteOffset, SEEK_SET) != 0) {
                    throw std::runtime_error("[LC_Disk] fseek falló en knnSearch");
                }
                size_t nRead = std::fread(buffer.data(), sizeof(int32_t), c.count, nodeFp);
                if (nRead != (size_t)c.count) {
                    throw std::runtime_error("[LC_Disk] fread incompleto en knnSearch");
                }

                for (int i = 0; i < c.count; ++i) {
                    int id = buffer[i];
                    double d = dist(qId, id);
                    pq.emplace(d, id);
                    if ((int)pq.size() > k) pq.pop();
                }
            }
        }

        out.clear();
        while (!pq.empty()) {
            out.push_back(pq.top());
            pq.pop();
        }
        std::reverse(out.begin(), out.end());

        auto t1 = clock::now();
        queryTime += std::chrono::duration_cast<std::chrono::microseconds>(t1 - t0).count();
    }

private:
    // Wrapper de distancia que incrementa compDist
    double dist(int a, int b) const {
        compDist++;
        return db->distance(a, b);
    }
};

#endif // LC_DISK_HPP
