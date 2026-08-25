# button.ps1 - Windows launcher for piececraft-xyz (parity with button.sh)
# Usage: powershell -ExecutionPolicy Bypass -File .\button.ps1 <action>
# Actions: compile|c|build | run|r|start | kill|k|stop | check|verify | help
# ASCII only. Linux stays on button.sh.

param(
    [Parameter(Position = 0)]
    [string]$Action = "help"
)

$ErrorActionPreference = "Continue"
$SCRIPT_DIR = Split-Path -Parent $MyInvocation.MyCommand.Path
Set-Location -LiteralPath $SCRIPT_DIR

$MSYS = "C:\msys64\mingw64\bin"
if (Test-Path $MSYS) {
    if ($env:Path -notlike "*$MSYS*") { $env:Path = "$MSYS;$env:Path" }
}

# House = two levels up from @.apps/piececraft-xyz
$HOUSE_DIR = Split-Path (Split-Path $SCRIPT_DIR -Parent) -Parent

function Get-Bin([string]$rel) {
    $exe = Join-Path $SCRIPT_DIR ($rel + ".exe")
    if (Test-Path -LiteralPath $exe) { return $exe }
    $plain = Join-Path $SCRIPT_DIR $rel
    if (Test-Path -LiteralPath $plain) { return $plain }
    return $null
}

function Invoke-Kill {
    Write-Host "Stopping piececraft-xyz processes (name-only)..."
    $names = @(
        "keyboard_input", "renderer", "prisc+x", "chtpm_parser_pal",
        "chtpm_rgb_render", "gl_mirror", "orchestrator",
        "pc_menu_input", "pc_compose_frame", "pc_clock_daemon",
        "pc_generate_chunk", "pc_phymoji_gen"
    )
    foreach ($n in $names) {
        Get-Process -ErrorAction SilentlyContinue |
            Where-Object { $_.ProcessName -eq $n -or $_.ProcessName -like "$n*" } |
            ForEach-Object { try { Stop-Process -Id $_.Id -Force } catch {} }
        & taskkill /F /IM "$n.exe" 2>$null | Out-Null
        & taskkill /F /IM $n 2>$null | Out-Null
    }
    # board-viewer scoped to this project (best-effort name match)
    Get-Process -ErrorAction SilentlyContinue |
        Where-Object { $_.ProcessName -match "board|viewer" } |
        ForEach-Object { try { Stop-Process -Id $_.Id -Force } catch {} }
    Write-Host "done"
}

function Invoke-Compile {
    $build = Join-Path $SCRIPT_DIR "scripts\build.ps1"
    if (-not (Test-Path -LiteralPath $build)) {
        Write-Error "MISSING scripts\build.ps1"
        return 1
    }
    & powershell -ExecutionPolicy Bypass -File $build
    return $LASTEXITCODE
}

function Copy-TreeLink([string]$src, [string]$dst) {
    if (-not (Test-Path -LiteralPath $src)) { return }
    if (Test-Path -LiteralPath $dst) { return }
    # Prefer junction/symlink; fallback copy
    try {
        New-Item -ItemType Junction -Path $dst -Target $src -ErrorAction Stop | Out-Null
    } catch {
        try {
            New-Item -ItemType SymbolicLink -Path $dst -Target $src -ErrorAction Stop | Out-Null
        } catch {
            if ((Get-Item $src).PSIsContainer) {
                Copy-Item -LiteralPath $src -Destination $dst -Recurse -Force
            } else {
                Copy-Item -LiteralPath $src -Destination $dst -Force
            }
        }
    }
}

