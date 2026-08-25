# button.ps1 - native Windows launcher for event-ez (no bash required)
# Usage:
#   powershell -ExecutionPolicy Bypass -File .\button.ps1 run
#   set EZ_PKG_NAME / EZ_PKG_DIR env before run (open_event_ez.ps1 does this)
#
# Mirrors button.sh CHTPM pipeline: session dir, muta system bins (.exe),
# ez ops, gl_mirror freeglut. Linux stays on button.sh.

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

$HOUSE = Split-Path (Split-Path $SCRIPT_DIR -Parent) -Parent
$MUTA = Get-ChildItem -LiteralPath $HOUSE -Directory -ErrorAction SilentlyContinue |
    Where-Object { $_.Name -like "101.mutaclsym*" } |
    Select-Object -First 1

function Get-SysBin([string]$sessionDir, [string]$name) {
    $exe = Join-Path $sessionDir "system\$name.exe"
    if (Test-Path -LiteralPath $exe) { return $exe }
    $plain = Join-Path $sessionDir "system\$name"
    if (Test-Path -LiteralPath $plain) { return $plain }
    return $null
}

function Invoke-Kill {
    $names = @("gl_mirror", "chtpm_rgb_render", "chtpm_parser_pal", "prisc+x",
               "renderer", "keyboard_input", "ez_compose_frame", "ez_menu_input")
    foreach ($n in $names) {
        Get-Process -ErrorAction SilentlyContinue |
            Where-Object { $_.ProcessName -eq $n -or $_.ProcessName -like "$n*" } |
            ForEach-Object { try { Stop-Process -Id $_.Id -Force } catch {} }
        & taskkill /F /IM "$n.exe" 2>$null | Out-Null
    }
    $sess = Join-Path $SCRIPT_DIR "pieces\sessions"
    if (Test-Path -LiteralPath $sess) {
        Get-ChildItem -LiteralPath $sess -Directory -ErrorAction SilentlyContinue |
            ForEach-Object { Remove-Item -LiteralPath $_.FullName -Recurse -Force -ErrorAction SilentlyContinue }
    }
    Write-Host "event-ez: kill done"
}

function Invoke-Compile {
    $opsX = Join-Path $SCRIPT_DIR "ops\+x"
    if (-not (Test-Path $opsX)) { New-Item -ItemType Directory -Path $opsX -Force | Out-Null }
    Push-Location (Join-Path $SCRIPT_DIR "ops")
    foreach ($op in @("ez_compose_frame", "ez_menu_input")) {
        if (-not (Test-Path "$op.c")) { continue }
        Write-Host "gcc $op.c"
        & gcc -Wall -O2 -o "+x\$op.+x" "$op.c" 2>&1 | Out-Null
        # also emit .exe name for Windows launch
        & gcc -Wall -O2 -o "+x\$op.exe" "$op.c" 2>&1 | Out-Null
        if ($LASTEXITCODE -eq 0) { Write-Host "OK $op" } else { Write-Host "FAIL $op" }
    }
    Pop-Location
    if (-not $MUTA) { Write-Host "MISSING muta"; return 1 }
    $need = @("chtpm_parser_pal.exe", "gl_mirror.exe", "prisc+x.exe", "chtpm_rgb_render.exe")
    foreach ($b in $need) {
        $p = Join-Path $MUTA.FullName "system\$b"
        if (Test-Path -LiteralPath $p) { Write-Host "OK muta $b" }
        else { Write-Host "MISSING muta $b" }
    }
    return 0
}

