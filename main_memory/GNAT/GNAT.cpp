#include "GNAT.h"
#include <algorithm>
#include <fstream>
using namespace std;
#include <iostream>
#include <chrono>
#include <set>
#include <cassert>
#include <cfloat>
using namespace chrono;

// GNAT (Geometric Near-neighbor Access Tree) es una generalización m-aria del GHT (Generalized Hyperplane Tree)
// Paper: "GHT can be generalized to m-ary trees, yielding Geometric Near-neighbor Access Tree (GNAT)"
// En este caso, pivot_cnt representa 'm' (el número de centros/pivotes seleccionados en cada nodo)
GNAT_t::GNAT_t(db_t* db, size_t avg_pivot_cnt) :
	index_t(db),
	max_pivot_cnt(min(4 * avg_pivot_cnt, (size_t)256)),
	min_pivot_cnt(2),
	avg_pivot_cnt(avg_pivot_cnt)
{
}

// Construcción del índice GNAT
// Paper: "construction cost is Ω(m*n*log_m(n))" donde n es el número total de objetos y m es la aridad del árbol
// Este método inicializa la construcción recursiva del árbol m-ario
void GNAT_t::build()
{
	vector<int> objects;
	for (int i = 0; i < (int)db->size(); ++i) {
		objects.push_back(i);
	}
	random_shuffle(objects.begin(), objects.end());
	cout << "database size: " << objects.size() << endl;
	_build(&root, objects, avg_pivot_cnt,1);
}

// Selección de los m centros (pivotes) para cada nodo
// Paper: "m centers c_i (1 ≤ i ≤ m) are selected each time"
// Este método implementa la estrategia de selección de pivotes usando sampling
void GNAT_t::select(size_t& pivot_cnt, vector<int>& objects, GNAT_node_t* root)
{
	//sampling
	size_t sample_cnt = min(pivot_cnt * 3, objects.size());
	vector<int> sample(sample_cnt);
	copy(objects.end() - sample_cnt, objects.end(), sample.begin());
	objects.resize(objects.size() - sample_cnt);

	//calc dist between samples
	vector<vector<double>> d(sample_cnt, vector<double>(sample_cnt));
	for (size_t i = 0; i < sample_cnt; ++i) {
		for (size_t j = i + 1; j < sample_cnt; ++j) {
			d[i][j] = d[j][i] = dist(sample[i], sample[j]);
		}
	}

	vector<size_t> pivot_pos(pivot_cnt);
	vector<bool> is_pivot(sample_cnt, false);
	//select first pivot
	// Estrategia: seleccionar pivotes que estén lo más dispersos posible para maximizar la discriminación
	for (size_t i = 0; i < sample_cnt; ++i) {
		d[i][i] = DBL_MAX;
	}
	vector<double> dist_other(sample_cnt);
	for (size_t i = 0; i < sample_cnt; ++i) {
		dist_other[i] = *min_element(d[i].begin(), d[i].end());
	}
	auto p = max_element(dist_other.begin(), dist_other.end()) - dist_other.begin();
	pivot_pos[0] = p;
	is_pivot[p] = true;

	//select pivots
	// Selección iterativa de los m pivotes restantes maximizando la distancia a pivotes ya seleccionados
	vector<double> dist_pivot(sample_cnt, DBL_MAX);
	for (size_t i = 0; i < sample_cnt; ++i) {
		d[i][i] = 0.0;
	}
	for (size_t i = 1; i < pivot_cnt; ++i) {
		for (size_t j = 0; j < sample_cnt; ++j) {
			dist_pivot[j] = min(dist_pivot[j], d[j][pivot_pos[i-1]]);
		}
		for (p = 0; is_pivot[p]; ++p)
			continue;
		for (auto j = p + 1; j < (int)sample_cnt; ++j) {
			if (dist_pivot[j] > dist_pivot[p] && !is_pivot[j]) {
				p = j;
			}
		}
		pivot_pos[i] = p;
		is_pivot[p] = true;
	}

	//save pivots
	// Almacenar los m centros seleccionados en el nodo actual
	for (auto i : pivot_pos) {
		root->pivot.push_back(sample[i]);
	}
	//put other samples back
	for (size_t i = 0; i < sample_cnt; ++i) {
		if (!is_pivot[i]) {
			objects.push_back(sample[i]);
		}
	}
}

