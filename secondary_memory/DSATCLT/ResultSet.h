#ifndef _RESULTSET_H_
#define _RESULTSET_H_
#include <stdlib.h>
#include "obj.h"
#include <cstring>

/*knn��ѯ������е�һ��Ԫ��*/

typedef struct {
	Objvector obj;
	double dist;
}resultElem;

class ResultSet {
public:
	int num;
	int k;
	double radius;
	resultElem *result;

	ResultSet(int k);
	~ResultSet();
	void addElem(resultElem elem);
	double resultRadius();
};
#endif