function Invoke-Run {
    if (-not $MUTA) {
        Write-Error "Need 101.mutaclsym* under house for system bins"
        return 1
    }
    $parser = Join-Path $MUTA.FullName "system\chtpm_parser_pal.exe"
    if (-not (Test-Path -LiteralPath $parser)) {
        $parser = Join-Path $MUTA.FullName "system\chtpm_parser_pal"
    }
    if (-not (Test-Path -LiteralPath $parser)) {
        Write-Error "Need muta system/chtpm_parser_pal(.exe)"
        return 1
    }

    $null = Invoke-Compile

    $pkgName = $env:EZ_PKG_NAME
    if (-not $pkgName) { $pkgName = "(none)" }
    $pkgDir = $env:EZ_PKG_DIR
    if (-not $pkgDir) { $pkgDir = "" }

    $sid = "{0}-{1}" -f [int][double]::Parse((Get-Date -UFormat %s)), $PID
    $SESSION = Join-Path $SCRIPT_DIR "pieces\sessions\$sid"
    $dirs = @(
        "pieces\system", "pieces\display", "pieces\apps\player_app",
        "pieces\keyboard", "projects\event-ez\manager", "pieces\os",
        "pieces\debug\frames", "system", "ops", "pal", "pieces\chtpm"
    )
    foreach ($d in $dirs) {
        New-Item -ItemType Directory -Path (Join-Path $SESSION $d) -Force | Out-Null
    }

    # copy system bins (no symlink required)
    $mutaSys = Join-Path $MUTA.FullName "system"
    $sessSys = Join-Path $SESSION "system"
    foreach ($b in @("chtpm_parser_pal", "chtpm_rgb_render", "gl_mirror", "prisc+x", "renderer", "keyboard_input")) {
        $exe = Join-Path $mutaSys "$b.exe"
        if (Test-Path -LiteralPath $exe) {
            Copy-Item -LiteralPath $exe -Destination (Join-Path $sessSys "$b.exe") -Force
        }
    }
    # ops
    $opsSrc = Join-Path $SCRIPT_DIR "ops"
    $opsDst = Join-Path $SESSION "ops"
    if (Test-Path $opsSrc) {
        Copy-Item -LiteralPath $opsSrc -Destination $opsDst -Recurse -Force
    }
    # pal + layout
    $palSrc = Join-Path $SCRIPT_DIR "pal"
    if (Test-Path $palSrc) {
        Copy-Item -LiteralPath $palSrc -Destination (Join-Path $SESSION "pal") -Recurse -Force
    }
    $chtpmSrc = Join-Path $SCRIPT_DIR "pieces\chtpm"
    if (Test-Path $chtpmSrc) {
        Copy-Item -LiteralPath $chtpmSrc -Destination (Join-Path $SESSION "pieces\chtpm") -Recurse -Force
    }
    $def = Join-Path $SCRIPT_DIR "default_op.txt"
    if (Test-Path $def) { Copy-Item $def (Join-Path $SESSION "default_op.txt") -Force }
    $reg = Join-Path $MUTA.FullName "pieces\registry"
    if (Test-Path $reg) {
        Copy-Item -LiteralPath $reg -Destination (Join-Path $SESSION "pieces\registry") -Recurse -Force
    }

    # session files
    $emptyFiles = @(
        "pieces\display\frame_changed.txt",
        "pieces\display\renderer_pulse.txt",
        "pieces\display\ez_screen_changed.txt",
        "pieces\apps\player_app\history.txt",
        "pieces\keyboard\history.txt",
        "pieces\apps\player_app\interact_relay.txt",
        "pieces\os\proc_list.txt",
        "projects\event-ez\manager\gui_state.txt"
    )
    foreach ($f in $emptyFiles) {
        $fp = Join-Path $SESSION $f
        New-Item -ItemType File -Path $fp -Force | Out-Null
    }

    @"
pkg_name=$pkgName
pkg_dir=$pkgDir
behavior=
last_message=widget profile: GL primary (button.ps1)
"@ | Set-Content -LiteralPath (Join-Path $SESSION "pieces\system\ez_state.txt") -Encoding utf8

    @"
module_path=system/prisc+x pal/main_loop_chtpm.pal
project_id=event-ez
active_target_id=event_ez
launch_profile=widget
ascii_renderer=0
gl_window=1
"@ | Set-Content -LiteralPath (Join-Path $SESSION "pieces\apps\player_app\state.txt") -Encoding utf8

    $env:PRISC_PROJECT_ROOT = $SESSION
    $env:PRISC_PROJECT_ID = "event-ez"
    $env:EZ_PKG_NAME = $pkgName
    $env:EZ_PKG_DIR = $pkgDir

    # compose once
    $compose = Join-Path $SESSION "ops\+x\ez_compose_frame.exe"
    if (-not (Test-Path $compose)) { $compose = Join-Path $SESSION "ops\+x\ez_compose_frame.+x" }
    if (Test-Path $compose) {
        Push-Location $SESSION
        try { & $compose 2>$null } catch {}
        Pop-Location
    }

    $procList = Join-Path $SESSION "pieces\os\proc_list.txt"
    function Start-Logged {
        param(
            [string]$Bin,
            [string[]]$ArgList = @(),
            [string]$WindowStyle = "Hidden"
        )
        if (-not $Bin -or -not (Test-Path -LiteralPath $Bin)) { return $null }
        $startArgs = @{
            FilePath         = $Bin
            WorkingDirectory = $SESSION
            PassThru         = $true
            WindowStyle      = $WindowStyle
            ErrorAction      = "SilentlyContinue"
        }
        if ($ArgList -and $ArgList.Count -gt 0) {
            $startArgs["ArgumentList"] = $ArgList
        }
        $p = Start-Process @startArgs
        if ($p) {
            Add-Content -LiteralPath $procList -Value "$($p.Id) $(Split-Path $Bin -Leaf)"
        }
        return $p
    }

    Write-Host "Event-EZ (button.ps1)"
    Write-Host "  session: $SESSION"
    Write-Host "  EZ_PKG_NAME=$pkgName"
    Write-Host "  EZ_PKG_DIR=$pkgDir"

    $layout = "pieces/chtpm/layouts/event_ez.chtpm"
    $null = Start-Logged -Bin (Get-SysBin $SESSION "chtpm_parser_pal") -ArgList @($layout)

    # wait for frame
    $frame = Join-Path $SESSION "pieces\display\current_frame.txt"
    for ($i = 0; $i -lt 40; $i++) {
        if ((Test-Path $frame) -and ((Get-Item $frame).Length -gt 0)) { break }
        Start-Sleep -Milliseconds 100
    }

    $null = Start-Logged -Bin (Get-SysBin $SESSION "chtpm_rgb_render")
    $gl = Get-SysBin $SESSION "gl_mirror"
    $pGl = $null
    if ($gl) {
        $pGl = Start-Logged -Bin $gl -WindowStyle "Normal"
        Write-Host "GL primary: gl_mirror pid=$($pGl.Id)"
    } else {
        Write-Host "WARN: no gl_mirror.exe in muta system"
    }

    $prisc = Get-SysBin $SESSION "prisc+x"
    if ($prisc -and (Test-Path (Join-Path $SESSION "pal\main_loop_chtpm.pal"))) {
        $null = Start-Logged -Bin $prisc -ArgList @("pal/main_loop_chtpm.pal")
    }

    if ($pGl) {
        Wait-Process -Id $pGl.Id -ErrorAction SilentlyContinue
    } else {
        Write-Host "No GL process - press Enter to cleanup"
        Read-Host | Out-Null
    }

    Invoke-Kill
    if (Test-Path $SESSION) {
        Remove-Item -LiteralPath $SESSION -Recurse -Force -ErrorAction SilentlyContinue
    }
    return 0
}

