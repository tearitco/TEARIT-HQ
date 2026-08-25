# button.ps1 - Windows launcher for board-viewer widget
# Parity with button.sh: run-widget <focused_project_root>
# ASCII only. Linux stays on button.sh.

param(
    [Parameter(Position = 0)]
    [string]$Action = "help",
    [Parameter(Position = 1)]
    [string]$FocusRoot = ""
)

$ErrorActionPreference = "Continue"
$SCRIPT_DIR = Split-Path -Parent $MyInvocation.MyCommand.Path
Set-Location -LiteralPath $SCRIPT_DIR

$MSYS = "C:\msys64\mingw64\bin"
if (Test-Path $MSYS) {
    if ($env:Path -notlike "*$MSYS*") { $env:Path = "$MSYS;$env:Path" }
}

# House = parent of &.widgits
$HOUSE_DIR = Split-Path (Split-Path $SCRIPT_DIR -Parent) -Parent

function Get-Bin([string]$rel) {
    $exe = Join-Path $SCRIPT_DIR ($rel + ".exe")
    if (Test-Path -LiteralPath $exe) { return $exe }
    $plain = Join-Path $SCRIPT_DIR $rel
    if (Test-Path -LiteralPath $plain) { return $plain }
    return $null
}

# UTF-8 NO BOM — C read_kv_str / fopen break on EF BB BF prefix
function Write-Utf8NoBom([string]$Path, [string]$Text) {
    $dir = Split-Path -Parent $Path
    if ($dir -and -not (Test-Path -LiteralPath $dir)) {
        New-Item -ItemType Directory -Path $dir -Force | Out-Null
    }
    $utf8 = New-Object System.Text.UTF8Encoding $false
    [System.IO.File]::WriteAllText($Path, $Text, $utf8)
}

