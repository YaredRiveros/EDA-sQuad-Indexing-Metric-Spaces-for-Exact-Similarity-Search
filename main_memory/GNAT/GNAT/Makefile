# Makefile para GNAT con control de altura y arity
# Uso: make run-l HEIGHT=5 DATASET=dataset.txt

CXX = g++
CXXFLAGS = -O3 -std=c++11 -Wall
TARGET = gnat
SOURCES = main.cpp GNAT.cpp db.cpp
OBJECTS = $(SOURCES:.cpp=.o)

# ============================================================================
# CONFIGURACIÓN DE EXPERIMENTOS (Paper Section 6.1)
# ============================================================================

# Parámetros configurables
DATASET ?= dataset.txt
QUERIES ?= queries.txt
HEIGHT ?= 5          # Altura objetivo (MaxHeight)
M ?= 5               # Arity (avg_pivot_cnt) - fijo según paper
QUERY_TYPE ?= range  # Tipo de query: range o knn
RADIUS ?= 100        # Radio para range queries
K ?= 20              # Número de vecinos para knn queries

# Directorios
RESULTS_DIR = resultados
LOGS_DIR = logs
INDEX_DIR = indices

.PHONY: all clean clean-all run-l run-experiments test help info

# ============================================================================
# COMPILACIÓN
# ============================================================================

all: $(TARGET)

$(TARGET): $(OBJECTS)
	$(CXX) $(CXXFLAGS) -o $@ $^
	@echo "✓ Compilación exitosa: $(TARGET)"

%.o: %.cpp
	$(CXX) $(CXXFLAGS) -c $< -o $@

# ============================================================================
# DIRECTORIOS
# ============================================================================

$(RESULTS_DIR) $(LOGS_DIR) $(INDEX_DIR):
	@mkdir -p $@

# ============================================================================
# INFORMACIÓN Y VALIDACIÓN
# ============================================================================

info:
	@echo "=========================================="
	@echo "GNAT - Configuración Actual"
	@echo "=========================================="
	@echo "Dataset:         $(DATASET)"
	@echo "Queries:         $(QUERIES)"
	@echo "Altura máxima:   $(HEIGHT) niveles"
	@echo "Arity (m):       $(M) centros por nodo"
	@echo "Tipo de query:   $(QUERY_TYPE)"
	@if [ "$(QUERY_TYPE)" = "range" ]; then \
		echo "Radio:           $(RADIUS)"; \
	else \
		echo "k vecinos:       $(K)"; \
	fi
	@echo ""
	@echo "Pivotes totales (aprox): m × (m^h - 1) / (m - 1)"
	@TOTAL=$$(echo "$(M) * ($(M)^$(HEIGHT) - 1) / ($(M) - 1)" | bc); \
	echo "                         $$TOTAL centros/pivotes"
	@echo ""
	@echo "Nota: GNAT es un índice HÍBRIDO"
	@echo "      Usa m centros por nivel (no 1 como FQT)"
	@echo "=========================================="

validate-dataset:
	@if [ ! -f "$(DATASET)" ]; then \
		echo "❌ Error: Dataset '$(DATASET)' no encontrado"; \
		echo "Uso: make run-l DATASET=<archivo> HEIGHT=<altura>"; \
		exit 1; \
	fi

# ============================================================================
# EJECUCIÓN CON ALTURA ESPECÍFICA
# ============================================================================

