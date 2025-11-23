#include <bits/stdc++.h>
#include <filesystem>
#include <chrono>
#include "../../../datasets/paths.hpp"

using namespace std;
using namespace chrono;

// ============================================================
// INLINE IMPLEMENTATIONS (db.h, db.cpp, index.h, GNAT.h, GNAT.cpp)
// ============================================================

// Variable global de GNAT
int MaxHeight;

// --- db.h ---
class db_t
{
public:
	virtual size_t size() const = 0;
	virtual void read(string path) = 0;
	virtual void read(string path, int max_objects) { read(path); }
	virtual double dist(int x, int y) const = 0;
};

class double_db_t : public db_t
{
protected:
	int dimension = 0;
	vector<vector<double>> objs;
public:
	size_t size() const { return objs.size(); }
	void read(string path);
};

class L1_db_t : public double_db_t
{
public:
	double dist(int x, int y) const;
};

class L2_db_t : public double_db_t
{
public:
	double dist(int x, int y) const;
};

class Linf_db_t : public double_db_t
{
public:
	double dist(int x, int y) const;
};

class str_db_t : public db_t
{
	vector<string> objs;
public:
	size_t size() const { return objs.size(); }
	void read(string path);
	void read(string path, int max_objects);
	double dist(int x, int y) const;
};

// --- db.cpp implementations ---
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
	return sqrt(dist);
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
	read(path, -1);
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
	
	stringstream ss(first_line);
	int maybe_n, maybe_p;
	bool has_header = false;
	
	if (ss >> maybe_n >> maybe_p && ss.eof()) {
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
		if (!first_line.empty()) {
			objs.push_back(first_line);
		}
		string line;
		int count = 1;
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

// --- index.h ---
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

// --- GNAT.h ---
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

// --- GNAT.cpp implementations ---
GNAT_t::GNAT_t(db_t* db, size_t avg_pivot_cnt) :
	index_t(db),
	max_pivot_cnt(min(4 * avg_pivot_cnt, (size_t)256)),
	min_pivot_cnt(2),
	avg_pivot_cnt(avg_pivot_cnt)
{
}

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

void GNAT_t::select(size_t& pivot_cnt, vector<int>& objects, GNAT_node_t* root)
{
	size_t sample_cnt = min(pivot_cnt * 3, objects.size());
	vector<int> sample(sample_cnt);
	copy(objects.end() - sample_cnt, objects.end(), sample.begin());
	objects.resize(objects.size() - sample_cnt);

	vector<vector<double>> d(sample_cnt, vector<double>(sample_cnt));
	for (size_t i = 0; i < sample_cnt; ++i) {
		for (size_t j = i + 1; j < sample_cnt; ++j) {
			d[i][j] = d[j][i] = dist(sample[i], sample[j]);
		}
	}

	vector<size_t> pivot_pos(pivot_cnt);
	vector<bool> is_pivot(sample_cnt, false);
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

	for (auto i : pivot_pos) {
		root->pivot.push_back(sample[i]);
	}
	for (size_t i = 0; i < sample_cnt; ++i) {
		if (!is_pivot[i]) {
			objects.push_back(sample[i]);
		}
	}
}

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
		min_dist.resize(pivot_cnt, vector<double>(pivot_cnt, DBL_MAX));
		max_dist.resize(pivot_cnt, vector<double>(pivot_cnt, 0.0));
		select(pivot_cnt, objects, root);

		static size_t completed = 0;
		completed += pivot_cnt;

		if (objects.empty()) {
			return;
		}

		vector<vector<int>> objs_children(pivot_cnt);
		for (int obj : objects) {
			vector<double> dist_pivot(pivot_cnt);
			for (size_t i = 0; i < pivot_cnt; ++i) {
				dist_pivot[i] = dist(obj, pivot[i]);
			}
			size_t closest_pivot = min_element(dist_pivot.begin(), dist_pivot.end()) - dist_pivot.begin();
			objs_children[closest_pivot].push_back(obj);
			for (size_t i = 0; i < pivot_cnt; ++i) {
				max_dist[i][closest_pivot] = max(max_dist[i][closest_pivot], dist_pivot[i]);
				min_dist[i][closest_pivot] = min(min_dist[i][closest_pivot], dist_pivot[i]);
			}
		}
		children.resize(pivot_cnt);
		for (size_t i = 0; i < pivot_cnt; ++i) {
			size_t next_pivot_cnt = objs_children[i].size() * avg_pivot_cnt * pivot_cnt / objects.size();
			next_pivot_cnt = max(min_pivot_cnt, next_pivot_cnt);
			next_pivot_cnt = min(max_pivot_cnt, next_pivot_cnt);
			_build(&children[i], objs_children[i], min(next_pivot_cnt, objs_children[i].size()), h + 1);
		}
	}
	else {
		root->num = objects.size();
		for (int i = 0; i < root->num; i++) root->bucket.push_back(objects[i]);
	}
}

