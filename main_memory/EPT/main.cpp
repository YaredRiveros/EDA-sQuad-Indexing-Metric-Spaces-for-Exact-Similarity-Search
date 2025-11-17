//
// Created by YANGHANYU on 8/21/15.
//

#include "main.h"
#include "Objvector.h"
#include "Interpreter.h"
#include "Tuple.h"
#include "Cache.h"
#define MAX(x, y) ((x>y)? (x): (y))
#include <iostream>
#include <cfloat>
#include <cstring>

using namespace std;

// ============================================================================
// IMPLEMENTACIÓN DE EPT* (Extreme Pivot Table con Pivot Selection Algorithm)
// ============================================================================
// CONFIRMACIÓN: Esta es una implementación de EPT* (no EPT básico) debido a:
// 1. Implementa MaxPrunning para seleccionar l_c=40 pivotes candidatos
// 2. Implementa PivotSelect (PSA) que selecciona los mejores l pivotes para cada objeto
// 3. Cada objeto tiene pivotes diferentes (característica clave de EPT vs LAESA)
//
// Paper: "EPT* is equipped with a new pivot selection algorithm (PSA)"
// Paper: "EPT selects different pivots for different objects in order to achieve better search performance"
// Paper: "EPT* achieves better similarity search performance than EPT contributed by higher quality pivots selected by PSA"
//
// COMPLEJIDADES:
// - Construcción: O(n*l*l_c*n_s) donde n=num_objetos, l=LGroup, l_c=num_cand=40, n_s=sampleSize
// - Almacenamiento: O(n*s + n*l + l_c*s)
// - Paper: "The pivot selection is costly, and thus, the construction cost of EPT* is very high"
//
// ESTRUCTURA DE DATOS:
// - G: Tabla principal que almacena para cada objeto sus l pivotes y distancias pre-calculadas
// - cand: Array de l_c=40 pivotes candidatos globales
// - samples_objs: Conjunto de muestra de tamaño n_s para selección de candidatos
// ============================================================================

// parse raw data
Interpreter* pi;
// each line pivId sequence
int **pivotSeq;
// pivId set
set<int> pivotSet;
// Paper: "EPT consists of a set of pivot groups"
// Tuple 2-dim array para almacenar grupos de pivotes
Tuple ** gb;
float* DB;
// Paper: "For each object, we will select the best pivot among each pivot group"
// G almacena los l pivotes seleccionados para cada objeto
Tuple* G;
// random sequence generation array
int * sequence;

double compDists = 0;

// Paper: "EPT has l groups while each group contains g pivots"
// LGroup representa 'l' (número de pivotes seleccionados por objeto)
int LGroup;
float* nobj;
#ifdef RESULT
ofstream ret_range("ret_range.txt", ios::out);
#endif
double  recyc_times;
// calculate L2 distance between two data
double calculateDistance(int v1, int v2)
{
	compDists++;
	return pi->df(DB + pi->dim * v1, DB + pi->dim * v2, pi->dim);
}

double calculateDistance(Objvector o1, int v2)
{
	for (int d = 0; d < pi->dim; ++d) {
		nobj[d] = o1.getValue()[d];
	}
	compDists++;

	return pi->df(nobj, DB + pi->dim * v2, pi->dim);
}

double calculateDistance(float* o1, int v2)
{
	compDists++;
	return pi->df(o1, DB + pi->dim * v2, pi->dim);
}

// Paper: "An object o belongs to A(p_i) iff |d(o,p_i) - μ_p_i| ≥ α, where μ_p_i is the expected value of d(o,p_i)"
// Calcula μ_p_i (valor esperado de la distancia entre datos y un pivote dado)
// calculate average L2 distance between each data in datalist and given pivot
double calculateMiu(int pid)
{
	double miu = 0;
	for (int i = 0; i < pi->num; i++) {
		miu += calculateDistance(i, pid);
	}
	return miu / pi->num;
}


// provide non-repeated pivot selecting
void getNonRepeatedRandomNum(int * &rs, int nums)
{
	int end = pi->num;
	int num;
	for (int i = 0; i < nums; i++) {
		num = rand() % end;
		rs[i] = sequence[num];
		sequence[num] = sequence[end - 1]; // move the last value to the selected position
		end--; // decrease random range by one
	}
	sort(rs, rs + nums);
}

