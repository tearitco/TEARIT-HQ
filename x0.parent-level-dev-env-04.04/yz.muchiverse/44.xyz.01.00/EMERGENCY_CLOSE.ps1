# EMERGENCY_CLOSE.ps1 - Windows parity of EMERGENCY_CLOSE.sh
# Close desk-pal entity windows (and khtpm subwindows). Does NOT kill the
# taskbar strip; that is $.crypts\button.ps1 kill (strip + entities).
# Name-only + PIDs from #.desktop\livedesk_open.txt. No Get-Process.Path.
#
# From house root:
#   powershell -ExecutionPolicy Bypass -File .\EMERGENCY_CLOSE.ps1

$ErrorActionPreference = "Continue"
$House = Split-Path -Parent $MyInvocation.MyCommand.Path
$OpenFile = Join-Path $House "#.desktop\livedesk_open.txt"

Write-Host "EMERGENCY_CLOSE: closing desk-pals..."

$procIds = New-Object "System.Collections.Generic.HashSet[int]"

if (Test-Path -LiteralPath $OpenFile) {
    Get-Content -LiteralPath $OpenFile -ErrorAction SilentlyContinue | ForEach-Object {
        if ($_ -match "^PID=(\d+)") { [void]$procIds.Add([int]$Matches[1]) }
    }
}

$names = @(
    "tp_desktop_window_rgb",
    "tp_desktop_window",
    "khtpm_open_hai_render",
    "khtpm_hq_render",
    "khtpm_core_render",
    "khtpm_hq_manager"
)
Get-CimInstance Win32_Process -ErrorAction SilentlyContinue | Where-Object {
    $n = $_.Name
    if (-not $n) { return $false }
    $stem = $n
    if ($stem.EndsWith(".exe")) { $stem = $stem.Substring(0, $stem.Length - 4) }
    $names -contains $stem
} | ForEach-Object { [void]$procIds.Add([int]$_.ProcessId) }

if ($procIds.Count -eq 0) {
    Write-Host "No desk-pals found to close"
    exit 0
}

Write-Host ("Found {0} pid(s): {1}" -f $procIds.Count, (($procIds | Sort-Object) -join " "))

Write-Host "Phase 1: CloseMainWindow..."
foreach ($procId in @($procIds)) {
    try {
        $p = Get-Process -Id $procId -ErrorAction Stop
        $null = $p.CloseMainWindow()
        Write-Host ("  close sent {0} {1}" -f $procId, $p.ProcessName)
    } catch {
        Write-Host ("  skip {0} (already gone)" -f $procId)
        [void]$procIds.Remove($procId)
    }
}
Start-Sleep -Milliseconds 800

Write-Host "Phase 2: Stop-Process -Force leftovers..."
foreach ($procId in @($procIds)) {
    try {
        $p = Get-Process -Id $procId -ErrorAction Stop
        Stop-Process -Id $procId -Force -ErrorAction Stop
        Write-Host ("  killed {0} {1}" -f $procId, $p.ProcessName)
    } catch {
        Write-Host ("  {0} gone" -f $procId)
    }
}

Start-Sleep -Milliseconds 300
$left = @()
foreach ($procId in $procIds) {
    try { $null = Get-Process -Id $procId -ErrorAction Stop; $left += $procId } catch {}
}
if ($left.Count -gt 0) {
    Write-Host ("STILL alive: {0}" -f ($left -join " "))
    exit 1
} else {
    Write-Host "Emergency close complete"
    exit 0
}
