# Pivot & Query Preparation for Metric-Space Experiments

Este módulo genera **todos los insumos experimentales necesarios** para replicar los resultados del paper *"Indexing Metric Spaces for Exact Similarity Search"* (Chen et al.).

El objetivo es **preprocesar cada dataset** para obtener:

* **Pivotes (HFI)** — usados por los índices pivot-based
* **Queries aleatorias** — usadas para range queries y kNN queries
* **Radios calibrados por selectividad** — usados para MRQ
* **Estructura unificada en JSON** — lista para alimentar tu benchmark



> ## ¿Qué hace este script?
> La herramienta procesa automáticamente cada dataset real ("LA", "Words", "Color") y el dataset sintético ("Synthetic"). Para cada dataset, genera:
>
> * pivotes seleccionados con HFI usando la métrica correcta
> * 100 queries aleatorias
> * radios calibrados para las selectividades {2%,4%,8%,16%,32%}
>
> Los archivos producidos son índices dentro del dataset (no vectores), y se guardan en formato JSON estandarizado.
>
> Estos resultados son usados posteriormente por todos los índices metric-space implementados en este repositorio.

---

# 1. **Selección de pivotes (HFI)**

Para cada dataset se ejecuta el algoritmo **Heuristic Furthest-Point Incremental (HFI)**, usando **su métrica correcta**:

| Dataset   | Métrica usada |
| --------- | ------------- |
| LA        | L2-norm       |
| Words     | Edit distance |
| Color     | L1-norm       |
| Synthetic | L∞-norm       |

Y se generan archivos JSON con los pivotes seleccionados para:

```
π ∈ {3, 5, 10, 15, 20}
```

Esto es crucial porque el paper **compara índices pivot-based usando el mismo conjunto de pivotes HFI**.

Salida ejemplo:

```
prepared_experiment/pivots/LA_pivots_5.json
```

Contenido típico:

```json
[1234, 98221, 55300, 11, 701991]
```

Son **índices de objetos del dataset**, no coordenadas.

## Explicación conceptual 
>En los índices basados en pivotes, seleccionar buenos pivotes es esencial para reducir el número de distancias. El paper utiliza HFI porque produce pivotes: muy dispersos, representativos del espacio, y con excelente poder de poda.
>
> **¿Qué intenta lograr HFI?**
> Obtener un conjunto de pivotes que:
> - cubran el espacio lo mejor posible,
> - estén alejados entre sí,
> - aumenten la eficiencia de poda.
>
> **Idea del algoritmo HFI**
> 1. El primer pivote es el primer objeto del dataset (índice 0).
> 2. Para cada nuevo pivote:Se calcula, para cada objeto, la suma de distancias hacia los pivotes ya elegidos.
> 3. Se elige el objeto cuya suma sea máxima.
> 4. Se repite hasta obtener π pivotes.
>    Código conceptual:
>
>    ```python
>    suma_de_distancias(objeto, pivotes ya elegidos)
>    ```
> 
> 
> Y se elige como nuevo pivote el objeto que maximiza esa suma.
> 
> Es decir: *“El siguiente pivote es el objeto que está globalmente más lejos de todos los pivotes previos”.*
> Se repite hasta tener π pivotes (por defecto π = 5).
> **¿Por qué funciona bien?**
> 
> Porque la suma de distancias aproxima la diversidad del conjunto, forzando que: *los pivotes estén alejados entre sí*, representen regiones distintas del espacio, permitan buenas cotas triangulares.
>
> **¿Qué retorna HFI?**
>
> Retorna una lista de índices dentro del dataset. No retorna coordenadas; solo posiciones.
---

# 2. **Selección de 100 queries aleatorias**

> Según el paper, para todas las evaluaciones: *"Each reported measurement is an average over 100 random queries. To facilitate a fair comparison, we use the same set of random queries for all indexes*
> 
> Esto garantiza: reproducibilidad, comparabilidad entre estructuras, igualdad de condiciones experimentales.
>
> ## ¿Qué representan esas 100 queries?
> 
> Son 100 objetos del dataset seleccionados al azar. Ejemplo:
>  Para cada dataset se eligen **100 objetos** que servirán como queries para:
>
> * **Range Query (MRQ):** Para cada query, se calcula su radio según selectividad.
> * **k-Nearest Neighbors (MkNN):** Para cada query, se busca sus k-vecinos más cercanos.

