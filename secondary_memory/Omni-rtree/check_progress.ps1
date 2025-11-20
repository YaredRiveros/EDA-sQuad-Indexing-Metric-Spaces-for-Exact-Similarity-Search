# check_progress.ps1 - Script simple para ver progreso de OmniR-tree
Write-Host "`n===========================================" -ForegroundColor Cyan
Write-Host "  MONITOR DE PROGRESO - OmniR-tree" -ForegroundColor Cyan  
Write-Host "===========================================" -ForegroundColor Cyan
Write-Host "Presiona Ctrl+C para detener el monitoreo`n" -ForegroundColor Yellow

while ($true) {
    Clear-Host
    Write-Host "===========================================" -ForegroundColor Cyan
    Write-Host "Timestamp: $(Get-Date -Format 'HH:mm:ss')" -ForegroundColor Green
    Write-Host "===========================================" -ForegroundColor Cyan
    
    $job = Get-Job -Name OmniRTree -ErrorAction SilentlyContinue
    
    if ($job) {
        Write-Host "Estado del Job: $($job.State)" -ForegroundColor $(
            if ($job.State -eq 'Running') { 'Green' } 
            elseif ($job.State -eq 'Completed') { 'Cyan' }
            else { 'Yellow' }
        )
        Write-Host ""
        
        $output = Receive-Job -Name OmniRTree -Keep 2>&1 | Select-Object -Last 35
        
        foreach ($line in $output) {
            $text = $line.ToString()
            if ($text -match 'Indexados|BUILD|DONE') {
                Write-Host $text -ForegroundColor Yellow
            }
            elseif ($text -match 'OK|Completado|generado') {
                Write-Host $text -ForegroundColor Green
            }
            elseif ($text -match 'MRQ|MkNN|sel=|k=') {
                Write-Host $text -ForegroundColor Cyan
            }
            elseif ($text -match 'ERROR|error|Error') {
                Write-Host $text -ForegroundColor Red
            }
            else {
                Write-Host $text
            }
        }
        
        if ($job.State -eq 'Completed') {
            Write-Host "`n===========================================" -ForegroundColor Green
            Write-Host "  PROCESO COMPLETADO" -ForegroundColor Green
            Write-Host "===========================================" -ForegroundColor Green
            break
        }
    }
    else {
        Write-Host "Job no encontrado o ya finalizado" -ForegroundColor Red
        break
    }
    
    Start-Sleep -Seconds 10
}

Write-Host "`nMonitoreo finalizado. Presiona Enter para salir..." -ForegroundColor Yellow
Read-Host
