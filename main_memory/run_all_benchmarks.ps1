# ============================================================
# Script PowerShell para ejecutar todos los benchmarks
# de estructuras de índices métricos en main_memory
# ============================================================

$ErrorActionPreference = "Continue"

$STRUCTURES = @("BST", "LAESA", "BKT", "mvpt", "EPT", "FQT", "GNAT")
$SCRIPT_DIR = $PSScriptRoot

Write-Host "==========================================" -ForegroundColor Green
Write-Host "Main-Memory Index Structures Benchmark" -ForegroundColor Green
Write-Host "==========================================" -ForegroundColor Green
Write-Host "CPU Info:"
Get-CimInstance -ClassName Win32_Processor | Select-Object Name | Format-List
Write-Host ""
Write-Host "Starting time: $(Get-Date)"
Write-Host "==========================================" -ForegroundColor Green
Write-Host ""

# Record CPU info
"CPU Info:" | Out-File -FilePath "benchmark_system_info.txt"
Get-CimInstance -ClassName Win32_Processor | Select-Object Name, NumberOfCores, MaxClockSpeed | Out-File -FilePath "benchmark_system_info.txt" -Append
"" | Out-File -FilePath "benchmark_system_info.txt" -Append
"Start time: $(Get-Date)" | Out-File -FilePath "benchmark_system_info.txt" -Append

foreach ($struct in $STRUCTURES) {
    Write-Host ""
    Write-Host "==========================================" -ForegroundColor Cyan
    Write-Host "Structure: $struct" -ForegroundColor Cyan
    Write-Host "==========================================" -ForegroundColor Cyan
    
    Set-Location "$SCRIPT_DIR\$struct"
    
    # Compile
    Write-Host "[1/3] Compiling..." -ForegroundColor Yellow
    
    if ($struct -eq "mvpt") {
        # mvpt paths are different
        $compileCmd = "g++ -O3 -std=gnu++17 test.cpp -o ${struct}_test.exe -I.."
        Write-Host "Command: $compileCmd" -ForegroundColor Gray
        Invoke-Expression $compileCmd
    } elseif ($struct -eq "EPT") {
        # EPT* uses existing source files
        Write-Host "Compiling EPT* with its dependencies..." -ForegroundColor Gray
        $compileCmd = "g++ -O3 -std=gnu++17 test.cpp Interpreter.cpp Objvector.cpp Tuple.cpp -o ${struct}_test.exe"
        Write-Host "Command: $compileCmd" -ForegroundColor Gray
        Invoke-Expression $compileCmd
    } elseif ($struct -eq "FQT") {
        # FQT is C code, needs special compilation
        Write-Host "Compiling FQT (C code) with C++ wrapper..." -ForegroundColor Gray
        Write-Host "Command: gcc -O3 -c fqt.c -o fqt.o" -ForegroundColor Gray
        gcc -O3 -c fqt.c -o fqt.o
        Write-Host "Command: gcc -O3 -c ../../index.c -o index_fqt.o" -ForegroundColor Gray
        gcc -O3 -c ../../index.c -o index_fqt.o
        Write-Host "Command: gcc -O3 -c ../../bucket.c -o bucket_fqt.o" -ForegroundColor Gray
        gcc -O3 -c ../../bucket.c -o bucket_fqt.o
        $compileCmd = "g++ -O3 -std=gnu++17 test.cpp fqt.o index_fqt.o bucket_fqt.o -o ${struct}_test.exe"
        Write-Host "Command: $compileCmd" -ForegroundColor Gray
        Invoke-Expression $compileCmd
    } elseif ($struct -eq "GNAT") {
        # GNAT is in nested directory
        Set-Location "GNAT"
        Write-Host "Compiling GNAT with its dependencies..." -ForegroundColor Gray
        $compileCmd = "g++ -O3 -std=gnu++17 test.cpp db.cpp GNAT.cpp -o ../GNAT_test.exe"
        Write-Host "Command: $compileCmd" -ForegroundColor Gray
        Invoke-Expression $compileCmd
        Set-Location ".."
    } else {
        $compileCmd = "g++ -O3 -std=gnu++17 test.cpp -o ${struct}_test.exe"
        Write-Host "Command: $compileCmd" -ForegroundColor Gray
        Invoke-Expression $compileCmd
    }
    
    if ($LASTEXITCODE -ne 0) {
        Write-Host "[ERROR] Compilation failed for $struct" -ForegroundColor Red
        Set-Location $SCRIPT_DIR
        continue
    }
    Write-Host "[✓] Compilation successful" -ForegroundColor Green
    
    # Run benchmark
    Write-Host "[2/3] Running benchmark..." -ForegroundColor Yellow
    Write-Host "This may take several minutes..." -ForegroundColor Gray
    
    $logFile = "${struct}_benchmark.log"
    & ".\${struct}_test.exe" > $logFile 2>&1
    
    if ($LASTEXITCODE -ne 0) {
        Write-Host "[ERROR] Execution failed for $struct" -ForegroundColor Red
        Write-Host "Check $logFile for details" -ForegroundColor Yellow
    } else {
        Write-Host "[✓] Benchmark completed successfully" -ForegroundColor Green
    }
    
    # Summary
    Write-Host "[3/3] Results summary:" -ForegroundColor Yellow
    if (Test-Path "results") {
        Get-ChildItem "results\results_${struct}*.json" | Format-Table Name, Length
        $count = (Get-ChildItem "results\results_${struct}*.json" -ErrorAction SilentlyContinue).Count
        Write-Host "Total result files: $count" -ForegroundColor Cyan
    } else {
        Write-Host "[WARN] No results directory found" -ForegroundColor Yellow
    }
    
    Set-Location $SCRIPT_DIR
    Write-Host ""
}

# Final summary
Write-Host ""
Write-Host "==========================================" -ForegroundColor Green
Write-Host "All Benchmarks Completed!" -ForegroundColor Green
Write-Host "==========================================" -ForegroundColor Green
Write-Host "End time: $(Get-Date)"
Write-Host ""

Write-Host "Results location:" -ForegroundColor Cyan
foreach ($struct in $STRUCTURES) {
    if (Test-Path "$struct\results") {
        Write-Host "  $struct\results\" -ForegroundColor White
    }
}

Write-Host ""
Write-Host "Logs:" -ForegroundColor Cyan
foreach ($struct in $STRUCTURES) {
    if (Test-Path "$struct\${struct}_benchmark.log") {
        Write-Host "  $struct\${struct}_benchmark.log" -ForegroundColor White
    }
}

Write-Host ""
Write-Host "Next steps:" -ForegroundColor Yellow
Write-Host "  1. Review individual logs for any warnings"
Write-Host "  2. Run Python aggregation script: python aggregate_results.py"
Write-Host "  3. Analyze consolidated results"
Write-Host ""

# Record end time
"" | Out-File -FilePath "benchmark_system_info.txt" -Append
"End time: $(Get-Date)" | Out-File -FilePath "benchmark_system_info.txt" -Append

Write-Host "Press any key to exit..." -ForegroundColor Gray
$null = $Host.UI.RawUI.ReadKey("NoEcho,IncludeKeyDown")