// calculate variance of X(distance between data and their pivot)
double calculateVx2(int group)
{
	double mean = 0, vx2 = 0;
	// calculate mean first
	for (int i = 0; i < pi->num; i++)
	{
		mean += gb[group][i].getDistance();
	}
	mean = mean / pi->num;
	for (int j = 0; j < pi->num; j++) {
		vx2 += pow((gb[group][j].getDistance() - mean), 2);
	}
	return vx2 / pi->num;
}

double calculateVy2()
{
	double mean = 0, vy2 = 0;
	int sampleSize = pi->num / 100;
	int * sample = (int *)malloc(sizeof(int) * sampleSize);
	// sampling
	getNonRepeatedRandomNum(sample, sampleSize);

	// calculate mean first
	int count = 0;
	for (int i = 0; i < sampleSize; i++) {
		for (int j = i + 1; j < sampleSize; j++) {
			mean += calculateDistance(sample[i], sample[j]);
			++count;
		}

	}
	mean = mean / count;
	for (int i = 0; i < sampleSize; i++) {
		for (int j = i + 1; j < sampleSize; j++) {
			vy2 += pow((calculateDistance(sample[i], sample[j]) - mean), 2);
		}
	}
	free(sample);
	return vy2 / count;
}

double calculateR2()
{
	double radius = 100;
	return radius;
}


// Algoritmo 1: EPT básico - Selección simple de pivotes
// Paper: "EPT tries to maximize α" donde |d(o,p_i) - μ_p_i| ≥ α
// Este algoritmo selecciona pivotes aleatoriamente y asigna objetos al pivote que maximiza |d(o,p_i) - μ_p_i|
void algorithm1(int group)
{

	double dis;
	double miu;

	cout << "algorithm1 start." << endl;

	// randomly select one pivot
	// add to pivot set
	pivotSet.insert(pivotSeq[group][0]);

	// initialize g array
	for (int i = 0; i < pi->num; i++) {
		Tuple t;
		t.setPivId(pivotSeq[group][0]);
		t.setObjId(i);
		t.setDistance(calculateDistance(i, pivotSeq[group][0]));
		gb[group][i] = t;
	}

	// Paper: "Each group G contains g pivots p_i (1 ≤ i ≤ g)"
	for (int i = 1; i < MPivots; i++) {
		// randomly select one pivot
		// add to pivot set
		pivotSet.insert(pivotSeq[group][i]);

		// calculate miu value
		miu = calculateMiu(pivotSeq[group][i]);

		// Paper: "An object o belongs to A(p_i) iff |d(o,p_i) - μ_p_i| ≥ α"
		// Seleccionar el pivote que maximiza |d(o,p_i) - μ_p_i| para cada objeto
		for (int j = 0; j < pi->num; j++) {
			// update g array
			dis = calculateDistance(j, pivotSeq[group][i]);
			if (fabs(dis - miu) > fabs(gb[group][j].getDistance() - miu)) {
				Tuple t;
				t.setPivId(pivotSeq[group][i]);
				t.setObjId(j);
				t.setDistance(dis);
				gb[group][j] = t;
			}
		}
	}

	cout << "algorithm1 end." << endl;
}

