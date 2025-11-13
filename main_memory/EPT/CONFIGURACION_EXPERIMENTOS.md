# Configuración de EPT* para Experimentos

## Descripción General

EPT* (Extreme Pivot Table) es un índice basado en pivotes que selecciona **pivotes diferentes para cada objeto** usando el Pivot Selection Algorithm (PSA).

**Paper Reference:** Section 5.8 - EPT

---

## Parámetros de Configuración

### 1. **Número de Pivotes por Objeto (l)**

**Variable:** `LGroup`

**Valores recomendados por el paper:**
```
l ∈ {3, 5, 10, 15, 20}
Valor por defecto: 5
```

**Configuración en el código:**

```cpp
// main.cpp - función main()
int pn = atoi(argv[5]);  // Leer número de pivotes desde argumentos
LGroup = pn;             // Asignar a LGroup
```

**Uso desde línea de comandos:**

```bash
# Compilar (Ubuntu Linux)
g++ -O3 -std=c++11 -o ept main.cpp Interpreter.cpp Objvector.cpp Tuple.cpp Cache.cpp

# Verificar compilación
./ept

# Ejecutar con diferentes valores de l
# Sintaxis: programa <dataset> <indexfile> <outputfile> <queryfile> <num_pivotes>
./ept dataset.txt index.bin results.txt queries.txt 3   # l=3
./ept dataset.txt index.bin results.txt queries.txt 5   # l=5 (default)
./ept dataset.txt index.bin results.txt queries.txt 10  # l=10
./ept dataset.txt index.bin results.txt queries.txt 15  # l=15
./ept dataset.txt index.bin results.txt queries.txt 20  # l=20
```

---

### 2. **Número de Pivotes Candidatos (l_c)**

**Variable:** `num_cand` (definido como macro)

**Valor actual:**
```cpp
#define num_cand 40  // Fijo en el código
```

**Paper:** "l_c is the number of candidate pivots"

**⚠️ Nota:** Este valor está hardcodeado. Para cambiar:
```cpp
// En main.cpp, modificar línea:
#define num_cand 40  // Cambiar según necesidad
```

---

### 3. **Tamaño del Conjunto de Muestra (n_s)**

**Cálculo automático:**
```cpp
int sampleSize = pi->num / 200;  // n_s = n/200
```

**Paper:** "n_s denotes the cardinality of the sample set"

**Fórmula:** `n_s = n / 200` donde `n` es el tamaño total del dataset.

---

## Características Especiales de EPT*

### ⚠️ Excepciones en Comparación Justa

**Del paper:**
> "We utilize the same pivot selection strategy for the pivot-based indexes... **This does, however, not apply to the EPT***, BKT, and GNAT families."

**Razón:** EPT* usa su propio **Pivot Selection Algorithm (PSA)**, no el algoritmo HFI usado por otros índices.

**Implicaciones:**
- ✅ EPT* mantiene su algoritmo PSA original
- ✅ Selecciona diferentes pivotes para cada objeto
- ✅ Usa `l_c=40` candidatos y selecciona los mejores `l` para cada objeto

---

## Consultas de Rango (MRQ)

**Parámetro:** Radio de búsqueda basado en **selectividad**

**Valores del paper:**
```
Selectividad ∈ {2%, 4%, 8%, 16%, 32%}
Default: 8%
```

**Nota:** El radio específico depende del dataset y debe calcularse previamente para lograr la selectividad deseada.

**Configuración en código:**
```cpp
// main.cpp - función rangeQuery()
int rangeQuery(float* q, double radius, int qj) {
    // radius debe ser calculado para lograr la selectividad deseada
    // ...
}
```

---

## Consultas k-NN (MkNNQ)

**Parámetro:** Número de vecinos más cercanos (k)

**Valores del paper:**
```
k ∈ {5, 10, 20, 50, 100}
Default: 20
```

**Configuración en código:**
```cpp
// main.cpp - bucle de k-NN
int kvalues[] = { 1, 5, 10, 20, 50, 100 };
for (int k = 0; k < 6; k++) {
    KNNQuery(obj, kvalues[k]);
}
```

