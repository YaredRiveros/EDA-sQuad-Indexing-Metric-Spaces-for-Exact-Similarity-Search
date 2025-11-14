//
// Created by YANGHANYU on 8/21/15.
//

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
	int getPivId();
	int getObjId();
	double getDistance();
	void setPivId(int p);
	void setObjId(int o);
	void setDistance(double d);
};

#endif