// Algoritmo 2: EPT con optimización de costo
// Versión mejorada que selecciona pivotes hasta minimizar una función de costo
// Usa varianzas para estimar la calidad de los pivotes seleccionados
void algorithm2(int group)
{
	
	double dis;
	double miu;
	double cost, prev;
	double vx2 = 0, vy2 = 0, r2 = 0;
	int m;
	

	cout << "algorithm2 start." << endl;

	// estimate vy^2 and r^2
	vy2 = calculateVy2();
	r2 = calculateR2();

	// randomly select one pivot
	// add to pivot set
	pivotSet.insert(pivotSeq[group][0]);

	// initialize g array
	for (int i = 0; i < pi->num; i++) {
		Tuple t;
		t.setPivId(pivotSeq[group][0]);
		t.setObjId(i);
		t.setDistance(calculateDistance(i, pivotSeq[group][0]));
		gb[group][i] = t;
	}
	m = 1;

	vx2 = calculateVx2(group); // calculate variance of X

	cost = m*LGroup + pi->num*pow((1 - (vx2 + vy2) / r2), LGroup); // calculate cost

	while (true) {
		// randomly select one pivot
		pivotSet.insert(pivotSeq[group][m]); // add to pivot set

											 // calculate miu value
		miu = calculateMiu(pivotSeq[group][m]);

		for (int j = 0; j < pi->num; j++) {
			// update g array
			dis = calculateDistance(j, pivotSeq[group][m]);
			if (fabs(dis - miu) > fabs(gb[group][j].getDistance() - miu)) {
				Tuple t;
				t.setPivId(pivotSeq[group][m]);
				t.setDistance(dis);
				gb[group][j] = t;
			}
		}
		m += 1; // increment m by one
		vx2 = calculateVx2(group); // update variance of X
		prev = cost; // record old cost for comparison
		cost = m*LGroup + pi->num*pow((1 - (vx2 + vy2) / r2), LGroup); // update cost
		if (cost >= prev) {
			break;
		}
	}

	cout << "algorithm2 end." << endl;
}

#define MAXREAL 1e20
// Paper: "The construction cost of EPT* is O(n*l*l_c*n_s), where l_c is the number of candidate pivots"
// num_cand representa l_c (número de pivotes candidatos)
#define num_cand 40
int * cand;
bool *ispivot;
double** O_P_matrix;
// Paper: "n_s denotes the cardinality of the sample set"
vector<int> samples_objs;

// ============================================================================
// ALGORITMO 3: EPT* - Pivot Selection Algorithm (PSA)
// Paper: "EPT* is equipped with a new pivot selection algorithm (PSA)"
// Paper: "To get high quality pivots, the construction cost of EPT* is O(n*l*l_c*n_s)"
// ============================================================================

//find num_cand pivots in O using maxpruning strategy
// Selecciona l_c pivotes candidatos usando estrategia de máxima dispersión
double** MaxPrunning(int num)
{
	cand = new int[num_cand];
	bool * indicator = new bool[num];
	for (int i = 0;i<num;i++)
		indicator[i] = true;
	int * idset = new int[num_cand];

	double d = 0.0;
	double t;
	int choose = 0;
	
	// Matriz de distancias entre objetos de muestra y candidatos
	double** distmatrix = new double*[num];
	for (int i = 0;i<num;i++)
	{
		distmatrix[i] = new double[num_cand];
		for (int j = 0;j<num_cand;j++)
			distmatrix[i][j] = 0;
	}


	// Selección del primer pivote candidato: el más lejano al primer objeto
	// Estrategia similar a GNAT pero aplicada a la selección de candidatos
	if (num_cand> 0)
	{

		for (int i = 1;i<num;i++)
		{
			t = calculateDistance(samples_objs[i], samples_objs[0]);
			if (t>d)
			{
				d = t;
				choose = i;
			}
		}

		idset[0] = choose;
		cand[0] = choose;
		indicator[choose] = false;
		
	}

	
	// Selección del segundo pivote candidato
	if (num_cand>1)
	{
		d = 0;
		for (int i = 0;i<num;i++)
		{
			if (indicator[i])
			{
				distmatrix[i][0] = calculateDistance(samples_objs[cand[0]], samples_objs[i]);
				if (distmatrix[i][0]>d)
				{
					d = distmatrix[i][0];
					choose = i;
				}
			}
		}

		idset[1] = choose;
		cand[1] = choose;
		indicator[choose] = false;
		
	}

	
	// Selección iterativa de los l_c pivotes candidatos restantes
	// Estrategia: minimizar la variación con respecto a una distancia de referencia (edge)
	// Paper: "To get high quality pivots" - usa estrategia sofisticada de selección
	double edge = d;
	d = MAXREAL;
	for (int i = 2;i<num_cand;i++)
	{
		d = MAXREAL;
		for (int j = 0;j<num;j++)
		{
			if (indicator[j])
			{
				t = 0;
				for (int k = 0;k<i - 1;k++)
				{
					t += fabs(edge - distmatrix[j][k]);
				}
				distmatrix[j][i - 1] = calculateDistance(samples_objs[j], samples_objs[cand[i - 1]]);
				t += fabs(edge - distmatrix[j][i - 1]);
				if (t<d)
				{
					d = t;
					choose = j;
				}
			}
		}

		idset[i] = choose;
		
		indicator[choose] = false;
		cand[i] = choose;
	}


	

	for (int i = 0;i<num;i++)
	{
		if (indicator[i])
		{
			distmatrix[i][num_cand - 1] = calculateDistance(samples_objs[i], samples_objs[cand[num_cand - 1]]);
		}
	}

	for (int i = 0;i<num_cand;i++)
		for (int j = i + 1;j<num_cand;j++)
			distmatrix[idset[i]][j] = calculateDistance(samples_objs[idset[i]], samples_objs[cand[j]]);

	delete[] indicator;
	delete[] idset;

	return distmatrix;
}

