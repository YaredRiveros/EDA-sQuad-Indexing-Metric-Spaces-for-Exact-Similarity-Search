#ifndef OBJINCLUDED
#define OBJINCLUDED

extern int func;
extern int dimension;
#include <iostream>
#include <fstream>
#include <vector>
using namespace std;
class Objvector
{
public:
	float *value = NULL;
	int dim;
	Objvector();
	Objvector(const Objvector &obj);
	Objvector(vector<float> v);

	Objvector & operator = (const Objvector & obj);
	float* getValue();
	void setValue(vector<float> v);
	
	double distance(Objvector obj);
	void freeResources();
	~Objvector();
};
/* loads a DB given its name, and returns its size. if there is
   another one already open, it will be closed first */
int openDB (char *name);

/* frees the currently open DB, if any */
void closeDB (void);

void printobj(Objvector obj);
void printDBelements();
#endif
