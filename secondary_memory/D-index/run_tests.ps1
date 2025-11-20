# run_tests.ps1
# Script para ejecutar tests de D-index en background con monitoreo automático
# Uso: .\run_tests.ps1 [-NoMonitor] [-Clean]

param(
    [switch]$NoMonitor = $false,
    [switch]$Clean = $false
)

Write-Host "`n==== D-Index Test Runner ====" -ForegroundColor Cyan
Write-Host "Configuración: 4 niveles, rho=5.0" -ForegroundColor Gray
Write-Host "Datasets: LA, Words, Synthetic" -ForegroundColor Gray
Write-Host "Experimentos: 10 por dataset (5 MRQ + 5 MkNN)" -ForegroundColor Gray

# Verificar que estamos en el directorio correcto
if (!(Test-Path "test.cpp")) {
    Write-Host "`n[ERROR] No se encuentra test.cpp" -ForegroundColor Red
    Write-Host "Asegúrate de estar en el directorio secondary_memory/D-index" -ForegroundColor Yellow
    exit 1
}

# Limpiar resultados anteriores si se solicitó
if ($Clean) {
    Write-Host "`n[CLEAN] Limpiando resultados anteriores..." -ForegroundColor Yellow
    if (Test-Path "results") {
        Remove-Item -Recurse -Force "results"
        Write-Host "  - Eliminado: results/" -ForegroundColor Gray
    }
    if (Test-Path "dindex_indexes") {
        Remove-Item -Recurse -Force "dindex_indexes"
        Write-Host "  - Eliminado: dindex_indexes/" -ForegroundColor Gray
    }
    if (Test-Path "dindex_run.log") {
        Remove-Item "dindex_run.log"
        Write-Host "  - Eliminado: dindex_run.log" -ForegroundColor Gray
    }
}

# Verificar compilación
Write-Host "`n[COMPILE] Verificando compilación..." -ForegroundColor Cyan
if (!(Test-Path "DIndex_test")) {
    Write-Host "  Ejecutable no encontrado, compilando..." -ForegroundColor Yellow
    wsl bash -c 'make clean && make'
    if ($LASTEXITCODE -ne 0) {
        Write-Host "`n[ERROR] Compilación falló" -ForegroundColor Red
        exit 1
    }
    Write-Host "  Compilación exitosa" -ForegroundColor Green
} else {
    Write-Host "  Ejecutable encontrado: DIndex_test" -ForegroundColor Green
}

# Limpiar jobs anteriores
$existingJob = Get-Job -Name "DIndex" -ErrorAction SilentlyContinue
if ($existingJob) {
    Write-Host "`n[WARN] Job 'DIndex' ya existe, limpiando..." -ForegroundColor Yellow
    Stop-Job -Name "DIndex" -ErrorAction SilentlyContinue
    Remove-Job -Name "DIndex" -ErrorAction SilentlyContinue
}

# Iniciar ejecución en background
Write-Host "`n[START] Iniciando ejecución en background..." -ForegroundColor Cyan
$job = Start-Job -Name "DIndex" -ScriptBlock {
    Set-Location $using:PWD
    wsl bash -c './DIndex_test 2>&1 | tee dindex_run.log'
}

Write-Host "  Job iniciado con ID: $($job.Id)" -ForegroundColor Green
Write-Host "  Estado: $($job.State)" -ForegroundColor Gray

# Esperar un momento para que inicie
Start-Sleep -Seconds 3

# Verificar que el job está corriendo
$jobState = (Get-Job -Name "DIndex").State
if ($jobState -ne "Running") {
    Write-Host "`n[ERROR] El job no está corriendo (Estado: $jobState)" -ForegroundColor Red
    Write-Host "`nOutput del job:" -ForegroundColor Yellow
    Receive-Job -Name "DIndex"
    Remove-Job -Name "DIndex"
    exit 1
}

Write-Host "`n[OK] D-index ejecutándose en background" -ForegroundColor Green

# Mostrar información de monitoreo
Write-Host "`n==== Información de Monitoreo ====" -ForegroundColor Cyan

Write-Host "`nTiempos estimados:" -ForegroundColor Yellow
Write-Host "  LA:        ~45-60 minutos" -ForegroundColor Gray
Write-Host "  Words:     ~25-35 minutos" -ForegroundColor Gray
Write-Host "  Synthetic: ~3-5 minutos" -ForegroundColor Gray
Write-Host "  TOTAL:     ~75-100 minutos" -ForegroundColor Gray

Write-Host "`nComandos útiles:" -ForegroundColor Yellow
Write-Host "  Ver progreso:    .\check_progress.ps1" -ForegroundColor White
Write-Host "  Ver estado:      Get-Job -Name DIndex" -ForegroundColor White
Write-Host "  Ver últimas 30:  Receive-Job -Name DIndex -Keep | Select-Object -Last 30" -ForegroundColor White
Write-Host "  Ver todo:        Receive-Job -Name DIndex -Keep" -ForegroundColor White
Write-Host "  Detener:         Stop-Job -Name DIndex; Remove-Job -Name DIndex" -ForegroundColor White

if (!$NoMonitor) {
    Write-Host "`n[MONITOR] Iniciando monitoreo automático..." -ForegroundColor Cyan
    Write-Host "Presiona Ctrl+C para salir del monitoreo (el test seguirá corriendo)" -ForegroundColor Yellow
    Write-Host "=" * 80 -ForegroundColor Gray
    
    $iteration = 0
    while ($true) {
        Start-Sleep -Seconds 10
        $iteration++
        
        # Verificar estado del job
        $job = Get-Job -Name "DIndex" -ErrorAction SilentlyContinue
        if (!$job) {
            Write-Host "`n[ERROR] Job no encontrado" -ForegroundColor Red
            break
        }
        
        $state = $job.State
        
        # Mostrar timestamp
        $timestamp = Get-Date -Format "HH:mm:ss"
        Write-Host "`n[$timestamp] Check #$iteration - Estado: $state" -ForegroundColor Cyan
        
        # Obtener últimas líneas
        $output = Receive-Job -Name "DIndex" -Keep | Select-Object -Last 35
        
        if ($output) {
            foreach ($line in $output) {
                # Colorear según el contenido
                if ($line -match "Cargados|BUILD") {
                    Write-Host $line -ForegroundColor Yellow
                }
                elseif ($line -match "OK|DONE") {
                    Write-Host $line -ForegroundColor Green
                }
                elseif ($line -match "MRQ|MkNN") {
                    Write-Host $line -ForegroundColor Cyan
                }
                elseif ($line -match "ERROR|error|Error") {
                    Write-Host $line -ForegroundColor Red
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
            Receive-Job -Name "DIndex" -Keep | Select-Object -Last 50
            break
        }
        
        Write-Host ("-" * 80) -ForegroundColor DarkGray
    }
    
    # Limpiar job
    Write-Host "`n[CLEANUP] Limpiando job..." -ForegroundColor Gray
    Remove-Job -Name "DIndex" -ErrorAction SilentlyContinue
    
} else {
    Write-Host "`n[INFO] Modo sin monitoreo. El test corre en background." -ForegroundColor Cyan
    Write-Host "Usa los comandos arriba para monitorear manualmente." -ForegroundColor Gray
}

Write-Host "`n[DONE] Script finalizado" -ForegroundColor Green
Write-Host "=" * 80 -ForegroundColor Gray
