# run_tests.ps1
# Script para ejecutar tests de SPB-tree en background con monitoreo automático
# Uso: .\run_tests.ps1 [-NoMonitor] [-Clean]

param(
    [switch]$NoMonitor = $false,
    [switch]$Clean = $false
)

Write-Host "`n==== SPB-Tree Test Runner ====" -ForegroundColor Cyan
Write-Host "Configuración: 4 pivotes, leafCap=128, fanout=64" -ForegroundColor Gray
Write-Host "Datasets: LA, Words, Synthetic" -ForegroundColor Gray
Write-Host "Experimentos: 10 por dataset (5 MRQ + 5 MkNN)" -ForegroundColor Gray

# Verificar que estamos en el directorio correcto
if (!(Test-Path "test.cpp")) {
    Write-Host "`n[ERROR] No se encuentra test.cpp" -ForegroundColor Red
    Write-Host "Asegúrate de estar en el directorio secondary_memory/SPB-tree" -ForegroundColor Yellow
    exit 1
}

# Limpiar resultados anteriores si se solicitó
if ($Clean) {
    Write-Host "`n[CLEAN] Limpiando resultados anteriores..." -ForegroundColor Yellow
    if (Test-Path "results") {
        Remove-Item -Recurse -Force "results"
        Write-Host "  - Eliminado: results/" -ForegroundColor Gray
    }
    if (Test-Path "spbtree_indexes") {
        Remove-Item -Recurse -Force "spbtree_indexes"
        Write-Host "  - Eliminado: spbtree_indexes/" -ForegroundColor Gray
    }
    if (Test-Path "spbtree_run.log") {
        Remove-Item "spbtree_run.log"
        Write-Host "  - Eliminado: spbtree_run.log" -ForegroundColor Gray
    }
}

# Verificar compilación
Write-Host "`n[COMPILE] Verificando compilación..." -ForegroundColor Cyan
if (!(Test-Path "SPBTree_test")) {
    Write-Host "  Ejecutable no encontrado, compilando..." -ForegroundColor Yellow
    wsl bash -c 'make clean && make'
    if ($LASTEXITCODE -ne 0) {
        Write-Host "`n[ERROR] Compilación falló" -ForegroundColor Red
        exit 1
    }
    Write-Host "  Compilación exitosa" -ForegroundColor Green
} else {
    Write-Host "  Ejecutable encontrado: SPBTree_test" -ForegroundColor Green
}

# Limpiar jobs anteriores
$existingJob = Get-Job -Name "SPBTree" -ErrorAction SilentlyContinue
if ($existingJob) {
    Write-Host "`n[WARN] Job 'SPBTree' ya existe, limpiando..." -ForegroundColor Yellow
    Stop-Job -Name "SPBTree" -ErrorAction SilentlyContinue
    Remove-Job -Name "SPBTree" -ErrorAction SilentlyContinue
}

# Iniciar ejecución en background
Write-Host "`n[START] Iniciando ejecución en background..." -ForegroundColor Cyan
$job = Start-Job -Name "SPBTree" -ScriptBlock {
    Set-Location $using:PWD
    wsl bash -c './SPBTree_test 2>&1 | tee spbtree_run.log'
}

Write-Host "  Job iniciado con ID: $($job.Id)" -ForegroundColor Green
Write-Host "  Estado: $($job.State)" -ForegroundColor Gray

# Esperar un momento para que inicie
Start-Sleep -Seconds 3

# Verificar que el job está corriendo
$jobState = (Get-Job -Name "SPBTree").State
if ($jobState -ne "Running") {
    Write-Host "`n[ERROR] El job no está corriendo (Estado: $jobState)" -ForegroundColor Red
    Write-Host "`nOutput del job:" -ForegroundColor Yellow
    Receive-Job -Name "SPBTree"
    Remove-Job -Name "SPBTree"
    exit 1
}

Write-Host "`n[OK] SPB-tree ejecutándose en background" -ForegroundColor Green

# Mostrar información de monitoreo
Write-Host "`n==== Información de Monitoreo ====" -ForegroundColor Cyan

Write-Host "`nTiempos estimados:" -ForegroundColor Yellow
Write-Host "  LA:        ~50-70 minutos" -ForegroundColor Gray
Write-Host "  Words:     ~30-40 minutos" -ForegroundColor Gray
Write-Host "  Synthetic: ~4-6 minutos" -ForegroundColor Gray
Write-Host "  TOTAL:     ~85-115 minutos" -ForegroundColor Gray

