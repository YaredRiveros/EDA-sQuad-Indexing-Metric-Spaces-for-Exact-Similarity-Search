#!/bin/bash

BASE="/mnt/c/Users/yared/Documents/eda-codigos/proyecto-paper/EDA-sQuad-Indexing-Metric-Spaces-for-Exact-Similarity-Search/main_memory"

while true; do
    clear
    echo "========================================"
    echo "Monitor de Benchmarks - $(date '+%H:%M:%S')"
    echo "========================================"
    echo ""
    
    # Procesos activos
    echo "=== Procesos Activos ==="
    if pgrep -x "GNAT_test" > /dev/null; then
        ps aux | grep GNAT_test | grep -v grep | awk '{print "GNAT: PID "$2", CPU "$3"%, MEM "$4"%, TIME "$10}'
    else
        echo "GNAT: No ejecutándose"
    fi
    
    if pgrep -x "FQT_test" > /dev/null; then
        ps aux | grep FQT_test | grep -v grep | awk '{print "FQT:  PID "$2", CPU "$3"%, MEM "$4"%, TIME "$10}'
    else
        echo "FQT: No ejecutándose"
    fi
    echo ""
    
    # Archivos JSON
    echo "=== Archivos JSON Generados ==="
    total=$(find "$BASE" -name "*.json" -path "*/results/*" 2>/dev/null | wc -l)
    echo "Total: $total/20"
    echo ""
    echo "Por estructura:"
    for struct in BST LAESA BKT MVPT EPT GNAT FQT; do
        count=$(find "$BASE" -name "results_${struct}_*.json" 2>/dev/null | wc -l)
        printf "  %-8s %d/3\n" "$struct:" "$count"
    done
    echo ""
    
    # Últimas líneas de logs
    echo "=== GNAT (últimas 3 líneas) ==="
    tail -3 "$BASE/GNAT/GNAT/GNAT_benchmark.log" 2>/dev/null
    echo ""
    
    echo "=== FQT (últimas 3 líneas) ==="
    tail -3 "$BASE/FQT/FQT_benchmark.log" 2>/dev/null
    echo ""
    
    # Verificar si terminaron
    if ! pgrep -x "GNAT_test" > /dev/null && ! pgrep -x "FQT_test" > /dev/null; then
        echo "========================================"
        echo "¡Ambos procesos han terminado!"
        echo "========================================"
        
        # Verificar si falta GNAT Words
        if [ ! -f "$BASE/GNAT/GNAT/results/results_GNAT_Words.json" ]; then
            echo ""
            echo "Iniciando GNAT Words..."
            cd "$BASE/GNAT/GNAT"
            nohup ./GNAT_test >> GNAT_benchmark.log 2>&1 &
            echo "GNAT Words iniciado con PID: $!"
        else
            echo ""
            echo "Todos los benchmarks no-EPT completados!"
            break
        fi
    fi
    
    sleep 10
done
