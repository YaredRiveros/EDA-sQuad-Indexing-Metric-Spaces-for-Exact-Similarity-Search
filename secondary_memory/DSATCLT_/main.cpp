#include <iostream>
#include <string>
#include <vector>
#include <sstream>
#include <algorithm>
#include <iomanip>

#include "DSATCTL.hpp"

// ====== Definiciones de las variables globales (declaradas como extern en headers DSACL) ======
double numDistances = 0.0;
double IOread = 0.0;
double IOwrite = 0.0;
int MaxArity = 2;
int ClusterSize = 2;
double CurrentTime = 0.0;
int dimension = 0;
double objNum = 0.0;
int func = 2;
int BlockSize = 4096;

extern DSACLfile indexFile; // definido en la lib/tu código DSACL

// ====== Helpers de parsing ======
static std::vector<int> parseCSVInt(const std::string& s, const std::vector<int>& def) {
    if (s.empty()) return def;
    std::vector<int> out;
    std::stringstream ss(s);
    std::string tok;
    while (std::getline(ss, tok, ',')) {
        if (!tok.empty()) out.push_back(std::stoi(tok));
    }
    return out.empty() ? def : out;
}

static std::vector<double> parseCSVDouble(const std::string& s, const std::vector<double>& def) {
    if (s.empty()) return def;
    std::vector<double> out;
    std::stringstream ss(s);
    std::string tok;
    while (std::getline(ss, tok, ',')) {
        if (!tok.empty()) out.push_back(std::stod(tok));
    }
    return out.empty() ? def : out;
}

static void printUsage(const char* prog) {
    std::cerr
        << "Usage:\n  " << prog
        << " <db_file> <index_file> <query_file> <block_size>"
        << " [--centers 3,5,10,15,20]"
        << " [--klist 1,5,10,20,50,100]"
        << " [--qcount 100]"
        << " [--rlist <r1,r2,...>]   # opcional (range)\n\n"
        << "Notas:\n"
        << " - <db_file> con header: <dim> <num> <func>\n"
        << " - <block_size> en bytes (p.ej. 4096 o 40960)\n"
        << " - Por default se replican los centros {3,5,10,15,20} y klist {1,5,10,20,50,100}\n";
}

int main(int argc, char** argv) {
    if (argc < 5) {
        printUsage(argv[0]);
        return 1;
    }

    std::string dbFile   = argv[1];
    std::string idxFile  = argv[2];
    std::string qryFile  = argv[3];
    int blockSize        = std::stoi(argv[4]);

    // Defaults orientados a la réplica de Chen 2022
    std::vector<int> centers = {3,5,10,15,20};
    std::vector<int> klist   = {1,5,10,20,50,100};
    std::vector<double> rlist; // si no se provee, no se ejecuta range
    int qcount = 100;

    // Parse flags simples
    for (int i = 5; i < argc; ++i) {
        std::string flag = argv[i];
        if (flag == "--centers" && i+1 < argc) {
            centers = parseCSVInt(argv[++i], centers);
        } else if (flag == "--klist" && i+1 < argc) {
            klist = parseCSVInt(argv[++i], klist);
        } else if (flag == "--rlist" && i+1 < argc) {
            rlist = parseCSVDouble(argv[++i], {});
        } else if (flag == "--qcount" && i+1 < argc) {
            qcount = std::max(1, std::stoi(argv[++i]));
        } else {
            std::cerr << "Warning: flag ignorada o mal formada: " << flag << "\n";
        }
    }

    try {
        SATCTL ctl(dbFile, idxFile, qryFile, blockSize, qcount);

        std::cout << std::fixed << std::setprecision(4);
        for (int C : centers) {
            // Construcción
            auto [build_secs, build_comp] = ctl.build(C);
            std::cout << "=== DSACLT centers=" << C << " ===\n";
            std::cout << "build_time_s=" << build_secs
                      << " build_compdists=" << build_comp << "\n";

            // kNN
            auto knnMetrics = ctl.evalKnn(klist);
            for (size_t i = 0; i < klist.size(); ++i) {
                std::cout << "knn,k=" << klist[i]
                          << ",avg_time_ms=" << knnMetrics[i].avg_time_ms
                          << ",avg_compdists=" << knnMetrics[i].avg_compdists
                          << "\n";
            }

            // Range (opcional; solo si el usuario pasó --rlist)
            if (!rlist.empty()) {
                auto rngMetrics = ctl.evalRange(rlist);
                for (size_t i = 0; i < rlist.size(); ++i) {
                    std::cout << "range,r=" << rlist[i]
                              << ",avg_time_ms=" << rngMetrics[i].avg_time_ms
                              << ",avg_compdists=" << rngMetrics[i].avg_compdists
                              << "\n";
                }
            }

            std::cout << "\n";
        }
    } catch (const std::exception& ex) {
        std::cerr << "[ERROR] " << ex.what() << "\n";
        return 2;
    }

    return 0;
}
