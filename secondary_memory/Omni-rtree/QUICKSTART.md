# OmniR-tree - Quick Start

## Ejecución Rápida

### Opción 1: Script Automático (Recomendado)
```powershell
# Windows PowerShell - Con monitoreo automático
.\run_tests.ps1

# Windows PowerShell - Sin monitoreo (ejecuta en background)
.\run_tests.ps1 -NoMonitor

# Windows PowerShell - Limpiar y ejecutar
.\run_tests.ps1 -Clean
```

```bash
# Linux/WSL - Con monitoreo automático
chmod +x run_tests.sh
./run_tests.sh
```

### Opción 2: Ejecución Manual

**Windows PowerShell:**
```powershell
# 1. Compilar
wsl bash -c 'make clean && make'

# 2. Ejecutar en background
$job = Start-Job -Name OmniRTree -ScriptBlock { 
    Set-Location 'c:\Users\yared\Documents\eda-codigos\proyecto-paper\EDA-sQuad-Indexing-Metric-Spaces-for-Exact-Similarity-Search\secondary_memory\Omni-rtree'
    wsl bash -c './OmniRTree_test 2>&1' 
}

# 3. Monitorear
.\check_progress.ps1
```

**Linux/WSL:**
```bash
# 1. Compilar
make clean && make

# 2. Ejecutar
./OmniRTree_test > execution.log 2>&1 &

# 3. Monitorear
tail -f execution.log
```

## Ver Progreso

### PowerShell - Comando Rápido
```powershell
# Ver últimas líneas
Receive-Job -Name OmniRTree -Keep | Select-Object -Last 20

# Ver estado
Get-Job -Name OmniRTree

# Script de monitoreo
.\check_progress.ps1
```

### Linux/WSL - Comando Rápido
```bash
# Ver últimas líneas del log
tail -20 execution.log

# Ver progreso de construcción
grep "Indexados" execution.log | tail -5

# Ver progreso de queries
grep -E "sel=|k=" execution.log | tail -10
```

## Tiempo Estimado

- **Synthetic**: ~4 minutos
- **Words**: ~25 minutos
- **LA**: ~45 minutos
- **TOTAL**: ~75 minutos (1h 15min)

## Verificar Resultados

```powershell
# PowerShell
Get-ChildItem results\*.json
```

```bash
# Linux/WSL
ls -lh results/*.json
```

**Esperado:** 3 archivos JSON con 12 líneas cada uno

## Detener Ejecución

```powershell
# PowerShell
Stop-Job -Name OmniRTree
Remove-Job -Name OmniRTree
```

```bash
# Linux/WSL
pkill OmniRTree_test
```

## Ayuda Completa

Ver `README.md` para documentación completa.
