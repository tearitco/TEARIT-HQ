# pal_watchdog.ps1 - Monitors and kills runaway Prisc VM instances for Windows.
# Part of the CPU Health & Throttling strategy.

$MAX_RUNTIME_SEC = 5
$CHECK_INTERVAL_SEC = 1
$LOG_FILE = "pieces\system\prisc\watchdog.log"

if (-not (Test-Path "pieces\system\prisc")) {
    New-Item -ItemType Directory "pieces\system\prisc" -Force
}

Add-Content -Path $LOG_FILE -Value "[$(Get-Date)] PAL Watchdog (PS) started. Max runtime: ${MAX_RUNTIME_SEC}s"

while ($true) {
    # 1. Monitor runaway Prisc VM instances (prisc+x)
    $procs = Get-Process | Where-Object { $_.Path -like "*prisc+x*" }
    
    foreach ($p in $procs) {
        $elapsed = (Get-Date) - $p.StartTime
        if ($elapsed.TotalSeconds -gt $MAX_RUNTIME_SEC) {
            Add-Content -Path $LOG_FILE -Value "[$(Get-Date)] [watchdog] Killing runaway Prisc process (PID: $($p.Id), Elapsed: $($elapsed.TotalSeconds)s)"
            Stop-Process -Id $p.Id -Force -ErrorAction SilentlyContinue
        }
    }
    
    # 2. Prevent Fork Bombs: Check total number of prisc+x processes
    if ($procs.Count -gt 15) {
        Add-Content -Path $LOG_FILE -Value "[$(Get-Date)] [watchdog] WARNING: High Prisc process count detected ($($procs.Count)). Cleaning up..."
        $procs | Stop-Process -Force -ErrorAction SilentlyContinue
    }

    Start-Sleep -Seconds $CHECK_INTERVAL_SEC
}
