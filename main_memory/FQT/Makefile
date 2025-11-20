# Makefile para FQT (C++ wrapper sobre ObjectDB)

CXX = g++
CXXFLAGS = -O3 -std=c++17 -Wall -Wno-unused-function
TARGET = FQT_test
SOURCES = test.cpp
OBJECTS = $(SOURCES:.cpp=.o)

.PHONY: all clean run test help clean-all

all: $(TARGET)

$(TARGET): $(OBJECTS)
	$(CXX) $(CXXFLAGS) -o $@ $^
	@echo "✓ Compilación exitosa: $(TARGET)"

%.o: %.cpp
	$(CXX) $(CXXFLAGS) -c $< -o $@

run: $(TARGET)
	@echo "Ejecutando FQT benchmark..."
	@./$(TARGET)

test: $(TARGET)
	@echo "Prueba rápida de FQT..."
	@timeout 60s ./$(TARGET) || echo "Timeout o error en ejecución"

clean:
	rm -f $(OBJECTS) $(TARGET)
	@echo "✓ Archivos objeto y ejecutable eliminados"

clean-all: clean
	rm -rf results/
	@echo "✓ Resultados eliminados"

help:
	@echo "=========================================="
	@echo "Makefile FQT - C++ Wrapper"
	@echo "=========================================="
	@echo ""
	@echo "COMANDOS:"
	@echo "  make        - Compilar FQT"
	@echo "  make run    - Ejecutar benchmark completo"
	@echo "  make test   - Prueba rápida (60s timeout)"
	@echo "  make clean  - Limpiar compilación"
	@echo "  make clean-all - Limpiar todo (incluye resultados)"
	@echo ""
	@echo "DATASETS: LA, Words, Synthetic"
	@echo "CONFIGURACIONES: 5 por dataset (bucket × arity)"
	@echo "=========================================="