---

## Métricas de Evaluación

### 1. **Número de Cálculos de Distancia**
```cpp
double compDists = 0;  // Variable global que cuenta distancias
```

### 2. **Tiempo de Construcción**
```cpp
clock_t buildEnd = clock() - begin;
fprintf(f, "finished... %f build time\n", (double)buildEnd / CLOCKS_PER_SEC);
```

### 3. **Tiempo de Consulta**
```cpp
clock_t times = clock() - start;
fprintf(f, "finished... %f query time\n", (double)times / CLOCKS_PER_SEC / qcount);
```

---

## Complejidades (Referencia del Paper)

### Construcción
**Costo:** `O(n * l * l_c * n_s)`

Donde:
- `n` = número total de objetos
- `l` = número de pivotes por objeto (LGroup)
- `l_c` = número de pivotes candidatos (num_cand = 40)
- `n_s` = tamaño del conjunto de muestra (n/200)

**Observación:** "The pivot selection is costly, and thus, the construction cost of EPT* is very high"

### Almacenamiento
**Costo:** `O(n*s + n*l + l_c*s)`

Donde:
- `s` = tamaño de un objeto

---

## Ejemplo de Script de Experimentos

```bash
#!/bin/bash
# experimentos_ept.sh

# Configuración
datasets=("LA.txt" "integer.txt" "mpeg_1M.txt" "sf.txt")
pivotes=(3 5 10 15 20)
kvalues=(5 10 20 50 100)

# Crear directorio para resultados
mkdir -p resultados

# Compilar
echo "Compilando EPT*..."
g++ -O3 -std=c++11 -o ept main.cpp Interpreter.cpp Objvector.cpp Tuple.cpp Cache.cpp

if [ $? -ne 0 ]; then
    echo "Error en compilación"
    exit 1
fi

# Ejecutar experimentos
for dataset in "${datasets[@]}"; do
    for l in "${pivotes[@]}"; do
        echo "========================================"
        echo "Ejecutando EPT* con dataset=$dataset, l=$l"
        echo "========================================"
        
        index_file="resultados/index_${dataset%.txt}_l${l}.bin"
        output_file="resultados/results_${dataset%.txt}_l${l}.txt"
        
        # Construcción del índice y consultas
        ./ept "$dataset" "$index_file" "$output_file" "queries.txt" $l
        
        if [ $? -eq 0 ]; then
            echo "✓ Completado: $dataset con l=$l"
        else
            echo "✗ Error en: $dataset con l=$l"
        fi
        echo ""
    done
done

echo "Todos los experimentos completados"
echo "Resultados guardados en: ./resultados/"
```

**Hacer ejecutable el script:**

```bash
chmod +x experimentos_ept.sh
./experimentos_ept.sh
```

### Alternativa: Usar Makefile (Recomendado)

El Makefile mejorado proporciona una forma más sencilla de ejecutar experimentos:

```bash
# Ejecutar todos los experimentos (L=3,5,10,15,20)
make run-experiments DATASET=LA.txt QUERIES=queries.txt

# O con múltiples datasets
for dataset in LA.txt integer.txt mpeg_1M.txt sf.txt; do
    echo "Procesando $dataset..."
    make run-experiments DATASET=$dataset QUERIES=queries_${dataset%.txt}.txt
done
```

**Ventajas del Makefile:**
- ✅ Organiza automáticamente resultados y logs
- ✅ Permite ejecutar experimentos individuales: `make run L=10`
- ✅ Fácil de usar: `make help` muestra todas las opciones
- ✅ Control de errores integrado

---

## Checklist de Configuración

- [ ] Compilar EPT* sin errores
- [ ] Preparar datasets con formato correcto
- [ ] Preparar archivo de queries (100 queries aleatorias)
- [ ] Configurar valores de `l` ∈ {3, 5, 10, 15, 20}
- [ ] Calcular radios para selectividades {2%, 4%, 8%, 16%, 32%}
- [ ] Ejecutar experimentos para cada combinación de parámetros
- [ ] Recolectar métricas: compdists, tiempo construcción, tiempo consulta
- [ ] Promediar resultados sobre 100 queries
- [ ] Comparar con otros índices usando el **mismo conjunto de queries**