// Pivot Selection Algorithm (PSA) para EPT*
// Paper: "EPT* is equipped with a new pivot selection algorithm (PSA)"
// Paper: "EPT selects different pivots for different objects"
// Selecciona los mejores l pivotes de los l_c candidatos para un objeto específico
// Maximiza la capacidad de poda usando estimaciones de distancias
int PivotSelect(int objId, int o_num, int q_num, int pivotNum)
{
	
	double ** Q_O_matrix = new double *[q_num];
	double ** Q_P_matrix = new double*[q_num];
	double ** esti = new double*[q_num];
	for (int i = 0;i<q_num;i++)
	{
		Q_O_matrix[i] = new double[o_num];
		Q_P_matrix[i] = new double[num_cand];
		esti[i] = new double[o_num];
	}
	bool* indicator = new bool[num_cand];
	for (int i = 0;i<num_cand;i++)
		indicator[i] = true;


	for (int i = 0;i<q_num;i++)
	{
		for (int j = 0;j<o_num;j++)
		{
			Q_O_matrix[i][j] = calculateDistance(objId, samples_objs[j]);
			esti[i][j] = 0;
		}
		for (int j = 0;j<num_cand;j++)
		{
			Q_P_matrix[i][j] = calculateDistance(objId, samples_objs[cand[j]]);
		}
	}

	double d = 0;
	double t = 0;
	int choose;
	
	// Paper: "For each object, we will select the best pivot among each pivot group"
	// Selecciona iterativamente los l mejores pivotes de los l_c candidatos
	// Criterio: maximizar la capacidad de estimación de distancias (poda efectiva)
	int i;
	for (i = 0;i<pivotNum; i++)
	{
		choose = -1;
		for (int j = 0;j<num_cand;j++)
		{
			if (indicator[j])
			{
				// Calcular la calidad del pivote candidato j
				// basado en su capacidad para estimar distancias usando desigualdad triangular
				t = 0;
				for (int m = 0;m < q_num;m++)
				{
					for (int n = 0;n <o_num;n++)
					{
						if (Q_O_matrix[m][n] != 0)
						{
							// Usar |d(q,p) - d(o,p)| como cota inferior de d(q,o)
							t += (MAX(fabs(Q_P_matrix[m][j] - O_P_matrix[n][j]), esti[m][n])) / Q_O_matrix[m][n];
						}
					}
				}
				t = t / (q_num*o_num);
				if (t>d)
				{
					choose = j;
					d = t;
				}
			}
		}
	
		if (choose == -1)
			break;
		indicator[choose] = false;
		if (!ispivot[choose])
			ispivot[choose] = true;
		// Paper: "it needs O(n*l) to store the pre-computed distances between objects and the best pivots"
		// Almacenar el pivote seleccionado y su distancia pre-calculada
		G[objId * pivotNum + i].setObjId(objId);
		G[objId * pivotNum + i].setPivId(choose);
		G[objId * pivotNum + i].setDistance(Q_P_matrix[0][choose]);
		for (int m = 0;m<q_num;m++)
			for (int n = 0;n<o_num;n++)
				esti[m][n] = MAX(fabs(Q_P_matrix[m][choose] - O_P_matrix[n][choose]), esti[m][n]);
		
	}

	if (i < pivotNum)
	{
		for(int j =0;j<num_cand;j++)
			if (indicator[j])
			{
				indicator[j] = false;
				
				G[objId * pivotNum + i].setObjId(objId);
				G[objId * pivotNum + i].setPivId(j);
				G[objId * pivotNum + i].setDistance(Q_P_matrix[0][j]);

				if (!ispivot[j])
					ispivot[j] = true;
				i++;
				if (i == pivotNum)
					break;
			}

	}

	for (i = 0;i<q_num;i++)
	{
		delete[] esti[i];
		delete[] Q_P_matrix[i];
		delete[] Q_O_matrix[i];
	}
	delete[] indicator;
	return pivotNum;
}

