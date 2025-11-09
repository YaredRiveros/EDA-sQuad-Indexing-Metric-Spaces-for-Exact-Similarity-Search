#include "ResultSet.h"
#include <cstring>

ResultSet::ResultSet(int k) {
	this->k = k;
	num = 0;
	result = (resultElem*)malloc((k+1)*sizeof(resultElem));
	memset(result, 0, (k + 1)*sizeof(resultElem));
	for (int i = 0; i < k; i++) {
		//result[i].dist = DBL_MAX;
		result[i].dist = 10000;
		//cout << "result[" << i << "].dist=" << result[i].dist << endl;
	}
	radius = result[k - 1].dist;
}

void ResultSet::addElem(resultElem elem) {

	int pos = 0;
	for (int i = num - 1; i >= 0; i--) {
		//if (result[i].dist > elem.dist) pos = i;
		//else break;
		if (result[i].dist < elem.dist) {
			pos = i + 1;
			break;
		} 
	}

	for (int i = num; i > pos; i--) {
		result[i] = result[i - 1];
	}
	if (pos<k) result[pos] = elem;
	num++;
	if (num > k) num = k;
	radius = result[k - 1].dist;
}

double ResultSet::resultRadius() {
	//cout <<"k-1="<<k-1<< ",in resultRadius,radius=" << result[k - 1].dist << endl;
	return result[k - 1].dist;
}

ResultSet::~ResultSet() {
	free(result);
	result = NULL;
}