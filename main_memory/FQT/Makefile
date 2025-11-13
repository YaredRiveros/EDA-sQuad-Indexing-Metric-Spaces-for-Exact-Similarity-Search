# Makefile para FQT con cálculo automático de bsize
# Uso: make run-l HEIGHT=5 DATASET=dataset.bin

CC = gcc
CFLAGS = -O3 -Wall -lm
TARGET = fqt
SOURCES = fqt.c basics.c
OBJECTS = $(SOURCES:.c=.o)

# ============================================================================
# CONFIGURACIÓN DE EXPERIMENTOS (Paper Section 6.1)
# ============================================================================

# Parámetros configurables
DATASET ?= dataset.bin
QUERIES ?= queries.bin
HEIGHT ?= 5          # Altura objetivo (l = número de pivotes)
ARITY ?= 5           # Arity fijo según paper
SHOW ?= 0            # Mostrar resultados (0=no, 1=sí)

# Directorios
RESULTS_DIR = resultados
LOGS_DIR = logs
INDEX_DIR = indices

# ============================================================================
# CÁLCULO AUTOMÁTICO DE BSIZE
# ============================================================================

# Script inline para calcular bsize basado en altura objetivo
define CALC_BSIZE
$(shell \
	if [ ! -f "$(DATASET)" ]; then \
		echo "ERROR: Dataset $(DATASET) no encontrado" >&2; \
		echo "1"; \
	else \
		N=$$(wc -c < $(DATASET) | awk '{print int($$1/32)}'); \
		if [ $$N -eq 0 ]; then N=1000; fi; \
		BSIZE=$$(echo "scale=0; $$N / ($(ARITY) ^ $(HEIGHT))" | bc -l); \
		if [ -z "$$BSIZE" ] || [ "$$BSIZE" -eq 0 ]; then BSIZE=1; fi; \
		echo $$BSIZE; \
	fi \
)
endef

# Variable calculada dinámicamente
BSIZE := $(CALC_BSIZE)

.PHONY: all clean clean-all run-l run-experiments test help info

# ============================================================================
# COMPILACIÓN
# ============================================================================

all: $(TARGET)

$(TARGET): $(OBJECTS)
	$(CC) $(CFLAGS) -o $@ $^
	@echo "✓ Compilación exitosa: $(TARGET)"

%.o: %.c
	$(CC) $(CFLAGS) -c $< -o $@

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
	@echo "FQT - Configuración Actual"
	@echo "=========================================="
	@echo "Dataset:         $(DATASET)"
	@echo "Queries:         $(QUERIES)"
	@echo "Altura objetivo: $(HEIGHT) niveles (pivotes)"
	@echo "Arity:           $(ARITY)"
	@if [ -f "$(DATASET)" ]; then \
		N=$$(wc -c < $(DATASET) | awk '{print int($$1/32)}'); \
		if [ $$N -eq 0 ]; then N=1000; fi; \
		BSIZE=$$(echo "scale=0; $$N / ($(ARITY) ^ $(HEIGHT))" | bc -l); \
		if [ -z "$$BSIZE" ] || [ "$$BSIZE" -eq 0 ]; then BSIZE=1; fi; \
		echo "Objetos (aprox): $$N"; \
		echo "Bsize calculado: $$BSIZE"; \
		echo ""; \
		echo "Fórmula: bsize = n / (arity^height)"; \
		echo "         $$BSIZE = $$N / ($(ARITY)^$(HEIGHT))"; \
	else \
		echo "⚠ Dataset no encontrado"; \
	fi
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
	@echo "Ejecutando FQT con HEIGHT=$(HEIGHT)"
	@echo "=========================================="
	@$(MAKE) --no-print-directory info
	@echo ""
	@echo ">>> Construyendo índice..."
	@./$(TARGET) $(DATASET) $(INDEX_DIR)/index_h$(HEIGHT).bin \
	             $(BSIZE) $(ARITY) \
	    | tee $(LOGS_DIR)/build_h$(HEIGHT).log
	@echo ""
	@echo ">>> Ejecutando búsquedas..."
	@if [ -f "$(QUERIES)" ]; then \
		./$(TARGET) $(INDEX_DIR)/index_h$(HEIGHT).bin \
		            $(QUERIES) $(SHOW) \
		    | tee $(LOGS_DIR)/search_h$(HEIGHT).log \
		    > $(RESULTS_DIR)/results_h$(HEIGHT).txt; \
		echo "✓ Resultados guardados en $(RESULTS_DIR)/results_h$(HEIGHT).txt"; \
	else \
		echo "⚠ Archivo de queries '$(QUERIES)' no encontrado. Solo se construyó el índice."; \
	fi
	@echo ""
	@echo "✓ Experimento completado para HEIGHT=$(HEIGHT)"
	@echo "=========================================="

