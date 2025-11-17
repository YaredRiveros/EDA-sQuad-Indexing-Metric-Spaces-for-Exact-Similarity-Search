#include <algorithm>
#include <fstream>
#include <iterator>
#include <iostream>
#include <sstream>
#include <cmath>

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
	read(path, -1); // Sin límite
}

void str_db_t::read(string path, int max_objects)
{
	ifstream in(path);
	if (!in.is_open()) {
		cerr << "[ERROR] No se pudo abrir archivo: " << path << endl;
		return;
	}
	
	string first_line;
	getline(in, first_line);
	
	// Detectar encabezado tipo "n p" (igual que StringDB)
	stringstream ss(first_line);
	int maybe_n, maybe_p;
	bool has_header = false;
	
	if (ss >> maybe_n >> maybe_p && ss.eof()) {
		// Es un encabezado, leer hasta maybe_n líneas (o max_objects si está definido)
		has_header = true;
		int limit = (max_objects > 0) ? min(maybe_n, max_objects) : maybe_n;
		objs.reserve(limit);
		for (int i = 0; i < limit; i++) {
			string line;
			if (getline(in, line) && !line.empty()) {
				objs.push_back(line);
			}
		}
	}
	
	if (!has_header) {
		// No es encabezado, la primera línea es un dato
		if (!first_line.empty()) {
			objs.push_back(first_line);
		}
		// Leer el resto (hasta max_objects si está definido)
		string line;
		int count = 1; // Ya leímos first_line
		while (getline(in, line)) {
			if (max_objects > 0 && count >= max_objects) break;
			if (!line.empty()) {
				objs.push_back(line);
				count++;
			}
		}
	}
	
	cerr << "[str_db_t] Cargadas " << objs.size() << " cadenas (edit distance)\n";
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