Write-Host "`nComandos útiles:" -ForegroundColor Yellow
Write-Host "  Ver progreso:    .\check_progress.ps1" -ForegroundColor White
Write-Host "  Ver estado:      Get-Job -Name SPBTree" -ForegroundColor White
Write-Host "  Ver últimas 30:  Receive-Job -Name SPBTree -Keep | Select-Object -Last 30" -ForegroundColor White
Write-Host "  Ver todo:        Receive-Job -Name SPBTree -Keep" -ForegroundColor White
Write-Host "  Detener:         Stop-Job -Name SPBTree; Remove-Job -Name SPBTree" -ForegroundColor White

if (!$NoMonitor) {
    Write-Host "`n[MONITOR] Iniciando monitoreo automático..." -ForegroundColor Cyan
    Write-Host "Presiona Ctrl+C para salir del monitoreo (el test seguirá corriendo)" -ForegroundColor Yellow
    Write-Host "=" * 80 -ForegroundColor Gray
    
    $iteration = 0
    while ($true) {
        Start-Sleep -Seconds 10
        $iteration++
        
        # Verificar estado del job
        $job = Get-Job -Name "SPBTree" -ErrorAction SilentlyContinue
        if (!$job) {
            Write-Host "`n[ERROR] Job no encontrado" -ForegroundColor Red
            break
        }
        
        $state = $job.State
        
        # Mostrar timestamp
        $timestamp = Get-Date -Format "HH:mm:ss"
        Write-Host "`n[$timestamp] Check #$iteration - Estado: $state" -ForegroundColor Cyan
        
        # Obtener últimas líneas
        $output = Receive-Job -Name "SPBTree" -Keep | Select-Object -Last 35
        
        if ($output) {
            foreach ($line in $output) {
                # Colorear según el contenido
                if ($line -match "Cargados|BUILD") {
                    Write-Host $line -ForegroundColor Yellow
                }
                elseif ($line -match "OK|DONE|construido") {
                    Write-Host $line -ForegroundColor Green
                }
                elseif ($line -match "MRQ|MkNN|sel=|k=") {
                    Write-Host $line -ForegroundColor Cyan
                }
                elseif ($line -match "ERROR|error|Error") {
                    Write-Host $line -ForegroundColor Red
                }
                elseif ($line -match "SPB-tree|pivots|records|SFC") {
                    Write-Host $line -ForegroundColor Magenta
                }
                else {
                    Write-Host $line -ForegroundColor Gray
                }
            }
        }
        
        # Verificar si terminó
        if ($state -eq "Completed") {
            Write-Host "`n" + ("=" * 80) -ForegroundColor Green
            Write-Host "[COMPLETADO] Ejecución finalizada!" -ForegroundColor Green
            Write-Host ("=" * 80) -ForegroundColor Green
            
            # Mostrar resultados
            Write-Host "`n[RESULTS] Archivos generados:" -ForegroundColor Cyan
            if (Test-Path "results") {
                Get-ChildItem "results\*.json" | ForEach-Object {
                    $lines = (Get-Content $_.FullName | Measure-Object -Line).Lines
                    $status = if ($lines -eq 12) { "[OK]" } else { "[WARN]" }
                    $color = if ($lines -eq 12) { "Green" } else { "Yellow" }
                    Write-Host "  $status $($_.Name): $lines líneas" -ForegroundColor $color
                }
            } else {
                Write-Host "  [WARN] Directorio results/ no encontrado" -ForegroundColor Yellow
            }
            
            break
        }
        elseif ($state -eq "Failed") {
            Write-Host "`n[ERROR] El job falló" -ForegroundColor Red
            Write-Host "`nÚltimas líneas del output:" -ForegroundColor Yellow
            Receive-Job -Name "SPBTree" -Keep | Select-Object -Last 50
            break
        }
        
        Write-Host "`n" + ("-" * 80) -ForegroundColor DarkGray
        Write-Host "Próxima actualización en 10 segundos... (Ctrl+C para salir)" -ForegroundColor DarkGray
    }
    
    # Limpiar job
    Write-Host "`n[CLEANUP] Limpiando job..." -ForegroundColor Gray
    Remove-Job -Name "SPBTree" -ErrorAction SilentlyContinue
    
} else {
    Write-Host "`n[INFO] Modo sin monitoreo. El test corre en background." -ForegroundColor Cyan
    Write-Host "Usa los comandos arriba para monitorear manualmente." -ForegroundColor Gray
}

Write-Host "`n[DONE] Script finalizado" -ForegroundColor Green
Write-Host "=" * 80 -ForegroundColor Gray
