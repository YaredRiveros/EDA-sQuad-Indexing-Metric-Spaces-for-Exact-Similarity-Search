# OmniR-tree - Guía de Ejecución y Monitoreo

## Descripción

OmniR-tree es una estructura de indexación de memoria secundaria basada en la familia Omni que combina:
- **Tabla de pivotes**: Para mapear objetos a espacio de distancias
- **R-tree**: Para indexar los vectores mapeados
- **RAF**: Random Access File para almacenar objetos

## Configuración

- **Pivotes**: 8 (configuración fija, como EGNAT)
- **Nodos R-tree**: 32 entradas máximas por nodo
- **Datasets**: LA, Words, Synthetic
- **Experimentos por dataset**: 10 (5 MRQ + 5 MkNN)
- **Total esperado**: 30 líneas JSON

## Compilación

```bash
# En Linux/WSL
cd secondary_memory/Omni-rtree
make clean && make

# O directamente con g++
g++ -std=c++17 -O3 -Wall -Wno-unused-result -Wno-unused-function -o OmniRTree_test test.cpp
```

En Windows con WSL:
```powershell
cd c:\Users\yared\Documents\eda-codigos\proyecto-paper\EDA-sQuad-Indexing-Metric-Spaces-for-Exact-Similarity-Search\secondary_memory\Omni-rtree
wsl bash -c 'make clean && make'
```

## Ejecución

### Opción 1: Ejecución Directa (Bloqueante)
```bash
./OmniRTree_test
```

### Opción 2: Ejecución en Background (Recomendado)

**En PowerShell:**
```powershell
# Iniciar job en background
$job = Start-Job -Name OmniRTree -ScriptBlock { 
    Set-Location 'c:\Users\yared\Documents\eda-codigos\proyecto-paper\EDA-sQuad-Indexing-Metric-Spaces-for-Exact-Similarity-Search\secondary_memory\Omni-rtree'
    wsl bash -c './OmniRTree_test 2>&1' 
}

Write-Host "Job iniciado con ID: $($job.Id)"
```

**En Linux/WSL:**
```bash
nohup ./OmniRTree_test > omni_execution.log 2>&1 &
echo $!  # Muestra el PID del proceso
```

## Monitoreo del Progreso

### Método 1: Script Automático de Monitoreo

```powershell
# Ejecutar el script de monitoreo (actualiza cada 10 segundos)
.\check_progress.ps1
```

### Método 2: Comandos Manuales en PowerShell

```powershell
# Ver últimas 20 líneas del job
Receive-Job -Name OmniRTree -Keep | Select-Object -Last 20

# Ver estado del job
Get-Job -Name OmniRTree

# Monitoreo continuo (actualiza cada 30 segundos)
while ($true) { 
    Clear-Host
    Write-Host "=== $(Get-Date -Format 'HH:mm:ss') ===" -ForegroundColor Cyan
    Receive-Job -Name OmniRTree -Keep | Select-Object -Last 15
    Start-Sleep -Seconds 30 
}
```

### Método 3: Monitoreo en Linux/WSL

```bash
# Ver log en tiempo real
tail -f omni_execution.log

# Ver últimas 30 líneas
tail -30 omni_execution.log

# Ver progreso de construcción
grep "Indexados" omni_execution.log | tail -5

# Ver progreso de queries
grep -E "sel=|k=" omni_execution.log | tail -10
```

## Interpretación del Progreso

### Fase 1: Construcción del Índice
```
[BUILD] Construyendo OmniR-tree con 8 pivotes...
[BUILD] Pivotes seleccionados: 8
[BUILD] Iniciando mapeo e inserción de N objetos...
  Indexados 1000 objetos (X%)
  Indexados 2000 objetos (X%)
  ...
[BUILD] Total objetos indexados: N (100%)
[BUILD] Completado en XXXX ms
```

**Tiempos estimados de construcción:**
- **Synthetic** (~29K objetos): 30-45 segundos
- **Words** (~597K objetos): 10-15 minutos
- **LA** (~1M objetos): 25-30 minutos

### Fase 2: Queries MRQ (Range)
```
[MRQ] Ejecutando queries de rango...
  sel=0.02 (R=XXX)... OK (avg XXXX compdists)
  sel=0.04 (R=XXX)... OK (avg XXXX compdists)
  sel=0.08 (R=XXX)... OK (avg XXXX compdists)
  sel=0.16 (R=XXX)... OK (avg XXXX compdists)
  sel=0.32 (R=XXX)... OK (avg XXXX compdists)
```

**Tiempo estimado por dataset:** 5-10 minutos

### Fase 3: Queries MkNN (k-NN)
```
[MkNN] Ejecutando queries k-NN...
  k=5... OK (avg XXXX compdists)
  k=10... OK (avg XXXX compdists)
  k=20... OK (avg XXXX compdists)
  k=50... OK (avg XXXX compdists)
  k=100... OK (avg XXXX compdists)
```

**Tiempo estimado por dataset:** 3-5 minutos

### Finalización
```
[DONE] Archivo generado: results/results_OmniRTree_DATASET.json
==========================================
```

## Tiempos Totales Estimados