// Paper: "n_s denotes the cardinality of the sample set"
// Muestreo de datos para selección de pivotes candidatos
// Reduce el costo computacional usando un subconjunto representativo
void sample_data(int nums, int objs_num)
{
	set<int> samples;

	int id;
	while (samples.size() < nums)
	{
		id = rand() % objs_num;
		if (samples.find(id) == samples.end())
		{
			samples.insert(id);
		}
	}
	id = 0;
	for (set<int>::iterator it = samples.begin(); it != samples.end(); ++it)
	{
		samples_objs.push_back(id + rand() % 200);
		id += 200;
	}
}

bool readIndex(Tuple* t, char* fname);

// ============================================================================
// ALGORITMO 3: Construcción completa de EPT* usando PSA
// Paper: "EPT* is equipped with a new pivot selection algorithm (PSA)"
// Paper: "The construction cost of EPT* is O(n*l*l_c*n_s)"
// Paper: "EPT* achieves better similarity search performance than EPT contributed by higher quality pivots"
// ============================================================================
void algorithm3(char* fname)
{
	cout << "algorithm3 start." << endl;

	// Paper: "n_s denotes the cardinality of the sample set"
	int sampleSize = pi->num / 200;
	sample_data(sampleSize, pi->num);
	ispivot = new bool[num_cand];
	for (int i = 0; i < num_cand; i++)
		ispivot[i] = false;
	// Seleccionar l_c pivotes candidatos usando MaxPruning
	O_P_matrix = MaxPrunning(sampleSize);
	// Paper: "it needs O(n*l) to store the pre-computed distances"
	G = (Tuple*)malloc(sizeof(Tuple) * pi->num * LGroup);
	if (!readIndex(G, fname))
	{
		// Paper: "EPT selects different pivots for different objects"
		// Para cada objeto, seleccionar sus mejores l pivotes de los l_c candidatos
		for (int i = 0; i < pi->num; i++) {
			// initialize random pivot selecting sequence
			//g[i] = (Tuple*)malloc(sizeof(Tuple) * LGroup);

			PivotSelect(i, sampleSize, 1, LGroup);
			if (i % 10000 == 0)
			{
				cout << i << endl;
			}
		}
	}
	/*
	for (int i = 0; i < pi->num; i++) {
		// initialize random pivot selecting sequence
		g[i] = (Tuple*)malloc(sizeof(Tuple) * LGroup);
		
		PivotSelect(i, sampleSize, 1, LGroup);
		if (i % 10000 == 0)
		{
			cout << i << endl;
		}
	}
	*/
	for (int i = 0;i<sampleSize;i++)
		delete[] O_P_matrix[i];
	
	cout << "algorithm3 end." << endl;
}