# Prefer house-relative host path so MinGW ANSI fopen can open chunks
function Normalize-FocusRoot([string]$Focus) {
    if (-not $Focus) { return "" }
    $full = $Focus
    try { $full = (Resolve-Path -LiteralPath $Focus -EA Stop).Path } catch {}
    $houseFull = $HOUSE_DIR
    try { $houseFull = (Resolve-Path -LiteralPath $HOUSE_DIR -EA Stop).Path } catch {}
    if ($full.StartsWith($houseFull, [StringComparison]::OrdinalIgnoreCase)) {
        $rel = $full.Substring($houseFull.Length).TrimStart('\', '/')
        return ($rel -replace '\\', '/')
    }
    return ($full -replace '\\', '/')
}

# 8.3 short path — emoji WorkingDirectory breaks MinGW fopen / freeglut
function Get-ShortPath([string]$Path) {
    if (-not $Path -or -not (Test-Path -LiteralPath $Path)) { return $Path }
    try {
        $fso = New-Object -ComObject Scripting.FileSystemObject
        if (Test-Path -LiteralPath $Path -PathType Container) {
            return $fso.GetFolder($Path).ShortPath
        }
        return $fso.GetFile($Path).ShortPath
    } catch { return $Path }
}

# Stage PE under %TEMP% (no emoji, no & in path). Always for . +x.
function Invoke-HouseBin {
    param(
        [string]$Path,
        [string]$WorkDir,
        [string[]]$BinArgs = $null,
        [switch]$Wait,
        [switch]$Hidden
    )
    if (-not $Path -or -not (Test-Path -LiteralPath $Path)) {
        Write-Host "  SKIP missing bin: $Path" -ForegroundColor Yellow
        return $null
    }
    $wd = if ($WorkDir -and (Test-Path -LiteralPath $WorkDir)) { $WorkDir } else { $SCRIPT_DIR }
    $wdShort = Get-ShortPath $wd
    $isPlusX = $Path -match '\.\+x$'
    $winStyle = if ($Hidden) { "Hidden" } else { "Normal" }
    $hasArgs = ($BinArgs -and $BinArgs.Count -gt 0)

    # Always stage: emoji house + &.widgits break CreateProcess paths
    $tmpExe = Join-Path $env:TEMP ("housebin_{0}_{1}.exe" -f $PID, [Guid]::NewGuid().ToString("N").Substring(0, 8))
    Copy-Item -LiteralPath $Path -Destination $tmpExe -Force
    # freeglut next to staged gl_mirror
    if ($Path -match 'gl_mirror') {
        foreach ($dll in @("libfreeglut.dll", "freeglut.dll")) {
            $cand = Join-Path (Split-Path $Path -Parent) $dll
            if (Test-Path -LiteralPath $cand) {
                Copy-Item -LiteralPath $cand -Destination (Join-Path $env:TEMP $dll) -Force -EA SilentlyContinue
            }
        }
    }
    try {
        $sp = @{
            FilePath         = $tmpExe
            WorkingDirectory = $wdShort
            WindowStyle      = $winStyle
            PassThru         = $true
        }
        if ($hasArgs) { $sp.ArgumentList = $BinArgs }
        $proc = Start-Process @sp
        if ($Wait -and $proc) {
            Wait-Process -Id $proc.Id -ErrorAction SilentlyContinue
            return $null
        }
        return $proc
    } finally {
        if ($Wait) {
            Remove-Item -LiteralPath $tmpExe -Force -EA SilentlyContinue
        }
    }
}

function Invoke-Compile {
    $build = Join-Path $SCRIPT_DIR "scripts\build.ps1"
    if (Test-Path -LiteralPath $build) {
        & powershell -ExecutionPolicy Bypass -File $build
        return $LASTEXITCODE
    }
    Write-Host "WARN: no scripts\build.ps1 - trying inline copy from 014.wsr"
    $wsr = Get-ChildItem -LiteralPath $HOUSE_DIR -Directory -EA SilentlyContinue |
        Where-Object { $_.Name -like "014.wsr*" } | Select-Object -First 1
    if (-not $wsr) { Write-Error "no 014.wsr"; return 1 }
    New-Item -ItemType Directory -Path (Join-Path $SCRIPT_DIR "system") -Force | Out-Null
    New-Item -ItemType Directory -Path (Join-Path $SCRIPT_DIR "ops\+x") -Force | Out-Null
    foreach ($b in @("prisc+x","keyboard_input","renderer","chtpm_parser_pal","chtpm_rgb_render","gl_mirror")) {
        foreach ($ext in @(".exe","")) {
            $src = Join-Path $wsr.FullName "system\$b$ext"
            if (Test-Path -LiteralPath $src) {
                $dest = Join-Path $SCRIPT_DIR "system\$b.exe"
                if ($ext -eq "" -and -not (Test-Path (Join-Path $SCRIPT_DIR "system\$b.exe"))) {
                    Copy-Item -LiteralPath $src -Destination $dest -Force
                } elseif ($ext -eq ".exe") {
                    Copy-Item -LiteralPath $src -Destination $dest -Force
                }
                break
            }
        }
    }
    $glut = "C:\msys64\mingw64\bin\libfreeglut.dll"
    if (Test-Path $glut) { Copy-Item $glut (Join-Path $SCRIPT_DIR "system\libfreeglut.dll") -Force }
    # compile ops
    $CFLAGS = @("-Wall","-Wextra","-O2")
    Get-ChildItem (Join-Path $SCRIPT_DIR "ops\*.c") | ForEach-Object {
        $name = $_.BaseName
        $out = Join-Path $SCRIPT_DIR "ops\+x\$name.+x"
        Write-Host "gcc $($_.Name) -> $out"
        & gcc @CFLAGS $_.FullName -o $out -lm 2>$null
    }
    return 0
}

function Copy-TreeLink([string]$src, [string]$dst) {
    if (-not (Test-Path -LiteralPath $src)) { return }
    if (Test-Path -LiteralPath $dst) { return }
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

function Invoke-Kill {
    $names = @("keyboard_input","renderer","prisc+x","chtpm_parser_pal",
               "chtpm_rgb_render","gl_mirror","bv_compose_frame","bv_menu_input",
               "bv_render_3d","ledger_append","ledger_peers")
    foreach ($n in $names) {
        Get-Process -EA SilentlyContinue |
            Where-Object { $_.ProcessName -eq $n -or $_.ProcessName -like "$n*" } |
            ForEach-Object { try { Stop-Process -Id $_.Id -Force } catch {} }
        & taskkill /F /IM "$n.exe" 2>$null | Out-Null
    }
    Write-Host "done"
}

function Invoke-RunWidget {
    param([string]$Focus = "")

    Write-Host "=== board-viewer Windows widget ===" -ForegroundColor Cyan
    if ($Focus) { Write-Host "  focus: $Focus" }

    # Ensure bins
    $need = @("system\chtpm_parser_pal.exe","system\gl_mirror.exe","ops\+x\bv_compose_frame.+x")
    $missing = @($need | Where-Object { -not (Test-Path (Join-Path $SCRIPT_DIR $_)) })
    # also bare gl_mirror
    if (-not (Test-Path (Join-Path $SCRIPT_DIR "system\gl_mirror.exe")) -and
        -not (Test-Path (Join-Path $SCRIPT_DIR "system\gl_mirror"))) {
        $missing += "gl_mirror"
    }
    if ($missing.Count -gt 0) {
        Write-Host "Missing bins - compiling/copying..." -ForegroundColor Yellow
        $null = Invoke-Compile
    }

    $sid = "{0}-{1}" -f [int][double]::Parse((Get-Date -UFormat %s)), $PID
    $SESSION = Join-Path $SCRIPT_DIR "pieces\sessions\$sid"
    foreach ($d in @(
        "pieces\system","pieces\display","pieces\apps\player_app",
        "pieces\keyboard","projects\board-viewer\manager","projects\board-viewer\pieces"
    )) {
        New-Item -ItemType Directory -Path (Join-Path $SESSION $d) -Force | Out-Null
    }

    Copy-TreeLink (Join-Path $SCRIPT_DIR "system") (Join-Path $SESSION "system")
    Copy-TreeLink (Join-Path $SCRIPT_DIR "ops") (Join-Path $SESSION "ops")
    Copy-TreeLink (Join-Path $SCRIPT_DIR "pal") (Join-Path $SESSION "pal")
    Copy-TreeLink (Join-Path $SCRIPT_DIR "default_op.txt") (Join-Path $SESSION "default_op.txt")
    Copy-TreeLink (Join-Path $SCRIPT_DIR "pieces\chtpm") (Join-Path $SESSION "pieces\chtpm")
    Copy-TreeLink (Join-Path $SCRIPT_DIR "pieces\registry") (Join-Path $SESSION "pieces\registry")
    $bvPiece = Join-Path $SCRIPT_DIR "projects\board-viewer\pieces\board_viewer"
    if (Test-Path -LiteralPath $bvPiece) {
        Copy-TreeLink $bvPiece (Join-Path $SESSION "projects\board-viewer\pieces\board_viewer")
    }

    foreach ($f in @(
        "pieces\apps\player_app\interact_relay.txt",
        "pieces\keyboard\history.txt",
        "pieces\display\bv_screen_changed.txt",
        "pieces\display\frame_changed.txt",
        "pieces\display\renderer_pulse.txt"
    )) {
        New-Item -ItemType File -Path (Join-Path $SESSION $f) -Force | Out-Null
    }

    $houseAbs = $HOUSE_DIR
    try { $houseAbs = (Resolve-Path -LiteralPath $HOUSE_DIR).Path } catch {}
    Write-Utf8NoBom (Join-Path $SESSION "pieces\system\house_root.txt") ($houseAbs + "`n")

    $emoji = if ($env:BV_EMOJI_MODE) { $env:BV_EMOJI_MODE } else { "1" }
    $focusNorm = Normalize-FocusRoot $Focus
    $hostBase = if ($focusNorm) { Split-Path ($focusNorm -replace '/', '\') -Leaf } else { "" }
    if ($focusNorm) {
        Write-Host ("  focus id={0} root={1}" -f $hostBase, $focusNorm)
        $bvState = @"
focused_project_id=$hostBase
focused_project_root=$focusNorm
emoji_mode=$emoji
selector_x=-1
selector_y=-1
"@
        Write-Utf8NoBom (Join-Path $SESSION "pieces\system\bv_state.txt") $bvState
    } else {
        Write-Host "  WARN: no focused project - board will be empty (pass run-widget <host root>)" -ForegroundColor Yellow
        Write-Utf8NoBom (Join-Path $SESSION "pieces\system\bv_state.txt") ("emoji_mode=$emoji`n")
    }

    Write-Utf8NoBom (Join-Path $SESSION "pieces\apps\player_app\state.txt") @"
module_path=system/prisc+x pal/main_module.pal
project_id=board-viewer
active_target_id=board_viewer
"@

    # Relative root — required for emoji house paths
    $env:PRISC_PROJECT_ROOT = "."
    $env:PRISC_PROJECT_ID = "board-viewer"
    $env:RUN_PROFILE = "widget"
    $env:SKIP_ORCH_COMPILE = "1"

    # freeglut next to gl
    foreach ($glut in @("C:\msys64\mingw64\bin\libfreeglut.dll","C:\msys64\mingw64\bin\freeglut.dll")) {
        if (Test-Path $glut) {
            $name = Split-Path $glut -Leaf
            $dest = Join-Path $SCRIPT_DIR "system\$name"
            if (-not (Test-Path $dest)) { Copy-Item $glut $dest -Force -EA SilentlyContinue }
            break
        }
    }

    # optional ledger_append ONLINE (.+x must go through Invoke-HouseBin)
    $ledgerId = if ($Focus) { "board-viewer:$hostBase" } else { "board-viewer" }
    $ledger = Get-Bin "ops\+x\ledger_append.+x"
    if (-not $ledger) {
        $cand = Join-Path $SCRIPT_DIR "ops\+x\ledger_append.+x"
        if (Test-Path -LiteralPath $cand) { $ledger = $cand }
    }
    if ($ledger) {
        $null = Invoke-HouseBin -Path $ledger -WorkDir $SESSION -Hidden -Wait `
            -BinArgs @("ONLINE","widget",$ledgerId,$SESSION,"$PID","Board Viewer","pieces/system/bv_state.txt")
    }

    # seed compose + 3d overlay so first GL frame is not empty/ASCII-only.
    # Order matches pal/main_module.pal: compose → render_3d → compose again
    # so MAP3D_MARKER + rgb_frame_3d_overlay.raw are both ready before RGB/GL.
    $compose = Get-Bin "ops\+x\bv_compose_frame.+x"
    if (-not $compose) {
        $cand = Join-Path $SCRIPT_DIR "ops\+x\bv_compose_frame.+x"
        if (Test-Path -LiteralPath $cand) { $compose = $cand }
    }
    $render3d = Get-Bin "ops\+x\bv_render_3d.+x"
    if (-not $render3d) {
        $cand = Join-Path $SCRIPT_DIR "ops\+x\bv_render_3d.+x"
        if (Test-Path -LiteralPath $cand) { $render3d = $cand }
    }
    if ($compose) {
        Write-Host "  seeding bv_compose_frame..."
        $null = Invoke-HouseBin -Path $compose -WorkDir $SESSION -Hidden -Wait
        if ($render3d) {
            Write-Host "  seeding bv_render_3d..."
            $null = Invoke-HouseBin -Path $render3d -WorkDir $SESSION -Hidden -Wait
            Write-Host "  re-compose after 3D overlay..."
            $null = Invoke-HouseBin -Path $compose -WorkDir $SESSION -Hidden -Wait
        }
        $vpath = Join-Path $SESSION "pieces\apps\player_app\view.txt"
        if (Test-Path -LiteralPath $vpath) {
            Write-Host ("  view.txt ready ({0} bytes)" -f (Get-Item -LiteralPath $vpath).Length)
        }
        $ov = Join-Path $SESSION "pieces\display\rgb_frame_3d_overlay.raw"
        if (Test-Path -LiteralPath $ov) {
            Write-Host ("  3d overlay ready ({0} bytes)" -f (Get-Item -LiteralPath $ov).Length)
        }
    } else {
        Write-Host "  WARN: no bv_compose_frame.+x" -ForegroundColor Yellow
    }

    $parser = Get-Bin "system\chtpm_parser_pal"
    if (-not $parser) {
        foreach ($c in @((Join-Path $SCRIPT_DIR "system\chtpm_parser_pal.exe"),
                         (Join-Path $SESSION "system\chtpm_parser_pal.exe"))) {
            if (Test-Path -LiteralPath $c) { $parser = $c; break }
        }
    }
    $rend = Get-Bin "system\renderer"
    $rgb = Get-Bin "system\chtpm_rgb_render"
    $gl = Get-Bin "system\gl_mirror"
    # Prefer real project system/ (junction may confuse Start-Process on some hosts)
    if (-not $rend) { $rend = Join-Path $SCRIPT_DIR "system\renderer.exe" }
    if (-not $rgb) { $rgb = Join-Path $SCRIPT_DIR "system\chtpm_rgb_render.exe" }
    if (-not $gl) { $gl = Join-Path $SCRIPT_DIR "system\gl_mirror.exe" }
    if (-not $parser) { $parser = Join-Path $SCRIPT_DIR "system\chtpm_parser_pal.exe" }

    if (-not $parser -or -not (Test-Path -LiteralPath $parser)) {
        Write-Error "MISSING chtpm_parser_pal.exe - run: .\button.ps1 compile"
        return 1
    }

    $pRend = $null
    if ($rend -and (Test-Path -LiteralPath $rend)) {
        $pRend = Invoke-HouseBin -Path $rend -WorkDir $SESSION -Hidden
    }
    $pParser = Invoke-HouseBin -Path $parser -WorkDir $SESSION -Hidden `
        -BinArgs @("pieces/chtpm/layouts/board_viewer.chtpm")
    if ($pParser) { Write-Host ("parser pid={0}" -f $pParser.Id) }
    else { Write-Host "FAIL: parser did not start" -ForegroundColor Red; return 1 }

    $frame = Join-Path $SESSION "pieces\display\current_frame.txt"
    for ($i = 0; $i -lt 50; $i++) {
        if ((Test-Path -LiteralPath $frame) -and ((Get-Item -LiteralPath $frame).Length -gt 0)) {
            $flen = (Get-Item -LiteralPath $frame).Length
            Write-Host ("  frame ready ({0} bytes)" -f $flen)
            break
        }
        Start-Sleep -Milliseconds 100
    }

    $pRgb = $null
    $pGl = $null
    if ($env:NO_GL -ne "1") {
        if ($rgb -and (Test-Path -LiteralPath $rgb)) {
            $pRgb = Invoke-HouseBin -Path $rgb -WorkDir $SESSION -Hidden
            if ($pRgb) { Write-Host ("RGB pid={0}" -f $pRgb.Id) }
        }
        $raw = Join-Path $SESSION "pieces\display\rgb_frame.raw"
        for ($i = 0; $i -lt 50; $i++) {
            if ((Test-Path -LiteralPath $raw) -and ((Get-Item -LiteralPath $raw).Length -gt 10000)) {
                $rlen = (Get-Item -LiteralPath $raw).Length
                Write-Host ("  rgb_frame.raw ready ({0} bytes)" -f $rlen)
                break
            }
            Start-Sleep -Milliseconds 100
        }
        if ($gl -and (Test-Path -LiteralPath $gl)) {
            $pGl = Invoke-HouseBin -Path $gl -WorkDir $SESSION
            Start-Sleep -Milliseconds 500
            if ($pGl -and $pGl.HasExited) {
                Write-Host ("FAIL: gl_mirror exited immediately (code={0})" -f $pGl.ExitCode) -ForegroundColor Red
            } elseif ($pGl) {
                Write-Host ("GL gl_mirror pid={0} (title often wsr-pal RGB mirror)" -f $pGl.Id)
            }
        } else {
            Write-Host "WARN: no gl_mirror.exe - run .\button.ps1 compile" -ForegroundColor Yellow
        }
    }

    Write-Host "Board-viewer running session=$sid (close GL or kill to stop)"
    # Widget profile: wait for parser (GL owns input)
    try {
        if ($pParser -and -not $pParser.HasExited) {
            Wait-Process -Id $pParser.Id -ErrorAction SilentlyContinue
        }
    } finally {
        foreach ($p in @($pGl,$pRgb,$pRend,$pParser)) {
            if ($p -and -not $p.HasExited) {
                try { Stop-Process -Id $p.Id -Force -EA SilentlyContinue } catch {}
            }
        }
        if (Test-Path -LiteralPath $SESSION) {
            Remove-Item -LiteralPath $SESSION -Recurse -Force -EA SilentlyContinue
        }
    }
    return 0
}

$a = $Action.ToLowerInvariant()
if ($a -in @("compile","c","build")) { exit (Invoke-Compile) }
elseif ($a -in @("run-widget","widget","w")) { exit (Invoke-RunWidget -Focus $FocusRoot) }
elseif ($a -in @("run","r","start")) { exit (Invoke-RunWidget -Focus $FocusRoot) }
elseif ($a -in @("run-app","app","a")) {
    $env:NO_GL = "1"
    exit (Invoke-RunWidget -Focus $FocusRoot)
}
elseif ($a -in @("kill","k","stop")) { Invoke-Kill; exit 0 }
elseif ($a -in @("check","verify")) {
    foreach ($b in @("system\chtpm_parser_pal.exe","system\gl_mirror.exe","ops\+x\bv_compose_frame.+x","ops\+x\bv_menu_input.+x")) {
        if (Test-Path (Join-Path $SCRIPT_DIR $b)) { Write-Host "OK   $b" }
        else { Write-Host "MISSING $b" }
    }
    exit 0
}
else {
    Write-Host @"
board-viewer button.ps1 (Windows)

  .\button.ps1 compile
  .\button.ps1 run-widget <focused_project_root>
  .\button.ps1 kill | check

Host apps call: OPEN_BOARD_WIDGET / CONFIRM_START -> this run-widget.
"@
    exit 0
}
