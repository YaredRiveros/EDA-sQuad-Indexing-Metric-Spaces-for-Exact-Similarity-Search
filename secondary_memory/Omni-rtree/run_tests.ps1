# run_tests.ps1 - Script completo para ejecutar tests de OmniR-tree en Windows
# Uso: .\run_tests.ps1

param(
    [switch]$NoMonitor,  # No mostrar monitoreo automático
    [switch]$Clean       # Limpiar archivos antes de ejecutar
)

$ErrorActionPreference = "Stop"

Write-Host ""
Write-Host "==========================================" -ForegroundColor Cyan
Write-Host "  OmniR-tree Test Execution" -ForegroundColor Cyan
Write-Host "==========================================" -ForegroundColor Cyan
Write-Host ""

# Directorio del proyecto
$ProjectDir = "c:\Users\yared\Documents\eda-codigos\proyecto-paper\EDA-sQuad-Indexing-Metric-Spaces-for-Exact-Similarity-Search\secondary_memory\Omni-rtree"

# Cambiar al directorio del proyecto
Set-Location $ProjectDir

# Limpiar si se solicita
if ($Clean) {
    Write-Host "[CLEAN] Limpiando archivos anteriores..." -ForegroundColor Yellow
    Remove-Item results -Recurse -Force -ErrorAction SilentlyContinue
    Remove-Item omni_*.log -Force -ErrorAction SilentlyContinue
    Remove-Item index_omni_* -Force -ErrorAction SilentlyContinue
    Remove-Item omni_raf.bin -Force -ErrorAction SilentlyContinue
    Write-Host "[CLEAN] Limpieza completada" -ForegroundColor Green
    Write-Host ""
}

# Verificar/compilar ejecutable
Write-Host "[BUILD] Verificando ejecutable..." -ForegroundColor Yellow
if (-not (Test-Path "OmniRTree_test")) {
    Write-Host "[BUILD] Compilando..." -ForegroundColor Yellow
    wsl bash -c 'make clean && make'
    if ($LASTEXITCODE -ne 0) {
        Write-Host "[ERROR] Compilación falló" -ForegroundColor Red
        exit 1
    }
    Write-Host "[BUILD] Compilación exitosa" -ForegroundColor Green
} else {
    Write-Host "[BUILD] Ejecutable encontrado" -ForegroundColor Green
}
Write-Host ""

# Crear directorio de resultados
New-Item -ItemType Directory -Force -Path "results" | Out-Null

# Detener job anterior si existe
$existingJob = Get-Job -Name OmniRTree -ErrorAction SilentlyContinue
if ($existingJob) {
    Write-Host "[WARN] Deteniendo job anterior..." -ForegroundColor Yellow
    Stop-Job -Name OmniRTree -ErrorAction SilentlyContinue
    Remove-Job -Name OmniRTree -ErrorAction SilentlyContinue
}

# Iniciar ejecución en background
Write-Host "[RUN] Iniciando tests en background..." -ForegroundColor Yellow
$job = Start-Job -Name OmniRTree -ScriptBlock { 
    Set-Location $using:ProjectDir
    wsl bash -c './OmniRTree_test 2>&1'
}

Write-Host "[RUN] Job iniciado con ID: $($job.Id)" -ForegroundColor Green
Write-Host "[RUN] Timestamp: $(Get-Date -Format 'yyyy-MM-dd HH:mm:ss')" -ForegroundColor Green
Write-Host ""

# Esperar un poco antes de empezar monitoreo
Start-Sleep -Seconds 5

