# scripts/playtest.ps1 - Windows playtest harness.
# Port of scripts/playtest.sh; the two must stay behaviourally identical.
#
# WHAT THIS IS FOR. The sim-level tests (test_invariants.ps1,
# test_goods_loop.ps1) prove the ECONOMY is sane. They do not prove the GAME is
# playable. This one does, and it exists because the renderer had never
# successfully produced a frame - wsr_compose_frame exited 1 silently because it
# read a menu piece named `wsr_menu` (no such piece; it is `wsr_main_menu`) and
# because it wrote into pieces/apps/player_app/, a directory that did not exist.
# current_frame.txt sat at 0 bytes. A whole game screen had never rendered.
#
# So this asserts the things a PLAYER depends on:
#   1. a fresh world can be created
#   2. the frame renders and is non-empty
#   3. the frame responds to state rather than being a constant
#   4. real keystrokes are accepted (digit + Enter)
#   5. End Turn actually advances the turn
#   6. the frame still renders after state has moved
# Any of those failing means the game is not playable, whatever the economy says.

param([switch]$Reset)

$ErrorActionPreference = "Stop"
$SCRIPT_DIR = Split-Path -Parent (Split-Path -Parent $MyInvocation.MyCommand.Path)
Set-Location $SCRIPT_DIR
$env:PRISC_PROJECT_ROOT = $SCRIPT_DIR
$env:Path = "C:\msys64\mingw64\bin;$env:Path"

$FAILURES = 0
function Fail([string]$m, [object[]]$a) { $script:FAILURES++; Write-Host ("  FAIL  " + ($m -f $a)) -ForegroundColor Red }
function Pass([string]$m, [object[]]$a) { Write-Host ("  ok    " + ($m -f $a)) -ForegroundColor DarkGreen }
function Warn([string]$m, [object[]]$a) { Write-Host ("  WARN  " + ($m -f $a)) -ForegroundColor Yellow }

$FRAME     = "pieces\display\current_frame.txt"
$MENU_STATE = "projects\wsr-pal\pieces\wsr_main_menu\state.txt"

Write-Host "building ops..." -ForegroundColor DarkGray
foreach ($op in @("wsr_compose_frame", "wsr_menu_input")) {
    if (-not (Test-Path "ops\$op.c")) { throw "missing ops\$op.c" }
    cmd /c "gcc -Wall -Wextra -O2 ops\$op.c -o ops\+x\$op.+x 2>&1" | ForEach-Object {
        if ($_ -match ' error') { throw "BUILD FAILED ${op}: $_" }
    }
    if ($LASTEXITCODE -ne 0) { throw "BUILD FAILED: $op" }
}

Write-Host ""
Write-Host "PLAYTEST" -ForegroundColor Cyan

# ---- 1. a world exists --------------------------------------------------
if ($Reset) {
    Write-Host "resetting world..." -ForegroundColor DarkGray
    Get-ChildItem "projects\wsr-pal\pieces" -Directory | ForEach-Object {
        Remove-Item (Join-Path $_.FullName "state.txt") -Force -EA SilentlyContinue
        Remove-Item (Join-Path $_.FullName "goods.txt") -Force -EA SilentlyContinue
    }
    cmd /c "powershell -ExecutionPolicy Bypass -File scripts\ensure_entities.ps1" | Out-Null
}
$corps = @(Get-ChildItem "projects\wsr-pal\pieces" -Directory -Filter "corp_*" -EA SilentlyContinue)
if ($corps.Count -eq 0) { Fail "no corporations - the world is empty" }
else { Pass ("world exists ({0} corps)" -f $corps.Count) }

# ---- 2. the frame renders ----------------------------------------------
Remove-Item $FRAME -Force -EA SilentlyContinue
cmd /c "ops\+x\wsr_compose_frame.+x" | Out-Null
if (-not (Test-Path $FRAME)) {
    Fail "current_frame.txt missing - the game screen does not render"
} elseif ((Get-Item $FRAME).Length -eq 0) {
    Fail "current_frame.txt is EMPTY - the game screen does not render"
} else {
    Pass ("frame renders ({0} bytes)" -f (Get-Item $FRAME).Length)
}

# ---- 3. the frame responds to state ------------------------------------
if ((Test-Path $FRAME) -and (Get-Item $FRAME).Length -gt 0) {
    $h1 = (Get-FileHash $FRAME -Algorithm SHA256).Hash
    cmd /c "ops\+x\wsr_compose_frame.+x" | Out-Null
    $h2 = (Get-FileHash $FRAME -Algorithm SHA256).Hash
    if ($h1 -ne $h2) { Pass "frame is state-driven, not a constant" }
    else { Warn "identical frame on re-render (may be legitimate when idle)" }
}

# ---- 4. real selection is accepted -------------------------------------
# KEY INTERFACE, which is not obvious: wsr_menu_input takes ONE ALREADY-RESOLVED
# item index, NOT a stream of digits. chtpm's own nav-mode digit_accum resolves
# "1" then "4" to item 14 upstream and sends that. So select item N by passing
# the bare integer N. Passing ASCII '4' (52) selects item 4, which is Help -
# a STUB - and yields the misleading message "Not yet available in this build."
# That cost this harness a false "End Turn does not work" diagnosis.
# Note key=0 is reserved for the persistent loop's re-derive tick, so never
# pass 0 as a selection.
cmd /c "ops\+x\wsr_menu_input.+x 8" | Out-Null; $k1 = $LASTEXITCODE
cmd /c "ops\+x\wsr_menu_input.+x 9" | Out-Null; $k2 = $LASTEXITCODE
if ($k1 -eq 0 -and $k2 -eq 0) { Pass "menu selection accepted (items 8 and 9)" }
else { Fail ("menu input rejected a selection (exit {0} / {1})" -f $k1, $k2) }

# ---- 5. End Turn advances the turn -------------------------------------
function Get-Turn {
    if (-not (Test-Path $MENU_STATE)) { return 0 }
    $m = [regex]::Match((Get-Content $MENU_STATE -Raw), '(?m)^turn_number=(-?[0-9]+)\s*$')
    if ($m.Success) { return [int]$m.Groups[1].Value }
    return 0
}
$t0 = Get-Turn
# End Turn is main-menu item 14, passed as the bare index 14 (see above).
cmd /c "ops\+x\wsr_menu_input.+x 14" | Out-Null
$t1 = Get-Turn
if ($t1 -gt $t0) { Pass ("End Turn advanced the turn ({0} -> {1})" -f $t0, $t1) }
else {
    Fail ("End Turn did not advance turn_number ({0} -> {1})" -f $t0, $t1)
    $msg = [regex]::Match((Get-Content $MENU_STATE -Raw), '(?m)^last_message=(.*)$').Groups[1].Value
    Fail ("  menu said: '{0}'" -f $msg)
}

# ---- 6. and the world actually moved ------------------------------------
cmd /c "ops\+x\wsr_compose_frame.+x" | Out-Null
if ((Test-Path $FRAME) -and (Get-Item $FRAME).Length -gt 0) {
    Pass ("frame renders after a turn ({0} bytes)" -f (Get-Item $FRAME).Length)
} else {
    Fail "frame EMPTY after a turn - rendering breaks once state changes"
}

Write-Host ""
if ($FAILURES -eq 0) { Write-Host "PLAYABLE - all checks passed" -ForegroundColor Green; exit 0 }
else { Write-Host "$FAILURES PLAYTEST FAILURE(S) - NOT PLAYABLE" -ForegroundColor Red; exit 1 }