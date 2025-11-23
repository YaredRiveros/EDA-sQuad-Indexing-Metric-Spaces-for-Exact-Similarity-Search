# check_progress.ps1
# Script para monitorear el progreso de SPB-tree con auto-refresh
# Uso: .\check_progress.ps1

Write-Host "`n==== SPB-Tree Progress Monitor ====" -ForegroundColor Cyan
Write-Host "Actualizando cada 10 segundos (Ctrl+C para salir)" -ForegroundColor Gray
Write-Host "=" * 80 -ForegroundColor Gray

$iteration = 0

try {
    while ($true) {
        $iteration++
        
        # Verificar si el job existe
        $job = Get-Job -Name "SPBTree" -ErrorAction SilentlyContinue
        
        if (!$job) {
            Write-Host "`n[WARN] No se encontró job 'SPBTree'" -ForegroundColor Yellow
            Write-Host "El proceso puede estar corriendo directamente o ya terminó." -ForegroundColor Gray
            
            # Intentar verificar con log
            if (Test-Path "spbtree_run.log") {
                Write-Host "`nÚltimas líneas del log:" -ForegroundColor Cyan
                Get-Content "spbtree_run.log" -Tail 30 | ForEach-Object {
                    if ($_ -match "Cargados|BUILD") {
                        Write-Host $_ -ForegroundColor Yellow
                    }
                    elseif ($_ -match "OK|DONE") {
                        Write-Host $_ -ForegroundColor Green
                    }
                    elseif ($_ -match "MRQ|MkNN") {
                        Write-Host $_ -ForegroundColor Cyan
                    }
                    else {
                        Write-Host $_ -ForegroundColor Gray
                    }
                }
            }
            
            break
        }
        
        # Limpiar pantalla y mostrar header
        Clear-Host
        Write-Host "`n==== SPB-Tree Progress Monitor ====" -ForegroundColor Cyan
        $timestamp = Get-Date -Format "HH:mm:ss"
        Write-Host "Check #$iteration - $timestamp" -ForegroundColor Gray
        Write-Host "Job Estado: $($job.State)" -ForegroundColor $(
            if ($job.State -eq "Running") { "Green" }
            elseif ($job.State -eq "Completed") { "Cyan" }
            else { "Yellow" }
        )
        Write-Host "=" * 80 -ForegroundColor Gray
        
        # Obtener output
        $output = Receive-Job -Name "SPBTree" -Keep | Select-Object -Last 35
        
        if ($output) {
            Write-Host "`nÚltimas líneas del output:" -ForegroundColor Cyan
            Write-Host ""
            
            foreach ($line in $output) {
                # Colorear según contenido
                if ($line -match "Cargados|BUILD") {
                    Write-Host $line -ForegroundColor Yellow
                }
                elseif ($line -match "OK|DONE|construido") {
                    Write-Host $line -ForegroundColor Green
                }
                elseif ($line -match "MRQ|MkNN|sel=|k=") {
                    Write-Host $line -ForegroundColor Cyan
                }
                elseif ($line -match "ERROR|error|Error|falló") {
                    Write-Host $line -ForegroundColor Red
                }
                elseif ($line -match "SPB-tree|pivots|records|SFC") {
                    Write-Host $line -ForegroundColor Magenta
                }
                else {
                    Write-Host $line -ForegroundColor Gray
                }
            }
        } else {
            Write-Host "`n[INFO] Esperando output..." -ForegroundColor Gray
        }
        
        # Verificar si terminó
        if ($job.State -eq "Completed") {
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
            
            Write-Host "`n[INFO] Puedes cerrar este monitor" -ForegroundColor Gray
            break
        }
        elseif ($job.State -eq "Failed") {
            Write-Host "`n[ERROR] El job falló" -ForegroundColor Red
            break
        }
        
        # Información de estado
        Write-Host "`n" + ("-" * 80) -ForegroundColor DarkGray
        Write-Host "Próxima actualización en 10 segundos... (Ctrl+C para salir)" -ForegroundColor DarkGray
        
        # Esperar 10 segundos
        Start-Sleep -Seconds 10
    }
}
catch {
    Write-Host "`n[INFO] Monitor detenido" -ForegroundColor Yellow
}

Write-Host "`n[DONE] Monitor finalizado" -ForegroundColor Gray
Write-Host "=" * 80 -ForegroundColor Gray