> ## Razones por las que 100 queries son suficientes
> 1. Para estimar estadísticamente el número de distancias, page accesses, tiempo promedio
> 2. Reduce la variancia
> 3. se usa exactamente el mismo conjunto en TODAS las pruebas
> ---
> **Por eso, guardar estas queries es indispensable.**


Salida:

```
prepared_experiment/queries/LA_queries.json
```

Ejemplo:

```json
[10023, 501991, 92311, ...]
```

Nuevamente, **son índices del dataset**.

---

# 3. **Metric Range Queries (MRQ)**

> **Cálculo de radios para selectividades**
> El paper define los range queries en términos de **selectividad**:
>   ```
>       {2%, 4%, 8%, 16%, 32%}
>   ```


Para calcular el radio correspondiente a cada selectividad, se sigue el mismo procedimiento descrito en el paper:

1. Para cada una de las 100 queries, se calculan todas las distancias entre la query y cada objeto del dataset.

2. Se obtiene el percentil asociado a la selectividad (por ejemplo, el percentil 8% para selectividad 0.08).

3. Este proceso se repite para todas las queries.

4. El radio final para cada selectividad es el promedio de los 100 radios individuales.

5. Cada dataset produce un archivo JSON con los radios oficiales a utilizar en todas las ejecuciones de MRQ.

Salida:

```
prepared_experiment/radii/LA_radii.json
```

Ejemplo:

```json
{
  "0.02": 0.00091,
  "0.04": 0.00192,
  "0.08": 0.00388,
  "0.16": 0.00721,
  "0.32": 0.01344
}
```


**Estos radios deben usarse exactamente como parámetros de los MRQ para replicar la metodología experimental del paper.**
> [!NOTE]
> **Observación importante sobre los radios promedio**
>
> Al promediar los radios calculados para cada query, la cantidad exacta de resultados devueltos por un *range query* puede no coincidir exactamente con la selectividad objetivo (2%, 4%, 8%, etc.).
>
> Esto se debe a variaciones naturales en la densidad local del dataset y al hecho de que *el promedio de percentiles individuales no es igual al percentil del conjunto*.
>
> Aun así, este procedimiento:
> - replica **exactamente** la metodología del paper,
> - produce radios consistentes para todas las estructuras,
> - garantiza comparabilidad entre índices,
> - y modela un escenario realista donde el radio es fijo por dataset.
>
> Lo relevante en la evaluación es el desempeño **promedio sobre las 100 queries**, no la coincidencia exacta en cada query individual.
----

# 4. **k-Nearest Neighbor Queries (MkNNQ)**

> Los kNN queries utilizan el mismo conjunto fijo de 100 queries:  
> 1. Para cada query q, se solicita encontrar sus k vecinos más cercanos según la métrica del dataset.
> 2. Los valores de k evaluados, siguiendo el paper, son:
>   ```
> k ∈ {5, 10, 20, 50, 100}
>   ```

**Durante los experimentos: cada índice ejecuta MkNN sobre las mismas queries y valores de k, se registran las métricas de rendimiento (distancias, páginas accedidas y tiempo), los resultados se promedian sobre las 100 consultas. Esto asegura una comparación estandarizada entre todos los métodos.**

# ¿Por qué es necesario este preprocesamiento?

Porque el paper exige:

* pivotes comunes (HFI) para todos los índices pivot-based
* queries fijas (si no, los resultados no son reproducibles)
* radios calibrados por selectividad, no arbitrarios
* métricas específicas por dataset

Con este módulo generas **todo lo que un framework de evaluación** necesita antes de medir:

* `compdists`
* `page_accesses`
* `time_ms`

Y puedes alimentar fácilmente tu benchmark.

---

# 📦 Estructura final generada

```
prepared_experiment/
├── pivots/
│   ├── LA_pivots_3.json
│   ├── LA_pivots_5.json
│   ├── Words_pivots_5.json
│   └── ...
│
├── queries/
│   ├── LA_queries.json
│   ├── Words_queries.json
│
├── radii/
│   ├── LA_radii.json
│   ├── Color_radii.json
│   └── Synthetic_radii.json
```

Todo está organizado por dataset y por tipo.

---

# ⚙️ Ejecución automática

El script detecta automáticamente si un dataset existe:

```python
DATASET_PATHS = {
    "LA": "/ruta/LA.txt",
    "Words": "/ruta/Words.txt",
    "Color": "/ruta/Color.txt",
    "Synthetic": "/ruta/Synthetic.txt"
}
```

Si un dataset no se encuentra → se omite sin error.



