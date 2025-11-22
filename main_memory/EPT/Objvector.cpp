#include "Objvector.h"

Objvector::Objvector(const Objvector & obj)
    : value(obj.value)
{
}

Objvector::Objvector(const vector<float>& v)
    : value(v)
{
}

Objvector & Objvector::operator=(const Objvector & obj)
{
    if (this == &obj) return *this;
    value = obj.value;
    return *this;
}

vector<float> & Objvector::getValue()
{
    return value;
}

const vector<float> & Objvector::getValue() const
{
    return value;
}

void Objvector::setValue(const vector<float>& v)
{
    value = v;
}
