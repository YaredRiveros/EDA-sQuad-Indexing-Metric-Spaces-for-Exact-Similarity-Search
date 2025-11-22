#include "Interpreter.h"
#include <fstream>
#include <iostream>
#include <cmath>
#include <stdexcept>

extern float* DB;   // defined in main.cpp
extern float* nobj; // defined in main.cpp

Interpreter::Interpreter() : dim(0), num(0), func(0), dataList(nullptr), df(nullptr) {}

Interpreter::~Interpreter()
{
    if (dataList != nullptr) {
        delete[] dataList;
        dataList = nullptr;
    }
}

// L2 distance (euclidean)
static double L2D(float* p1, float* p2, int dim)
{
    double dist = 0.0;
    for (int i = 0; i < dim; ++i) {
        double d = static_cast<double>(p1[i]) - static_cast<double>(p2[i]);
        dist += d * d;
    }
    return std::sqrt(dist);
}

static double L1D(float* p1, float* p2, int dim)
{
    double tot = 0.0;
    for (int i = 0; i < dim; ++i) {
        double dif = static_cast<double>(p1[i]) - static_cast<double>(p2[i]);
        tot += std::fabs(dif);
    }
    return tot;
}

static double L5D(float* p1, float* p2, int dim)
{
    double tot = 0.0;
    for (int i = 0; i < dim; ++i) {
        double dif = std::fabs(static_cast<double>(p1[i]) - static_cast<double>(p2[i]));
        tot += std::pow(dif, 5.0);
    }
    return std::pow(tot, 0.2);
}

static double LiD(float* p1, float* p2, int dim)
{
    double maxv = 0.0;
    for (int i = 0; i < dim; ++i) {
        double dif = std::fabs(static_cast<double>(p1[i]) - static_cast<double>(p2[i]));
        if (dif > maxv) maxv = dif;
    }
    return maxv;
}

void Interpreter::parseRawData(const char * path)
{
    std::ifstream fin(path, std::ios::in);
    if (!fin.is_open()) {
        throw std::runtime_error(std::string("Interpreter: cannot open ") + path);
    }

    fin >> dim >> num >> func;
    if (!fin.good()) {
        throw std::runtime_error("Interpreter: failed to read header (dim num func)");
    }

    if (func == 1) df = L1D;
    else if (func == 2) df = L2D;
    else if (func == 5) df = L5D;
    else df = LiD;

    // allocate global DB and nobj (DB and nobj are extern in interpreter.cpp and defined in main)
    DB = new float[static_cast<size_t>(num) * static_cast<size_t>(dim)];
    nobj = new float[static_cast<size_t>(dim)];

    // read data values
    for (int i = 0; i < num; ++i) {
        for (int j = 0; j < dim; ++j) {
            float v;
            fin >> v;
            if (!fin.good()) {
                // if read failed, clean and throw
                delete[] DB;
                DB = nullptr;
                delete[] nobj;
                nobj = nullptr;
                throw std::runtime_error("Interpreter: error while reading data values");
            }
            DB[static_cast<size_t>(i) * dim + j] = v;
        }
    }

    fin.close();
    std::cout << "Interpreter: read " << num << " objects of dim " << dim << std::endl;
}

vector<string> Interpreter::split(const string& str, const string& pattern)
{
    vector<string> result;
    if (pattern.empty()) {
        result.push_back(str);
        return result;
    }
    size_t pos = 0;
    size_t next;
    while ((next = str.find(pattern, pos)) != string::npos) {
        result.push_back(str.substr(pos, next - pos));
        pos = next + pattern.size();
    }
    // add remainder
    if (pos <= str.size()) result.push_back(str.substr(pos));
    return result;
}
