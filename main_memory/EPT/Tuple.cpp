#include "Tuple.h"

Tuple::Tuple() : pivId(-1), objId(-1), distance(0.0) {}

int Tuple::getPivId() const {
    return pivId;
}

int Tuple::getObjId() const {
    return objId;
}

double Tuple::getDistance() const {
    return distance;
}

void Tuple::setPivId(int p) {
    pivId = p;
}

void Tuple::setObjId(int o) {
    objId = o;
}

void Tuple::setDistance(double d) {
    distance = d;
}
