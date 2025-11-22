# Quick Start Checklist — Benchmarking Workflow

## ✅ Pre-Ejecución

### 1. Verificar Prerequisitos

```bash
# Compilador C++17
g++ --version  # >= 7.0

# Python con pandas
python3 --version
pip3 install pandas openpyxl

# Datasets presentes
ls ../../datasets/*.txt

# Queries y radii precomputados
ls ../../datasets/prepared_experiment/queries/
ls ../../datasets/prepared_experiment/radii/
```

### 2. Permisos

```bash
chmod +x run_all_benchmarks.sh
```

---

## 🚀 Ejecución

### Opción A: Todo Automático (Recomendado)

**Linux/Mac:**
```bash
cd main_memory

# Ejecutar todos los benchmarks (7 estructuras)
./run_all_benchmarks.sh

# Esperar ~20-90 minutos dependiendo de CPU
# BST, LAESA, BKT, MVPT, EPT*, FQT, GNAT
# Revisar progreso en terminal

# Agregar resultados
python3 aggregate_results.py
```

**Windows:**
```powershell
cd main_memory

# Ejecutar todos los benchmarks (7 estructuras)
.\run_all_benchmarks.ps1

# Esperar ~20-90 minutos dependiendo de CPU
# Revisar progreso en terminal

# Agregar resultados
python aggregate_results.py
```

### Opción B: Paso a Paso

```bash
cd main_memory

# 1. BST
cd BST
g++ -O3 -std=gnu++17 test.cpp -o bst_test
./bst_test
cd ..

# 2. LAESA
cd LAESA
g++ -O3 -std=gnu++17 test.cpp -o laesa_test
./laesa_test
cd ..

# 3. BKT
cd BKT
g++ -O3 -std=gnu++17 test.cpp -o bkt_test
./bkt_test
cd ..

# 4. MVPT
cd mvpt
g++ -O3 -std=gnu++17 test.cpp -o mvpt_test -I..
./mvpt_test
cd ..

# 5. EPT*
cd EPT
g++ -O3 -std=gnu++17 test.cpp Interpreter.cpp Objvector.cpp Tuple.cpp -o EPT_test
./EPT_test
cd ..

# 6. FQT (C code + C++ wrapper)
cd FQT
gcc -O3 -c fqt.c -o fqt.o
gcc -O3 -c ../../index.c -o index_fqt.o
gcc -O3 -c ../../bucket.c -o bucket_fqt.o
g++ -O3 -std=gnu++17 test.cpp fqt.o index_fqt.o bucket_fqt.o -o FQT_test
./FQT_test
cd ..

# 7. GNAT
cd GNAT/GNAT
g++ -O3 -std=gnu++17 test.cpp db.cpp GNAT.cpp -o ../../GNAT_test
cd ../..
./GNAT_test

# 8. Agregar resultados
python3 aggregate_results.py
```

---

## 📊 Verificación de Resultados

### Archivos Esperados

```bash
# Por estructura
ls BST/results/results_BST_*.json
ls LAESA/results/results_LAESA_*.json
ls BKT/results/results_BKT_*.json
ls mvpt/results/results_MVPT_*.json

# Consolidados
ls consolidated_results.csv
ls consolidated_results.json
ls summary_MRQ.csv
ls summary_MkNN.csv
```

### Conteo de Experimentos

```bash
# Verificar número de experimentos
wc -l consolidated_results.csv

# Debería ser aprox:
# BST: 4 datasets × 5 heights × (5 MRQ + 5 MkNN) = 200 experimentos
# LAESA: 4 datasets × 5 pivots × 10 queries = 200 experimentos
# BKT: 4 datasets × 5 buckets × 10 queries = 200 experimentos
# MVPT: 4 datasets × 5 buckets × 10 queries = 200 experimentos
# Total: ~800 experimentos
```

---

## 🐛 Troubleshooting Rápido

### Error de Compilación

```bash
# Verificar rutas
ls ../../datasets/paths.hpp
ls ../../objectdb.hpp

# Compilar con includes explícitos
g++ -O3 -std=gnu++17 -I../.. test.cpp -o test
```

### Datasets No Encontrados

```bash
# Verificar estructura
ls ../../datasets/
ls ../../datasets/prepared_experiment/
```

### Python Sin Pandas

```bash
pip3 install pandas
# o
conda install pandas
```

### Espacio en Disco

```bash
# Verificar espacio disponible
df -h .
```

---

## 📈 Análisis Post-Ejecución

### 1. Revisar Resúmenes

```bash
# Ver primeras líneas
head -20 summary_MRQ.csv
head -20 summary_MkNN.csv
```

### 2. Importar a Excel/R/Python

```python
import pandas as pd

# Cargar resultados
df = pd.read_csv('consolidated_results.csv')

# Análisis básico
print(df.groupby('index')['compdists'].mean())
print(df.groupby('index')['time_ms'].mean())

# Filtrar por dataset
la_results = df[df['dataset'] == 'LA']
print(la_results.groupby('index')['compdists'].mean())
```

### 3. Visualizar

```python
import matplotlib.pyplot as plt

# Compdists vs Pivots para LAESA
laesa = df[df['index'] == 'LAESA']
laesa_mrq = laesa[laesa['query_type'] == 'MRQ']

for dataset in ['LA', 'Words', 'Color', 'Synthetic']:
    data = laesa_mrq[laesa_mrq['dataset'] == dataset]
    plt.plot(data['num_pivots'], data['compdists'], label=dataset)

plt.xlabel('Number of Pivots')
plt.ylabel('Average Distance Computations')
plt.title('LAESA: Compdists vs Pivots')
plt.legend()
plt.show()
```

---

## 📝 Documentar Observaciones

Actualizar `COMPARATIVE_ANALYSIS.md` con:

1. **Rendimiento relativo**:
   - ¿Qué estructura tuvo menos compdists?
   - ¿Cuál fue más rápida?

2. **Patrones por dataset**:
   - ¿Qué datasets favorecen CP vs PB?
   - ¿Efecto de dimensionalidad?

3. **Escalabilidad**:
   - ¿Cómo escala con l/h?
   - ¿Trade-offs memoria vs performance?

4. **Recomendaciones**:
   - ¿Cuándo usar cada estructura?
   - ¿Parámetros óptimos?

---

## 🎯 Checklist Final

- [ ] Todos los benchmarks ejecutados sin errores
- [ ] 4 archivos JSON por estructura (uno por dataset)
- [ ] `consolidated_results.csv` generado
- [ ] ~800 experimentos en total
- [ ] Logs revisados para warnings
- [ ] CPU info registrada en `benchmark_system_info.txt`
- [ ] Análisis preliminar completado
- [ ] Gráficos generados
- [ ] Observaciones documentadas

---

## 💡 Tips

1. **Tiempo**: Ejecutar durante la noche si es lento
2. **CPU**: Cerrar otros procesos para timing consistente
3. **Memoria**: LAESA con l=20 puede usar mucha RAM
4. **Backups**: Guardar `results/` antes de re-ejecutar
5. **Logs**: Revisar `*_benchmark.log` para debug

---

## 📞 Si Algo Falla

1. Revisar logs: `cat <estructura>_benchmark.log`
2. Verificar permisos: `ls -la run_all_benchmarks.sh`
3. Compilar manualmente uno por uno
4. Verificar paths en `paths.hpp`
5. Consultar `README.md` sección Troubleshooting

---

**Ready to go!** 🚀

Ejecuta: `./run_all_benchmarks.sh`