void GNAT_t::_rangeSearch(const GNAT_node_t * root, int query, double range, int& res_size)
{
	if (root->num < 0) {
		auto& pivot = root->pivot;
		auto& children = root->children;
		auto& max_dist = root->max_dist;
		auto& min_dist = root->min_dist;
		size_t n = pivot.size();
		vector<double> d(n);
		for (size_t i = 0; i < n; ++i) {
			d[i] = dist(pivot[i], query);
			if (d[i] <= range) {
				++res_size;
			}
		}
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
		double d;
		for (int i = 0; i < root->num; i++) {
			d = dist(query, root->bucket[i]);
			if (d <= range) ++res_size;
		}
	}
}

void GNAT_t::rangeSearch(vector<int> &queries, double range, int& res_size)
{
	for (int query : queries) {
		_rangeSearch(&root, query, range, res_size);
	}
}

static void addResult(int k, double d, priority_queue<double>& result, double& r) {
	if (result.size() < k || d < result.top()) {
		result.push(d);
	}
	if (result.size() > k) {
		result.pop();
	}
	r = result.top();
}

void GNAT_t::_knnSearch(const GNAT_node_t * root, int query, int k, priority_queue<double>& result, double& ave_r)
{
	if (root->num < 0) {
		auto& pivot = root->pivot;
		auto& children = root->children;
		auto& max_dist = root->max_dist;
		vector<pair<double, int>> od(pivot.size());
		for (int i = 0; i < pivot.size(); i++)
		{
			od[i].first = dist(query, pivot[i]);
			od[i].second = i;
			addResult(k, od[i].first, result, ave_r);
		}
		if (children.empty())
			return;
		sort(od.begin(), od.end());
		for (int i = 0; i < pivot.size(); i++)
		{
			if (result.size() == k && ((od[i].first - od[0].first) / 2) > result.top())
				break;
			if (result.empty() || (od[i].first - result.top() <= max_dist[od[i].second][od[i].second]))
				_knnSearch(&children[od[i].second], query, k, result, ave_r);
		}
	}
	else {
		double d;
		for (int i = 0; i < root->num; i++) {
			d = dist(query, root->bucket[i]);
			addResult(k, d, result, ave_r);
		}
	}
}

void GNAT_t::knnSearch(vector<int> &queries, int k, double& ave_r)
{
	double r = 0;
	for (int q : queries) {
		priority_queue<double> result;
		_knnSearch(&root, q, k, result, r);
		ave_r += r;
	}
}

// ============================================================
// END OF INLINE IMPLEMENTATIONS
// ============================================================

// ============================================================
// CONFIGURACIÓN EXACTA DEL PAPER (Tabla 6)
// ============================================================

static const vector<double> SELECTIVITIES = {0.02, 0.04, 0.08, 0.16, 0.32};
static const vector<int>    K_VALUES      = {5, 10, 20, 50, 100};
static const vector<string> DATASETS      = {"LA", "Words", "Color", "Synthetic"};
static const vector<int> HEIGHT_VALUES = {3, 5, 10, 15, 20};

// ============================================================
// FUNCIONES AUXILIARES
// ============================================================

string dataset_category(const string& dataset) {
    if (dataset == "LA") return "vectors";
    if (dataset == "Color") return "vectors";
    if (dataset == "Synthetic") return "vectors";
    if (dataset == "Words") return "strings";
    return "unknown";
}