// Construcción recursiva del árbol GNAT (m-ary tree)
// Paper: "GHT can be generalized to m-ary trees, yielding Geometric Near-neighbor Access Tree (GNAT)"
// Paper: "GNAT has storage cost O(n*s + m*n) and construction cost Ω(m*n*log_m(n))"
void GNAT_t::_build(GNAT_node_t * root, vector<int> objects, size_t pivot_cnt,int h)
{
	if (objects.empty()) {
		return;
	}

	if (h < MaxHeight) {
		root->num = -1;
		auto& pivot = root->pivot;
		auto& children = root->children;
		auto& max_dist = root->max_dist;
		auto& min_dist = root->min_dist;
		// Paper: "GNAT stores the minimum bounding box MBB_ij = [mindist(o,c_j), maxdist(o,c_j)] (o ∈ R_i)"
		// Aquí min_dist y max_dist almacenan los MBBs para cada región con respecto a cada centro
		// Paper: "O(m²*n_node) MBB storage cost, where n_node denotes the number of non-leaf nodes"
		min_dist.resize(pivot_cnt, vector<double>(pivot_cnt, DBL_MAX));
		max_dist.resize(pivot_cnt, vector<double>(pivot_cnt, 0.0));
		select(pivot_cnt, objects, root);

		static size_t completed = 0;
		completed += pivot_cnt;

		if (objects.empty()) {
			return;
		}

		// Paper: "objects are assigned to the nearest center"
		// Particionamiento: asignar cada objeto al centro/pivote más cercano
		vector<vector<int>> objs_children(pivot_cnt);
		for (int obj : objects) {
			vector<double> dist_pivot(pivot_cnt);
			for (size_t i = 0; i < pivot_cnt; ++i) {
				dist_pivot[i] = dist(obj, pivot[i]);
			}
			size_t closest_pivot = min_element(dist_pivot.begin(), dist_pivot.end()) - dist_pivot.begin();
			objs_children[closest_pivot].push_back(obj);
			// Cálculo de los MBBs (Minimum Bounding Boxes) para poda durante búsqueda
			// Paper: "MBB_ij = [mindist(o,c_j), maxdist(o,c_j)] (o ∈ R_i)"
			// Almacena min y max distancia desde cada región i hacia cada centro j
			for (size_t i = 0; i < pivot_cnt; ++i) {
				max_dist[i][closest_pivot] = max(max_dist[i][closest_pivot], dist_pivot[i]);
				min_dist[i][closest_pivot] = min(min_dist[i][closest_pivot], dist_pivot[i]);
			}
		}
		// Construcción recursiva de los m hijos (árbol m-ario)
		// Paper: GNAT es un árbol m-ario donde cada nodo puede tener hasta m hijos
		children.resize(pivot_cnt);
		for (size_t i = 0; i < pivot_cnt; ++i) {
			size_t next_pivot_cnt = objs_children[i].size() * avg_pivot_cnt * pivot_cnt / objects.size();
			next_pivot_cnt = max(min_pivot_cnt, next_pivot_cnt);
			next_pivot_cnt = min(max_pivot_cnt, next_pivot_cnt);
			_build(&children[i], objs_children[i], min(next_pivot_cnt, objs_children[i].size()), h + 1);
		}
	}
	else {
		// Nodo hoja: almacena objetos directamente en un bucket
		// Paper: "GHT, GNAT, and EGNAT are unbalanced trees" - esto puede generar hojas a diferentes alturas
		root->num = objects.size();
		for (int i = 0; i < root->num; i++) root->bucket.push_back(objects[i]);
	}

}