# ============================================================================
# EJECUCIÓN DE TODOS LOS EXPERIMENTOS (Paper: l={3,5,10,15,20})
# ============================================================================

run-experiments: $(TARGET) validate-dataset | $(RESULTS_DIR) $(LOGS_DIR) $(INDEX_DIR)
	@echo "=========================================="
	@echo "Ejecutando Experimentos FQT"
	@echo "Heights: 3, 5, 10, 15, 20 (según paper)"
	@echo "=========================================="
	@echo ""
	@for h in 3 5 10 15 20; do \
		echo ""; \
		echo ">>> Experimento HEIGHT=$$h <<<"; \
		$(MAKE) --no-print-directory run-l HEIGHT=$$h DATASET=$(DATASET) QUERIES=$(QUERIES) || true; \
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
# EXPERIMENTOS CON MÚLTIPLES DATASETS
# ============================================================================

run-all-datasets: $(TARGET)
	@echo "Ejecutando experimentos con múltiples datasets..."
	@for dataset in LA.bin integer.bin mpeg_1M.bin sf.bin; do \
		if [ -f "$$dataset" ]; then \
			echo ""; \
			echo "=== Dataset: $$dataset ==="; \
			$(MAKE) --no-print-directory run-experiments DATASET=$$dataset QUERIES=queries_$${dataset%.bin}.bin; \
		fi; \
	done

# ============================================================================
# PRUEBA RÁPIDA
# ============================================================================

test: $(TARGET) validate-dataset
	@echo "Prueba rápida con HEIGHT=$(HEIGHT)..."
	@$(MAKE) --no-print-directory info
	@echo ""
	@echo "Comando que se ejecutará:"
	@echo "./$(TARGET) $(DATASET) test_index.bin $(BSIZE) $(ARITY)"
	@echo ""
	@read -p "¿Continuar? (y/n): " confirm; \
	if [ "$$confirm" = "y" ]; then \
		./$(TARGET) $(DATASET) test_index.bin $(BSIZE) $(ARITY); \
		rm -f test_index.bin; \
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
	@echo "Makefile FQT - Cálculo Automático de bsize"
	@echo "=========================================="
	@echo ""
	@echo "COMANDOS PRINCIPALES:"
	@echo "  make                     - Compilar FQT"
	@echo "  make info                - Mostrar configuración actual"
	@echo "  make run-l HEIGHT=5      - Ejecutar con altura específica"
	@echo "  make run-experiments     - Ejecutar con HEIGHT={3,5,10,15,20}"
	@echo "  make test HEIGHT=10      - Prueba rápida con altura 10"
	@echo "  make clean               - Limpiar compilación"
	@echo "  make clean-all           - Limpiar todo (incluye resultados)"
	@echo ""
	@echo "PARÁMETROS CONFIGURABLES:"
	@echo "  DATASET=<archivo>        - Dataset a usar (default: dataset.bin)"
	@echo "  QUERIES=<archivo>        - Archivo de queries (default: queries.bin)"
	@echo "  HEIGHT=<número>          - Altura objetivo (l) (default: 5)"
	@echo "  ARITY=<número>           - Arity del árbol (default: 5)"
	@echo "  SHOW=<0|1>               - Mostrar resultados (default: 0)"
	@echo ""
	@echo "EJEMPLOS:"
	@echo "  # Ejecutar con altura 5 (5 pivotes)"
	@echo "  make run-l HEIGHT=5 DATASET=LA.bin"
	@echo ""
	@echo "  # Ejecutar todos los experimentos del paper"
	@echo "  make run-experiments DATASET=LA.bin QUERIES=queries_LA.bin"
	@echo ""
	@echo "  # Ver configuración antes de ejecutar"
	@echo "  make info HEIGHT=10 DATASET=LA.bin"
	@echo ""
	@echo "  # Prueba rápida"
	@echo "  make test HEIGHT=3 DATASET=small.bin"
	@echo ""
	@echo "CÁLCULO DE BSIZE:"
	@echo "  El Makefile calcula automáticamente:"
	@echo "    bsize = n / (arity^height)"
	@echo "  donde:"
	@echo "    n      = número de objetos en el dataset"
	@echo "    arity  = factor de ramificación (default: 5)"
	@echo "    height = altura objetivo (número de pivotes)"
	@echo ""
	@echo "VALORES DEL PAPER (Table 6):"
	@echo "  HEIGHT ∈ {3, 5, 10, 15, 20}"
	@echo "  ARITY = 5 (fijo)"
	@echo "=========================================="
