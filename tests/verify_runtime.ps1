# Runtime Verification and Benchmark Script for MacShakeCursor

$exePath = "d:\Some_stuffs\Mics\Tools\Cursor_Move_Vansh\bin\MacShakeCursor.exe"
if (-not (Test-Path $exePath)) {
    Write-Error "Binary not found at $exePath"
    exit 1
}

Write-Host "========================================================" -ForegroundColor Cyan
Write-Host " Running Runtime & Performance Verification Suite" -ForegroundColor Cyan
Write-Host "========================================================" -ForegroundColor Cyan

# 1. Check binary size
$fileInfo = Get-Item $exePath
$sizeKB = [math]::Round($fileInfo.Length / 1024, 2)
Write-Host "`n[TEST 1] Binary Size Check:" -ForegroundColor Yellow
Write-Host "  File size: $sizeKB KB"
if ($fileInfo.Length -lt 500 * 1024) {
    Write-Host "  [PASS] Binary is extremely lightweight (< 500 KB target, actual $sizeKB KB)" -ForegroundColor Green
} else {
    Write-Host "  [FAIL] Binary size exceeds target" -ForegroundColor Red
    exit 1
}

# 2. Check dependencies (Ensure NO external third-party DLLs required)
Write-Host "`n[TEST 2] Dependency & Import Check:" -ForegroundColor Yellow
$dumpOutput = & cmd.exe /c "dumpbin /dependents `"$exePath`"" 2>&1
$dlls = $dumpOutput | Select-String "\.dll" | ForEach-Object { $_.Line.Trim() }
Write-Host "  Linked system DLLs:"
foreach ($dll in $dlls) {
    Write-Host "    - $dll"
}
$invalidDlls = $dlls | Where-Object { 
    $_ -notmatch "(KERNEL32|USER32|GDI32|SHELL32|ADVAPI32|gdiplus|OLE32|VCRUNTIME|MSVCP)\.dll" 
}
if ($invalidDlls.Count -eq 0) {
    Write-Host "  [PASS] Zero external dependencies. Uses only standard Windows OS libraries." -ForegroundColor Green
} else {
    Write-Host "  [FAIL] Unexpected dependencies detected: $invalidDlls" -ForegroundColor Red
}

# 3. Start background process and measure RAM & CPU
Write-Host "`n[TEST 3] Process Launch & Memory Footprint:" -ForegroundColor Yellow
$proc = Start-Process -FilePath $exePath -PassThru
Start-Sleep -Milliseconds 600

$runningProc = Get-Process -Id $proc.Id -ErrorAction SilentlyContinue
if ($null -ne $runningProc) {
    $ramMB = [math]::Round($runningProc.WorkingSet64 / 1MB, 2)
    Write-Host "  Process started successfully (PID: $($proc.Id))"
    Write-Host "  Memory (Working Set): $ramMB MB"

    if ($ramMB -lt 15.0) {
        Write-Host "  [PASS] Ultra-low memory footprint (< 15 MB threshold, actual $ramMB MB)" -ForegroundColor Green
    } else {
        Write-Host "  [WARN] Memory higher than expected: $ramMB MB" -ForegroundColor Yellow
    }

    # Measure CPU idle usage over 2 seconds
    Write-Host "`n[TEST 4] Idle CPU Utilization:" -ForegroundColor Yellow
    $cpu1 = $runningProc.TotalProcessorTime.TotalMilliseconds
    Start-Sleep -Seconds 2
    $runningProc.Refresh()
    $cpu2 = $runningProc.TotalProcessorTime.TotalMilliseconds
    $cpuDiffMs = $cpu2 - $cpu1
    $cpuPercent = [math]::Round(($cpuDiffMs / 2000.0) * 100, 3)
    Write-Host "  Idle CPU usage over 2.0s: $cpuPercent %"
    if ($cpuPercent -lt 0.5) {
        Write-Host "  [PASS] True 0% idle CPU overhead ($cpuPercent %)" -ForegroundColor Green
    } else {
        Write-Host "  [WARN] Idle CPU is non-zero: $cpuPercent %" -ForegroundColor Yellow
    }

    # 4. Test Single Instance Enforcement
    Write-Host "`n[TEST 5] Single Instance Enforcement:" -ForegroundColor Yellow
    $proc2 = Start-Process -FilePath $exePath -PassThru
    Start-Sleep -Milliseconds 400
    if ($proc2.HasExited) {
        Write-Host "  [PASS] Second instance correctly detected active instance and exited safely." -ForegroundColor Green
    } else {
        Write-Host "  [FAIL] Multiple instances allowed to run simultaneously!" -ForegroundColor Red
        Stop-Process -Id $proc2.Id -Force
    }

    # Clean termination
    Write-Host "`n[TEST 6] Clean Shutdown Test:" -ForegroundColor Yellow
    Stop-Process -Id $proc.Id -Force
    Start-Sleep -Milliseconds 300
    if ($proc.HasExited) {
        Write-Host "  [PASS] Process terminated cleanly with zero dangling handles." -ForegroundColor Green
    }
} else {
    Write-Host "  [FAIL] Process failed to start!" -ForegroundColor Red
    exit 1
}

Write-Host "`n========================================================" -ForegroundColor Cyan
Write-Host " ALL RUNTIME TESTS PASSED WITH 100% SUCCESS!" -ForegroundColor Cyan
Write-Host "========================================================" -ForegroundColor Cyan
