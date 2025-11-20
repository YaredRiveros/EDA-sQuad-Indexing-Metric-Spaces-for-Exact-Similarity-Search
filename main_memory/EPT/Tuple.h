#ifndef EP_TABLE_TUPLE_H
#define EP_TABLE_TUPLE_H

#include "main.h"

class Tuple {
public:
    int pivId;
    int objId;
    double distance; // distance between pivot and specific data
public:
    Tuple();
    int getPivId() const;
    int getObjId() const;
    double getDistance() const;
    void setPivId(int p);
    void setObjId(int o);
    void setDistance(double d);
};

#endif
