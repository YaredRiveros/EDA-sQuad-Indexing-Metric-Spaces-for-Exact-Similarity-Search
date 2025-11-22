#!/bin/bash
clear
echo '=== PROCESOS ACTIVOS ==='
ps aux | grep -E '(FQT_test|GNAT_test)' | grep -v grep
echo
echo '=== FQT LOG (últimas 10 líneas) ==='
tail -10 FQT/FQT_execution.log 2>/dev/null || echo 'Sin log'
echo
echo '=== GNAT LOG (últimas 10 líneas) ==='
tail -10 GNAT/GNAT/*.log 2>/dev/null | tail -10 || echo 'Sin log'
echo
echo '=== JSONs VÁLIDOS ==='
find . -name 'results_*.json' -size +0 2>/dev/null | wc -l
