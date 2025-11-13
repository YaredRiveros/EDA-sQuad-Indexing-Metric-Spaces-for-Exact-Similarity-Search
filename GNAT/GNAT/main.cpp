/*********************************************************************
*                                                                    *
* Copyright (c)                                                      *
* ZJU-DBL, Zhejiang University, Hangzhou, China                      *
*                                                                    *
* All Rights Reserved.                                               *
*                                                                    *
* Permission to use, copy, and distribute this software and its      *
* documentation for NON-COMMERCIAL purposes and without fee is       *
* hereby granted provided  that this copyright notice appears in     *
* all copies.                                                        *
*                                                                    *
* THE AUTHORS MAKE NO REPRESENTATIONS OR WARRANTIES ABOUT THE        *
* SUITABILITY OF THE SOFTWARE, EITHER EXPRESS OR IMPLIED, INCLUDING  *
* BUT NOT LIMITED TO THE IMPLIED WARRANTIES OF MERCHANTABILITY,      *
* FITNESS FOR A PARTICULAR PURPOSE, OR NON-INFRINGEMENT. THE AUTHOR  *
* SHALL NOT BE LIABLE FOR ANY DAMAGES SUFFERED BY LICENSEE AS A      *
* RESULT OF USING, MODIFYING OR DISTRIBUTING THIS SOFTWARE OR ITS    *
* DERIVATIVES.                                                       *
*                                                                    *
*********************************************************************/

#include <chrono>
#include <iostream>
#include <fstream>
#include <iterator>
#include <algorithm>
#include "LC.h"
#include "GHT.h"
#include "GNAT.h"
using namespace std;
using namespace chrono;
int MaxHeight;
int main(int argc, char* argv[])
{
	char *dbFile = argv[1];
	char *queryFile = argv[2];
	char *costFile = argv[3];
	MaxHeight = atoi(argv[4]);
	//int arity = atoi(argv[5]);
	int arity = 5; //tree_arity,a parameter that users can specify
	/*-------------------------------------------*/
	//different db_t for different dataset
	//L2_db_t db;
	//str_db_t db;
	//Linf_db_t db;
	L1_db_t db;
	db.read(dbFile);
	string query_path(queryFile);


	/*-------------------------------------------*/


	GNAT_t index(&db, arity);

	/*-------------------------------------------*/

	clock_t start_time, end_time,sum_time;

	FILE *fout;
	fout = fopen(costFile, "w");


	cout << "start building GNAT..." << endl;
	start_time = clock();
	index.build();  //build GNAT
	sum_time = clock() - start_time;

	fprintf(fout,"building...\n");
	fprintf(fout, "build time:%f\n", (double)sum_time / CLOCKS_PER_SEC);
	fprintf(fout, "compdists:%f\n\n", (double)index.dist_call_cnt);

	float radius[7];
	int kvalues[] = { 1, 5, 10, 20, 50, 100 };
	if (string(query_path).find("LA") != -1) {
		float r[] = { 473, 692, 989, 1409, 1875, 2314, 3096 };
		memcpy(radius, r, sizeof(r));
	}
	else if (string(query_path).find("integer") != -1) {
		float r[] = { 2321,2733, 3229,3843, 4614, 5613, 7090 };
		memcpy(radius, r, sizeof(r));
	}
	else if (string(query_path).find("sf") != -1) {
		float r[] = { 100, 200, 300, 400, 500, 600, 700 };
		memcpy(radius, r, sizeof(r));
	}
	else if (string(query_path).find("mpeg") != -1) {
		float r[] = { 3838, 4092, 4399, 4773, 5241, 5904, 7104 };
		memcpy(radius, r, sizeof(r));
	}
	else if (string(query_path).find("moby") != -1) {
		float r[] = { 2, 3, 4, 5, 6, 8, 10 };
		memcpy(radius, r, sizeof(r));
	}

	ifstream in(query_path);
	vector<int> queries;
	int qcount = 100;
	for (int i = 0; i < qcount; i++) {
		int tmp = 0;
		in >> tmp;
		queries.push_back(tmp);
	}




	cout << "qcount=" << qcount << endl;
	cout << "start knnSearching......" << endl;
	int sum_dist_call_cnt = 0;
	for (int i = 0; i < 6; i++) {
		double ave_r = 0;
		index.dist_call_cnt = 0;
		start_time = clock();
		index.knnSearch(queries, kvalues[i], ave_r); //knnSearch

		sum_time = clock() - start_time;

		fprintf(fout, "k: %d\n", kvalues[i]);
		fprintf(fout, "queryTime: %f\n", (double)sum_time / CLOCKS_PER_SEC / qcount);
		fprintf(fout, "compdists: %f\n", (double)index.dist_call_cnt / qcount);
		fprintf(fout, "resultRadius: %f\n\n", (double)ave_r / qcount);
	}

	cout << "start rangeSearching......" << endl;
	sum_dist_call_cnt = 0;

	for (int i = 0; i < 7;i++) {

		index.dist_call_cnt = 0;
		start_time = clock();
		int res_size = 0;
		index.rangeSearch(queries, radius[i], res_size); //rangeSearch

		sum_time = clock() - start_time;


		fprintf(fout, "radius: %f\n", radius[i]);
		fprintf(fout, "queryTime: %f\n", (double)sum_time / CLOCKS_PER_SEC / qcount);
		fprintf(fout, "compdists: %f\n", (double)index.dist_call_cnt/qcount);
		fprintf(fout, "resultNum: %f\n\n",(double)res_size/qcount);
	}

	return 0;
}