//int EP_rangeQuery(Objvector q, double radius, int qj)
//{
//	vector<int> result;
//	double* pivotQueryDist = new double[num_cand];
//	for (int i = 0; i < num_cand; i++) {
//		if (ispivot[i])
//			pivotQueryDist[i] = calculateDistance(q, samples_objs[cand[i]]);
//	}
//	
//	double tempdist;
//	bool next;
//	for (int i = 0; i < pi->num; i++) {
//		next = false;
//		for (int j = 0; j < LGroup; j++) {
//			if (fabs(pivotQueryDist[g[i][j].getPivId()] - g[i][j].getDistance()) > radius) {
//				next = true;
//				break;
//			}
//		}
//		if (!next) {
//			tempdist = calculateDistance(q, i);
//			if (tempdist <= radius) {
//				result.push_back(i);
//			}
//		}
//	}
//
//	delete[] pivotQueryDist;
//	return result.size();
//}
//
//double EP_kNNQuery(Objvector q, int k)
//{
//	vector<SortEntry> result;
//	double* pivotQueryDist = new double[num_cand];
//	//memset(pivotQueryDist, 0, sizeof(double) * num_cand);
//	for (int i = 0; i < num_cand;i++) {
//		if (ispivot[i])
//		{
//			pivotQueryDist[i] = calculateDistance(q, samples_objs[cand[i]]);
//		}
//	}
//	
//	double kdist = numeric_limits<double>::max();
//	double tempdist;
//	bool next;
//	int j;
//
//	for (int i = 0; i < pi->num; i++) {
//		next = false;
//		for (j = 0; j < LGroup; j++) {
//			recyc_times++;
//			if (fabs(pivotQueryDist[g[i][j].getPivId()] - g[i][j].getDistance()) > kdist) {
//				next = true;
//				break;
//			}
//		}
//		if (!next) {
//			tempdist = calculateDistance(q, i);
//			if (tempdist <= kdist) {
//				SortEntry s;
//				s.dist = tempdist;
//				result.push_back(s);
//				sort(result.begin(), result.end());
//				if (result.size() > k) {
//					result.pop_back();
//				}
//				if (result.size() >= k)
//					kdist = result.back().dist;
//			}
//		}
//	}
//	//delete[] pivotQueryDist;
//	free(pivotQueryDist);
//	return kdist;
//}

// Paper: "MkNNQ processing on EPT or EPT* are the same as those on LAESA"
// Búsqueda de k vecinos más cercanos usando distancias pre-calculadas
// Paper: "EPT and EPT* utilize different pivots for different objects, while LAESA uses the same pivots"
double KNNQuery(float* q, int k)
{
	vector<SortEntry> result;
	double pivotsquery[num_cand];
	// Calcular distancias del query a todos los pivotes usados
	for (int i = 0; i < num_cand; ++i)
	{
		if (ispivot[i])
		{
			pivotsquery[i] = calculateDistance(q, samples_objs[cand[i]]);
		}
	}
	double kdist = FLT_MAX;
	bool next = false;
	double tempdist;
	int pos = -LGroup;
	// Paper: "EPT uses tables to store pre-computed distances"
	// Usar desigualdad triangular para poda: |d(q,p) - d(o,p)| ≤ d(q,o)
	for (int i = 0; i < pi->num; ++i)
	{
		next = false;
		pos += LGroup;
		// Verificar con los l pivotes específicos del objeto i
		for (int j = 0; j < LGroup; ++j)
		{
			// Si |d(q,p) - d(o,p)| > kdist, entonces d(q,o) > kdist (podar)
			if (fabs(pivotsquery[G[pos+j].pivId] - G[pos+j].distance) > kdist)
			{
				next = true;
				break;
			}
		}
		if (!next)
		{
			tempdist = calculateDistance(q, i);
			if (tempdist <= kdist)
			{
				SortEntry s;
				s.dist = tempdist;
				result.push_back(s);
				sort(result.begin(), result.end());
				if (result.size() > k) {
					result.pop_back();
				}
				if (result.size() >= k)
					kdist = result.back().dist;
			}
		}
	}

	return kdist;
}

