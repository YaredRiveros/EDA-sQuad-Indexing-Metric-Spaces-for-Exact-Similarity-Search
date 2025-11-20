# D-Index: Tests de Memoria Secundaria

Implementación y tests del **D-index** (familia D-index) para espacios métricos con soporte de memoria secundaria, siguiendo el formato del paper de Chen et al. (2022).

## 📋 Descripción

El D-index utiliza:
- **ρ-split multilevel buckets**: Organización jerárquica de datos
- **RAF (Random Access File)**: Simulación de acceso a disco
- **Pivot mapping**: Distancias precomputadas a pivotes
- **Configuración fija**: 4 niveles/pivotes, ρ = 5.0

## ⚙️ Configuración

```cpp
int numLevels = 4;    // Número de niveles/pivotes
double rho = 5.0;     // Parámetro ρ para el split
```

### Datasets

- **LA**: 1,073,728 vectores 3D (p=2, Euclidean)
- **Words**: 597,193 cadenas (distancia Levenshtein)
- **Synthetic**: 28,659 vectores (p=0, Chebyshev)

### Experimentos por Dataset

Cada dataset genera **10 experimentos**:

1. **MRQ (5 experimentos)**: Selectividades 0.02, 0.04, 0.08, 0.16, 0.32
2. **MkNN (5 experimentos)**: k = 5, 10, 20, 50, 100

## 🔨 Compilación

### Linux/WSL
```bash
make clean
make
```

### Windows (usando WSL)
```powershell
wsl bash -c 'make clean && make'
```

Esto generará el ejecutable `DIndex_test`.

## 🚀 Ejecución

### Método 1: Ejecución Directa (Bloqueante)

**Linux/WSL:**
```bash
./DIndex_test 2>&1 | tee dindex_run.log
```

**Windows PowerShell:**
```powershell
wsl bash -c './DIndex_test 2>&1 | tee dindex_run.log'
```

### Método 2: Ejecución en Background (PowerShell - RECOMENDADO)

```powershell
# Opción A: Con el script automatizado (incluye monitoreo)
.\run_tests.ps1

# Opción B: Con el script sin monitoreo automático
.\run_tests.ps1 -NoMonitor

# Opción C: Limpiar resultados anteriores antes de ejecutar
.\run_tests.ps1 -Clean
```

### Método 3: Ejecución en Background (Linux/WSL)

```bash
# Con el script automatizado
chmod +x run_tests.sh
./run_tests.sh

# O manualmente
nohup ./DIndex_test > dindex_run.log 2>&1 &
echo $! > dindex.pid
```

## 📊 Monitoreo del Progreso

### Windows PowerShell

**Opción 1: Script automático con auto-refresh**
```powershell
.\check_progress.ps1
```
Actualiza cada 10 segundos automáticamente.

**Opción 2: Manualmente con PowerShell Job**
```powershell
# Ver estado del job
Get-Job -Name DIndex

# Ver últimas 30 líneas del output
Receive-Job -Name DIndex -Keep | Select-Object -Last 30

# Ver todo el output
Receive-Job -Name DIndex -Keep
```

**Opción 3: Ver log directamente**
```powershell
wsl tail -f dindex_run.log
```

### Linux/WSL

```bash
# Ver últimas líneas del log
tail -30 dindex_run.log

# Seguir el log en tiempo real
tail -f dindex_run.log

# Ver estado del proceso
ps aux | grep DIndex_test
```

## 📈 Interpretación del Progreso

### Fase 1: BUILD (Construcción del índice)

```
[BUILD] Construyendo D-index con 4 niveles y rho=5...
[BUILD] Cargando 1073728 objetos...
  Cargados 10000 objetos (0%)
  Cargados 100000 objetos (9%)
  ...
[BUILD] Iniciando construcción del índice...
[BUILD] OK - D-index construido
DIndex stats: levels=4 rho=5
Number of buckets: 142
Total indexed objects: 1073728
```

**Progreso**: Muestra cada 10,000 objetos cargados con porcentaje

### Fase 2: MRQ (Range Queries)

```
[MRQ] Ejecutando experimentos con 5 selectividades...
  [MRQ] sel=0.02 R=12.345 ... OK
  [MRQ] sel=0.04 R=15.678 ... OK
  ...
```

**Progreso**: 5 líneas (una por selectividad)

### Fase 3: MkNN (k-Nearest Neighbors)

```
[MkNN] Ejecutando experimentos con 5 valores de k...
  [MkNN] k=5 ... OK
  [MkNN] k=10 ... OK
  ...
```

**Progreso**: 5 líneas (una por valor de k)

### Finalización

```
[DONE] Archivo generado: results/results_DIndex_LA.json
```

## ⏱️ Tiempos Estimados

