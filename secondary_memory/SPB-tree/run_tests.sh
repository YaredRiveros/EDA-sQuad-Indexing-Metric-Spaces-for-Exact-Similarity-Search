#!/bin/bash
# run_tests.sh
# Script para ejecutar tests de SPB-tree en Linux/WSL
# Uso: ./run_tests.sh

echo ""
echo "==== SPB-Tree Test Runner ===="
echo "Configuración: 4 pivotes, leafCap=128, fanout=64"
echo "Datasets: LA, Words, Synthetic"
echo "Experimentos: 10 por dataset (5 MRQ + 5 MkNN)"
echo ""

# Verificar que estamos en el directorio correcto
if [ ! -f "test.cpp" ]; then
    echo "[ERROR] No se encuentra test.cpp"
    echo "Asegúrate de estar en el directorio secondary_memory/SPB-tree"
    exit 1
fi

# Compilar si es necesario
if [ ! -f "SPBTree_test" ]; then
    echo "[COMPILE] Ejecutable no encontrado, compilando..."
    make clean && make
    if [ $? -ne 0 ]; then
        echo "[ERROR] Compilación falló"
        exit 1
    fi
    echo "[OK] Compilación exitosa"
else
    echo "[OK] Ejecutable encontrado: SPBTree_test"
fi

# Iniciar ejecución en background
echo ""
echo "[START] Iniciando ejecución en background..."
LOGFILE="spbtree_run_$(date +%Y%m%d_%H%M%S).log"
nohup ./SPBTree_test > "$LOGFILE" 2>&1 &
PID=$!
echo $PID > spbtree.pid

echo "[OK] Proceso iniciado con PID: $PID"
echo "[OK] Log: $LOGFILE"

# Información de tiempos
echo ""
echo "==== Tiempos Estimados ===="
echo "  LA:        ~50-70 minutos"
echo "  Words:     ~30-40 minutos"
echo "  Synthetic: ~4-6 minutos"
echo "  TOTAL:     ~85-115 minutos"

# Información de monitoreo
echo ""
echo "==== Comandos de Monitoreo ===="
echo "  Ver progreso:     tail -f $LOGFILE"
echo "  Ver últimas 30:   tail -30 $LOGFILE"
echo "  Ver estado:       ps -p $PID"
echo "  Detener:          kill $PID"
echo ""

# Función para mostrar progreso
show_progress() {
    echo ""
    echo "Mostrando progreso cada 10 segundos (Ctrl+C para salir)..."
    echo "============================================================"
    
    while kill -0 $PID 2>/dev/null; do
        sleep 10
        echo ""
        echo "[$(date +%H:%M:%S)] Últimas líneas del log:"
        echo "------------------------------------------------------------"
        tail -20 "$LOGFILE"
        echo "------------------------------------------------------------"
    done
    
    echo ""
    echo "[DONE] Proceso finalizado"
    
    # Verificar resultados
    if [ -d "results" ]; then
        echo ""
        echo "[RESULTS] Archivos generados:"
        for file in results/*.json; do
            if [ -f "$file" ]; then
                lines=$(wc -l < "$file")
                basename=$(basename "$file")
                if [ "$lines" -eq 12 ]; then
                    echo "  [OK] $basename: $lines líneas"
                else
                    echo "  [WARN] $basename: $lines líneas (esperado: 12)"
                fi
            fi
        done
    fi
}

# Preguntar si quiere monitoreo automático
echo "¿Iniciar monitoreo automático? (y/N): "
read -t 5 -n 1 response
echo ""

if [[ "$response" =~ ^[Yy]$ ]]; then
    show_progress
else
    echo "[INFO] Ejecutando en background sin monitoreo"
    echo "Usa 'tail -f $LOGFILE' para ver el progreso"
fi

echo ""
echo "[DONE] Script finalizado"
