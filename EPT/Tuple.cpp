//
// Created by YANGHANYU on 8/21/15.
//

#include "Tuple.h"


Tuple::Tuple()
{
	distance = 0;
}


int Tuple::getPivId() {
	return pivId;
}

int Tuple::getObjId()
{
	return objId;
}

double Tuple::getDistance()
{
	return distance;
}

void Tuple::setPivId(int p)
{
	pivId = p;
}

void Tuple::setObjId(int o)
{
	objId = o;
}

void Tuple::setDistance(double d)
{
	distance = d;
}
