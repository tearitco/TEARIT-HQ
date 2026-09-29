# tick_all.ps1 <rounds> - Windows parity with tick_all.sh
param([int]$Rounds = 1)

$ErrorActionPreference = "Continue"
$SCRIPT_DIR = Split-Path -Parent (Split-Path -Parent $MyInvocation.MyCommand.Path)
$env:PRISC_PROJECT_ROOT = $SCRIPT_DIR
Set-Location $SCRIPT_DIR

function Invoke-Op {
    param([string]$Name, [string[]]$OpArgs = @())
    $op = Join-Path $SCRIPT_DIR "ops\+x\$Name.+x"
    if (Test-Path ($op + ".exe")) { $op = $op + ".exe" }
    if (-not (Test-Path $op)) {
        Write-Warning "missing op: $Name"
        return
    }
    $argStr = ($OpArgs -join " ")
    if ($argStr) {
        & cmd /c "`"$op`" $argStr" 2>$null | Out-Null
    } else {
        & cmd /c "`"$op`"" 2>$null | Out-Null
    }
}

function Get-StateValue([string]$statePath, [string]$key) {
    if (-not (Test-Path $statePath)) { return $null }
    $line = Get-Content $statePath | Where-Object { $_ -like "$key=*" } | Select-Object -First 1
    if (-not $line) { return $null }
    return ($line -split "=", 2)[1]
}

function Tick-One([string]$pieceId, [string]$prefix, [string]$actionVerb = "trade") {
    $statePath = Join-Path $SCRIPT_DIR "projects\wsr-pal\pieces\$pieceId\state.txt"
    $cs = Get-StateValue $statePath "current_state"
    switch ($cs) {
        "0" { Invoke-Op "${prefix}_tick_idle" @($pieceId) }
        "1" { Invoke-Op "${prefix}_decide" @($pieceId) }
        "2" { Invoke-Op "${prefix}_${actionVerb}" @($pieceId) }
    }
}

function Tick-One2State([string]$pieceId, [string]$prefix) {
    $statePath = Join-Path $SCRIPT_DIR "projects\wsr-pal\pieces\$pieceId\state.txt"
    $cs = Get-StateValue $statePath "current_state"
    switch ($cs) {
        "0" { Invoke-Op "${prefix}_tick_idle" @($pieceId) }
        "1" { Invoke-Op "${prefix}_update" @($pieceId) }
    }
}

$piecesRoot = Join-Path $SCRIPT_DIR "projects\wsr-pal\pieces"

# NOTE: the world clock is deliberately NOT advanced here. It is a
# wall-clock daemon (ops/econ_calendar.c, a port of the original
# wsr_clock.c) started alongside the orchestrator by button.ps1, because in
# the original the clock runs on real time and the market moves whether or not
# the player acts. Calling it per-End-Turn would be the unfaithful behaviour,
# not the faithful one. See docs/PORT-FIDELITY.md gap 1 and gap 3.

# Per-turn finances + price + news (once per End Turn, before FSM rounds)
Get-ChildItem $piecesRoot -Directory -Filter "corp_*" | ForEach-Object {
    Invoke-Op "corp_apply_finances" @($_.Name)
    Invoke-Op "corp_update_price" @($_.Name)
}
Invoke-Op "wsr_news_op"
Invoke-Op "player_settle_futures"

for ($round = 1; $round -le $Rounds; $round++) {
    Write-Host "=== round $round/$Rounds ==="
    Get-ChildItem $piecesRoot -Directory -Filter "corp_*" | ForEach-Object {
        Tick-One $_.Name "corp"
    }
    Get-ChildItem $piecesRoot -Directory -Filter "gov_*" | ForEach-Object {
        Tick-One $_.Name "gov"
    }
    Get-ChildItem $piecesRoot -Directory -Filter "realestate_*" | ForEach-Object {
        Tick-One $_.Name "realestate" "act"
    }
    Get-ChildItem $piecesRoot -Directory -Filter "pop_*" | ForEach-Object {
        Tick-One2State $_.Name "pop"
    }
    Get-ChildItem $piecesRoot -Directory -Filter "weather_*" | ForEach-Object {
        Tick-One2State $_.Name "weather"
    }
}
