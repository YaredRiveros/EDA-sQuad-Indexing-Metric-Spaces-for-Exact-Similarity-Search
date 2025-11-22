#!/bin/bash

# ============================================================
# Script para ejecutar todos los benchmarks de estructuras
# de índices métricos en main_memory
# ============================================================

set -e  # Exit on error

STRUCTURES=("BST" "LAESA" "BKT" "mvpt" "EPT" "FQT" "GNAT")
SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"

echo "=========================================="
echo "Main-Memory Index Structures Benchmark"
echo "=========================================="
echo "CPU Info:"
lscpu | grep "Model name"
echo ""
echo "Starting time: $(date)"
echo "=========================================="
echo ""

# Record CPU info
echo "CPU Info:" > benchmark_system_info.txt
lscpu >> benchmark_system_info.txt
echo "" >> benchmark_system_info.txt
echo "Start time: $(date)" >> benchmark_system_info.txt

for struct in "${STRUCTURES[@]}"; do
    echo ""
    echo "=========================================="
    echo "Structure: $struct"
    echo "=========================================="
    
    cd "$SCRIPT_DIR/$struct"
    
    # Compile
    echo "[1/3] Compiling..."
    if [ "$struct" == "mvpt" ]; then
        # mvpt paths are different
        g++ -O3 -std=gnu++17 test.cpp -o ${struct}_test -I..
    elif [ "$struct" == "EPT" ]; then
        # EPT* uses Makefile with existing source files
        echo "Compiling EPT* with its dependencies..."
        g++ -O3 -std=gnu++17 test.cpp Interpreter.cpp Objvector.cpp Tuple.cpp -o ${struct}_test
    elif [ "$struct" == "FQT" ]; then
        # FQT is C code, needs special compilation
        echo "Compiling FQT (C code) with C++ wrapper..."
        gcc -O3 -c fqt.c -o fqt.o
        gcc -O3 -c ../../index.c -o index_fqt.o
        gcc -O3 -c ../../bucket.c -o bucket_fqt.o
        g++ -O3 -std=gnu++17 test.cpp fqt.o index_fqt.o bucket_fqt.o -o ${struct}_test
    elif [ "$struct" == "GNAT" ]; then
        # GNAT is in nested directory
        cd GNAT
        echo "Compiling GNAT with its dependencies..."
        g++ -O3 -std=gnu++17 test.cpp db.cpp GNAT.cpp -o ../GNAT_test
        cd ..
    else
        g++ -O3 -std=gnu++17 test.cpp -o ${struct}_test
    fi
    
    if [ $? -ne 0 ]; then
        echo "[ERROR] Compilation failed for $struct"
        cd "$SCRIPT_DIR"
        continue
    fi
    echo "[✓] Compilation successful"
    
    # Run benchmark
    echo "[2/3] Running benchmark..."
    echo "This may take several minutes..."
    
    ./${struct}_test > ${struct}_benchmark.log 2>&1
    
    if [ $? -ne 0 ]; then
        echo "[ERROR] Execution failed for $struct"
        echo "Check ${struct}_benchmark.log for details"
    else
        echo "[✓] Benchmark completed successfully"
    fi
    
    # Summary
    echo "[3/3] Results summary:"
    if [ -d "results" ]; then
        # Convert struct name to uppercase for matching JSON files
        struct_upper=$(echo "$struct" | tr '[:lower:]' '[:upper:]')
        ls -lh results/results_${struct_upper}*.json 2>/dev/null
        if [ $? -ne 0 ]; then
            # Try with original case if uppercase didn't work
            ls -lh results/results_${struct}*.json 2>/dev/null
        fi
        count=$(ls results/results_${struct_upper}*.json 2>/dev/null | wc -l)
        if [ $count -eq 0 ]; then
            count=$(ls results/results_${struct}*.json 2>/dev/null | wc -l)
        fi
        echo "Total result files: $count"
    else
        echo "[WARN] No results directory found"
    fi
    
    cd "$SCRIPT_DIR"
    echo ""
done

# Final summary
echo ""
echo "=========================================="
echo "All Benchmarks Completed!"
echo "=========================================="
echo "End time: $(date)"
echo ""

echo "Results location:"
for struct in "${STRUCTURES[@]}"; do
    if [ -d "$struct/results" ]; then
        echo "  $struct/results/"
    fi
done

echo ""
echo "Logs:"
for struct in "${STRUCTURES[@]}"; do
    if [ -f "$struct/${struct}_benchmark.log" ]; then
        echo "  $struct/${struct}_benchmark.log"
    fi
done

echo ""
echo "Next steps:"
echo "  1. Review individual logs for any warnings"
echo "  2. Run Python aggregation script: python3 aggregate_results.py"
echo "  3. Analyze consolidated results"
echo ""

# Record end time
echo "" >> benchmark_system_info.txt
echo "End time: $(date)" >> benchmark_system_info.txt