| Dataset   | Objetos   | Tiempo Estimado | Fase Lenta |
|-----------|-----------|-----------------|------------|
| LA        | 1,073,728 | ~45-60 min      | BUILD      |
| Words     | 597,193   | ~25-35 min      | BUILD      |
| Synthetic | 28,659    | ~3-5 min        | MRQ/MkNN   |
| **TOTAL** |           | **~75-100 min** |            |

**Nota**: Los tiempos dependen del hardware. LA es el más lento por su tamaño.

## ✅ Verificación de Resultados

### Después de la Ejecución

**PowerShell:**
```powershell
# Listar archivos generados
Get-ChildItem results\*.json

# Contar líneas de cada archivo (debe ser 12 cada uno)
Get-ChildItem results\*.json | ForEach-Object {
    $lines = (Get-Content $_.FullName | Measure-Object -Line).Lines
    Write-Host "$($_.Name): $lines líneas"
}

# Ver contenido de un archivo
Get-Content results\results_DIndex_LA.json
```

**Linux/WSL:**
```bash
# Listar archivos
ls -lh results/*.json

# Contar líneas
wc -l results/*.json

# Ver contenido
cat results/results_DIndex_LA.json
```

### Formato Esperado

Cada archivo debe tener exactamente **12 líneas**:
- 1 línea: `[` (apertura)
- 10 líneas: experimentos (5 MRQ + 5 MkNN)
- 1 línea: `]` (cierre)

## 🛑 Detener la Ejecución

### Windows PowerShell

```powershell
# Si usaste el script
Stop-Job -Name DIndex
Remove-Job -Name DIndex

# O forzar
wsl pkill -9 DIndex_test
```

### Linux/WSL

```bash
# Con el PID guardado
kill $(cat dindex.pid)

# O buscar el proceso
pkill DIndex_test

# Forzar si no responde
pkill -9 DIndex_test
```

## 🔍 Troubleshooting

### Problema 1: "make: command not found"

**Solución:**
```bash
# Ubuntu/Debian
sudo apt-get install build-essential

# Arch Linux
sudo pacman -S base-devel
```

### Problema 2: Error de compilación con ObjectDB

**Verificar** que existen:
- `../../objectdb.hpp`
- `../../datasets/paths.hpp`

**Solución**: Asegurarse de estar en el directorio correcto.

### Problema 3: No se generan archivos JSON

**Verificar**:
```powershell
# ¿El proceso está corriendo?
Get-Job -Name DIndex  # PowerShell
ps aux | grep DIndex  # Linux

# ¿Hay errores en el log?
Get-Content dindex_run.log -Tail 50
```

### Problema 4: "Dataset no encontrado"

**Causa**: Archivos de dataset no están en la ubicación esperada.

**Solución**: Verificar rutas en `../../datasets/paths.hpp`

### Problema 5: Ejecución muy lenta

**Normal para LA**: 1M+ objetos toma tiempo.

**Verificar progreso**:
- Debe mostrar "Cargados X objetos" periódicamente
- Si se congela, revisar memoria disponible

## 📄 Formato de Salida JSON

Cada experimento genera un objeto JSON con:

```json
{
  "index": "DIndex",
  "dataset": "LA",
  "category": "D",
  "num_levels": 4,
  "rho": 5.0,
  "query_type": "MRQ",
  "selectivity": 0.02,
  "radius": 12.345,
  "k": null,
  "compdists": 1234.567890,
  "time_ms": 12.345678,
  "pages": 123.456789,
  "n_queries": 100,
  "run_id": 1
}
```

**Métricas**:
- `compdists`: Promedio de cálculos de distancia por query
- `time_ms`: Tiempo promedio por query (milisegundos)
- `pages`: Promedio de lecturas de página por query
- `n_queries`: Número de queries (100)

## 📝 Ejemplo de Sesión Completa

```powershell
# 1. Compilar
cd secondary_memory\D-index
wsl bash -c 'make clean && make'

# 2. Ejecutar en background con monitoreo
.\run_tests.ps1

# 3. Esperar a que termine (o monitorear manualmente)
.\check_progress.ps1

# 4. Verificar resultados
Get-ChildItem results\*.json | ForEach-Object {
    $lines = (Get-Content $_.FullName | Measure-Object -Line).Lines
    Write-Host "$($_.Name): $lines líneas"
}

# 5. Ver un archivo
Get-Content results\results_DIndex_LA.json | ConvertFrom-Json | ConvertTo-Json -Depth 10
```

## 📚 Referencias

- **Paper**: Chen et al. (2022) - Section 5.6 D-index Family
- **Configuración**: Similar a EGNAT y OmniR-tree (configuración fija)
- **Formato**: JSON compatible con framework de benchmarking

---

**Generado**: 2025-11-20  
**Versión**: D-index v1.0