run-l: $(TARGET) validate-dataset | $(RESULTS_DIR) $(LOGS_DIR) $(INDEX_DIR)
	@echo "=========================================="
	@echo "Ejecutando GNAT con HEIGHT=$(HEIGHT), M=$(M)"
	@echo "=========================================="
	@$(MAKE) --no-print-directory info
	@echo ""
	@echo ">>> Construyendo índice..."
	@echo "MaxHeight=$(HEIGHT)" > $(INDEX_DIR)/config_h$(HEIGHT)_m$(M).txt
	@echo "Arity=$(M)" >> $(INDEX_DIR)/config_h$(HEIGHT)_m$(M).txt
	@./$(TARGET) build $(DATASET) \
	             $(INDEX_DIR)/index_h$(HEIGHT)_m$(M).idx \
	             $(HEIGHT) $(M) \
	    | tee $(LOGS_DIR)/build_h$(HEIGHT)_m$(M).log
	@echo ""
	@echo ">>> Ejecutando búsquedas $(QUERY_TYPE)..."
	@if [ -f "$(QUERIES)" ]; then \
		if [ "$(QUERY_TYPE)" = "range" ]; then \
			./$(TARGET) search-range \
			            $(INDEX_DIR)/index_h$(HEIGHT)_m$(M).idx \
			            $(QUERIES) $(RADIUS) \
			    | tee $(LOGS_DIR)/search_h$(HEIGHT)_m$(M).log \
			    > $(RESULTS_DIR)/results_h$(HEIGHT)_m$(M).txt; \
		else \
			./$(TARGET) search-knn \
			            $(INDEX_DIR)/index_h$(HEIGHT)_m$(M).idx \
			            $(QUERIES) $(K) \
			    | tee $(LOGS_DIR)/search_h$(HEIGHT)_m$(M).log \
			    > $(RESULTS_DIR)/results_h$(HEIGHT)_m$(M).txt; \
		fi; \
		echo "✓ Resultados guardados en $(RESULTS_DIR)/results_h$(HEIGHT)_m$(M).txt"; \
	else \
		echo "⚠ Archivo de queries '$(QUERIES)' no encontrado. Solo se construyó el índice."; \
	fi
	@echo ""
	@echo "✓ Experimento completado para HEIGHT=$(HEIGHT), M=$(M)"
	@echo "=========================================="

# ============================================================================
# EJECUCIÓN DE TODOS LOS EXPERIMENTOS (Paper: altura={3,5,10,15,20})
# ============================================================================

run-experiments: $(TARGET) validate-dataset | $(RESULTS_DIR) $(LOGS_DIR) $(INDEX_DIR)
	@echo "=========================================="
	@echo "Ejecutando Experimentos GNAT"
	@echo "Heights: 3, 5, 10, 15, 20 (según paper)"
	@echo "Arity: $(M) (default=5 según paper)"
	@echo "=========================================="
	@echo ""
	@for h in 3 5 10 15 20; do \
		echo ""; \
		echo ">>> Experimento HEIGHT=$$h, M=$(M) <<<"; \
		$(MAKE) --no-print-directory run-l HEIGHT=$$h M=$(M) \
		        DATASET=$(DATASET) QUERIES=$(QUERIES) \
		        QUERY_TYPE=$(QUERY_TYPE) RADIUS=$(RADIUS) K=$(K) || true; \
		echo "✓ Completado HEIGHT=$$h"; \
		echo ""; \
	done
	@echo "=========================================="
	@echo "Todos los experimentos completados"
	@echo "Índices en:    $(INDEX_DIR)/"
	@echo "Resultados en: $(RESULTS_DIR)/"
	@echo "Logs en:       $(LOGS_DIR)/"
	@echo "=========================================="

# ============================================================================
# EXPERIMENTOS VARIANDO M (exploratorio)
# ============================================================================

run-vary-m: $(TARGET) validate-dataset | $(RESULTS_DIR) $(LOGS_DIR) $(INDEX_DIR)
	@echo "=========================================="
	@echo "Experimentos variando M (arity)"
	@echo "HEIGHT fijo: $(HEIGHT)"
	@echo "=========================================="
	@echo ""
	@for m in 3 5 7 10; do \
		echo ""; \
		echo ">>> Experimento M=$$m <<<"; \
		$(MAKE) --no-print-directory run-l HEIGHT=$(HEIGHT) M=$$m \
		        DATASET=$(DATASET) QUERIES=$(QUERIES) \
		        QUERY_TYPE=$(QUERY_TYPE) RADIUS=$(RADIUS) K=$(K) || true; \
		echo "✓ Completado M=$$m"; \
		echo ""; \
	done

# ============================================================================
# EXPERIMENTOS CON MÚLTIPLES DATASETS
# ============================================================================

run-all-datasets: $(TARGET)
	@echo "Ejecutando experimentos con múltiples datasets..."
	@for dataset in LA.txt integer.txt mpeg_1M.txt sf.txt; do \
		if [ -f "$$dataset" ]; then \
			echo ""; \
			echo "=== Dataset: $$dataset ==="; \
			$(MAKE) --no-print-directory run-experiments \
			        DATASET=$$dataset QUERIES=queries_$${dataset%.txt}.txt; \
		fi; \
	done

