#!/bin/bash

BASE="/mnt/c/Users/yared/Documents/eda-codigos/proyecto-paper/EDA-sQuad-Indexing-Metric-Spaces-for-Exact-Similarity-Search/main_memory"

echo "========================================"
echo "Estado Final de Benchmarks"
echo "$(date '+%Y-%m-%d %H:%M:%S')"
echo "========================================"
echo ""

# Verificar procesos activos
echo "=== PROCESOS ACTIVOS ==="
GNAT_PID=$(ps aux | grep GNAT_test | grep -v grep | awk '{print $2}')
FQT_PID=$(ps aux | grep FQT_test | grep -v grep | awk '{print $2}')

if [ -n "$GNAT_PID" ]; then
    ps aux | grep GNAT_test | grep -v grep | awk '{printf "GNAT: PID %s, CPU %s%%, MEM %s%%, TIME %s\n", $2, $3, $4, $10}'
else
    echo "GNAT: ✓ Completado"
fi

if [ -n "$FQT_PID" ]; then
    ps aux | grep FQT_test | grep -v grep | awk '{printf "FQT:  PID %s, CPU %s%%, MEM %s%%, TIME %s\n", $2, $3, $4, $10}'
else
    echo "FQT:  ✓ Completado"
fi

echo ""
echo "=== ARCHIVOS JSON GENERADOS ==="
total=$(find "$BASE" -name "*.json" -path "*/results/*" -type f ! -size 0 2>/dev/null | wc -l)
echo "Total archivos válidos: $total/20"
echo ""

echo "Por estructura:"
for struct in BST LAESA BKT MVPT EPT GNAT FQT; do
    count=$(find "$BASE" -name "results_${struct}_*.json" -type f ! -size 0 2>/dev/null | wc -l)
    if [ $count -eq 3 ]; then
        status="✓"
    elif [ $count -eq 0 ]; then
        status="✗"
    else
        status="◐"
    fi
    printf "  %-8s %d/3  %s\n" "$struct:" "$count" "$status"
done

echo ""
echo "=== PROGRESO ACTUAL ==="

if [ -n "$GNAT_PID" ]; then
    echo "GNAT (últimas 3 líneas):"
    tail -3 "$BASE/GNAT/GNAT/GNAT_benchmark.log" 2>/dev/null | sed 's/^/  /'
    echo ""
fi

if [ -n "$FQT_PID" ]; then
    echo "FQT (últimas 3 líneas):"
    tail -3 "$BASE/FQT/FQT_complete.log" 2>/dev/null | sed 's/^/  /'
    echo ""
fi

echo "=== FALTANTES ==="
echo ""
echo "Pendientes por completar:"
missing=0

if [ ! -f "$BASE/GNAT/GNAT/results/results_GNAT_Words.json" ]; then
    echo "  - GNAT → Words (en progreso)"
    missing=$((missing+1))
fi

for dataset in LA Words Synthetic; do
    if [ ! -f "$BASE/FQT/results/results_FQT_${dataset}.json" ] || [ ! -s "$BASE/FQT/results/results_FQT_${dataset}.json" ]; then
        echo "  - FQT → $dataset"
        missing=$((missing+1))
    fi
done

for dataset in LA Synthetic; do
    if [ ! -f "$BASE/EPT/results/results_EPT_${dataset}.json" ]; then
        echo "  - EPT → $dataset (requiere corrección)"
        missing=$((missing+1))
    fi
done

if [ $missing -eq 0 ]; then
    echo "  ¡Ninguno! Todos los datasets completados."
fi

echo ""
echo "========================================"
echo "Tiempo estimado de finalización:"
if [ -n "$GNAT_PID" ]; then
    echo "  GNAT: 5-10 minutos"
fi
if [ -n "$FQT_PID" ]; then
    echo "  FQT:  40-60 minutos (3 datasets × 5 heights)"
fi
echo "========================================"