| Dataset   | Construcción | Queries | Total      |
|-----------|--------------|---------|------------|
| Synthetic | 30-45s       | 2-3min  | 3-4min     |
| Words     | 10-15min     | 8-12min | 20-25min   |
| LA        | 25-30min     | 10-15min| 35-45min   |
| **TOTAL** |              |         | **60-75min**|

## Verificación de Resultados

### Verificar que se generaron los archivos JSON

```powershell
# PowerShell
Get-ChildItem results\*.json | Select-Object Name, Length, LastWriteTime
```

```bash
# Linux/WSL
ls -lh results/*.json
```

### Verificar formato de los JSON

```powershell
# PowerShell - Contar líneas de cada JSON
Get-ChildItem results\*.json | ForEach-Object {
    $lines = (Get-Content $_.FullName | Measure-Object -Line).Lines
    Write-Host "$($_.Name): $lines líneas"
}
```

```bash
# Linux/WSL - Contar líneas
wc -l results/*.json
```

**Esperado:** Cada archivo debe tener **12 líneas** (1 apertura + 10 experimentos + 1 cierre)

### Validar contenido JSON

```powershell
# PowerShell - Ver primeros 3 experimentos
Get-Content results\results_OmniRTree_LA.json | Select-Object -First 5
```

```bash
# Linux - Pretty print
jq '.[0:2]' results/results_OmniRTree_LA.json
```

## Detener la Ejecución

### En PowerShell (si está en Job)
```powershell
Stop-Job -Name OmniRTree
Remove-Job -Name OmniRTree
```

### En Linux/WSL (proceso en background)
```bash
# Encontrar PID
ps aux | grep OmniRTree_test

# Detener proceso
kill -TERM <PID>

# Forzar si no responde
kill -9 <PID>
```

## Limpieza de Archivos

```powershell
# PowerShell
Remove-Item results -Recurse -Force
Remove-Item omni_*.log -Force
Remove-Item index_omni_* -Force
Remove-Item omni_raf.bin -Force
```

```bash
# Linux/WSL
make clean
# O manualmente:
rm -rf results
rm -f omni_*.log index_omni_* omni_raf.bin
```

## Solución de Problemas

### Problema: Job se queda congelado
```powershell
# Verificar si realmente está congelado
Get-Job -Name OmniRTree | Format-List *

# Ver si hay salida nueva
Receive-Job -Name OmniRTree -Keep | Select-Object -Last 5
```

**Solución:** La construcción del índice es lenta para datasets grandes. Esperar o verificar log.

### Problema: WSL da error "Failed to start systemd"
```powershell
# Ignorar este warning, no afecta la ejecución
# El proceso continuará normalmente
```

### Problema: Compilación falla
```bash
# Verificar que tienes g++ instalado
g++ --version

# Instalar si es necesario (Ubuntu/Debian)
sudo apt-get install g++

# Verificar que objectdb.hpp existe
ls -la ../../objectdb.hpp
```

### Problema: "Dataset no encontrado"
```bash
# Verificar rutas en paths.hpp
cat ../../datasets/paths.hpp

# Verificar que los archivos existen
ls -la ../../datasets/*.txt
```

## Estructura de Salida JSON

Cada experimento tiene el siguiente formato:

```json
{
  "index": "OmniR-tree",
  "dataset": "LA",
  "category": "DM",
  "num_pivots": 8,
  "num_centers_path": null,
  "arity": null,
  "query_type": "MRQ",
  "selectivity": 0.02,
  "radius": 408.823460,
  "k": null,
  "compdists": 12345.67,
  "time_ms": 123.456,
  "pages": 1234.56,
  "n_queries": 100,
  "run_id": 1
}
```

**Campos importantes:**
- `compdists`: Promedio de cálculos de distancia por query
- `time_ms`: Tiempo promedio por query en milisegundos
- `pages`: Promedio de accesos a páginas de disco
- `n_queries`: Número de queries ejecutadas (100)

## Ejemplo Completo de Sesión

```powershell
# 1. Compilar
cd c:\Users\yared\Documents\eda-codigos\proyecto-paper\EDA-sQuad-Indexing-Metric-Spaces-for-Exact-Similarity-Search\secondary_memory\Omni-rtree
wsl bash -c 'make clean && make'

# 2. Iniciar ejecución en background
$job = Start-Job -Name OmniRTree -ScriptBlock { 
    Set-Location 'c:\Users\yared\Documents\eda-codigos\proyecto-paper\EDA-sQuad-Indexing-Metric-Spaces-for-Exact-Similarity-Search\secondary_memory\Omni-rtree'
    wsl bash -c './OmniRTree_test 2>&1' 
}

# 3. Monitorear progreso
.\check_progress.ps1

# 4. O verificar manualmente cada minuto
Receive-Job -Name OmniRTree -Keep | Select-Object -Last 20

# 5. Esperar a que termine (opcional)
Wait-Job -Name OmniRTree

# 6. Ver resultado final completo
Receive-Job -Name OmniRTree

# 7. Verificar archivos generados
Get-ChildItem results\*.json

# 8. Limpiar job
Remove-Job -Name OmniRTree
```

## Contacto y Soporte

Si encuentras problemas:
1. Verifica que WSL esté funcionando: `wsl --list`
2. Verifica que g++ esté instalado: `wsl g++ --version`
3. Revisa los logs de error
4. Asegúrate de que los datasets existen en `../../datasets/`
