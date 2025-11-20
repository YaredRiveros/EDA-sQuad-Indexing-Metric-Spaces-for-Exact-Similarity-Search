# SPB-Tree: Quick Start Guide

Guía rápida para ejecutar los tests de SPB-tree (4 pivotes, leafCap=128, fanout=64).

## ⚡ Inicio Rápido

### Windows (PowerShell)

```powershell
# 1. Compilar
wsl bash -c 'make'

# 2. Ejecutar con monitoreo automático
.\run_tests.ps1

# O ejecutar sin monitoreo
.\run_tests.ps1 -NoMonitor

# 3. Monitorear (si usaste -NoMonitor)
.\check_progress.ps1
```

### Linux/WSL

```bash
# 1. Compilar
make

# 2. Ejecutar
chmod +x run_tests.sh
./run_tests.sh

# O directamente
./SPBTree_test 2>&1 | tee spbtree_run.log
```

## ⏱️ Tiempos Estimados

- **LA**: 50-70 min
- **Words**: 30-40 min  
- **Synthetic**: 4-6 min
- **TOTAL**: ~85-115 min

## 📊 Monitoreo Rápido

### PowerShell
```powershell
# Ver últimas 20 líneas
Receive-Job -Name SPBTree -Keep | Select-Object -Last 20

# Ver estado
Get-Job -Name SPBTree

# Ver log completo
wsl tail -f spbtree_run.log
```

### Linux/WSL
```bash
# Ver progreso en tiempo real
tail -f spbtree_run.log

# Ver últimas 30 líneas
tail -30 spbtree_run.log
```

## ✅ Verificar Resultados

```powershell
# PowerShell
Get-ChildItem results\*.json | ForEach-Object {
    $lines = (Get-Content $_.FullName | Measure-Object -Line).Lines
    Write-Host "$($_.Name): $lines líneas"
}
```

```bash
# Linux/WSL
wc -l results/*.json
```

**Esperado**: 3 archivos con 12 líneas cada uno.

## 🛑 Detener

```powershell
# PowerShell
Stop-Job -Name SPBTree; Remove-Job -Name SPBTree
```

```bash
# Linux/WSL
kill $(cat spbtree.pid)
# o
pkill SPBTree_test
```

## 📂 Archivos Generados

- `results/results_SPBTree_LA.json` (12 líneas)
- `results/results_SPBTree_Words.json` (12 líneas)
- `results/results_SPBTree_Synthetic.json` (12 líneas)

## 🔍 Progreso Esperado

```
[BUILD] Construyendo SPB-tree...
  Cargados 10000 objetos (0%)
  Cargados 100000 objetos (9%)
  ...
[BUILD] OK - SPB-tree construido
SPB-tree: pivots=4, records=1073728, SFC bits/dim=16

[MRQ] Ejecutando experimentos...
  [MRQ] sel=0.02 R=... OK
  [MRQ] sel=0.04 R=... OK
  ...

[MkNN] Ejecutando experimentos...
  [MkNN] k=5 ... OK
  [MkNN] k=10 ... OK
  ...

[DONE] Archivo generado: results/results_SPBTree_LA.json
```

## 📚 Documentación Completa

Ver `README.md` para detalles completos sobre:
- Detalles técnicos del SPB-tree
- Space-filling curves (Morton Z-order)
- Configuración del B+ bulk-load
- Troubleshooting
- Formato JSON de salida

---

**Config**: 4 pivotes, leafCap=128, fanout=64 | **Datasets**: 3 | **Experimentos**: 30 total