// Búsqueda por rango (MRQ - Metric Range Query)
// Paper: "Query processing traverses the GHT in depth-first manner, where Lemma 4.3 (double-pivot filtering) is used"
// Paper: "In the case of GNAT, the additional MBBs stored in non-leaf nodes enable pruning using Lemma 4.1"
void GNAT_t::_rangeSearch(const GNAT_node_t * root, int query, double range, int& res_size)
{
	if (root->num < 0) {
		auto& pivot = root->pivot;
		auto& children = root->children;
		auto& max_dist = root->max_dist;
		auto& min_dist = root->min_dist;
		size_t n = pivot.size();
		vector<double> d(n);
		// Calcular distancias a todos los pivotes del nodo actual
		for (size_t i = 0; i < n; ++i) {
			d[i] = dist(pivot[i], query);
			if (d[i] <= range) {
				++res_size;
			}
		}
		// Paper: "the additional MBBs stored in non-leaf nodes enable pruning using Lemma 4.1"
		// Poda usando MBBs: verificar si la región puede contener resultados
		// Condición: max_dist[j][i] >= d[j] - range AND min_dist[j][i] <= d[j] + range
		for (size_t i = 0; i < n; ++i) {
			bool ok = true;
			for (int j = 0; ok && j < n; ++j) {
				ok &= (max_dist[j][i] >= d[j] - range);
				ok &= (min_dist[j][i] <= d[j] + range);
			}
			if (ok) {
				_rangeSearch(&children[i], query, range, res_size);
			}
		}
	}
	else {
		// Nodo hoja: verificar todos los objetos del bucket directamente
		double d;
		for (int i = 0; i < root->num; i++) {
			d = dist(query, root->bucket[i]);
			if (d <= range) ++res_size;
		}
	}

}

// Interfaz pública para búsqueda por rango
// Paper: "MRQ and MkNNQ Processing. Query processing traverses the GHT in depth-first manner"
void GNAT_t::rangeSearch(vector<int> &queries, double range, int& res_size)
{
	for (int query : queries) {
		_rangeSearch(&root, query, range, res_size);
	}
}

// Mantener los k mejores resultados (distancias más pequeñas)
static void addResult(int k, double d, priority_queue<double>& result, double& r) {
	if (result.size() < k || d < result.top()) {
		result.push(d);
	}
	if (result.size() > k) {
		result.pop();
	}
	r = result.top();
}

// Búsqueda de k vecinos más cercanos (MkNNQ - Metric k-Nearest Neighbor Query)
// Paper: "MkNNQ(q,k) processing based on GHT, GNAT, and EGNAT follows the second approach introduced in Section 2.2"
// Paper: "In the case of GNAT, the additional MBBs stored in non-leaf nodes enable pruning using Lemma 4.1"
void GNAT_t::_knnSearch(const GNAT_node_t * root, int query, int k, priority_queue<double>& result, double& ave_r)
{
	if (root->num < 0) {
		auto& pivot = root->pivot;
		auto& children = root->children;
		auto& max_dist = root->max_dist;
		vector<pair<double, int>> od(pivot.size());
		// Calcular distancias a todos los pivotes y actualizar resultados
		for (int i = 0; i < pivot.size(); i++)
		{
			od[i].first = dist(query, pivot[i]);
			od[i].second = i;
			addResult(k, od[i].first, result, ave_r);
		}
		if (children.empty())
			return;
		// Ordenar regiones por distancia al query para exploración eficiente
		sort(od.begin(), od.end());
		// Poda usando MBBs y distancia al pivote más cercano
		// Paper: utiliza "double-pivot filtering" (Lemma 4.3) y MBBs (Lemma 4.1)
		for (int i = 0; i < pivot.size(); i++)
		{
			if (result.size() == k && ((od[i].first - od[0].first) / 2) > result.top())
				break;
			if (result.empty() || (od[i].first - result.top() <= max_dist[od[i].second][od[i].second]))
				_knnSearch(&children[od[i].second], query, k, result, ave_r);
		}
	}
	else {
		// Nodo hoja: revisar todos los objetos del bucket
		double d;
		for (int i = 0; i < root->num; i++) {
			d = dist(query, root->bucket[i]);
			addResult(k, d, result, ave_r);
		}
	}


}

// Interfaz pública para búsqueda de k-NN
// Paper: "MkNNQ(q,k) processing" - búsqueda de los k vecinos más cercanos
void GNAT_t::knnSearch(vector<int> &queries, int k, double& ave_r)
{
	double r = 0;
	for (int q : queries) {
		priority_queue<double> result;
		_knnSearch(&root, q, k, result, r);
		ave_r += r;
	}
}
