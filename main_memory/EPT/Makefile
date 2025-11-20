CXX = g++
CXXFLAGS = -O3 -std=c++17 -Wall -Wno-unused-variable -Wno-unused-function -Wno-sign-compare -Wno-maybe-uninitialized -Wno-return-type -Wno-unused-result
TARGET = EPT_test
SOURCES = test.cpp main.cpp Interpreter.cpp Objvector.cpp Tuple.cpp
OBJECTS = $(SOURCES:.cpp=.o)

.PHONY: all clean run test help

all: $(TARGET)

$(TARGET): $(OBJECTS)
	$(CXX) $(CXXFLAGS) -o $@ $^
	@echo "✓ Compilación exitosa: $(TARGET)"

%.o: %.cpp
	$(CXX) $(CXXFLAGS) -c $< -o $@

run: $(TARGET)
	@echo "Ejecutando EPT benchmark..."
	./$(TARGET)

test: $(TARGET)
	@echo "Prueba rápida EPT (60s)..."
	timeout 60s ./$(TARGET) || echo "Timeout"

clean:
	rm -f $(OBJECTS) $(TARGET)
	@echo "✓ Archivos objeto y ejecutable eliminados"

clean-all: clean
	rm -rf results/
	@echo "✓ Resultados eliminados"

help:
	@echo "Makefile EPT - Benchmark completo"
	@echo ""
	@echo "COMANDOS:"
	@echo "  make        - Compilar EPT"
	@echo "  make run    - Ejecutar benchmark completo"
	@echo "  make test   - Prueba rápida (60s)"
	@echo "  make clean  - Limpiar compilación"
	@echo ""
	@echo "DATASETS: LA, Words, Synthetic"
	@echo "L_VALUES: 10, 15, 20, 25, 30"
