#ifndef EP_TABLE_INTERPRETER_H
#define EP_TABLE_INTERPRETER_H

#include "main.h"
#include "Objvector.h"

#include <string>
#include <vector>

using namespace std;

// to parse the raw data into Objvector
class Interpreter
{
public:
    Interpreter();
    ~Interpreter();

    // turn raw data into structural data
    void parseRawData(const char * path);

    // split a line by pattern (returns vector)
    static vector<string> split(const string& str, const string& pattern);

    int dim;
    int num;
    int func;
    Objvector* dataList; // list of vector data

    double(*df) (float* p1, float* p2, int dim);
};

#endif //EP_TABLE_INTERPRETER_H