// Paper: "MRQ processing on EPT or EPT* are the same as those on LAESA"
// Búsqueda por rango usando distancias pre-calculadas y poda por desigualdad triangular
int rangeQuery(float* q, double radius, int qj)
{
	int result;
	double pivotsquery[num_cand];
	// Calcular distancias del query a todos los pivotes
	for (int i = 0; i < num_cand; ++i)
	{
		if (ispivot[i])
		{
			pivotsquery[i] = calculateDistance(q, samples_objs[cand[i]]);
		}
	}
	double kdist = FLT_MAX;
	bool next = false;
	result = 0;
	int pos = -LGroup;
	// Paper: "EPT and EPT* utilize different pivots for different objects"
	// Cada objeto usa sus propios l pivotes para poda
	for (int i = 0; i < pi->num;  ++i)
	{
		next = false;
		pos += LGroup;
		for (int j = 0; j < LGroup; ++j)
		{
			// Poda: si |d(q,p) - d(o,p)| > radius, entonces d(q,o) > radius
			if (fabs(pivotsquery[G[pos+j].pivId] - G[pos+j].distance) > radius)
			{
				next = true;
				break;
			}
		}
		if (!next)
		{
			if (calculateDistance(q, i) <= radius)
			{
				result++;
			}
		}
	}

	return result;
}

// Paper: "The storage cost of EPT* is O(n*s + n*l + l_c*s)"
// Guardar el índice EPT* en disco
// Paper: "EPT* needs O(n*l) to store the pre-computed distances"
void saveIndex(Tuple * t, char *fname)
{
	long int j, i;
	FILE *fp;

	if ((fp = fopen(fname, "wb")) == NULL)
	{
		fprintf(stderr, "Error opening output file\n");
		exit(-1);
	}
	int pid, oid;
	double d;
	// Guardar para cada objeto sus l pivotes seleccionados y distancias pre-calculadas
	for (i = 0; i < pi->num; i++) {
		for (j = 0; j < LGroup; j++) {
			pid = t[i*LGroup + j].getPivId();
			oid = t[i * LGroup + j].getObjId();
			d = t[i * LGroup + j].getDistance();
			fwrite(&pid, sizeof(int), 1, fp);
			fwrite(&oid, sizeof(int), 1, fp);
			fwrite(&d, sizeof(double), 1, fp);
		}
	}
	fclose(fp);
}


// Cargar índice EPT* pre-construido desde disco
// Paper: "The storage cost of EPT* is O(n*s + n*l + l_c*s)"
bool readIndex(Tuple* t, char* fname)
{
	long int j, i;
	FILE* fp;

	if ((fp = fopen(fname, "rb")) == NULL)
	{
		//fprintf(stderr, "Error opening output file\n");
		return false;
	}
	int pid, oid;
	double d;
	// Leer para cada objeto sus l pivotes y distancias pre-calculadas
	// Paper: "EPT selects different pivots for different objects"
	for (i = 0; i < pi->num; i++) {
		//t[i] = (Tuple*)malloc(sizeof(Tuple) * LGroup);
		for (j = 0; j < LGroup; j++) {
			fread(&pid, sizeof(int), 1, fp);
			fread(&oid, sizeof(int), 1, fp);
			fread(&d, sizeof(double), 1, fp);
			t[i * LGroup + j].setPivId(pid);
			t[i * LGroup + j].setObjId(oid);
			t[i * LGroup + j].setDistance(d);
			ispivot[pid] = true;
		}
	}
	fclose(fp);
	
	return true;
}