function Invoke-Run {
    Write-Host "=== piececraft-xyz Windows launcher ===" -ForegroundColor Cyan

    Write-Host "[1/4] Checking binaries..."
    $need = @(
        "system\orchestrator.exe",
        "system\keyboard_input.exe",
        "system\prisc+x.exe",
        "system\chtpm_parser_pal.exe",
        "ops\+x\pc_menu_input.+x",
        "ops\+x\pc_compose_frame.+x"
    )
    $missing = @($need | Where-Object { -not (Test-Path (Join-Path $SCRIPT_DIR $_)) })
    if ($missing.Count -gt 0) {
        Write-Host "Missing bins - compiling..." -ForegroundColor Yellow
        $rc = Invoke-Compile
        if ($rc -ne 0) { return $rc }
    } else {
        Write-Host "  required bins present"
    }

    Write-Host "[2/4] Kill previous session..."
    Invoke-Kill

    Write-Host "[3/4] Session isolation..."
    $sid = "{0}-{1}" -f [int][double]::Parse((Get-Date -UFormat %s)), $PID
    $SESSION = Join-Path $SCRIPT_DIR "pieces\sessions\$sid"
    foreach ($d in @(
        "pieces\system", "pieces\display", "pieces\apps\player_app",
        "pieces\keyboard", "pieces\os", "projects\piececraft-xyz\manager"
    )) {
        New-Item -ItemType Directory -Path (Join-Path $SESSION $d) -Force | Out-Null
    }
    New-Item -ItemType Directory -Path (Join-Path $SCRIPT_DIR "data") -Force | Out-Null

    Copy-TreeLink (Join-Path $SCRIPT_DIR "system") (Join-Path $SESSION "system")
    Copy-TreeLink (Join-Path $SCRIPT_DIR "ops") (Join-Path $SESSION "ops")
    Copy-TreeLink (Join-Path $SCRIPT_DIR "pal") (Join-Path $SESSION "pal")
    Copy-TreeLink (Join-Path $SCRIPT_DIR "default_op.txt") (Join-Path $SESSION "default_op.txt")
    Copy-TreeLink (Join-Path $SCRIPT_DIR "pieces\chtpm") (Join-Path $SESSION "pieces\chtpm")
    Copy-TreeLink (Join-Path $SCRIPT_DIR "pieces\registry") (Join-Path $SESSION "pieces\registry")
    Copy-TreeLink (Join-Path $SCRIPT_DIR "projects\piececraft-xyz\pieces") (Join-Path $SESSION "projects\piececraft-xyz\pieces")
    Copy-TreeLink (Join-Path $SCRIPT_DIR "data") (Join-Path $SESSION "data")

    # persistent config / board / entities
    $sysReal = Join-Path $SCRIPT_DIR "pieces\system"
    New-Item -ItemType Directory -Path $sysReal -Force | Out-Null
    $cfg = Join-Path $sysReal "config.txt"
    if (-not (Test-Path $cfg)) {
        @"
game_id=piececraft-xyz-001
turn=1
turn_order_index=0
victory_condition=
map_scale=
combat_resolution=
treasury=50
city_count=1
game_state=setup
"@ | Set-Content -LiteralPath $cfg -Encoding utf8
    }
    foreach ($f in @("board.txt", "entities.txt")) {
        $fp = Join-Path $sysReal $f
        if (-not (Test-Path $fp)) { New-Item -ItemType File -Path $fp -Force | Out-Null }
    }
    Copy-TreeLink $cfg (Join-Path $SESSION "pieces\system\config.txt")
    Copy-TreeLink (Join-Path $sysReal "board.txt") (Join-Path $SESSION "pieces\system\board.txt")
    Copy-TreeLink (Join-Path $sysReal "entities.txt") (Join-Path $SESSION "pieces\system\entities.txt")

    $wcmd = Join-Path $sysReal "widget_cmds"
    New-Item -ItemType Directory -Path $wcmd -Force | Out-Null
    $inbox = Join-Path $wcmd "inbox.txt"
    if (-not (Test-Path $inbox)) { New-Item -ItemType File -Path $inbox -Force | Out-Null }
    Copy-TreeLink $wcmd (Join-Path $SESSION "pieces\system\widget_cmds")

    @"
inbox_path=pieces/system/widget_cmds/inbox.txt
kind=board_game
project_id=piececraft-xyz
display_name=Aomorai-editor
"@ | Set-Content -LiteralPath (Join-Path $sysReal "board_widget_bridge.txt") -Encoding utf8
    Copy-TreeLink (Join-Path $sysReal "board_widget_bridge.txt") (Join-Path $SESSION "pieces\system\board_widget_bridge.txt")

    # seed session empties
    foreach ($f in @(
        "pieces\apps\player_app\interact_relay.txt",
        "pieces\keyboard\history.txt",
        "pieces\system\quit_flag.txt",
        "pieces\display\pc_screen_changed.txt",
        "pieces\display\frame_changed.txt",
        "projects\piececraft-xyz\manager\gui_state.txt"
    )) {
        $fp = Join-Path $SESSION $f
        New-Item -ItemType File -Path $fp -Force | Out-Null
    }

    # UTF-8 NO BOM + house-relative real root (emoji abs paths break MinGW fopen)
    $utf8NoBom = New-Object System.Text.UTF8Encoding $false
    function Write-NoBom([string]$Path, [string]$Text) {
        [System.IO.File]::WriteAllText($Path, $Text, $utf8NoBom)
    }
    $houseAbs = $HOUSE_DIR
    try { $houseAbs = (Resolve-Path -LiteralPath $HOUSE_DIR).Path } catch {}
    $scriptAbs = $SCRIPT_DIR
    try { $scriptAbs = (Resolve-Path -LiteralPath $SCRIPT_DIR).Path } catch {}
    Write-NoBom (Join-Path $SESSION "pieces\system\house_root.txt") ($houseAbs + "`n")
    # Prefer relative @.apps/... so C ops can join house_root safely
    $realRel = $scriptAbs
    if ($scriptAbs.StartsWith($houseAbs, [StringComparison]::OrdinalIgnoreCase)) {
        $realRel = ($scriptAbs.Substring($houseAbs.Length).TrimStart('\', '/') -replace '\\', '/')
    }
    Write-NoBom (Join-Path $SESSION "pieces\system\real_project_root.txt") ($realRel + "`n")
    Write-Host ("  real_project_root={0}" -f $realRel)

    Write-NoBom (Join-Path $SESSION "pieces\apps\player_app\state.txt") @"
module_path=system/prisc+x pal/new_game_module.pal
project_id=piececraft-xyz
active_target_id=new_game
"@

    Write-Host "[4/4] Launch (session=$sid)..."
    # CRITICAL (wsr/TPMOS Win pitfall): use relative "." not absolute
    # house paths. Absolute paths with emoji/Unicode break MinGW ANSI
    # fopen → empty current_frame / empty terminal / black GL.
    $env:PRISC_PROJECT_ROOT = "."
    $env:PRISC_PROJECT_ID = "piececraft-xyz"
    $env:NO_NET = "1"
    $env:PAL_LAYOUT = "pieces/chtpm/layouts/new_game.chtpm"
    $env:SKIP_ORCH_COMPILE = "1"

    # Prefer bins from session (junction) so relative paths resolve
    $orch = $null
    $kb = $null
    foreach ($cand in @(
        (Join-Path $SESSION "system\orchestrator.exe"),
        (Join-Path $SESSION "system\orchestrator"),
        (Get-Bin "system\orchestrator")
    )) {
        if ($cand -and (Test-Path -LiteralPath $cand)) { $orch = $cand; break }
    }
    foreach ($cand in @(
        (Join-Path $SESSION "system\keyboard_input.exe"),
        (Join-Path $SESSION "system\keyboard_input"),
        (Get-Bin "system\keyboard_input")
    )) {
        if ($cand -and (Test-Path -LiteralPath $cand)) { $kb = $cand; break }
    }
    if (-not $orch -or -not $kb) {
        Write-Error "MISSING orchestrator or keyboard_input"
        return 1
    }

    $orchLog = Join-Path $SESSION "pieces\system\orchestrator.log"
    $pOrch = Start-Process -FilePath $orch -WorkingDirectory $SESSION `
        -RedirectStandardError $orchLog -WindowStyle Hidden -PassThru

    # Wait for parser to compose first frame (relative path under session CWD)
    $frame = Join-Path $SESSION "pieces\display\current_frame.txt"
    for ($i = 0; $i -lt 50; $i++) {
        if ((Test-Path -LiteralPath $frame) -and ((Get-Item -LiteralPath $frame).Length -gt 0)) {
            Write-Host "  frame ready ($((Get-Item -LiteralPath $frame).Length) bytes) after $($i * 100)ms"
            break
        }
        Start-Sleep -Milliseconds 100
    }
    if (-not (Test-Path -LiteralPath $frame) -or ((Get-Item -LiteralPath $frame).Length -eq 0)) {
        Write-Host "WARN: current_frame.txt still empty - check session debug.txt / orch log" -ForegroundColor Yellow
        $dbg = Join-Path $SESSION "debug.txt"
        if (Test-Path -LiteralPath $dbg) { Get-Content -LiteralPath $dbg -Tail 15 }
        if (Test-Path -LiteralPath $orchLog) { Get-Content -LiteralPath $orchLog -Tail 15 }
    }

    $gl = $null
    $rgb = $null
    foreach ($cand in @(
        (Join-Path $SESSION "system\chtpm_rgb_render.exe"),
        (Join-Path $SESSION "system\chtpm_rgb_render"),
        (Get-Bin "system\chtpm_rgb_render")
    )) {
        if ($cand -and (Test-Path -LiteralPath $cand)) { $rgb = $cand; break }
    }
    foreach ($cand in @(
        (Join-Path $SESSION "system\gl_mirror.exe"),
        (Join-Path $SESSION "system\gl_mirror"),
        (Get-Bin "system\gl_mirror")
    )) {
        if ($cand -and (Test-Path -LiteralPath $cand)) { $gl = $cand; break }
    }

    # libfreeglut.dll must be loadable (MSYS2 MinGW name). Copy next to
    # gl_mirror so session launches work even if PATH is thin.
    $glutCandidates = @(
        "C:\msys64\mingw64\bin\libfreeglut.dll",
        "C:\msys64\mingw64\bin\freeglut.dll"
    )
    foreach ($glutDll in $glutCandidates) {
        if (-not (Test-Path $glutDll)) { continue }
        $dllName = Split-Path $glutDll -Leaf
        foreach ($destDir in @((Join-Path $SCRIPT_DIR "system"), (Join-Path $SESSION "system"))) {
            if (-not (Test-Path -LiteralPath $destDir)) { continue }
            $dest = Join-Path $destDir $dllName
            if (-not (Test-Path -LiteralPath $dest)) {
                try { Copy-Item -LiteralPath $glutDll -Destination $dest -Force -EA Stop } catch {}
            }
        }
        break
    }

    $pGl = $null
    $pRgb = $null
    if ($env:NO_GL -ne "1") {
        if ($rgb) {
            $pRgb = Start-Process -FilePath $rgb -WorkingDirectory $SESSION -WindowStyle Hidden -PassThru
            Write-Host "RGB chtpm_rgb_render pid=$($pRgb.Id)"
        }
        # Wait for a non-trivial rgb_frame.raw (receipt-sized frame, not empty)
        $raw = Join-Path $SESSION "pieces\display\rgb_frame.raw"
        $rawReady = $false
        for ($i = 0; $i -lt 60; $i++) {
            if ((Test-Path -LiteralPath $raw) -and ((Get-Item -LiteralPath $raw).Length -gt 10000)) {
                Write-Host "  rgb_frame.raw ready ($((Get-Item -LiteralPath $raw).Length) bytes) after $($i * 100)ms"
                $rawReady = $true
                break
            }
            Start-Sleep -Milliseconds 100
        }
        if (-not $rawReady) {
            Write-Host "WARN: rgb_frame.raw not ready - GL may be black/missing" -ForegroundColor Yellow
        }
        if ($gl) {
            # GUI process: own window (not Hidden). Pass env so root stays "."
            $pGl = Start-Process -FilePath $gl -WorkingDirectory $SESSION `
                -WindowStyle Normal -PassThru
            Start-Sleep -Milliseconds 400
            if ($pGl.HasExited) {
                Write-Host "FAIL: gl_mirror exited immediately (code=$($pGl.ExitCode)) - freeglut/DLL?" -ForegroundColor Red
                Write-Host "  try: ensure C:\msys64\mingw64\bin is on PATH and freeglut installed"
            } else {
                Write-Host "GL gl_mirror pid=$($pGl.Id)  (window title usually includes 'mirror' / freeglut)"
                try {
                    # Raise GL window if Win32 can see it
                    $null = $pGl.Refresh()
                    if ($pGl.MainWindowHandle -ne [IntPtr]::Zero) {
                        Add-Type @"
using System;
using System.Runtime.InteropServices;
public class WinRaise {
  [DllImport("user32.dll")] public static extern bool SetForegroundWindow(IntPtr h);
  [DllImport("user32.dll")] public static extern bool ShowWindow(IntPtr h, int n);
}
"@ -ErrorAction SilentlyContinue
                        [WinRaise]::ShowWindow($pGl.MainWindowHandle, 9) | Out-Null  # SW_RESTORE
                        [WinRaise]::SetForegroundWindow($pGl.MainWindowHandle) | Out-Null
                    }
                } catch {}
            }
        } else {
            Write-Host "OPTIONAL: no gl_mirror.exe (ASCII/orchestrator only)"
        }
    }

    # Terminal frames in THIS console (Linux/wsr usual), not a second window.
    # -NoNewWindow: renderer stdout shares this console; keyboard keeps stdin.
    $rend = $null
    foreach ($cand in @(
        (Join-Path $SESSION "system\renderer.exe"),
        (Join-Path $SESSION "system\renderer"),
        (Get-Bin "system\renderer")
    )) {
        if ($cand -and (Test-Path -LiteralPath $cand)) { $rend = $cand; break }
    }
    $pRend = $null
    if ($rend -and $env:NO_TERM -ne "1") {
        $pRend = Start-Process -FilePath $rend -WorkingDirectory $SESSION `
            -NoNewWindow -PassThru
        Write-Host "TERM renderer pid=$($pRend.Id) (same console)"
    }

    Write-Host ""
    Write-Host "Keys: focus THIS console for arrows (native conhost/WT, not mintty)."
    Write-Host "GL: freeglut window should be open; click it for mouse/keys if focused there."
    Write-Host "Ctrl+C here quits."
    Write-Host ""

    Push-Location -LiteralPath $SESSION
    try {
        & $kb
    } finally {
        Pop-Location
        if ($pOrch -and -not $pOrch.HasExited) { Stop-Process -Id $pOrch.Id -Force -EA SilentlyContinue }
        if ($pGl -and -not $pGl.HasExited) { Stop-Process -Id $pGl.Id -Force -EA SilentlyContinue }
        if ($pRgb -and -not $pRgb.HasExited) { Stop-Process -Id $pRgb.Id -Force -EA SilentlyContinue }
        if ($pRend -and -not $pRend.HasExited) { Stop-Process -Id $pRend.Id -Force -EA SilentlyContinue }
        Invoke-Kill
        if (Test-Path -LiteralPath $SESSION) {
            Remove-Item -LiteralPath $SESSION -Recurse -Force -ErrorAction SilentlyContinue
        }
    }
    return 0
}

function Invoke-Check {
    $bins = @(
        "system\prisc+x.exe", "system\keyboard_input.exe", "system\renderer.exe",
        "system\chtpm_parser_pal.exe", "system\orchestrator.exe",
        "ops\+x\pc_menu_input.+x", "ops\+x\pc_compose_frame.+x"
    )
    foreach ($b in $bins) {
        if (Test-Path (Join-Path $SCRIPT_DIR $b)) { Write-Host "OK   $b" }
        else { Write-Host "MISSING $b" }
    }
    foreach ($b in @("system\chtpm_rgb_render.exe", "system\gl_mirror.exe")) {
        if (Test-Path (Join-Path $SCRIPT_DIR $b)) { Write-Host "OK   $b (GL)" }
        else { Write-Host "OPTIONAL-MISS $b" }
    }
    return 0
}

$a = $Action.ToLowerInvariant()
if ($a -in @("compile", "c", "build")) { exit (Invoke-Compile) }
elseif ($a -in @("run", "r", "start")) { exit (Invoke-Run) }
elseif ($a -in @("kill", "k", "stop")) { Invoke-Kill; exit 0 }
elseif ($a -in @("check", "verify")) { exit (Invoke-Check) }
else {
    Write-Host @"
piececraft-xyz button.ps1 (Windows)

  .\button.ps1 compile | run | kill | check

Linux: sh button.sh ...
See house WIN-CONVERSION-STATUS.md
"@
    exit 0
}
