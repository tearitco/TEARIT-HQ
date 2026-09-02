# button.ps1 - Windows launcher for wsr-pal (parity with button.sh)
# Usage: .\button.ps1 <action> [-Pal]
#
# If you get execution policy errors, run:
#   powershell -ExecutionPolicy Bypass -File .\button.ps1 <action>
#
# Linux stays on button.sh. Keep action names in lockstep with button.sh.

param(
    [Parameter(Position = 0)]
    [string]$Action = "help",
    [switch]$Pal,
    [Parameter(ValueFromRemainingArguments = $true)]
    [string[]]$Rest
)

$ErrorActionPreference = "Continue"
$SCRIPT_DIR = Split-Path -Parent $MyInvocation.MyCommand.Path
Set-Location $SCRIPT_DIR

# Ensure MinGW64 bin is on PATH (freeglut/gcc DLLs + toolchain)
$MSYS_BIN = "C:\msys64\mingw64\bin"
if (Test-Path $MSYS_BIN) {
    if ($env:Path -notlike "*$MSYS_BIN*") {
        $env:Path = "$MSYS_BIN;$env:Path"
    }
}

# Also accept --pal anywhere in remaining args (mirrors button.sh)
if ($Rest) {
    foreach ($a in $Rest) {
        if ($a -eq "--pal") { $Pal = $true }
    }
}

function Invoke-Build {
    & powershell -ExecutionPolicy Bypass -File (Join-Path $SCRIPT_DIR "scripts\build.ps1")
    if ($LASTEXITCODE -ne 0) { return $LASTEXITCODE }
    $orchSrc = Join-Path $SCRIPT_DIR "system\orchestrator.c"
    $orchOut = Join-Path $SCRIPT_DIR "system\orchestrator"
    if (Test-Path $orchSrc) {
        & gcc -Wall -Wextra -O2 $orchSrc -o $orchOut 2>$null
        if ($LASTEXITCODE -eq 0) { Write-Host "OK   system/orchestrator" }
        else { Write-Host "SKIP system/orchestrator" }
    }
    return 0
}

function Invoke-Kill {
    # IMPORTANT: do NOT read Process.Path here - enumerating Path for every
    # process on Windows can hang for a long time (access denied / WMI).
    # Match by ProcessName only.
    Write-Host "Stopping previous wsr processes..."
    $names = @(
        "keyboard_input", "renderer", "prisc+x", "chtpm_parser_pal",
        "chtpm_rgb_render", "gl_mirror", "orchestrator"
    )
    foreach ($n in $names) {
        # Stop-Process -Name supports wildcards on some hosts; use filter
        Get-Process -ErrorAction SilentlyContinue |
            Where-Object { $_.ProcessName -eq $n -or $_.ProcessName -like "$n*" } |
            ForEach-Object {
                try { Stop-Process -Id $_.Id -Force -ErrorAction Stop } catch { }
            }
    }
    # Extra safety: taskkill by image name (fast, no Path access)
    foreach ($n in $names) {
        & taskkill /F /IM "$n.exe" 2>$null | Out-Null
        & taskkill /F /IM $n 2>$null | Out-Null
    }
    $quit = Join-Path $SCRIPT_DIR "pieces\system\quit_flag.txt"
    $procList = Join-Path $SCRIPT_DIR "pieces\os\proc_list.txt"
    if (Test-Path $quit) { Remove-Item $quit -Force -ErrorAction SilentlyContinue }
    if (Test-Path $procList) { Remove-Item $procList -Force -ErrorAction SilentlyContinue }
    Write-Host "done"
}

