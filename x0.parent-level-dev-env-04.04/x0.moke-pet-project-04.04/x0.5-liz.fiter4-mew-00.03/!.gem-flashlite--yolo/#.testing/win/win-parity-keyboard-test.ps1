# Windows Parity Keyboard Test
# Simulates key presses by writing to keyboard history file
# Format: [YYYY-MM-DD HH:MM:SS] KEY_PRESSED: <keycode>

param([string]$action = "test")

$HISTORY_FILE = "pieces\keyboard\history.txt"
$MASTER_LEDGER = "pieces\master_ledger\master_ledger.txt"

# Key codes
$KEY_UP = 1002
$KEY_DOWN = 1003
$KEY_LEFT = 1000
$KEY_RIGHT = 1001
$KEY_ENTER = 10
$KEY_1 = 49
$KEY_2 = 50
$KEY_3 = 51

function Write-Key {
    param([int]$keyCode)
    $timestamp = Get-Date -Format "yyyy-MM-dd HH:mm:ss"
    $line = "[$timestamp] KEY_PRESSED: $keyCode`n"
    Add-Content -Path $HISTORY_FILE -Value $line
    Write-Host "Injected: KEY_PRESSED: $keyCode" -ForegroundColor Green
}

function Test-Navigation {
    Write-Host "`n=== Navigation Test (Arrow Keys) ===" -ForegroundColor Cyan
    Write-Host "This simulates pressing DOWN 3 times, then UP once" -ForegroundColor Gray
    
    # Clear history
    Set-Content -Path $HISTORY_FILE -Value ""
    
    Write-Host "`nInjecting keys..." -ForegroundColor Yellow
    Write-Key $KEY_DOWN
    Start-Sleep -Milliseconds 500
    Write-Key $KEY_DOWN
    Start-Sleep -Milliseconds 500
    Write-Key $KEY_DOWN
    Start-Sleep -Milliseconds 500
    Write-Key $KEY_UP
    Start-Sleep -Milliseconds 500
    
    Write-Host "`nKeyboard history content:" -ForegroundColor Cyan
    Get-Content $HISTORY_FILE
    
    Write-Host "`nNow run orchestrator and check if selector moved..." -ForegroundColor Yellow
}

function Test-MenuSelect {
    Write-Host "`n=== Menu Select Test (Press 2) ===" -ForegroundColor Cyan
    
    Set-Content -Path $HISTORY_FILE -Value ""
    
    Write-Key $KEY_2
    Start-Sleep -Milliseconds 500
    
    Write-Host "`nKeyboard history:" -ForegroundColor Cyan
    Get-Content $HISTORY_FILE
}

function Test-FullSequence {
    Write-Host "`n=== Full Navigation Sequence ===" -ForegroundColor Cyan
    Write-Host "Simulates: DOWN, DOWN, Press 2, ENTER" -ForegroundColor Gray
    
    Set-Content -Path $HISTORY_FILE -Value ""
    
    Write-Key $KEY_DOWN
    Start-Sleep -Milliseconds 300
    Write-Key $KEY_DOWN
    Start-Sleep -Milliseconds 300
    Write-Key $KEY_2
    Start-Sleep -Milliseconds 300
    Write-Key $KEY_ENTER
    
    Write-Host "`nFull sequence injected!" -ForegroundColor Green
    Write-Host "Check pieces/display/current_frame.txt for updated menu" -ForegroundColor Yellow
}

function Show-Help {
    Write-Host "Windows Parity Keyboard Test" -ForegroundColor Cyan
    Write-Host "Usage: .\#.testing\win-parity-keyboard-test.ps1 <action>"
    Write-Host ""
    Write-Host "Actions:"
    Write-Host "  nav      - Test arrow key navigation (DOWN x3, UP x1)"
    Write-Host "  select   - Test menu select (Press 2)"
    Write-Host "  full     - Full sequence (DOWN, DOWN, 2, ENTER)"
    Write-Host "  inject <code> - Inject specific key code"
    Write-Host "  help     - Show this help"
}

# Main
switch ($action.ToLower()) {
    "nav" { Test-Navigation }
    "select" { Test-MenuSelect }
    "full" { Test-FullSequence }
    "inject" { Write-Key 0 }  # Placeholder
    "help" { Show-Help }
    default { Show-Help }
}
