#!/bin/bash
# Script para ejecutar los benchmarks faltantes después de que GNAT y FQT terminen

echo "==================================================="
echo "Script de Completación de Resultados Faltantes"
echo "==================================================="
echo ""

# Función para esperar a que termine un proceso
wait_for_process() {
    local process_name=$1
    echo "[$(date '+%H:%M:%S')] Esperando a que termine $process_name..."
    while pgrep -x "$process_name" > /dev/null; do
        sleep 10
    done
    echo "[$(date '+%H:%M:%S')] ✓ $process_name ha terminado."
}

# Función para ejecutar benchmark
run_benchmark() {
    local structure=$1
    local dataset=$2
    local executable=$3
    local log_file=$4
    
    echo ""
    echo "---------------------------------------------------"
    echo "[$(date '+%H:%M:%S')] Ejecutando: $structure → $dataset"
    echo "---------------------------------------------------"
    
    cd "$(dirname "$executable")"
    nohup ./$(basename "$executable") >> "$log_file" 2>&1 &
    local pid=$!
    
    echo "PID: $pid"
    echo "Log: $log_file"
    
    # Esperar a que termine
    wait $pid
    local exit_code=$?
    
    if [ $exit_code -eq 0 ]; then
        echo "✓ $structure → $dataset completado exitosamente"
    else
        echo "✗ $structure → $dataset terminó con código de error: $exit_code"
    fi
    
    cd - > /dev/null
}

# Directorio base
BASE_DIR="/mnt/c/Users/yared/Documents/eda-codigos/proyecto-paper/EDA-sQuad-Indexing-Metric-Spaces-for-Exact-Similarity-Search/main_memory"
cd "$BASE_DIR"

echo "Directorio base: $BASE_DIR"
echo ""

# ==========================================
# PASO 1: Esperar a que terminen los actuales
# ==========================================
echo "PASO 1: Verificando procesos en ejecución..."
echo ""

if pgrep -x "GNAT_test" > /dev/null; then
    wait_for_process "GNAT_test"
else
    echo "[$(date '+%H:%M:%S')] GNAT_test no está ejecutándose"
fi

if pgrep -x "FQT_test" > /dev/null; then
    wait_for_process "FQT_test"
else
    echo "[$(date '+%H:%M:%S')] FQT_test no está ejecutándose"
fi

echo ""
echo "==================================================="
echo "PASO 2: Ejecutando benchmarks faltantes"
echo "==================================================="

# ==========================================
# GNAT → Words
# ==========================================
if [ ! -f "GNAT/GNAT/results/results_GNAT_Words.json" ]; then
    echo ""
    echo ">>> Ejecutando GNAT → Words <<<"
    run_benchmark "GNAT" "Words" \
        "$BASE_DIR/GNAT/GNAT/GNAT_test" \
        "$BASE_DIR/GNAT/GNAT/GNAT_benchmark.log"
else
    echo "[SKIP] GNAT → Words ya existe"
fi

# ==========================================
# FQT → Words
# ==========================================
if [ ! -f "FQT/results/results_FQT_Words.json" ]; then
    echo ""
    echo ">>> Ejecutando FQT → Words <<<"
    run_benchmark "FQT" "Words" \
        "$BASE_DIR/FQT/FQT_test" \
        "$BASE_DIR/FQT/FQT_benchmark.log"
else
    echo "[SKIP] FQT → Words ya existe"
fi

# ==========================================
# FQT → Synthetic
# ==========================================
if [ ! -f "FQT/results/results_FQT_Synthetic.json" ]; then
    echo ""
    echo ">>> Ejecutando FQT → Synthetic <<<"
    run_benchmark "FQT" "Synthetic" \
        "$BASE_DIR/FQT/FQT_test" \
        "$BASE_DIR/FQT/FQT_benchmark.log"
else
    echo "[SKIP] FQT → Synthetic ya existe"
fi

# ==========================================
# EPT → LA (si se corrigió el código)
# ==========================================
echo ""
echo "---------------------------------------------------"
echo "NOTA: EPT requiere corrección antes de ejecutar"
echo "---------------------------------------------------"
echo "EPT necesita que se corrija load_queries_float()"
echo "para manejar queries en formato JSON (índices)."
echo ""
echo "Después de corregir EPT, ejecutar manualmente:"
echo "  cd $BASE_DIR/EPT"
echo "  nohup ./EPT_test >> EPT_benchmark.log 2>&1 &"
echo ""

# ==========================================
# Resumen final
# ==========================================
echo ""
echo "==================================================="
echo "PASO 3: Resumen de resultados"
echo "==================================================="
echo ""

total_json=$(find "$BASE_DIR" -name "*.json" -path "*/results/*" | wc -l)
echo "Total de archivos JSON generados: $total_json"
echo ""

echo "Archivos por estructura:"
for structure in BST LAESA BKT MVPT EPT GNAT FQT; do
    count=$(find "$BASE_DIR" -name "results_${structure}_*.json" | wc -l)
    echo "  $structure: $count/3 datasets"
done

echo ""
echo "==================================================="
echo "Script completado: $(date)"
echo "==================================================="
echo ""
echo "Próximos pasos:"
echo "1. Corregir EPT para manejar queries desde JSON"
echo "2. Ejecutar EPT manualmente con LA y Synthetic"
echo "3. Ejecutar aggregate_results.py para análisis final"
echo ""