if ($NoMonitor) {
    Write-Host "=========================================" -ForegroundColor Cyan
    Write-Host "Ejecución iniciada en background" -ForegroundColor Cyan
    Write-Host "=========================================" -ForegroundColor Cyan
    Write-Host ""
    Write-Host "Para monitorear el progreso:" -ForegroundColor Yellow
    Write-Host "  Receive-Job -Name OmniRTree -Keep | Select-Object -Last 20" -ForegroundColor White
    Write-Host ""
    Write-Host "Para ejecutar monitoreo automático:" -ForegroundColor Yellow
    Write-Host "  .\check_progress.ps1" -ForegroundColor White
    Write-Host ""
    Write-Host "Para detener:" -ForegroundColor Yellow
    Write-Host "  Stop-Job -Name OmniRTree; Remove-Job -Name OmniRTree" -ForegroundColor White
    Write-Host ""
} else {
    Write-Host "=========================================" -ForegroundColor Cyan
    Write-Host "Iniciando monitoreo automático..." -ForegroundColor Cyan
    Write-Host "Presiona Ctrl+C para salir del monitoreo" -ForegroundColor Yellow
    Write-Host "(El proceso continuará en background)" -ForegroundColor Yellow
    Write-Host "=========================================" -ForegroundColor Cyan
    Write-Host ""
    Start-Sleep -Seconds 2
    
    # Monitoreo automático
    $lastLineCount = 0
    $checkCount = 0
    
    while ($true) {
        $checkCount++
        $job = Get-Job -Name OmniRTree -ErrorAction SilentlyContinue
        
        if (-not $job) {
            Write-Host ""
            Write-Host "[ERROR] Job no encontrado" -ForegroundColor Red
            break
        }
        
        Clear-Host
        Write-Host "==========================================" -ForegroundColor Cyan
        Write-Host "  OmniR-tree Progress Monitor #$checkCount" -ForegroundColor Cyan
        Write-Host "==========================================" -ForegroundColor Cyan
        Write-Host "Timestamp: $(Get-Date -Format 'HH:mm:ss')" -ForegroundColor Yellow
        Write-Host "Estado: $($job.State)" -ForegroundColor $(
            if ($job.State -eq 'Running') { 'Green' }
            elseif ($job.State -eq 'Completed') { 'Cyan' }
            else { 'Red' }
        )
        Write-Host ""
        
        $output = Receive-Job -Name OmniRTree -Keep 2>&1
        $currentLineCount = $output.Count
        
        if ($currentLineCount -gt $lastLineCount) {
            $lastLineCount = $currentLineCount
        }
        
        # Mostrar últimas 35 líneas con colores
        $output | Select-Object -Last 35 | ForEach-Object {
            $line = $_.ToString()
            if ($line -match 'Indexados|Completado|Total objetos') {
                Write-Host $line -ForegroundColor Yellow
            }
            elseif ($line -match 'BUILD|TESTING|Dataset:') {
                Write-Host $line -ForegroundColor Cyan
            }
            elseif ($line -match 'OK|DONE|generado|Completado') {
                Write-Host $line -ForegroundColor Green
            }
            elseif ($line -match 'sel=|k=|MRQ|MkNN') {
                Write-Host $line -ForegroundColor Magenta
            }
            elseif ($line -match 'ERROR|error|Error|falló') {
                Write-Host $line -ForegroundColor Red
            }
            else {
                Write-Host $line
            }
        }
        
        if ($job.State -eq 'Completed') {
            Write-Host ""
            Write-Host "==========================================" -ForegroundColor Green
            Write-Host "  EJECUCIÓN COMPLETADA" -ForegroundColor Green
            Write-Host "==========================================" -ForegroundColor Green
            Write-Host ""
            
            # Verificar resultados
            if (Test-Path "results") {
                $jsonFiles = Get-ChildItem "results\*.json"
                if ($jsonFiles) {
                    Write-Host "Archivos generados:" -ForegroundColor Cyan
                    $jsonFiles | ForEach-Object {
                        $lines = (Get-Content $_.FullName | Measure-Object -Line).Lines
                        $size = [math]::Round($_.Length / 1KB, 2)
                        Write-Host "  $($_.Name): $lines líneas, $size KB" -ForegroundColor White
                    }
                    Write-Host ""
                } else {
                    Write-Host "[WARN] No se encontraron archivos JSON en results/" -ForegroundColor Yellow
                }
            }
            
            # Limpiar job
            Remove-Job -Name OmniRTree -ErrorAction SilentlyContinue
            
            Write-Host "Presiona Enter para salir..." -ForegroundColor Yellow
            Read-Host
            break
        }
        
        if ($job.State -eq 'Failed') {
            Write-Host ""
            Write-Host "==========================================" -ForegroundColor Red
            Write-Host "  EJECUCIÓN FALLÓ" -ForegroundColor Red
            Write-Host "==========================================" -ForegroundColor Red
            Write-Host ""
            
            # Mostrar error completo
            $output | Select-Object -Last 50
            
            Remove-Job -Name OmniRTree -ErrorAction SilentlyContinue
            
            Write-Host ""
            Write-Host "Presiona Enter para salir..." -ForegroundColor Yellow
            Read-Host
            break
        }
        
        Start-Sleep -Seconds 10
    }
}

Write-Host ""
Write-Host "Script finalizado." -ForegroundColor Cyan
