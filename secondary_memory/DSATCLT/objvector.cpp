
#include "obj.h"

#include <math.h>
#include <sys/types.h>
#include <sys/stat.h>

extern double numDistances;

Objvector::Objvector() {
	value = NULL;
}

Objvector::Objvector(const Objvector & obj) {
	dim = obj.dim;		
	if (value == NULL) {		
		value = new float[dim];
	}
	for (int i = 0; i < dim; i++) {
		value[i] = obj.value[i];
	}
}

Objvector::Objvector(vector<float> v) {	
	dim = v.size();
	if (value == NULL) {
		value = new float[dim];
	}
	for (int i = 0; i < dim; i++) {
		value[i] = v[i];
	}
}

Objvector & Objvector::operator=(const Objvector & obj) {
	dim = obj.dim;	
	if (value == NULL) {		
		value = new float[dim];
	}
	for (int i = 0; i < dim; i++) {
		value[i] = obj.value[i];
	}
	return *this;	
}

float* Objvector::getValue()
{
	return value;
}


void Objvector::setValue(vector<float> v) {
	dim = v.size();
	if (value == NULL) {		
		value = new float[dim];
	}
	for (int i = 0; i < dim; i++) {
		value[i] = v[i];
	}
}

double Objvector::distance(Objvector obj) {
	numDistances++;
	double tot = 0, dif;
	if (func == 1) {		
		for (int i = 0; i<dimension; i++){
			dif = (value[i] - obj.value[i]);
			if (dif < 0) dif = -dif;
			tot += dif;
		}		
	}
	else if (func == 2) {
		for (int i = 0; i<dimension; i++){		
			tot += pow(value[i] - obj.value[i], 2);
		}
		tot=sqrt(tot);
	}
	else {
		double max = 0;
		for (int i = 0; i<dimension; i++)
		{
			dif = (value[i] - obj.value[i]);
			if (dif < 0) dif = -dif;
			if (dif > max) max = dif;
		}
		tot = max;
	}
	return tot;
}

void Objvector::freeResources() {
	if (value != NULL) {		
		delete[] value;
		value = NULL;
	}
}

Objvector::~Objvector() {
	freeResources();
}

typedef struct sEuclDB { 
	double *nums;  /* coords all together */
    int nnums;	  /* number of vectors (with space for one more) */
    int coords;  /* coordinates */
    double (*df) (double *p1, double *p2, int k); /* distance to use */
	double *nobj;
} EuclDB;

static int never = 1;
static EuclDB DB;

#define db(p) (DB.nums + DB.coords*(int)p)

/* L2 distance */
static double L2D (double *p1, double *p2, int k) { 
	int i;
    double tot = 0,dif;
    for (i=0;i<k;i++) 
	{ 
		dif = (p1[i]-p2[i]);
		tot += dif*dif;
	}
    return sqrt(tot);
}

static double L1D (double *p1, double *p2, int k) {
	int i;
    double tot = 0,dif;
    for (i=0;i<k;i++) { 
		dif = (p1[i]-p2[i]);
		if (dif < 0) dif = -dif;
		tot += dif;
	}
    return tot;
}

static double LiD (double *p1, double *p2, int k) { 
	int i;
    double max = 0,dif;
    for (i=0;i<k;i++) { 
		dif = (p1[i]-p2[i]);
		if (dif < 0) dif = -dif;
		if (dif > max) max = dif;
	}
    return max;
}

int openDB(char *name) {

	FILE *f = fopen(name, "r");
    if (!f) return 0;

	int dim, num, func;
	if (fscanf(f, "%d %d %d\n", &dim, &num, &func) != 3) { fclose(f); return 0; }
	if (func == 1) DB.df = L1D;
	else if (func == 2) DB.df = L2D;
	//else if (func == 5) DB.df = L5D;
	else DB.df = LiD;
	DB.coords = dim;
	DB.nnums = num;
	DB.nums = (double *)malloc((DB.nnums + 1) * sizeof(double)* DB.coords);
	DB.nobj = (double *)malloc(DB.coords * sizeof(double));

	for (int i = 0; i < DB.nnums; ++i) {
		for (int j = 0; j < DB.coords; ++j) {
			if (fscanf(f, "%lf", &DB.nums[i*DB.coords + j]) != 1) {
                fclose(f); return 0;
            }
		}
		// (void)fscanf(f, "\n");

	}
	fclose(f);
	return DB.nnums;
}

void closeDB (void) {
	if (never) { DB.nums = NULL; never=0;}
    if (DB.nums == NULL) return;
    free (DB.nums);
    DB.nums = NULL;
}

void printDBelements() {
	for (int i = 0; i < DB.nnums; ++i) {
		for (int j = 0; j < DB.coords; ++j) {
			//fscanf(f, "%lf", &DB.nums[i*DB.coords + j]);
			cout << DB.nums[i*DB.coords + j] << " ";
		}
		cout << endl;
	}
}

void printobj(Objvector obj) {
	for (int i = 0; i < dimension; i++) {
		cout << obj.value[i] << " ";
	}
}