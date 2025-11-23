#pragma once
#ifndef EP_TABLE_OBJVECTOR_H
#define EP_TABLE_OBJVECTOR_H

#include "main.h"
#include <vector>

using namespace std;

class Objvector
{
public:
    vector<float> value;
    Objvector() {}
    Objvector(const Objvector &obj);
    explicit Objvector(const vector<float>& v);
    Objvector & operator = (const Objvector & obj);
    vector<float> & getValue();
    const vector<float> & getValue() const;
    void setValue(const vector<float>& v);
    ~Objvector() {}
};

#endif
