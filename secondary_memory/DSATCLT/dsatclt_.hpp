#ifndef SATCTL_HPP
#define SATCTL_HPP

#include <vector>
#include <string>
#include <fstream>
#include <chrono>
#include <utility>
#include <stdexcept>
#include <cstdint>
#include <cstring>

#include "obj.h"
#include "DSACLX.h"

// === Variables globales provistas por tu base DSACL ===
// (se DEFINEN en main.cpp; aquí solo las declaramos)
extern double numDistances;
extern double IOread;
extern double IOwrite;
extern int MaxArity;
extern int ClusterSize;
extern double CurrentTime;
extern int dimension;
extern double objNum;
extern int func;
extern int BlockSize;
extern DSACLfile indexFile;

// ============ Métricas agregadas ============
struct MetricResult {
    double avg_time_ms   = 0.0;  // tiempo promedio por query
    double avg_compdists = 0.0;  // distancias computadas promedio por query
    double avg_pages     = 0.0;  // páginas leídas promedio por query (IOread)
};

// ============ Controlador DSACLT ============
class SATCTL {
public:
    SATCTL(const std::string& dbFile,
           const std::string& idxFile,
           const std::string& queryFile,
           int block_size,
           int qcount = 100)
        : dbName_(dbFile),
          idxName_(idxFile),
          queryPath_(queryFile),
          blockSize_(block_size),
          qcount_(qcount)
    {
        // Leer encabezado del DB para conocer dimensión y función de distancia
        // (buildSecondaryMemory también lo hace, pero aquí lo usamos para cargar queries)
        int hdr_dim  = 0;
        int hdr_num  = 0;
        int hdr_func = 0;
        parseDBHeader_(dbName_, hdr_dim, hdr_num, hdr_func);
        fileDim_ = hdr_dim;

        // Cargar queries en memoria (solo los primeros qcount_)
        loadQueries_(queryPath_, fileDim_, qcount_, queries_);
        if (queries_.empty()) {
            throw std::runtime_error("[SATCTL] No se pudieron cargar queries del archivo.");
        }
    }

    // Construye el índice DSACLT con un valor de "centers" (MaxArity)
    // Devuelve: {build_time_seconds, build_compdists}
    std::pair<double, double> build(int centers) {
        // Configurar globals
        MaxArity   = centers;
        BlockSize  = blockSize_;
        CurrentTime = 0;

        // Reiniciar contadores
        numDistances = 0.0;
        IOread = IOwrite = 0.0;

        // Asegurar que el archivo de índice esté cerrado antes de recrear
        // (DSACLfile::create retorna si está abierto)
        if (indexFile.isOpened()) {
            indexFile.close();
        }

        auto t0 = std::chrono::high_resolution_clock::now();
        // Nota: buildSecondaryMemory vuelve a leer el header del DB y setea (dimension, objNum, func)
        //       y crea el índice en disco.
        buildSecondaryMemory(const_cast<char*>(idxName_.c_str()),
                             const_cast<char*>(dbName_.c_str()));
        auto t1 = std::chrono::high_resolution_clock::now();

        double secs = std::chrono::duration<double>(t1 - t0).count();
        double comp = numDistances;
        return {secs, comp};
    }

    // Evalúa KNN para una lista de k’s. Devuelve una métrica por cada k.
    std::vector<MetricResult> evalKnn(const std::vector<int>& ks) const {
        std::vector<MetricResult> out;
        out.reserve(ks.size());

        for (int k : ks) {
            // Reiniciar contadores para este k
            numDistances = 0.0;
            IOread = IOwrite = 0.0;

            auto t0 = std::chrono::high_resolution_clock::now();
            for (int i = 0; i < (int)queries_.size(); ++i) {
                // buildSecondaryMemory ya fijó 'dimension'; ahora cada Objvector usa ese 'dimension'
                Objvector q(queries_[i]);
                (void)knnSearch(q, k); // retorna radio; aquí nos importan tiempo, distancias, I/O
            }
            auto t1 = std::chrono::high_resolution_clock::now();

            MetricResult mr;
            double total_ms = std::chrono::duration<double, std::milli>(t1 - t0).count();
            mr.avg_time_ms   = total_ms / queries_.size();
            mr.avg_compdists = numDistances / queries_.size();
            mr.avg_pages     = IOread      / queries_.size();
            out.push_back(mr);
        }
        return out;
    }

    // Evalúa Range para una lista de radios. Devuelve una métrica por cada radio.
    // (Si no quieres range, simplemente no la invoques desde main.cpp)
    std::vector<MetricResult> evalRange(const std::vector<double>& radii) const {
        std::vector<MetricResult> out;
        out.reserve(radii.size());

        for (double r : radii) {
            numDistances = 0.0;
            IOread = IOwrite = 0.0;

            auto t0 = std::chrono::high_resolution_clock::now();
            for (int i = 0; i < (int)queries_.size(); ++i) {
                Objvector q(queries_[i]);
                (void)rangeSearch(q, r); // retorna #resultados; medimos tiempo, distancias, I/O
            }
            auto t1 = std::chrono::high_resolution_clock::now();

            MetricResult mr;
            double total_ms = std::chrono::duration<double, std::milli>(t1 - t0).count();
            mr.avg_time_ms   = total_ms / queries_.size();
            mr.avg_compdists = numDistances / queries_.size();
            mr.avg_pages     = IOread      / queries_.size();
            out.push_back(mr);
        }
        return out;
    }

    int queryCount() const { return (int)queries_.size(); }
    int queryDim()   const { return fileDim_; }

private:
    std::string dbName_;
    std::string idxName_;
    std::string queryPath_;
    int blockSize_ = 4096;
    int qcount_    = 100;

    int fileDim_   = 0;
    std::vector<std::vector<float>> queries_;

    static void parseDBHeader_(const std::string& db,
                               int& out_dim, int& out_num, int& out_func)
    {
        std::ifstream f(db);
        if (!f.is_open()) {
            throw std::runtime_error("[SATCTL] No se pudo abrir DB: " + db);
        }
        // Formato esperado: <dim> <num> <func> \n
        f >> out_dim >> out_num >> out_func;
        if (!f.good()) {
            throw std::runtime_error("[SATCTL] Header inválido en " + db);
        }
    }

    static void loadQueries_(const std::string& qfile, int dim, int qcount,
                             std::vector<std::vector<float>>& out)
    {
        out.clear();
        std::ifstream f(qfile);
        if (!f.is_open()) {
            throw std::runtime_error("[SATCTL] No se pudo abrir queries: " + qfile);
        }
        out.reserve(qcount);
        for (int qi = 0; qi < qcount; ++qi) {
            std::vector<float> v(dim, 0.0f);
            for (int j = 0; j < dim; ++j) {
                if (!(f >> v[j])) {
                    // no hay suficientes queries en archivo
                    out.shrink_to_fit();
                    return;
                }
            }
            out.push_back(std::move(v));
        }
    }
};

#endif // SATCTL_HPP