// ============================================================
// MAIN — EXPERIMENTACIÓN COMPLETA
// ============================================================
int main()
{
    // Crear directorio results
    std::filesystem::create_directories("results");

    for (const string& dataset : DATASETS)
    {
        // ------------------------------------------------------------
        // 1. Resolver dataset físico
        // ------------------------------------------------------------
        string dbfile = path_dataset(dataset);
        if (dbfile == "" || !std::filesystem::exists(dbfile)) {
            cerr << "[WARN] Dataset no encontrado, omitido: " << dataset << "\n";
            continue;
        }

        cerr << "\n==========================================\n";
        cerr << "[INFO] Dataset: " << dataset << "   File=" << dbfile << "\n";
        cerr << "==========================================\n";

        // ------------------------------------------------------------
        // 2. Cargar base de datos (tipos correctos)
        // ------------------------------------------------------------
        unique_ptr<db_t> db;

        if (dataset == "LA")
        {
            db = make_unique<L2_db_t>();
        }
        else if (dataset == "Color")
        {
            db = make_unique<L1_db_t>();
        }
        else if (dataset == "Synthetic")
        {
            db = make_unique<Linf_db_t>();
        }
        else if (dataset == "Words")
        {
            db = make_unique<str_db_t>();
        }
        else
        {
            cerr << "[WARN] Tipo de dataset no reconocido: " << dataset << "\n";
            continue;
        }

        // Limitar Words a 30K objetos desde la carga
        // (el archivo tiene 597K líneas pero las queries fueron generadas para ~26K)
        if (dataset == "Words") {
            cerr << "[INFO] Cargando Words con límite de 30000 objetos\n";
            db->read(dbfile, 30000);
        } else {
            db->read(dbfile);
        }

        int nObjects = db->size();
        if (nObjects == 0) {
            cerr << "[WARN] Dataset vacío, omitido: " << dataset << "\n";
            continue;
        }

        cerr << "[INFO] Objetos: " << nObjects << "\n";

        // ------------------------------------------------------------
        // 3. Cargar queries y radios (precomputados desde JSON)
        // ------------------------------------------------------------
        vector<int> queries = load_queries_file(path_queries(dataset));
        auto radii          = load_radii_file(path_radii(dataset));

        if (queries.empty()) {
            cerr << "[WARN] Queries ausentes, omitiendo dataset: " << dataset << "\n";
            continue;
        }

        // Filtrar queries que están fuera del rango de objetos cargados
        if (dataset == "Words") {
            vector<int> valid_queries;
            for (int q : queries) {
                if (q < nObjects) {
                    valid_queries.push_back(q);
                }
            }
            cerr << "[INFO] Filtrando queries: " << queries.size() << " -> " << valid_queries.size() << " (dentro de rango [0," << nObjects << "))\n";
            queries = valid_queries;
            
            if (queries.empty()) {
                cerr << "[WARN] No hay queries válidas después del filtrado, omitiendo dataset: " << dataset << "\n";
                continue;
            }
        }

        cerr << "[INFO] Loaded " << queries.size() << " queries\n";
        cerr << "[INFO] Loaded " << radii.size() << " radii\n";

        // ------------------------------------------------------------
        // 4. Archivo JSON
        // ------------------------------------------------------------
        string jsonOut = "results/results_GNAT_" + dataset + ".json";
        ofstream J(jsonOut);
        if (!J.is_open()) {
            cerr << "[ERROR] No se pudo abrir JSON para escribir.\n";
            return 1;
        }

        J << "[\n";

        // ------------------------------------------------------------
        // 5. Experimentos con diferentes valores de HEIGHT
        // ------------------------------------------------------------
        bool firstConfig = true;

        for (int HEIGHT : HEIGHT_VALUES)
        {
            cerr << "[INFO] ============ HEIGHT=" << HEIGHT << " ============\n";
            
            MaxHeight = HEIGHT; // Variable global de GNAT
            int arity = 5; // M (arity) fijo como en el main.cpp original
            
            // CONSTRUCCIÓN DEL ÍNDICE
            cerr << "[INFO] Construyendo índice GNAT con HEIGHT=" << HEIGHT << ", M=" << arity << "...\n";
            GNAT_t index(db.get(), arity);
            
            auto t1 = high_resolution_clock::now();
            unsigned long prevDistCount = index.dist_call_cnt;
            
            index.build();
            
            auto t2 = high_resolution_clock::now();
            double buildTime = duration_cast<milliseconds>(t2 - t1).count();
            double buildDists = index.dist_call_cnt - prevDistCount;

            cerr << "[INFO] Construcción completada: " << buildTime << " ms, " 
                 << buildDists << " compdists\n";

            // ========================================
            // EXPERIMENTOS MkNN
            // ========================================
            cerr << "[INFO] Ejecutando MkNN queries...\n";
            
            for (int k : K_VALUES)
            {
                cerr << "  k=" << k << "...\n";
                
                // Reset contadores
                index.dist_call_cnt = 0;
                auto start = high_resolution_clock::now();
                
                double avgRadius = 0;
                index.knnSearch(queries, k, avgRadius);
                
                auto end = high_resolution_clock::now();
                double totalTime = duration_cast<milliseconds>(end - start).count();
                
                double avgTime = totalTime / queries.size();
                double avgDists = (double)index.dist_call_cnt / queries.size();

                // JSON output
                if (!firstConfig) J << ",\n";
                firstConfig = false;

                J << "  {\n";
                J << "    \"index\": \"GNAT\",\n";
                J << "    \"dataset\": \"" << dataset << "\",\n";
                J << "    \"category\": \"" << dataset_category(dataset) << "\",\n";
                J << "    \"num_pivots\": " << HEIGHT << ",\n";
                J << "    \"num_centers_path\": null,\n";
                J << "    \"arity\": " << arity << ",\n";
                J << "    \"bucket_size\": null,\n";
                J << "    \"query_type\": \"MkNN\",\n";
                J << "    \"selectivity\": null,\n";
                J << "    \"radius\": " << avgRadius << ",\n";
                J << "    \"k\": " << k << ",\n";
                J << "    \"compdists\": " << avgDists << ",\n";
                J << "    \"time_ms\": " << avgTime << ",\n";
                J << "    \"n_queries\": " << queries.size() << ",\n";
                J << "    \"run_id\": \"GNAT_" << dataset << "_H" << HEIGHT << "_k" << k << "\"\n";
                J << "  }";
            }

            // ========================================
            // EXPERIMENTOS MRQ
            // ========================================
            cerr << "[INFO] Ejecutando MRQ queries...\n";

            for (const auto& entry : radii)
            {
                double sel = entry.first;   // selectivity es la clave del map
                double radius = entry.second; // radius es el valor
                
                cerr << "  selectivity=" << sel << ", radius=" << radius << "...\n";
                
                // Reset contadores
                index.dist_call_cnt = 0;
                auto start = high_resolution_clock::now();
                
                int totalResults = 0;
                index.rangeSearch(queries, radius, totalResults);
                
                auto end = high_resolution_clock::now();
                double totalTime = duration_cast<milliseconds>(end - start).count();
                
                double avgTime = totalTime / queries.size();
                double avgDists = (double)index.dist_call_cnt / queries.size();
                double avgResults = (double)totalResults / queries.size();

                // JSON output
                J << ",\n";
                J << "  {\n";
                J << "    \"index\": \"GNAT\",\n";
                J << "    \"dataset\": \"" << dataset << "\",\n";
                J << "    \"category\": \"" << dataset_category(dataset) << "\",\n";
                J << "    \"num_pivots\": " << HEIGHT << ",\n";
                J << "    \"num_centers_path\": null,\n";
                J << "    \"arity\": " << arity << ",\n";
                J << "    \"bucket_size\": null,\n";
                J << "    \"query_type\": \"MRQ\",\n";
                J << "    \"selectivity\": " << sel << ",\n";
                J << "    \"radius\": " << radius << ",\n";
                J << "    \"k\": null,\n";
                J << "    \"compdists\": " << avgDists << ",\n";
                J << "    \"time_ms\": " << avgTime << ",\n";
                J << "    \"n_queries\": " << queries.size() << ",\n";
                J << "    \"run_id\": \"GNAT_" << dataset << "_H" << HEIGHT << "_sel" << sel << "\"\n";
                J << "  }";
            }
        }

        J << "\n]\n";
        J.close();

        cerr << "[INFO] Resultados guardados en: " << jsonOut << "\n";
    }

    cerr << "\n[DONE] GNAT benchmark completado.\n";
    return 0;
}