# ============================================================================
# PRUEBA RÁPIDA
# ============================================================================

test: $(TARGET) validate-dataset
	@echo "Prueba rápida con HEIGHT=$(HEIGHT), M=$(M)..."
	@$(MAKE) --no-print-directory info
	@echo ""
	@echo "Comando que se ejecutará:"
	@echo "./$(TARGET) build $(DATASET) test_index.idx $(HEIGHT) $(M)"
	@echo ""
	@read -p "¿Continuar? (y/n): " confirm; \
	if [ "$$confirm" = "y" ]; then \
		./$(TARGET) build $(DATASET) test_index.idx $(HEIGHT) $(M); \
		rm -f test_index.idx; \
	fi

# ============================================================================
# LIMPIEZA
# ============================================================================

clean:
	rm -f $(OBJECTS) $(TARGET)
	@echo "✓ Archivos objeto y ejecutable eliminados"

clean-all: clean
	rm -rf $(RESULTS_DIR) $(LOGS_DIR) $(INDEX_DIR)
	@echo "✓ Resultados, logs e índices eliminados"

# ============================================================================
# AYUDA
# ============================================================================

help:
	@echo "=========================================="
	@echo "Makefile GNAT - Control de Altura y Arity"
	@echo "=========================================="
	@echo ""
	@echo "COMANDOS PRINCIPALES:"
	@echo "  make                     - Compilar GNAT"
	@echo "  make info                - Mostrar configuración actual"
	@echo "  make run-l HEIGHT=5      - Ejecutar con altura específica"
	@echo "  make run-experiments     - Ejecutar con HEIGHT={3,5,10,15,20}"
	@echo "  make run-vary-m          - Experimentos variando M"
	@echo "  make test HEIGHT=10      - Prueba rápida"
	@echo "  make clean               - Limpiar compilación"
	@echo "  make clean-all           - Limpiar todo (incluye resultados)"
	@echo ""
	@echo "PARÁMETROS CONFIGURABLES:"
	@echo "  DATASET=<archivo>        - Dataset a usar (default: dataset.txt)"
	@echo "  QUERIES=<archivo>        - Archivo de queries (default: queries.txt)"
	@echo "  HEIGHT=<número>          - Altura máxima (default: 5)"
	@echo "  M=<número>               - Arity (centros/nodo) (default: 5)"
	@echo "  QUERY_TYPE=<range|knn>   - Tipo de consulta (default: range)"
	@echo "  RADIUS=<número>          - Radio para range queries (default: 100)"
	@echo "  K=<número>               - k vecinos para knn (default: 20)"
	@echo ""
	@echo "EJEMPLOS:"
	@echo "  # Ejecutar con altura 5 y m=5 (default del paper)"
	@echo "  make run-l HEIGHT=5 DATASET=LA.txt"
	@echo ""
	@echo "  # Ejecutar con altura 5 y m=7 (experimental)"
	@echo "  make run-l HEIGHT=5 M=7 DATASET=LA.txt"
	@echo ""
	@echo "  # Ejecutar todos los experimentos del paper"
	@echo "  make run-experiments DATASET=LA.txt QUERIES=queries_LA.txt"
	@echo ""
	@echo "  # Explorar diferentes valores de m con altura fija"
	@echo "  make run-vary-m HEIGHT=5 DATASET=LA.txt"
	@echo ""
	@echo "  # Ver configuración antes de ejecutar"
	@echo "  make info HEIGHT=10 M=5 DATASET=LA.txt"
	@echo ""
	@echo "  # Consultas knn"
	@echo "  make run-l HEIGHT=5 QUERY_TYPE=knn K=10 DATASET=LA.txt"
	@echo ""
	@echo "NOTAS IMPORTANTES:"
	@echo "  • GNAT es un índice HÍBRIDO (compact-partitioning + pivot-based)"
	@echo "  • Usa M centros por nivel (no 1 como FQT/BST)"
	@echo "  • Pivotes totales ≈ M × (M^HEIGHT - 1) / (M - 1)"
	@echo "  • Paper recomienda M=5 (arity=5)"
	@echo ""
	@echo "VALORES DEL PAPER (Table 6):"
	@echo "  HEIGHT ∈ {3, 5, 10, 15, 20}"
	@echo "  M = 5 (fijo)"
	@echo "=========================================="
