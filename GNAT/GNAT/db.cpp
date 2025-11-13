#include <algorithm>
#include <fstream>
#include <iterator>
#include <iostream>

#include "db.h"
using namespace std;

void double_db_t::read(string path)
{
	ifstream in(path);
	size_t n;
	int func;
	in >> dimension >> n >> func;
	for (int i = 0; i < n; ++i) {
		objs.emplace_back();
		copy_n(istream_iterator<double>(in), dimension, back_inserter(objs.back()));
	}
}

double L1_db_t::dist(int x, int y) const
{
	double dist = 0.0;
	for (int i = 0; i < dimension; i++) {
		dist += fabs(objs[x][i] - objs[y][i]);
	}

	return dist;
}

double L2_db_t::dist(int x, int y) const
{
	double dist = 0.0;
	for (int i = 0; i < dimension; i++) {
		dist += pow(objs[x][i] - objs[y][i], 2);
	}
	dist = sqrt(dist);
	return dist;
}

double L5_db_t::dist(int x, int y) const
{
	double dist = 0.0;
	for (int i = 0; i < dimension; i++) {
		dist += pow(fabs(objs[x][i] - objs[y][i]), 5);
	}
	dist = pow(dist, 0.2);
	return dist;
}

double Linf_db_t::dist(int x, int y) const
{
	double dist = 0.0;
	for (int i = 0; i < dimension; i++) {
		dist = max(dist, fabs(objs[x][i] - objs[y][i]));
	}
	return dist;
}

void str_db_t::read(string path)
{
	ifstream in(path);
	size_t n;
	in >> n;
	for (int i = 0; i < n; ++i) {
		objs.emplace_back();
		in >> objs.back();
	}
}

double str_db_t::dist(int x, int y) const
{
	const string& s1 = objs[x];
	const string& s2 = objs[y];
	auto n1 = s1.size();
	auto n2 = s2.size();
	vector<vector<int>> dist(n1 + 1, vector<int>(n2 + 1));
	for (size_t i = 0; i <= n1; i++)
		dist[i][0] = (int)i;
	for (size_t j = 0; j <= n2; j++)
		dist[0][j] = (int)j;
	for (size_t i = 1; i <= n1; i++) {
		for (size_t j = 1; j <= n2; j++) {
			if (s1[i - 1] == s2[j - 1])
				dist[i][j] = dist[i - 1][j - 1];
			else
				dist[i][j] = min({ dist[i - 1][j], dist[i][j - 1], dist[i - 1][j - 1] }) + 1;
		}
	}
	return dist[n1][n2];
}