// ============================================================================
// PROGRAMA PRINCIPAL: Construcción y Evaluación de EPT*
// Paper: "EPT* achieves better similarity search performance than EPT"
// Paper: "The construction cost of EPT* is O(n*l*l_c*n_s)"
// Paper: "The pivot selection is costly, and thus, the construction cost of EPT* is very high"
// ============================================================================
// Original main() commented out - using test.cpp main() instead
#if 0
int main(int argc, char **argv)
{
	clock_t begin, buildEnd, queryEnd;
	double buildComp, queryComp;
	
	cout << "main start." << endl;

	srand((unsigned int)time(NULL)); // randomize seed

	char * input = argv[1];
	char * indexfile = argv[2];

	//parse raw data into structural data
	pi = new Interpreter();
	pi->parseRawData(input);

	double radius[7];
	int kvalues[] = { 1, 5, 10, 20, 50, 100 };
	char * querydata;
	querydata = argv[4];
	if (string(input).find("LA") != -1) {
		double r[] = { 473, 692, 989, 1409, 1875, 2314, 3096 };
		memcpy(radius, r, sizeof(r));
		
	}
	else if (string(input).find("integer") != -1) {
		double r[] = { 2321, 2733, 3229, 3843, 4614, 5613, 7090 };
		memcpy(radius, r, sizeof(r));
		
	}
	else if (string(input).find("mpeg_1M") != -1) {
		double r[] = { 3838, 4092, 4399, 4773, 5241, 5904, 7104 };
		memcpy(radius, r, sizeof(r));
		
	}
	else if (string(input).find("sf") != -1) {
		double r[] = { 100, 200, 300, 400, 500, 600, 700 };
		memcpy(radius, r, sizeof(r));
	}

	FILE * f = fopen(argv[3], "w");
	
	int pn = atoi(argv[5]);

	{
		// Paper: "EPT has l groups while each group contains g pivots"
		// LGroup = l (número de pivotes por objeto)
		LGroup = pn;
		fprintf(f, "pivotnum: %d\n", LGroup);
		begin = clock();
		compDists = 0;
		// Paper: "EPT* is equipped with a new pivot selection algorithm (PSA)"
		// Construir índice EPT* usando algorithm3 (PSA)
		algorithm3(indexfile);
		saveIndex(G, indexfile);
		buildEnd = clock() - begin;
		buildComp = compDists;
		fprintf(f, "building...\n");
		fprintf(f, "finished... %f build time\n", (double)buildEnd / CLOCKS_PER_SEC);
		fprintf(f, "finished... %f distances computed\n", buildComp);

		
		/*struct stat sdata;
		stat(indexfile, &sdata);
		fprintf(f, "saved... %lli bytes\n", (long long)sdata.st_size);*/
		cout << "finish building" << endl;
		
		
		fprintf(f, "\nquerying...\n");

		float* obj = new float[pi->dim];
		memset(obj, 0, sizeof(obj));
		FILE * fp;
		int qcount = 100;
		double rad;
		cout << "start knnSearching......" << endl; clock_t start, times;
		for (int k = 0; k < 6; k++) {
			//fp = fopen(querydata, "r");
			ifstream fin(querydata, ios::in);
			start = clock();
			compDists = 0;
			rad = 0; 
			for (int j = 0; j < qcount;j++) {

				for (int x = 0; x < pi->dim; x++)
					fin >> obj[x];
					//fscanf(fp, "%f", &obj[x]);
				//fscanf(fp, "\n");
				//Objvector q(obj);
				
				rad += KNNQuery(obj, kvalues[k]);
				
			}
			times = clock() - start;
			queryComp = compDists;
				
			fprintf(f, "k: %d\n", kvalues[k]);
			
			fprintf(f, "finished... %f query time\n", (double)times / CLOCKS_PER_SEC / qcount);
			fprintf(f, "finished... %f distances computed\n", queryComp / qcount);
			fprintf(f, "finished... %f radius\n", rad / qcount);
			fflush(f);
			//fclose(fp);
			fin.close();
		}
		cout << "start rangeSearching......" << endl; 
		for (int k = 0; k < 7; ++k) {
			//fp = fopen(querydata, "r");
			ifstream fin(querydata, ios::in);
			start = clock();
			compDists = 0;
			rad = 0;
			for (int j = 0; j < qcount;j++) {
				for (int x = 0; x < pi->dim; x++)
					fin >> obj[x];
					//fscanf(fp, "%f", &obj[x]);
				//fscanf(fp, "\n");
				//Objvector q(obj);
				
				int temp = rangeQuery(obj, radius[k], j);
				rad += temp;
			}
			times = clock() - start;
			queryComp = compDists;
			fprintf(f, "r: %f\n", radius[k]);
			fprintf(f, "finished... %f query time\n", (double)times / CLOCKS_PER_SEC / qcount);
			fprintf(f, "finished... %f distances computed\n", queryComp / qcount);
			fprintf(f, "finished... %f objs\n", rad / qcount);
			fflush(f);
			//fclose(fp);
			fin.close();
		}
		//free(g);
		free(cand);

		delete[] DB;
		delete[] nobj;
		//g = NULL;
		DB = NULL;
		fprintf(f, "\n");
	}
	fclose(f);
	cout << "main end." << endl;
	return 0;
}
#endif  // End of commented original main()