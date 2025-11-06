#ifndef LAESA_HPP
#define LAESA_HPP

#include "../objectdb.hpp"
#include <vector>
#include <queue>
#include <algorithm>
#include <cmath>
#include <limits>
using namespace std;

/**
 * Estructura para resultados de búsqueda k-NN
 */
struct ResultElem {
    int id;
    double dist;
    bool operator<(const ResultElem &o) const { return dist < o.dist; }
};

/**
 * Implementación del índice LAESA (Linear Approximating Eliminating Search Algorithm)
 * 
 * LAESA es un índice métrico que utiliza pivotes para acelerar búsquedas por similitud.
 * Características principales:
 * - Precalcula distancias entre todos los objetos y un conjunto de pivotes
 * - Usa desigualdad triangular para filtrar objetos sin calcular distancia real
 * - Trade-off: más pivotes = mejor filtrado pero más memoria
 */
class LAESA {
    ObjectDB *db;           // Base de datos de objetos
    int nPivots;            // Número de pivotes utilizados
    vector<int> pivots;     // IDs de los pivotes (primeros nPivots objetos)
    vector<vector<double>> distMatrix;  // Matriz de distancias precalculadas [nObjects x nPivots]
                                        // distMatrix[i][j] = distancia del objeto i al pivote j

public:
    /**
     * Constructor: construye el índice LAESA
     * 
     * @param db Puntero a la base de datos de objetos
     * @param nPivots Número de pivotes a utilizar
     *                Recomendación: entre sqrt(n) y 0.1*n, donde n = número de objetos
     */
    LAESA(ObjectDB *db, int nPivots);

    /**
     * Búsqueda por rango: encuentra todos los objetos a distancia <= radius del query
     * 
     * @param queryId ID del objeto query
     * @param radius Radio de búsqueda
     * @param result Vector donde se almacenarán los IDs de objetos encontrados
     */
    void rangeSearch(int queryId, double radius, vector<int> &result) const;

    /**
     * Búsqueda de k vecinos más cercanos
     * 
     * @param queryId ID del objeto query
     * @param k Número de vecinos a buscar
     * @param out Vector donde se almacenarán los k vecinos (ordenados por distancia)
     */
    void knnSearch(int queryId, int k, vector<ResultElem> &out) const;

private:
    /**
     * Calcula la cota inferior de distancia usando desigualdad triangular
     * 
     * Para cada pivote p: |d(q,p) - d(o,p)| <= d(q,o)
     * Retorna el máximo de todas las diferencias (cota inferior más ajustada)
     * 
     * @param queryDists Distancias del query a los pivotes
     * @param objectIdx Índice del objeto candidato
     * @return Cota inferior de la distancia d(query, objeto)
     */
    double lowerBound(const vector<double> &queryDists, int objectIdx) const;
};

LAESA::LAESA(ObjectDB *db, int nPivots) 
    : db(db), nPivots(nPivots)
{
    int n = db->size();
    if (nPivots > n) nPivots = n;
    this->nPivots = nPivots;

    // Paso 1: Seleccionar pivotes (los primeros nPivots objetos)
    pivots.resize(nPivots);
    for (int i = 0; i < nPivots; i++) {
        pivots[i] = i;
    }

    // Paso 2: Precalcular matriz de distancias (n × nPivots)
    distMatrix.resize(n, vector<double>(nPivots));
    for (int i = 0; i < n; i++) {
        for (int j = 0; j < nPivots; j++) {
            distMatrix[i][j] = db->distance(i, pivots[j]);
        }
    }

    cerr << "[LAESA] Index built with " << nPivots << " pivots ("
         << (n * nPivots * sizeof(double)) / (1024.0 * 1024.0)
         << " MB precalculated distances)\n";
}

