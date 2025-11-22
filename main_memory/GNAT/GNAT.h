#pragma once
#pragma once
#include <unordered_map>
#include <queue>
#include "index.h"
extern int MaxHeight;
struct GNAT_node_t
{
	vector<int> pivot;
	vector<GNAT_node_t> children;
	vector<vector<double>> min_dist, max_dist;


	vector<int> bucket;
	int num;
};

class GNAT_t : public index_t
{
	GNAT_node_t root;
	size_t max_pivot_cnt, min_pivot_cnt, avg_pivot_cnt;
	void select(size_t& pivot_cnt, vector<int>& objects, GNAT_node_t* root);
	void _build(GNAT_node_t* root, vector<int> objects, size_t pivot_size,int h);
	void _rangeSearch(const GNAT_node_t* root, int query, double range, int& res_size);
	void _knnSearch(const GNAT_node_t * root, int query, int k, priority_queue<double>& result, double& ave_r);
public:

	GNAT_t(db_t* db, size_t avg_pivot_cnt);
	void build();
	void rangeSearch(vector<int> &query, double range, int& result_size) override;
	void knnSearch(vector<int> &queries, int k, double& ave_r) override;
};
