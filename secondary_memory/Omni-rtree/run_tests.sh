#!/bin/bash
# run_tests.sh - Script para ejecutar tests de OmniR-tree con monitoreo

echo "=========================================="
echo "  OmniR-tree Test Execution"
echo "=========================================="
echo ""

# Verificar que el ejecutable existe
if [ ! -f "./OmniRTree_test" ]; then
    echo "[ERROR] Ejecutable no encontrado. Compilando..."
    make clean && make
    if [ $? -ne 0 ]; then
        echo "[ERROR] Compilación falló"
        exit 1
    fi
fi

# Crear directorio de resultados
mkdir -p results

# Nombre del archivo de log
LOGFILE="omni_execution_$(date +%Y%m%d_%H%M%S).log"

echo "[INFO] Iniciando ejecución..."
echo "[INFO] Log file: $LOGFILE"
echo "[INFO] Presiona Ctrl+C para detener"
echo ""

# Función para mostrar progreso
show_progress() {
    local count=0
    while kill -0 $1 2>/dev/null; do
        sleep 10
        echo "[$(date +%H:%M:%S)] Progreso:"
        tail -15 "$LOGFILE" | grep -E "Indexados|sel=|k=|BUILD|DONE" | tail -5
        echo ""
        count=$((count + 1))
    done
}

# Ejecutar tests
./OmniRTree_test > "$LOGFILE" 2>&1 &
PID=$!

echo "[INFO] Proceso iniciado con PID: $PID"
echo ""

# Esperar un poco antes de empezar a mostrar progreso
sleep 5

# Mostrar progreso
show_progress $PID

# Esperar a que termine
wait $PID
EXIT_CODE=$?

echo ""
echo "=========================================="
if [ $EXIT_CODE -eq 0 ]; then
    echo "  EJECUCIÓN COMPLETADA EXITOSAMENTE"
    echo "=========================================="
    echo ""
    echo "Resultados generados:"
    ls -lh results/*.json
    echo ""
    echo "Verificación:"
    for file in results/*.json; do
        lines=$(wc -l < "$file")
        echo "  $(basename $file): $lines líneas"
    done
else
    echo "  EJECUCIÓN FALLÓ (código: $EXIT_CODE)"
    echo "=========================================="
    echo ""
    echo "Últimas 20 líneas del log:"
    tail -20 "$LOGFILE"
fi

echo ""
echo "Log completo en: $LOGFILE"
