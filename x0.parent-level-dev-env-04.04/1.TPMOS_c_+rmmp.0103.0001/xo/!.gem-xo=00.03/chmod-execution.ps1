# set-execution.ps1 - Set PowerShell execution policy for CHTPM scripts
# Run this once to allow unsigned scripts in this directory
# Usage: .\set-execution.ps1

Write-Host "CHTPM Execution Policy Setup" -ForegroundColor Cyan
Write-Host ""

$SCRIPT_DIR = Split-Path -Parent $MyInvocation.MyCommand.Path

# Force policy at all scopes to ensure no overrides
Write-Host "Forcing execution policy at all scopes..." -ForegroundColor Yellow

# Use Unrestricted to allow unsigned local scripts (RemoteSigned may be overridden by GPO)
$POLICY = "Unrestricted"

# Option 1: Set policy for CurrentUser (recommended, no admin needed)
Write-Host "  Setting CurrentUser scope..." -ForegroundColor Gray
try {
    Set-ExecutionPolicy -ExecutionPolicy $POLICY -Scope CurrentUser -Force -ErrorAction Stop
    Write-Host "    [OK] $POLICY policy set for CurrentUser" -ForegroundColor Green
} catch {
    Write-Host "    [!] Failed: $_" -ForegroundColor Red
}

# Option 2: Set policy for Process scope (temporary, current session only)
Write-Host "  Setting Process scope..." -ForegroundColor Gray
try {
    Set-ExecutionPolicy -ExecutionPolicy $POLICY -Scope Process -Force -ErrorAction Stop
    Write-Host "    [OK] $POLICY policy set for Process" -ForegroundColor Green
} catch {
    Write-Host "    [!] Failed: $_" -ForegroundColor Red
}

# Option 3: Try LocalMachine scope (may require Admin)
Write-Host "  Setting LocalMachine scope (if Admin)..." -ForegroundColor Gray
try {
    Set-ExecutionPolicy -ExecutionPolicy $POLICY -Scope LocalMachine -Force -ErrorAction Stop
    Write-Host "    [OK] $POLICY policy set for LocalMachine" -ForegroundColor Green
} catch {
    Write-Host "    [!] Skipped (Admin required): $_" -ForegroundColor Yellow
}

Write-Host ""
Write-Host "Execution policy setup complete!" -ForegroundColor Green
Write-Host ""
Write-Host "Current effective policy:" -ForegroundColor Cyan
Get-ExecutionPolicy -List | Format-Table -AutoSize

Write-Host ""
Write-Host "You can now run: .\button.ps1" -ForegroundColor Green
