#!/usr/bin/env bash

set -euo pipefail

# Estructuras de memoria secundaria
STRUCTURES=(
  "CPT"
  "D-index"
  "DSATCLT"
  "EGNAT"
  "LC"
  "M-Tree"
  "M-index_star"
  "MB_plus_tree"
  "Omni-rtree"
  "PM-Tree"
  "SPB-tree"
)

SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"

# Dataset objetivo desde argumento (default LA)
TARGET_DATASET="${1:-LA}"

echo "=========================================="
echo "Secondary-Memory Index Structures Benchmark"
echo "=========================================="
echo "CPU Info:"
lscpu | grep "Model name" || true
echo ""
echo "Starting time: $(date)"
echo "Dataset objetivo: ${TARGET_DATASET}"
echo "=========================================="
echo ""

{
  echo "CPU Info:"
  lscpu
  echo ""
  echo "Start time: $(date)"
  echo "Dataset: ${TARGET_DATASET}"
} > "${SCRIPT_DIR}/benchmark_system_info_secondary.txt"

for struct in "${STRUCTURES[@]}"; do
  echo ""
  echo "=========================================="
  echo "Structure: $struct"
  echo "=========================================="

  cd "${SCRIPT_DIR}/${struct}"

  echo "[1/3] Compiling..."
  g++ -O3 -std=c++17 test.cpp -o "${struct}_test"
  compile_exit=$?

  if [ "$compile_exit" -ne 0 ]; then
    echo "[ERROR] Compilation FAILED for ${struct} (exit code=${compile_exit})"
    cd "$SCRIPT_DIR"
    continue
  fi

  echo "[✓] Compilation finished (exit code ${compile_exit})"

  echo "[2/3] Running benchmark for dataset=${TARGET_DATASET}..."
  "./${struct}_test" "${TARGET_DATASET}" > "${struct}_benchmark_${TARGET_DATASET}.log" 2>&1
  run_exit=$?

  if [ "$run_exit" -ne 0 ]; then
    echo "[ERROR] Benchmark for ${struct} FAILED (exit code=${run_exit})"
    echo "        Check ${struct}_benchmark_${TARGET_DATASET}.log"
  else
    echo "[✓] Benchmark completed successfully"
  fi

  echo "[3/3] Results summary:"
  if [ -d "results" ]; then
    ls -lh results/*.json 2>/dev/null || echo "No JSON results found for ${struct}"
    count=$(ls results/*.json 2>/dev/null | wc -l || echo 0)
    echo "Total result files: ${count}"
  else
    echo "[WARN] No results directory found for ${struct}"
  fi

  cd "$SCRIPT_DIR"
done

echo ""
echo "=========================================="
echo "All Secondary-Memory Benchmarks Completed for dataset=${TARGET_DATASET}!"
echo "=========================================="
echo "End time: $(date)"
echo ""

{
  echo ""
  echo "End time: $(date)"
} >> "${SCRIPT_DIR}/benchmark_system_info_secondary.txt"