---

## Notas Importantes

### 1. **Pivotes Diferentes por Objeto**
```cpp
// EPT* selecciona l pivotes DIFERENTES para cada objeto
// G[objId * LGroup + i] almacena el i-ésimo pivote del objeto objId
```

### 2. **Algoritmo PSA**
```cpp
// MaxPrunning() selecciona l_c=40 candidatos
// PivotSelect() selecciona los mejores l para cada objeto
```

### 3. **Comparación con LAESA**
- **LAESA:** Usa los mismos l pivotes para todos los objetos
- **EPT*:** Usa l pivotes diferentes para cada objeto
- **Ventaja:** Mejor capacidad de poda
- **Desventaja:** Construcción más costosa

---

---

## Preparación del Entorno en Ubuntu Linux

### Instalación de Dependencias

```bash
# Actualizar paquetes
sudo apt update

# Instalar compilador C++ y herramientas
sudo apt install -y build-essential g++ make

# Verificar instalación
g++ --version
```

### Makefile para Compilación y Experimentos

Crear un archivo `Makefile` en el directorio EPT/EPT:

```makefile
# Makefile para EPT* con soporte de experimentos

CXX = g++
CXXFLAGS = -O3 -std=c++11 -Wall
TARGET = ept
SOURCES = main.cpp Interpreter.cpp Objvector.cpp Tuple.cpp Cache.cpp
OBJECTS = $(SOURCES:.cpp=.o)

# Configuración de experimentos (valores del paper)
DATASET ?= dataset.txt
QUERIES ?= queries.txt
L ?= 5  # Número de pivotes (default=5)

# Directorios
RESULTS_DIR = resultados
LOGS_DIR = logs

.PHONY: all clean run-experiments test-l help

all: $(TARGET)

$(TARGET): $(OBJECTS)
	$(CXX) $(CXXFLAGS) -o $@ $^
	@echo "✓ Compilación exitosa: $(TARGET)"

%.o: %.cpp
	$(CXX) $(CXXFLAGS) -c $< -o $@

# Crear directorios necesarios
$(RESULTS_DIR) $(LOGS_DIR):
	mkdir -p $@

# Ejecutar con un valor específico de L
run: $(TARGET) | $(RESULTS_DIR) $(LOGS_DIR)
	@echo "Ejecutando EPT* con L=$(L)..."
	./$(TARGET) $(DATASET) \
	            $(RESULTS_DIR)/index_l$(L).bin \
	            $(RESULTS_DIR)/results_l$(L).txt \
	            $(QUERIES) \
	            $(L) \
	    | tee $(LOGS_DIR)/log_l$(L).txt

# Ejecutar experimentos con todos los valores de L del paper
run-experiments: $(TARGET) | $(RESULTS_DIR) $(LOGS_DIR)
	@echo "=========================================="
	@echo "Ejecutando experimentos EPT* (L=3,5,10,15,20)"
	@echo "=========================================="
	@for l in 3 5 10 15 20; do \
	    echo ""; \
	    echo ">>> Experimento con L=$$l <<<"; \
	    $(MAKE) run L=$$l DATASET=$(DATASET) QUERIES=$(QUERIES); \
	    echo "✓ Completado L=$$l"; \
	    echo ""; \
	done
	@echo "=========================================="
	@echo "Todos los experimentos completados"
	@echo "Resultados en: $(RESULTS_DIR)/"
	@echo "Logs en: $(LOGS_DIR)/"

# Probar un valor específico de L
test-l: $(TARGET) | $(RESULTS_DIR) $(LOGS_DIR)
	@echo "Probando EPT* con L=$(L)..."
	@if [ -z "$(L)" ]; then \
	    echo "Error: Especifica L=<valor>"; \
	    exit 1; \
	fi
	$(MAKE) run L=$(L) DATASET=$(DATASET) QUERIES=$(QUERIES)

# Limpiar archivos compilados
clean:
	rm -f $(OBJECTS) $(TARGET)
	@echo "✓ Archivos objeto y ejecutable eliminados"

# Limpiar todo (incluye resultados)
clean-all: clean
	rm -rf $(RESULTS_DIR) $(LOGS_DIR)
	@echo "✓ Resultados y logs eliminados"

# Ayuda
help:
	@echo "Makefile para EPT* - Comparación Justa según Paper"
	@echo ""
	@echo "Uso:"
	@echo "  make                  - Compilar EPT*"
	@echo "  make run L=5          - Ejecutar con L=5 pivotes"
	@echo "  make run-experiments  - Ejecutar con L={3,5,10,15,20}"
	@echo "  make test-l L=10      - Probar con L=10"
	@echo "  make clean            - Limpiar compilación"
	@echo "  make clean-all        - Limpiar todo (incluye resultados)"
	@echo ""
	@echo "Parámetros configurables:"
	@echo "  L=<valor>            - Número de pivotes (default: 5)"
	@echo "  DATASET=<archivo>    - Dataset a usar (default: dataset.txt)"
	@echo "  QUERIES=<archivo>    - Queries a usar (default: queries.txt)"
	@echo ""
	@echo "Valores de L según paper: {3, 5, 10, 15, 20}"
	@echo ""
	@echo "Ejemplo:"
	@echo "  make run-experiments DATASET=LA.txt QUERIES=queries_LA.txt"
```

