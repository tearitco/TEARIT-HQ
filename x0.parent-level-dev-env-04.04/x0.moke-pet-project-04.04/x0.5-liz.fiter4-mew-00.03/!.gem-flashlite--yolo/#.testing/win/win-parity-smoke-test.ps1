# Windows/Linux Parity Smoke Test
# Purpose: Verify arrow keys and ASCII box characters work on Windows
# Date: March 31, 2026

param([string]$action = "run")

$PROJECT_ROOT = Split-Path -Parent (Split-Path -Parent $MyInvocation.MyCommand.Path)
Set-Location $PROJECT_ROOT

function Test-Kill {
    Write-Host "=== Killing existing processes ===" -ForegroundColor Yellow
    Get-Process | Where-Object {$_.Path -like "*TPMOS*"} | Stop-Process -Force -ErrorAction SilentlyContinue
    Get-Process | Where-Object {$_.Name -like "*orchestrator*"} | Stop-Process -Force -ErrorAction SilentlyContinue
    Get-Process | Where-Object {$_.Name -like "*renderer*"} | Stop-Process -Force -ErrorAction SilentlyContinue
    Get-Process | Where-Object {$_.Name -like "*parser*"} | Stop-Process -Force -ErrorAction SilentlyContinue
    Get-Process | Where-Object {$_.Name -like "*clock*"} | Stop-Process -Force -ErrorAction SilentlyContinue
    Start-Sleep -Seconds 2
    Write-Host "Processes killed" -ForegroundColor Green
}

function Test-Compile {
    Write-Host "=== Compiling ===" -ForegroundColor Yellow
    powershell -ExecutionPolicy Bypass -File ".\button.ps1" compile
    if ($LASTEXITCODE -eq 0) {
        Write-Host "Compilation successful" -ForegroundColor Green
    } else {
        Write-Host "Compilation FAILED" -ForegroundColor Red
        return $false
    }
    return $true
}

function Test-Binaries {
    Write-Host "=== Checking binary timestamps ===" -ForegroundColor Yellow
    $binaries = @("orchestrator.+x", "chtpm_parser.+x", "renderer.+x")
    $now = Get-Date
    foreach ($bin in $binaries) {
        $path = "pieces\chtpm\plugins\+x\$bin"
        if (Test-Path $path) {
            $file = Get-Item $path
            $age = ($now - $file.LastWriteTime).TotalMinutes
            Write-Host "  $bin : $($file.LastWriteTime) ($([math]::Round($age, 1)) min ago)"
        } else {
            Write-Host "  $bin : NOT FOUND" -ForegroundColor Red
        }
    }
}

function Test-Unicode {
    Write-Host "=== Checking for hardcoded Unicode ===" -ForegroundColor Yellow
    $files = @(
        "pieces\chtpm\plugins\chtpm_parser.c",
        "pieces\chtpm\plugins\chtpm_player.c",
        "pieces\apps\player_app\world\plugins\player_render.c",
        "pieces\apps\fuzzpet_app\fuzzpet\plugins\fuzzpet.c"
    )
    $unicode_found = $false
    foreach ($file in $files) {
        $content = Get-Content $file -Raw
        if ($content -match '║|╔|╗|╚|╝|╠|╣|═') {
            Write-Host "  $file : UNICODE FOUND!" -ForegroundColor Red
            $unicode_found = $true
        } else {
            Write-Host "  $file : Clean (ASCII only)" -ForegroundColor Green
        }
    }
    return -not $unicode_found
}

function Test-Run {
    Write-Host "=== Running orchestrator (10 second test) ===" -ForegroundColor Yellow
    Write-Host "INSTRUCTIONS:" -ForegroundColor Cyan
    Write-Host "  1. Watch for box characters (should be + = | not garbled)"
    Write-Host "  2. Press arrow keys (up/down)"
    Write-Host "  3. Watch [>] selector move"
    Write-Host "  4. Watch Nav > line show key pressed"
    Write-Host "  5. After 10 seconds, test will auto-check results"
    Write-Host ""
    
    # Clear old logs
    $null = New-Item -ItemType File -Path "pieces\keyboard\history.txt" -Force
    $null = New-Item -ItemType File -Path "pieces\display\ledger.txt" -Force
    
    # Start orchestrator
    $proc = Start-Process -FilePath "pieces\chtpm\plugins\+x\orchestrator.+x" -PassThru -NoNewWindow
    Write-Host "Orchestrator PID: $($proc.Id)" -ForegroundColor Green
    
    # Wait for user input
    Start-Sleep -Seconds 10
    
    # Check keyboard history
    Write-Host "`n=== Checking keyboard history ===" -ForegroundColor Yellow
    if (Test-Path "pieces\keyboard\history.txt") {
        $history = Get-Content "pieces\keyboard\history.txt" | Select-Object -Last 10
        if ($history.Count -gt 0) {
            Write-Host "Keyboard events captured:" -ForegroundColor Green
            $history | ForEach-Object { Write-Host "  $_" }
            
            # Check for arrow key codes (1000-1003)
            $arrow_keys = $history | Where-Object { $_ -match 'KEY_PRESSED: (1000|1001|1002|1003)' }
            if ($arrow_keys) {
                Write-Host "Arrow keys detected!" -ForegroundColor Green
                $arrow_keys | ForEach-Object { Write-Host "  $_" }
            } else {
                Write-Host "NO arrow keys detected (expected 1000-1003)" -ForegroundColor Red
            }
        } else {
            Write-Host "No keyboard events captured" -ForegroundColor Red
        }
    }
    
    # Check frame output
    Write-Host "`n=== Checking frame output ===" -ForegroundColor Yellow
    if (Test-Path "pieces\display\current_frame.txt") {
        $frame = Get-Content "pieces\display\current_frame.txt" -Raw
        if ($frame -match '\+') {
            Write-Host "ASCII box characters found (+)" -ForegroundColor Green
        } else {
            Write-Host "No ASCII box characters found" -ForegroundColor Red
        }
        if ($frame -match 'Γ') {
            Write-Host "WARNING: Garbled Unicode still present (Γ characters)" -ForegroundColor Red
        }
        
        # Show first 20 lines
        Write-Host "`nFirst 20 lines of current_frame.txt:" -ForegroundColor Cyan
        Get-Content "pieces\display\current_frame.txt" | Select-Object -First 20 | ForEach-Object { Write-Host "  $_" }
    }
    
    # Kill orchestrator
    Write-Host "`n=== Cleaning up ===" -ForegroundColor Yellow
    Stop-Process -Id $proc.Id -Force -ErrorAction SilentlyContinue
    Write-Host "Test complete" -ForegroundColor Green
}

function Show-Help {
    Write-Host "Windows Parity Smoke Test" -ForegroundColor Cyan
    Write-Host "Usage: .\#.testing\win-parity-smoke-test.ps1 <action>"
    Write-Host ""
    Write-Host "Actions:"
    Write-Host "  kill     - Kill all TPM processes"
    Write-Host "  compile  - Compile all binaries"
    Write-Host "  check    - Check binaries and Unicode"
    Write-Host "  run      - Full test (compile + run + verify)"
    Write-Host "  help     - Show this help"
}

# Main
switch ($action.ToLower()) {
    "kill" { Test-Kill }
    "compile" { Test-Kill; Test-Compile }
    "check" { Test-Binaries; Test-Unicode }
    "run" { Test-Kill; if (Test-Compile) { Test-Binaries; Test-Unicode; Test-Run } }
    "help" { Show-Help }
    default { Show-Help }
}
