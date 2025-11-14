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