double LAESA::lowerBound(const vector<double> &queryDists, int objectIdx) const 
{
    double maxDiff = 0.0;
    for (int j = 0; j < nPivots; j++) {
        double diff = fabs(queryDists[j] - distMatrix[objectIdx][j]);
        maxDiff = max(maxDiff, diff);
    }
    return maxDiff;
}

void LAESA::rangeSearch(int queryId, double radius, vector<int> &result) const 
{
    int n = db->size();
    
    // Paso 1: Calcular distancias del query a los pivotes
    vector<double> queryDists(nPivots);
    for (int j = 0; j < nPivots; j++) {
        queryDists[j] = db->distance(queryId, pivots[j]);
        // Verificar si el pivote está en el rango
        if (queryDists[j] <= radius) {
            result.push_back(pivots[j]);
        }
    }

    // Paso 2: Filtrar y verificar objetos no-pivotes
    for (int i = 0; i < n; i++) {
        // Saltar si es un pivote (ya lo verificamos)
        bool isPivot = false;
        for (int p : pivots) {
            if (i == p) {
                isPivot = true;
                break;
            }
        }
        if (isPivot) continue;

        // Usar cota inferior para filtrar
        double lb = lowerBound(queryDists, i);
        if (lb <= radius) {
            // El objeto puede estar en el rango, calcular distancia real
            double d = db->distance(queryId, i);
            if (d <= radius) {
                result.push_back(i);
            }
        }
        // Si lb > radius, el objeto queda descartado sin calcular distancia
    }
}

void LAESA::knnSearch(int queryId, int k, vector<ResultElem> &out) const 
{
    int n = db->size();
    priority_queue<ResultElem> pq;  // Max-heap para mantener k mejores
    
    // Paso 1: Calcular distancias del query a los pivotes y añadirlos
    vector<double> queryDists(nPivots);
    for (int j = 0; j < nPivots; j++) {
        queryDists[j] = db->distance(queryId, pivots[j]);
        if ((int)pq.size() < k) {
            pq.push({pivots[j], queryDists[j]});
        } else if (queryDists[j] < pq.top().dist) {
            pq.pop();
            pq.push({pivots[j], queryDists[j]});
        }
    }

    double tau = pq.size() == k ? pq.top().dist : numeric_limits<double>::infinity();

    // Paso 2: Calcular distancias L1 para ordenar candidatos (heurística)
    vector<pair<double, int>> candidates;  // (distancia L1, objeto ID)
    
    for (int i = 0; i < n; i++) {
        // Saltar si es un pivote
        bool isPivot = false;
        for (int p : pivots) {
            if (i == p) {
                isPivot = true;
                break;
            }
        }
        if (isPivot) continue;

        // Calcular distancia L1 como heurística de proximidad
        double dist1 = 0.0;
        for (int j = 0; j < nPivots; j++) {
            dist1 += fabs(queryDists[j] - distMatrix[i][j]);
        }
        candidates.push_back({dist1, i});
    }

    // Paso 3: Ordenar candidatos por distancia L1 (procesar primero los más prometedores)
    sort(candidates.begin(), candidates.end());

    // Paso 4: Procesar candidatos en orden con filtrado
    for (const auto &cand : candidates) {
        int i = cand.second;
        
        // Usar cota inferior para filtrar
        double lb = lowerBound(queryDists, i);
        if (lb <= tau || (int)pq.size() < k) {
            // Calcular distancia real
            double d = db->distance(queryId, i);
            if ((int)pq.size() < k) {
                pq.push({i, d});
                if ((int)pq.size() == k) tau = pq.top().dist;
            } else if (d < pq.top().dist) {
                pq.pop();
                pq.push({i, d});
                tau = pq.top().dist;
            }
        }
        // Si lb > tau, el objeto queda descartado
    }

    // Paso 5: Extraer resultados y ordenar
    while (!pq.empty()) {
        out.push_back(pq.top());
        pq.pop();
    }
    reverse(out.begin(), out.end());  // Ordenar de menor a mayor distancia
}

#endif
