# D-Index: Quick Start Guide

Guía rápida para ejecutar los tests de D-index (4 niveles, rho=5.0).

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
./DIndex_test 2>&1 | tee dindex_run.log
```

## ⏱️ Tiempos Estimados

- **LA**: 45-60 min
- **Words**: 25-35 min  
- **Synthetic**: 3-5 min
- **TOTAL**: ~75-100 min

## 📊 Monitoreo Rápido

### PowerShell
```powershell
# Ver últimas 20 líneas
Receive-Job -Name DIndex -Keep | Select-Object -Last 20

# Ver estado
Get-Job -Name DIndex

# Ver log completo
wsl tail -f dindex_run.log
```

### Linux/WSL
```bash
# Ver progreso en tiempo real
tail -f dindex_run.log

# Ver últimas 30 líneas
tail -30 dindex_run.log
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
Stop-Job -Name DIndex; Remove-Job -Name DIndex
```

```bash
# Linux/WSL
kill $(cat dindex.pid)
# o
pkill DIndex_test
```

## 📂 Archivos Generados

- `results/results_DIndex_LA.json` (12 líneas)
- `results/results_DIndex_Words.json` (12 líneas)
- `results/results_DIndex_Synthetic.json` (12 líneas)

## 🔍 Progreso Esperado

```
[BUILD] Construyendo D-index...
  Cargados 10000 objetos (0%)
  Cargados 100000 objetos (9%)
  ...
[BUILD] OK - D-index construido
DIndex stats: levels=4 rho=5

[MRQ] Ejecutando experimentos...
  [MRQ] sel=0.02 R=... OK
  [MRQ] sel=0.04 R=... OK
  ...

[MkNN] Ejecutando experimentos...
  [MkNN] k=5 ... OK
  [MkNN] k=10 ... OK
  ...

[DONE] Archivo generado: results/results_DIndex_LA.json
```

## 📚 Documentación Completa

Ver `README.md` para detalles completos sobre:
- Configuración del D-index
- Interpretación del progreso
- Troubleshooting
- Formato JSON de salida

---

**Config**: 4 niveles, ρ=5.0 | **Datasets**: 3 | **Experimentos**: 30 total
