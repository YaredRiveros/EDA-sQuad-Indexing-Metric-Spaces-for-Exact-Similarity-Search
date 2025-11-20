# SPB-Tree: Tests de Memoria Secundaria

Implementación y tests del **SPB-tree** (Space-filling curve + Pivot + B+-tree) para espacios métricos con soporte de memoria secundaria, siguiendo el formato del paper de Chen et al. (2022).

## 📋 Descripción

El SPB-tree combina:
- **Pivot mapping**: Distancias precomputadas a pivotes
- **SFC mapping**: Curva Z-order (Morton) para mapeo espacial
- **B+-tree bulk-load**: Índice en memoria con MBB para pruning
- **Configuración fija**: 4 pivotes, leafCap=128, fanout=64

## ⚙️ Configuración

```cpp
int numPivots = 4;        // Número de pivotes (fijo)
int leafCap = 128;        // Capacidad de hojas B+
int fanout = 64;          // Branching factor
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

Esto generará el ejecutable `SPBTree_test`.

## 🚀 Ejecución

### Método 1: Ejecución Directa (Bloqueante)

**Linux/WSL:**
```bash
./SPBTree_test 2>&1 | tee spbtree_run.log
```

**Windows PowerShell:**
```powershell
wsl bash -c './SPBTree_test 2>&1 | tee spbtree_run.log'
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
nohup ./SPBTree_test > spbtree_run.log 2>&1 &
echo $! > spbtree.pid
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
Get-Job -Name SPBTree

# Ver últimas 30 líneas del output
Receive-Job -Name SPBTree -Keep | Select-Object -Last 30

# Ver todo el output
Receive-Job -Name SPBTree -Keep
```

**Opción 3: Ver log directamente**
```powershell
wsl tail -f spbtree_run.log
```

### Linux/WSL

```bash
# Ver últimas líneas del log
tail -30 spbtree_run.log

# Seguir el log en tiempo real
tail -f spbtree_run.log

# Ver estado del proceso
ps aux | grep SPBTree_test
```

## 📈 Interpretación del Progreso

### Fase 1: BUILD (Construcción del índice)

```
[BUILD] Construyendo SPB-tree con 4 pivotes, leafCap=128, fanout=64...
[BUILD] Cargando 1073728 objetos...
  Cargados 10000 objetos (0%)
  Cargados 100000 objetos (9%)
  ...
[BUILD] Iniciando construcción del índice...
[BUILD] OK - SPB-tree construido
SPB-tree: pivots=4, records=1073728, SFC bits/dim=16
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
[DONE] Archivo generado: results/results_SPBTree_LA.json
```

## ⏱️ Tiempos Estimados

| Dataset   | Objetos   | Tiempo Estimado | Fase Lenta |
|-----------|-----------|-----------------|------------|
| LA        | 1,073,728 | ~50-70 min      | BUILD      |
| Words     | 597,193   | ~30-40 min      | BUILD      |
| Synthetic | 28,659    | ~4-6 min        | MRQ/MkNN   |
| **TOTAL** |           | **~85-115 min** |            |

**Nota**: Los tiempos dependen del hardware. El SPB-tree puede ser ligeramente más lento que otras estructuras debido al mapeo SFC.

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
Get-Content results\results_SPBTree_LA.json
```

**Linux/WSL:**
```bash
# Listar archivos
ls -lh results/*.json

# Contar líneas
wc -l results/*.json

# Ver contenido
cat results/results_SPBTree_LA.json
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
Stop-Job -Name SPBTree
Remove-Job -Name SPBTree

# O forzar
wsl pkill -9 SPBTree_test
```

### Linux/WSL

```bash
# Con el PID guardado
kill $(cat spbtree.pid)

# O buscar el proceso
pkill SPBTree_test

# Forzar si no responde
pkill -9 SPBTree_test
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
Get-Job -Name SPBTree  # PowerShell
ps aux | grep SPBTree  # Linux

# ¿Hay errores en el log?
Get-Content spbtree_run.log -Tail 50
```

### Problema 4: "Dataset no encontrado"

**Causa**: Archivos de dataset no están en la ubicación esperada.

**Solución**: Verificar rutas en `../../datasets/paths.hpp`

### Problema 5: Ejecución muy lenta

**Normal para LA**: 1M+ objetos toma tiempo, especialmente con mapeo SFC.

**Verificar progreso**:
- Debe mostrar "Cargados X objetos" periódicamente
- Si se congela, revisar memoria disponible

## 📄 Formato de Salida JSON

Cada experimento genera un objeto JSON con:

```json
{
  "index": "SPBTree",
  "dataset": "LA",
  "category": "SPB",
  "num_pivots": 4,
  "leaf_cap": 128,
  "fanout": 64,
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

**Parámetros específicos de SPB-tree**:
- `num_pivots`: Número de pivotes usados (4)
- `leaf_cap`: Capacidad de hojas del B+ (128)
- `fanout`: Branching factor del B+ (64)

## 📝 Ejemplo de Sesión Completa

```powershell
# 1. Compilar
cd secondary_memory\SPB-tree
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
Get-Content results\results_SPBTree_LA.json | ConvertFrom-Json | ConvertTo-Json -Depth 10
```

## 📚 Referencias

- **Paper**: Chen et al. (2022) - SPB-tree (Space-filling + Pivot + B+-tree)
- **SFC**: Morton Z-order curve para mapeo multidimensional
- **Configuración**: Similar a EGNAT y OmniR-tree (configuración fija)
- **Formato**: JSON compatible con framework de benchmarking

## 🔬 Detalles Técnicos

### Space-Filling Curve (SFC)

El SPB-tree usa **Morton Z-order** para mapear vectores de distancias a pivotes (espacio multidimensional) a una curva unidimensional:

1. **Normalización**: Cada dimensión se normaliza a [0, 1]
2. **Discretización**: Cada valor se convierte a entero de N bits
3. **Interleaving**: Los bits se entrelazan para crear una clave Morton 64-bit

**Bits por dimensión**: `bits_per_dim = 64 / num_pivots`
- Para 4 pivotes: 16 bits/dimensión
- Total: 64 bits de clave Morton

### B+-tree Bulk-Load

1. Ordenar todos los objetos por clave Morton
2. Particionar en hojas de capacidad `leafCap`
3. Construir niveles superiores con fanout `fanout`
4. Cada nodo almacena MBB (Minimum Bounding Box) para pruning

### Pruning Strategies

- **Lemma 4.1**: Lower bound usando MBB
- **Lemma 4.5**: Pivot validation para eliminación temprana

---

**Generado**: 2025-11-20  
**Versión**: SPB-tree v1.0