**Uso Básico:**

```bash
# Compilar
make

# Ver ayuda
make help

# Compilar y ejecutar con L=5 (default)
make run

# Ejecutar con L=10
make run L=10

# Ejecutar todos los experimentos del paper (L=3,5,10,15,20)
make run-experiments

# Limpiar compilación
make clean

# Limpiar todo (incluye resultados)
make clean-all
```

**Uso Avanzado:**

```bash
# Ejecutar experimentos con dataset específico
make run-experiments DATASET=LA.txt QUERIES=queries_LA.txt

# Probar con valor específico de L
make test-l L=15 DATASET=mpeg_1M.txt

# Ejecutar múltiples datasets
for dataset in LA.txt integer.txt mpeg_1M.txt sf.txt; do
    make run-experiments DATASET=$dataset QUERIES=queries_${dataset}
done
```

---

## Ejemplos Prácticos Completos

### Ejemplo 1: Experimento Rápido con L=5

```bash
cd EPT/EPT/

# Compilar
make

# Ejecutar con L=5 (valor por defecto del paper)
make run DATASET=LA.txt QUERIES=queries_LA.txt

# Resultados en:
# - resultados/index_l5.bin
# - resultados/results_l5.txt
# - logs/log_l5.txt
```

### Ejemplo 2: Comparación Completa (Todos los valores de L)

```bash
cd EPT/EPT/

# Ejecutar experimentos con L={3,5,10,15,20}
make run-experiments DATASET=LA.txt QUERIES=queries_LA.txt

# Salida esperada:
# ==========================================
# Ejecutando experimentos EPT* (L=3,5,10,15,20)
# ==========================================
# 
# >>> Experimento con L=3 <<<
# Ejecutando EPT* con L=3...
# ✓ Completado L=3
# 
# >>> Experimento con L=5 <<<
# Ejecutando EPT* con L=5...
# ✓ Completado L=5
# ...

# Resultados organizados:
ls resultados/
# index_l3.bin   results_l3.txt
# index_l5.bin   results_l5.txt
# index_l10.bin  results_l10.txt
# index_l15.bin  results_l15.txt
# index_l20.bin  results_l20.txt

ls logs/
# log_l3.txt  log_l5.txt  log_l10.txt  log_l15.txt  log_l20.txt
```

### Ejemplo 3: Experimentos con Múltiples Datasets

