#ifndef OBJECTDB_HPP
#define OBJECTDB_HPP

#include <bits/stdc++.h>
using namespace std;

class ObjectDB {
public:
    virtual int size() const = 0;
    virtual double distance(int a, int b) const = 0;
    virtual void print(int id) const = 0;
    virtual ~ObjectDB() {}
};

class VectorDB : public ObjectDB {
    vector<vector<double>> data;
    int p;   // tipo de norma
    int dim; // dimensión

public:
    VectorDB(const string &filename, int p_default = 2)
        : p(p_default), dim(0) {
        ifstream f(filename);
        if (!f.is_open())
            throw runtime_error("No se pudo abrir el archivo: " + filename);

        string first;
        getline(f, first);
        stringstream ss(first);
        int maybe_dim, maybe_n, maybe_p;
        bool header = false;

        // Detectar encabezado tipo “dim n p”
        if (ss >> maybe_dim >> maybe_n >> maybe_p && ss.eof()) {
            header = true;
            dim = maybe_dim;
            p = maybe_p;
            cerr << "[VectorDB] Encabezado detectado: dim=" << dim
                 << " n=" << maybe_n << " p=" << p << "\n";
            data.reserve(maybe_n);
            for (int i = 0; i < maybe_n; i++) {
                vector<double> v(dim);
                for (int j = 0; j < dim; j++) f >> v[j];
                if (!v.empty()) data.push_back(v);
            }
        }

        if (!header) {
            f.clear();
            f.seekg(0);
            cerr << "[VectorDB] Archivo sin encabezado, usando p=" << p << "\n";
            string line;
            while (getline(f, line)) {
                stringstream ls(line);
                vector<double> v;
                double x;
                while (ls >> x) v.push_back(x);
                if (!v.empty()) data.push_back(v);
            }
            if (!data.empty()) dim = data[0].size();
        }

        cerr << "[VectorDB] Cargados " << data.size()
             << " objetos (" << dim << "D, p=" << p << ")\n";
    }

    int size() const override { return data.size(); }

    double distance(int o1, int o2) const override {
        const auto &a = data[o1], &b = data[o2];
        double s = 0;
        if (p == 1) { for (int i = 0; i < dim; i++) s += fabs(a[i] - b[i]); return s; }
        else if (p == 2) { for (int i = 0; i < dim; i++) s += (a[i]-b[i])*(a[i]-b[i]); return sqrt(s); }
        else { double m=0; for (int i=0;i<dim;i++) m=max(m,fabs(a[i]-b[i])); return m; }
    }

    void print(int o) const override {
        for (int j = 0; j < dim; j++)
            cout << data[o][j] << (j + 1 == dim ? '\n' : ' ');
    }
};

class StringDB : public ObjectDB {
    vector<string> data;

public:
    StringDB(const string &filename) {
        ifstream f(filename);
        if (!f.is_open())
            throw runtime_error("No se pudo abrir el archivo: " + filename);

        string line;
        getline(f, line);

        // Detectar encabezado tipo “n p”, pero ignoramos p
        stringstream ss(line);
        int maybe_n, maybe_p;
        bool header = false;

        if (ss >> maybe_n >> maybe_p && ss.eof()) {
            header = true;
            cerr << "[StringDB] Encabezado detectado: n=" << maybe_n 
                 << " (distancia edit/Levenshtein)\n";
            data.reserve(maybe_n);
            for (int i = 0; i < maybe_n && getline(f, line); i++)
                if (!line.empty()) data.push_back(line);
        }

        if (!header) {
            f.clear();
            f.seekg(0);
            cerr << "[StringDB] Archivo sin encabezado, usando distancia edit\n";
            while (getline(f, line))
                if (!line.empty()) data.push_back(line);
        }

        cerr << "[StringDB] Cargadas " << data.size()
             << " cadenas (distancia = Levenshtein)\n";
    }

    int size() const override { return data.size(); }

    double distance(int o1, int o2) const override {
        const string &a = data[o1], &b = data[o2];
        int n = a.size(), m = b.size();
        vector<vector<int>> dp(n+1, vector<int>(m+1));

        for (int i=0; i<=n; i++) dp[i][0] = i;
        for (int j=0; j<=m; j++) dp[0][j] = j;

        for (int i=1; i<=n; i++)
            for (int j=1; j<=m; j++)
                dp[i][j] = min({
                    dp[i-1][j] + 1,
                    dp[i][j-1] + 1,
                    dp[i-1][j-1] + (a[i-1] != b[j-1])
                });

        return dp[n][m];
    }

    void print(int o) const override { cout << data[o] << "\n"; }
};

#endif
