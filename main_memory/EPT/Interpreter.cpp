//
// Created by YANGHANYU on 8/21/15.
//

#include "Interpreter.h"
#include <iostream>

extern float* DB;
extern float* nobj;

Interpreter::Interpreter()
{

	dataList = NULL;
}


/* L2 distance */

static double L2D(float* p1, float* p2, int dim)
{
	double dist = 0;
	for (int i = 0; i < dim; i++) dist += pow(p1[i] - p2[i], 2); // L2
	dist = sqrt(dist);
	return dist;
}

static double L1D(float* p1, float* p2, int dim)
{
	register int i;
	double tot = 0, dif;
	for (i = 0; i < dim; i++)
	{
		dif = (p1[i] - p2[i]);
		if (dif < 0) dif = -dif;
		tot += dif;
	}
	return tot;
}

static double L5D(float* p1, float* p2, int dim)
{
	register int i;
	double tot = 0, dif;
	for (i = 0; i < dim; i++)
	{
		dif = (p1[i] - p2[i]);
		if (dif < 0) dif = -dif;
		tot += pow(dif, 5);
	}
	return pow(tot, 0.2);
}

static double LiD(float* p1, float* p2, int dim)
{
	register int i;
	double max = 0, dif;
	for (i = 0; i < dim; i++)
	{
		dif = (p1[i] - p2[i]);
		if (dif < 0) dif = -dif;
		if (dif > max) max = dif;
	}
	return max;
}

void Interpreter::parseRawData(const char * path)
{
	ifstream fin(path, ios::in);
	fin >> dim >> num >> func;
	/*FILE *f = fopen(path, "r");
	fscanf(f, "%d %d %d\n", &dim, &num, &func);*/
	if (func == 1) df = L1D;
	else if (func == 2) df = L2D;
	else if (func == 5) df = L5D;
	else df = LiD;
	DB = new float[num * dim];
	nobj = new float[dim];
	cout << "read from file." << endl;
	
	float d;
	int pos = 0;
	for (int i = 0; i< num; i++) {
		for (int j = 0; j < dim; ++j) {
			fin >> DB[i * dim + j];
		}		
		//fscanf(f, "\n");
	}
	
	//fclose(f);
	fin.close();
	cout << "finish reading." << endl;
}

// string split
void Interpreter::split(string str, string pattern)
{
	string::size_type pos;
	string s;
	str += pattern;   // extend string for splitting
	unsigned long size = str.size();


	for (unsigned long i = 0; i < size; i++)
	{
		pos = str.find(pattern, i);
		if (pos < size)
		{
			s = str.substr(i, pos - i);
			i = pos + pattern.size() - 1;
		}
	}
}