```bash
cd EPT/EPT/

# Script para procesar todos los datasets del paper
for dataset in LA.txt integer.txt mpeg_1M.txt sf.txt; do
    echo "====================================="
    echo "Procesando dataset: $dataset"
    echo "====================================="
    
    # Extraer nombre base
    base_name=${dataset%.txt}
    
    # Ejecutar experimentos
    make run-experiments \
        DATASET=$dataset \
        QUERIES=queries_${base_name}.txt
    
    # Renombrar resultados para incluir nombre del dataset
    mkdir -p resultados/${base_name}
    mv resultados/index_l*.bin resultados/${base_name}/
    mv resultados/results_l*.txt resultados/${base_name}/
    mv logs/log_l*.txt logs/${base_name}_
    
    echo "✓ Completado $dataset"
    echo ""
done

echo "Todos los datasets procesados"
```

### Ejemplo 4: Verificar Configuración Antes de Experimentos Largos

```bash
cd EPT/EPT/

# Probar con L=3 en un dataset pequeño primero
make test-l L=3 DATASET=test_small.txt QUERIES=queries_test.txt

# Si funciona correctamente, ejecutar experimentos completos
if [ $? -eq 0 ]; then
    echo "✓ Prueba exitosa, ejecutando experimentos completos..."
    make run-experiments DATASET=LA.txt QUERIES=queries_LA.txt
else
    echo "✗ Error en prueba, revisar configuración"
fi
```

### Ejemplo 5: Análisis de Resultados

```bash
# Después de ejecutar experimentos, analizar resultados

# Ver resumen de tiempos de construcción para cada L
echo "=== Tiempos de Construcción ==="
for l in 3 5 10 15 20; do
    echo -n "L=$l: "
    grep "Tiempo de construcción" logs/log_l${l}.txt
done

# Ver resumen de cálculos de distancia
echo ""
echo "=== Cálculos de Distancia ==="
for l in 3 5 10 15 20; do
    echo -n "L=$l: "
    grep "Distance calculations" logs/log_l${l}.txt
done

# Comparar tamaños de índices
echo ""
echo "=== Tamaños de Índices ==="
ls -lh resultados/index_l*.bin | awk '{print $9, $5}'
```

### Ejemplo 6: Limpieza y Re-ejecución

```bash
cd EPT/EPT/

# Limpiar resultados anteriores pero mantener ejecutable
rm -rf resultados/ logs/

# O limpiar todo (incluye compilación)
make clean-all

# Recompilar y ejecutar
make && make run-experiments DATASET=LA.txt QUERIES=queries_LA.txt
```

---

## Estructura de Archivos Generada

Después de ejecutar `make run-experiments`, la estructura será:

```
EPT/EPT/
├── main.cpp
├── Interpreter.cpp
├── Objvector.cpp
├── Tuple.cpp
├── Cache.cpp
├── Makefile
├── ept                          # Ejecutable compilado
├── resultados/                  # Resultados de experimentos
│   ├── index_l3.bin            # Índice con L=3
│   ├── results_l3.txt          # Resultados con L=3
│   ├── index_l5.bin
│   ├── results_l5.txt
│   ├── index_l10.bin
│   ├── results_l10.txt
│   ├── index_l15.bin
│   ├── results_l15.txt
│   ├── index_l20.bin
│   └── results_l20.txt
└── logs/                        # Logs de ejecución
    ├── log_l3.txt
    ├── log_l5.txt
    ├── log_l10.txt
    ├── log_l15.txt
    └── log_l20.txt
```

---

## Resumen de Comandos Útiles

| Comando | Descripción |
|---------|-------------|
| `make` | Compilar EPT* |
| `make help` | Ver ayuda completa |
| `make run L=5` | Ejecutar con L=5 |
| `make run-experiments` | Ejecutar con L={3,5,10,15,20} |
| `make test-l L=10` | Probar con L=10 |
| `make clean` | Limpiar compilación |
| `make clean-all` | Limpiar todo |

---

## Referencias

**Paper:** Section 5.8 - Extreme Pivot Table (EPT)

**Citas clave:**
- "EPT selects different pivots for different objects in order to achieve better search performance"
- "EPT* is equipped with a new pivot selection algorithm (PSA)"
- "EPT* achieves better similarity search performance than EPT contributed by the higher quality pivots selected by PSA"
- "The pivot selection is costly, and thus, the construction cost of EPT* is very high"
