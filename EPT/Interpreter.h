//
// Created by YANGHANYU on 8/21/15.
//

#ifndef EP_TABLE_INTERPRETER_H
#define EP_TABLE_INTERPRETER_H

#include "main.h"
#include "Objvector.h"

using namespace std;

// to parse the raw data into Objvector
class Interpreter
{
	
public:
	Interpreter();
	~Interpreter()
	{
		if (dataList != NULL)
		{
			delete[] dataList;
		}
	}

	void parseRawData(const char * path); // turn raw data into structural data
	void split(string str, string pattern); // split a line of raw data by pattern

	int dim;
	int num;
	int func;
	Objvector* dataList; // list of vector data

	double(*df) (float* p1, float* p2, int dim);
};

#endif //EP_TABLE_INTERPRETER_H
