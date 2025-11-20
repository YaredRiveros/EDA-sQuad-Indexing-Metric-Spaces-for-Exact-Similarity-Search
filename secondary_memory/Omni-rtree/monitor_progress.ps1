# monitor_progress.ps1 - Monitorear el progreso de OmniR-tree tests
# Uso: .\monitor_progress.ps1

$logFile = "omni_output.log"
$lastSize = 0
$lastLines = 0

Write-Host "========================================" -ForegroundColor Cyan
Write-Host "Monitor de Progreso - OmniR-tree Tests" -ForegroundColor Cyan
Write-Host "========================================" -ForegroundColor Cyan
Write-Host "Log file: $logFile" -ForegroundColor Yellow
Write-Host "Presiona Ctrl+C para detener el monitor (no detiene el proceso)" -ForegroundColor Yellow
Write-Host ""

while ($true) {
    if (Test-Path $logFile) {
        $currentSize = (Get-Item $logFile).Length
        $content = Get-Content $logFile -ErrorAction SilentlyContinue
        $currentLines = $content.Count
        
        if ($currentLines -gt $lastLines) {
            Clear-Host
            Write-Host "========================================" -ForegroundColor Cyan
            Write-Host "Monitor de Progreso - OmniR-tree Tests" -ForegroundColor Cyan
            Write-Host "========================================" -ForegroundColor Cyan
            Write-Host "Timestamp: $(Get-Date -Format 'HH:mm:ss')" -ForegroundColor Green
            Write-Host "Líneas totales: $currentLines" -ForegroundColor Green
            Write-Host "Tamaño archivo: $([math]::Round($currentSize/1KB, 2)) KB" -ForegroundColor Green
            Write-Host ""
            Write-Host "--- ÚLTIMAS 40 LÍNEAS ---" -ForegroundColor Yellow
            Write-Host ""
            
            $content | Select-Object -Last 40 | ForEach-Object {
                if ($_ -match "ERROR|error|Error") {
                    Write-Host $_ -ForegroundColor Red
                } elseif ($_ -match "OK|completado|COMPLETED|generado") {
                    Write-Host $_ -ForegroundColor Green
                } elseif ($_ -match "CONFIG|BUILD|EXP") {
                    Write-Host $_ -ForegroundColor Cyan
                } elseif ($_ -match "Indexados|sel=|k=") {
                    Write-Host $_ -ForegroundColor Yellow
                } else {
                    Write-Host $_
                }
            }
            
            $lastLines = $currentLines
        }
        
        $lastSize = $currentSize
    } else {
        Write-Host "Esperando que se cree el archivo $logFile..." -ForegroundColor Yellow
    }
    
    Start-Sleep -Seconds 5
}
