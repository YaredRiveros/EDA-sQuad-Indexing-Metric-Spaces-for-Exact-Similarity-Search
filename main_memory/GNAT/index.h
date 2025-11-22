#pragma once
#include "db.h"

class index_t
{
protected:
	db_t* db = nullptr;

	double dist(int x, int y)
	{
		++dist_call_cnt;
		return db->dist(x, y);
	}
public:
	long long dist_call_cnt = 0;
	index_t(db_t* db) :db(db) {}
	virtual void build() = 0;
	virtual void rangeSearch(vector<int> &queries, double range, int& res_size) = 0;
	virtual void knnSearch(vector<int> &queries, int k, double& ave_r) = 0;
};