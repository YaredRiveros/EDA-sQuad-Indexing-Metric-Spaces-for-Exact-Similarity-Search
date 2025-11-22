#!/bin/bash

# ============================================================
# Script para verificar el progreso de los benchmarks
# ============================================================

SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
cd "$SCRIPT_DIR"

echo "=========================================="
echo "Benchmark Progress Monitor"
echo "=========================================="
echo ""

# Check log file
if [ -f "benchmark_execution.log" ]; then
    echo "📊 Last 30 lines of execution log:"
    echo "------------------------------------------"
    tail -30 benchmark_execution.log
    echo ""
fi

echo "=========================================="
echo "📁 Results Status:"
echo "=========================================="

STRUCTURES=("BST" "LAESA" "BKT" "mvpt" "EPT" "FQT" "GNAT")

for struct in "${STRUCTURES[@]}"; do
    echo ""
    echo "[$struct]"
    
    # Check if test executable exists
    if [ "$struct" == "GNAT" ]; then
        if [ -f "GNAT_test" ]; then
            echo "  ✓ Executable compiled"
        else
            echo "  ⏳ Not compiled yet"
        fi
        
        if [ -d "GNAT/GNAT/results" ]; then
            count=$(ls GNAT/GNAT/results/results_GNAT*.json 2>/dev/null | wc -l)
            echo "  📊 Results: $count JSON files"
            if [ $count -gt 0 ]; then
                ls -lh GNAT/GNAT/results/results_GNAT*.json | awk '{print "     ", $9, "("$5")"}'
            fi
        fi
    else
        if [ -f "$struct/${struct}_test" ] || [ -f "$struct/${struct}_test.exe" ]; then
            echo "  ✓ Executable compiled"
        else
            echo "  ⏳ Not compiled yet"
        fi
        
        if [ -d "$struct/results" ]; then
            count=$(ls $struct/results/results_${struct}*.json 2>/dev/null | wc -l)
            echo "  📊 Results: $count JSON files"
            if [ $count -gt 0 ]; then
                ls -lh $struct/results/results_${struct}*.json | awk '{print "     ", $9, "("$5")"}'
            fi
        fi
    fi
    
    # Check log file
    if [ -f "$struct/${struct}_benchmark.log" ]; then
        echo "  📝 Log file exists"
    fi
done

echo ""
echo "=========================================="
echo "⏱️  Estimated Progress:"
echo "=========================================="

total_results=0
for struct in "${STRUCTURES[@]}"; do
    if [ "$struct" == "GNAT" ]; then
        if [ -d "GNAT/GNAT/results" ]; then
            count=$(ls GNAT/GNAT/results/results_GNAT*.json 2>/dev/null | wc -l)
            total_results=$((total_results + count))
        fi
    else
        if [ -d "$struct/results" ]; then
            count=$(ls $struct/results/results_${struct}*.json 2>/dev/null | wc -l)
            total_results=$((total_results + count))
        fi
    fi
done

expected_total=$((7 * 4))  # 7 structures × 4 datasets
echo "Results completed: $total_results / $expected_total ($(($total_results * 100 / $expected_total))%)"

echo ""
echo "To see real-time logs:"
echo "  tail -f benchmark_execution.log"
echo ""
echo "To check again:"
echo "  ./check_progress.sh"
echo ""
