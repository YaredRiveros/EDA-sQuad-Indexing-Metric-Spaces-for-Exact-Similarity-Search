#pragma once

#ifndef EP_TABLE_OBJVECTOR_H
#define EP_TABLE_OBJVECTOR_H

#include "main.h"

using namespace std;

class Objvector
{
public:
	vector<float> value;
	Objvector() {}
	Objvector(const Objvector &obj);
	Objvector(vector<float> v);
	Objvector & operator = (const Objvector & obj);
	vector<float> & getValue();

	void setValue(vector<float> v);

	~Objvector() {}
};

#endif