$a = $Action.ToLowerInvariant()
if ($a -in @("run", "r", "start", "run-widget", "widget")) {
    exit (Invoke-Run)
}
elseif ($a -in @("compile", "c", "build")) {
    exit (Invoke-Compile)
}
elseif ($a -in @("kill", "k", "stop")) {
    Invoke-Kill
    exit 0
}
elseif ($a -eq "check") {
    $ops = Join-Path $SCRIPT_DIR "ops\+x"
    foreach ($b in @("ez_compose_frame.exe", "ez_compose_frame.+x", "ez_menu_input.exe", "ez_menu_input.+x")) {
        $p = Join-Path $ops $b
        if (Test-Path $p) { Write-Host "OK $b" } else { Write-Host "MISSING $b" }
    }
    if ($MUTA) {
        foreach ($b in @("chtpm_parser_pal.exe", "gl_mirror.exe")) {
            $p = Join-Path $MUTA.FullName "system\$b"
            if (Test-Path $p) { Write-Host "OK muta $b" } else { Write-Host "MISSING muta $b" }
        }
    } else { Write-Host "MISSING muta tree" }
    exit 0
}
else {
    Write-Host @"
event-ez button.ps1 (Windows native - no bash)

  .\button.ps1 run | compile | check | kill

  Env (set by open_event_ez.ps1 / crypts menu):
    EZ_PKG_NAME=m6_golddeity
    EZ_PKG_DIR=<path>\event_pkg
"@
    exit 0
}
