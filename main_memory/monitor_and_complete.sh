#!/bin/bash
# Script de monitoreo y completación automática

BASE_DIR="/mnt/c/Users/yared/Documents/eda-codigos/proyecto-paper/EDA-sQuad-Indexing-Metric-Spaces-for-Exact-Similarity-Search/main_memory"
cd "$BASE_DIR"

echo "========================================"
echo "Monitor de Benchmarks - $(date '+%H:%M:%S')"
echo "========================================"
echo ""

# Función para esperar proceso
wait_for() {
    local name=$1
    echo "[$(date '+%H:%M:%S')] Esperando a que termine $name..."
    while pgrep -x "$name" > /dev/null; do
        sleep 30
        echo "  [$(date '+%H:%M:%S')] $name aún ejecutándose..."
    done
    echo "[$(date '+%H:%M:%S')] ✓ $name terminó"
}

# Esperar GNAT
if pgrep -x "GNAT_test" > /dev/null; then
    wait_for "GNAT_test"
    echo ""
    echo "GNAT completado. Archivos generados:"
    ls -lh GNAT/GNAT/results/*.json
    echo ""
fi

# Esperar FQT
if pgrep -x "FQT_test" > /dev/null; then
    wait_for "FQT_test"
    echo ""
    echo "FQT completado. Archivos generados:"
    ls -lh FQT/results/*.json
    echo ""
fi

echo "========================================"
echo "Ejecutando GNAT Words..."
echo "========================================"

if [ ! -f "GNAT/GNAT/results/results_GNAT_Words.json" ]; then
    cd GNAT/GNAT
    nohup ./GNAT_test >> GNAT_benchmark.log 2>&1 &
    GNAT_PID=$!
    echo "GNAT Words iniciado con PID: $GNAT_PID"
    cd "$BASE_DIR"
    
    # Esperar a que termine
    wait_for "GNAT_test"
    
    echo ""
    echo "✓ GNAT Words completado"
    ls -lh GNAT/GNAT/results/results_GNAT_Words.json
else
    echo "[SKIP] GNAT Words ya existe"
fi

echo ""
echo "========================================"
echo "Resumen Final"
echo "========================================"
echo ""
echo "Archivos JSON totales:"
find . -name "*.json" -path "*/results/*" | wc -l
echo ""
echo "Por estructura:"
for struct in BST LAESA BKT MVPT EPT GNAT FQT; do
    count=$(find . -name "results_${struct}_*.json" | wc -l)
    echo "  $struct: $count/3"
done
echo ""
echo "========================================"
echo "Todos los benchmarks no-EPT completados!"
echo "Siguiente: Corregir EPT para LA y Synthetic"
echo "========================================"
