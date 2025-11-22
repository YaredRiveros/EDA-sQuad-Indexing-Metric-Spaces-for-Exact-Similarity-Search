#!/bin/bash

# Script para ejecutar EPT, GNAT y FQT en segundo plano

cd /mnt/c/Users/yared/Documents/eda-codigos/proyecto-paper/EDA-sQuad-Indexing-Metric-Spaces-for-Exact-Similarity-Search/main_memory

echo "Iniciando EPT benchmark..."
cd EPT
nohup ./EPT_test > EPT_benchmark.log 2>&1 &
EPT_PID=$!
echo "EPT iniciado con PID: $EPT_PID"
cd ..

echo "Iniciando GNAT benchmark..."
cd GNAT/GNAT
nohup ./GNAT_test > GNAT_benchmark.log 2>&1 &
GNAT_PID=$!
echo "GNAT iniciado con PID: $GNAT_PID"
cd ../..

echo "Iniciando FQT benchmark..."
cd FQT
nohup ./FQT_test > FQT_benchmark.log 2>&1 &
FQT_PID=$!
echo "FQT iniciado con PID: $FQT_PID"
cd ..

echo ""
echo "Benchmarks iniciados:"
echo "  EPT  PID: $EPT_PID"
echo "  GNAT PID: $GNAT_PID"
echo "  FQT  PID: $FQT_PID"
echo ""
echo "Monitorear con:"
echo "  tail -f EPT/EPT_benchmark.log"
echo "  tail -f GNAT/GNAT/GNAT_benchmark.log"
echo "  tail -f FQT/FQT_benchmark.log"
echo ""
echo "Verificar progreso con:"
echo "  ps -p $EPT_PID $GNAT_PID $FQT_PID"