function Invoke-Run {
    Write-Host "=== wsr-pal Windows launcher ===" -ForegroundColor Cyan

    if (Test-Path $MSYS_BIN) {
        if ($env:Path -notlike "*$MSYS_BIN*") { $env:Path = "$MSYS_BIN;$env:Path" }
    }

    Write-Host "[1/4] Checking binaries..."
    $need = @(
        "system\orchestrator.exe",
        "system\renderer.exe",
        "system\keyboard_input.exe",
        "system\chtpm_parser_pal.exe",
        "system\chtpm_rgb_render.exe",
        "system\prisc+x.exe",
        "ops\+x\wsr_compose_frame.+x",
        "ops\+x\wsr_menu_input.+x"
    )
    $missing = @()
    foreach ($b in $need) {
        if (-not (Test-Path (Join-Path $SCRIPT_DIR $b))) { $missing += $b }
    }
    if ($missing.Count -gt 0) {
        Write-Host "Missing binaries - running compile first..." -ForegroundColor Yellow
        foreach ($m in $missing) { Write-Host "  MISS $m" }
        $rc = Invoke-Build
        if ($rc -ne 0) { exit $rc }
    } else {
        Write-Host "  all required bins present"
    }

    Write-Host "[2/4] Killing previous session..."
    Invoke-Kill

    Write-Host "[3/4] Preparing session files..."
    $dirs = @(
        (Join-Path $SCRIPT_DIR "pieces\display"),
        (Join-Path $SCRIPT_DIR "pieces\apps\player_app"),
        (Join-Path $SCRIPT_DIR "pieces\keyboard"),
        (Join-Path $SCRIPT_DIR "pieces\system"),
        (Join-Path $SCRIPT_DIR "pieces\os")
    )
    foreach ($d in $dirs) {
        New-Item -ItemType Directory -Force -Path $d | Out-Null
    }

    $clearFiles = @(
        "pieces\display\renderer_pulse.txt",
        "pieces\display\frame_changed.txt",
        "pieces\apps\player_app\history.txt",
        "pieces\apps\player_app\interact_relay.txt",
        "pieces\keyboard\history.txt",
        "pieces\system\quit_flag.txt",
        "pieces\display\wsr_screen_changed.txt",
        "pieces\os\proc_list.txt"
    )
    foreach ($f in $clearFiles) {
        Set-Content -Path (Join-Path $SCRIPT_DIR $f) -Value "" -NoNewline
    }
    Set-Content -Path (Join-Path $SCRIPT_DIR "pieces\display\current_layout.txt") `
        -Value "pieces/chtpm/layouts/wsr_main_menu.chtpm" -NoNewline

    if ($Pal) {
        Write-Host "Mode: PAL layout explicit"
        $env:PAL_LAYOUT = "pieces/chtpm/layouts/wsr_main_menu.chtpm"
    } else {
        Write-Host "Mode: C / default chtpm layout"
        $env:PAL_LAYOUT = ""
    }

    # Relative root required on Windows for emoji house paths
    $env:PRISC_PROJECT_ROOT = "."
    $env:PRISC_PROJECT_ID = "wsr-pal"
    # Skip full recompile on every run
    $env:SKIP_ORCH_COMPILE = "1"

    Write-Host "[4/4] Starting orchestrator..." -ForegroundColor Green
    Write-Host "  - Look for GL window: 'wsr-pal RGB mirror'"
    Write-Host "  - Click that window, then use number keys + Enter"
    Write-Host "  - Ctrl+C here stops the orchestrator; then: .\button.ps1 kill"
    Write-Host ""

    $orchExe = Join-Path $SCRIPT_DIR "system\orchestrator.exe"
    $orch = Join-Path $SCRIPT_DIR "system\orchestrator"
    if (Test-Path $orchExe) {
        & $orchExe
    } elseif (Test-Path $orch) {
        & $orch
    } else {
        Write-Error "system/orchestrator not found. Run: .\button.ps1 compile"
        exit 1
    }
}

$act = $Action.ToLower()

if ($act -in @("compile", "c", "build")) {
    exit (Invoke-Build)
}
elseif ($act -in @("run", "r", "start")) {
    Invoke-Run
}
elseif ($act -in @("kill", "k", "stop")) {
    Invoke-Kill
}
elseif ($act -eq "sim-key") {
    $keycode = if ($Rest -and $Rest.Count -ge 1) { $Rest[0] } else { "" }
    $wait = 1
    if ($Rest -and $Rest.Count -ge 2) { $wait = [int]$Rest[1] }
    $env:PRISC_PROJECT_ROOT = "."
    $env:PRISC_PROJECT_ID = "wsr-pal"
    New-Item -ItemType Directory -Force -Path (Join-Path $SCRIPT_DIR "pieces\apps\player_app") | Out-Null
    New-Item -ItemType Directory -Force -Path (Join-Path $SCRIPT_DIR "pieces\display") | Out-Null
    $hist = Join-Path $SCRIPT_DIR "pieces\apps\player_app\history.txt"
    Add-Content -Path $hist -Value $keycode
    $prisc = Join-Path $SCRIPT_DIR "system\prisc+x"
    if (Test-Path ($prisc + ".exe")) { $prisc = $prisc + ".exe" }
    $log = Join-Path $env:TEMP "wsr_simkey.log"
    $proc = Start-Process -FilePath $prisc -ArgumentList "pal/main_loop.pal" `
        -WorkingDirectory $SCRIPT_DIR -PassThru -NoNewWindow `
        -RedirectStandardOutput $log -RedirectStandardError $log
    Start-Sleep -Seconds $wait
    if (-not $proc.HasExited) { Stop-Process -Id $proc.Id -Force -ErrorAction SilentlyContinue }
}
elseif ($act -eq "new-game") {
    $env:PRISC_PROJECT_ROOT = "."
    $pdl = Join-Path $SCRIPT_DIR "projects\wsr-pal\pieces\wsr_main_menu\piece.pdl"
    if (-not (Test-Path $pdl)) {
        Write-Error "Could not find wsr_main_menu/piece.pdl - aborting."
        exit 1
    }
    $methodIdx = 0
    $newGameRow = $null
    Get-Content $pdl | ForEach-Object {
        if ($_ -match '^METHOD') {
            $methodIdx++
            if ($_ -match 'START_WIZARD:new_game' -or $_ -match 'NEW_GAME') {
                $newGameRow = $methodIdx
            }
        }
    }
    if (-not $newGameRow) {
        Write-Error "Could not find New Game row in wsr_main_menu/piece.pdl - aborting."
        exit 1
    }
    New-Item -ItemType Directory -Force -Path (Join-Path $SCRIPT_DIR "pieces\display") | Out-Null
    Set-Content -Path (Join-Path $SCRIPT_DIR "pieces\display\current_layout.txt") `
        -Value "pieces/chtpm/layouts/wsr_main_menu.chtpm" -NoNewline
    $op = Join-Path $SCRIPT_DIR "ops\+x\wsr_menu_input.+x"
    if (Test-Path ($op + ".exe")) { $op = $op + ".exe" }
    $key = 48 + [int]$newGameRow
    & cmd /c "`"$op`" $key"
    & cmd /c "`"$op`" 10"
    Write-Host "new game started (world reset from pieces_template)"
}
elseif ($act -in @("tick-all", "ta")) {
    $rounds = if ($Rest -and $Rest.Count -ge 1) { $Rest[0] } else { "1" }
    & powershell -ExecutionPolicy Bypass -File (Join-Path $SCRIPT_DIR "scripts\ensure_entities.ps1")
    & powershell -ExecutionPolicy Bypass -File (Join-Path $SCRIPT_DIR "scripts\tick_all.ps1") $rounds
}
elseif ($act -in @("test-tick", "test-run")) {
    & powershell -ExecutionPolicy Bypass -File (Join-Path $SCRIPT_DIR "scripts\ensure_entities.ps1")
    $env:PRISC_PROJECT_ROOT = "."
    $env:PRISC_PROJECT_ID = "wsr-pal"
    $prisc = Join-Path $SCRIPT_DIR "system\prisc+x"
    if (Test-Path ($prisc + ".exe")) { $prisc = $prisc + ".exe" }
    & cmd /c "`"$prisc`" pal/single_tick.pal"
    Get-Content (Join-Path $SCRIPT_DIR "projects\wsr-pal\pieces\corp_ORB\state.txt")
}
elseif ($act -eq "test-choose") {
    $env:PRISC_PROJECT_ROOT = "."
    $decision = if ($Rest -and $Rest.Count -ge 1) { $Rest[0] } else { "" }
    $op = Join-Path $SCRIPT_DIR "ops\+x\corp_set_human_decision.+x"
    if (Test-Path ($op + ".exe")) { $op = $op + ".exe" }
    & cmd /c "`"$op`" corp_ORB $decision"
}
elseif ($act -eq "reset") {
    $state = Join-Path $SCRIPT_DIR "projects\wsr-pal\pieces\corp_ORB\state.txt"
    $content = @(
        "current_state=0",
        "decision_mode=1",
        "cash=183.44",
        "stock_price=167.70",
        "book_value=1740.05",
        "shares_outstanding=11.24",
        "market_cap=1884.31",
        "debt_to_equity=0.08",
        "risk_bias=9",
        "shares_held=0",
        "pending_action=",
        "last_action=",
        "human_decision="
    ) -join "`n"
    Set-Content -Path $state -Value $content -NoNewline
    Write-Host "corp_ORB reset (real seed data: Orbital Express / ORB)."
}
elseif ($act -in @("check", "verify")) {
    $bins = @(
        "system/prisc+x", "system/keyboard_input", "system/renderer",
        "ops/+x/corp_tick_idle.+x", "ops/+x/corp_decide.+x", "ops/+x/corp_trade.+x",
        "ops/+x/wsr_menu_input.+x", "ops/+x/wsr_compose_frame.+x",
        "ops/+x/connect_op.+x", "ops/+x/json_parser.+x"
    )
    foreach ($b in $bins) {
        $p = Join-Path $SCRIPT_DIR ($b -replace '/', '\')
        if ((Test-Path $p) -or (Test-Path ($p + ".exe"))) {
            Write-Host "OK   $b"
        } else {
            Write-Host "MISSING $b"
        }
    }
}
elseif ($act -in @("help", "h", "-h", "--help")) {
    Write-Host "wsr-pal button.ps1 (Windows)" -ForegroundColor Cyan
    Write-Host ""
    Write-Host "Usage: .\button.ps1 <action> [-Pal]"
    Write-Host "  compile, c, build   - Build prisc+x + orchestrator + ops"
    Write-Host "  run, r              - THE REAL PLAYABLE GAME"
    Write-Host "  -Pal                - Use PAL script mode (passed to run)"
    Write-Host "  kill, k, stop       - Kill any lingering wsr-pal processes"
    Write-Host "  sim-key <code>      - Test playable interface non-interactively"
    Write-Host "  new-game            - Reset the world to initial entity state"
    Write-Host "  tick-all [rounds]   - Advance every entity N rounds"
    Write-Host "  test-tick/test-choose/reset - single-corp-ORB CLI tests"
    Write-Host "  check, verify       - Verify all binaries exist"
    Write-Host "  help, h             - Show this help"
    Write-Host ""
    Write-Host "Linux: use ./button.sh (unchanged)."
    Write-Host "Toolchain: MSYS2 MinGW64 (gcc + freeglut)."
}
else {
    Write-Error "Unknown action: $Action"
    Write-Host "Run '.\button.ps1 help' for usage."
    exit 1
}
