# button.ps1 - Windows launcher for START_BUTTON (house loader)
# Windows twin of button.sh. Keep action names in lockstep with button.sh.
#
# If you get execution policy errors, run:
#   powershell -ExecutionPolicy Bypass -File .\button.ps1 <action>
#
# Linux stays on button.sh.

param(
    [Parameter(Position = 0)]
    [string]$Action = "help"
)

$ErrorActionPreference = "Continue"
$SCRIPT_DIR = Split-Path -Parent $MyInvocation.MyCommand.Path

$MSYS_BIN = "C:\msys64\mingw64\bin"
if (Test-Path -LiteralPath $MSYS_BIN) {
    if ($env:Path -notlike "*$MSYS_BIN*") { $env:Path = "$MSYS_BIN;$env:Path" }
}

# MinGW emits foo.exe, but the committed tree also carries LINUX ELF
# binaries under the same extensionless names (system/keyboard_input etc
# are checked-in ELF, 20K-ish; the Windows build lands beside them as
# .exe, ~280K). So on Windows we MUST probe .exe FIRST - probing the bare
# name returns the ELF and every exec then fails.
#
# The ops binaries are the awkward case: gcc gives them NO .exe suffix at
# all (start_scan.+x is already a valid PE filename), and PowerShell's &
# operator refuses to execute an extensionless PE. Those are launched
# through Start-Process, which accepts any PE filename.
function Resolve-Bin([string]$rel) {
    $p = Join-Path $SCRIPT_DIR ($rel -replace '/', '\')
    if (Test-Path -LiteralPath "$p.exe") { return "$p.exe" }
    if (Test-Path -LiteralPath $p) { return $p }
    return $null
}

# Launch a PE that may lack a .exe suffix (the ops/+x ones). Start-Process
# rather than cmd /c: cmd treats '&' as a command separator, and this house
# is full of '&.widgits' / '@.apps' paths that would split mid-argument.
function Invoke-Bin([string]$bin, [string]$workingDir, [string[]]$opArgs) {
    $a = @()
    foreach ($x in $opArgs) { $a += "`"$x`"" }
    $p = Start-Process -FilePath $bin -ArgumentList $a -WorkingDirectory $workingDir `
        -NoNewWindow -PassThru -Wait
    return $p.ExitCode
}

function Invoke-Build {
    & powershell -ExecutionPolicy Bypass -File (Join-Path $SCRIPT_DIR "scripts\build.ps1")
    return $LASTEXITCODE
}

function Invoke-Kill {
    # Do NOT read Process.Path — enumerating Path for every process on
    # Windows can hang (access denied / WMI). Match by ProcessName, same
    # as 014.wsr-pal's button.ps1 Invoke-Kill.
    $names = @("keyboard_input", "renderer", "prisc+x", "chtpm_parser_pal")
    foreach ($n in $names) {
        Get-Process -ErrorAction SilentlyContinue |
            Where-Object { $_.ProcessName -eq $n -or $_.ProcessName -like "$n*" } |
            ForEach-Object {
                try { Stop-Process -Id $_.Id -Force -ErrorAction Stop } catch { }
            }
    }
    foreach ($n in $names) {
        & taskkill /F /IM "$n.exe" 2>$null | Out-Null
    }
    Write-Host "done"
}

# Kill only prisc+x processes whose command line points INSIDE this
# session dir. button.sh does the same via /proc/$pid/cwd; Windows has no
# /proc, so match the full command line instead. Scoping matters: a
# concurrent session in another worktree/session dir must survive.
function Stop-SessionModule([string]$sessionDir) {
    $marker = $sessionDir.TrimEnd('\')
    try {
        $procs = Get-CimInstance Win32_Process -Filter "Name='prisc+x.exe'" -ErrorAction SilentlyContinue
    } catch { $procs = $null }
    if (-not $procs) { return }
    foreach ($p in $procs) {
        if ($p.CommandLine -and $p.CommandLine -like "*$marker*") {
            try { Stop-Process -Id $p.ProcessId -Force -ErrorAction Stop } catch { }
        }
    }
}

function Invoke-Run {
    Write-Host "=== START_BUTTON: auto-compile (dev) ==="
    $rc = Invoke-Build
    if ($rc -ne 0) {
        Write-Host "START_BUTTON: compile failed - not launching." -ForegroundColor Red
        exit 1
    }

    # Durable frame audit log, cleared on every new top-level run.
    New-Item -ItemType Directory -Force -Path (Join-Path $SCRIPT_DIR "debug") | Out-Null
    Set-Content -Path (Join-Path $SCRIPT_DIR "debug\frame_history.txt") -Value "" -NoNewline
    $env:PRISC_FRAME_HISTORY = Join-Path $SCRIPT_DIR "debug\frame_history.txt"
    New-Item -ItemType Directory -Force -Path (Join-Path $SCRIPT_DIR "pieces\system") | Out-Null

    while ($true) {
        if (-not (Invoke-OneSession)) { break }
        $handoff = Join-Path $SCRIPT_DIR "pieces\system\last_handoff.txt"
        $target = $null
        if (Test-Path -LiteralPath $handoff) {
            $target = (Get-Content -LiteralPath $handoff -TotalCount 1)
            Remove-Item -LiteralPath $handoff -Force -ErrorAction SilentlyContinue
        }
        if ($target) {
            $target = $target.Trim()
            $childPs1 = Join-Path $target "button.ps1"
            $childSh = Join-Path $target "button.sh"
            if ((Test-Path -LiteralPath $childPs1) -or (Test-Path -LiteralPath $childSh)) {
                Write-Host ""
                Write-Host "=== START_BUTTON: launching $target (same terminal) ===" -ForegroundColor Green
                Write-Host ""
                if (Test-Path -LiteralPath $childPs1) {
                    & powershell -ExecutionPolicy Bypass -File $childPs1 run
                } else {
                    & bash (Join-Path $target "button.sh") run
                }
                Write-Host ""
                Write-Host "=== START_BUTTON: returned from $target - reopening loader ===" -ForegroundColor Green
                Write-Host ""
                continue
            }
        }
        break
    }
}

# One loader session. $true  = handoff happened, caller should launch target
#                  $false = user quit, caller should stop looping
function Invoke-OneSession {
    $sid = ([int][double]::Parse((Get-Date -UFormat %s))) + "-" + $PID
    $sessionDir = Join-Path $SCRIPT_DIR "pieces\sessions\$sid"

    foreach ($d in @(
        "pieces\system", "pieces\display", "pieces\apps\player_app",
        "pieces\keyboard", "projects\start-button\manager",
        "projects\start-button\pieces"
    )) {
        New-Item -ItemType Directory -Force -Path (Join-Path $sessionDir $d) | Out-Null
    }

    # No symlinks (Windows). C processes resolve shared/persistent files
    # via PRISC_PROJECT_ROOT, same contract as button.sh.
    foreach ($s in @("home", "system", "widgets", "apps", "store")) {
        $d = Join-Path $sessionDir "projects\start-button\pieces\$s"
        New-Item -ItemType Directory -Force -Path $d | Out-Null
        $src = Join-Path $SCRIPT_DIR "projects\start-button\pieces\$s\piece.pdl"
        if (Test-Path -LiteralPath $src) {
            Copy-Item -LiteralPath $src -Destination (Join-Path $d "piece.pdl") -Force
        }
    }

    $sp = Join-Path $sessionDir "pieces\system"
    $da = Join-Path $sessionDir "pieces\apps\player_app"

    Set-Content -Path (Join-Path $da "interact_relay.txt") -Value "" -NoNewline
    Set-Content -Path (Join-Path $sessionDir "pieces\keyboard\history.txt") -Value "" -NoNewline
    Set-Content -Path (Join-Path $sessionDir "pieces\display\start_screen_changed.txt") -Value "" -NoNewline
    Set-Content -Path (Join-Path $sp "start_state.txt") `
        -Value "last_message=Pick a category (System / Widgets / Apps / App Store)." -NoNewline
    foreach ($f in @("handoff_launch.txt", "quit_request.txt", "quit_flag.txt")) {
        Remove-Item -LiteralPath (Join-Path $sp $f) -Force -ErrorAction SilentlyContinue
    }
    $state = @(
        "module_path=system/prisc+x pal/main_loop_chtpm.pal",
        "project_id=start-button",
        "active_target_id=home"
    ) -join "`n"
    Set-Content -Path (Join-Path $da "state.txt") -Value $state -NoNewline
    Set-Content -Path (Join-Path $sessionDir "pieces\display\current_layout.txt") `
        -Value "pieces/chtpm/layouts/home.chtpm" -NoNewline

    $env:PRISC_PROJECT_ROOT = $SCRIPT_DIR
    $env:PRISC_INSTALL_ROOT = $SCRIPT_DIR
    $env:PRISC_PROJECT_ID = "start-button"

    Push-Location -LiteralPath $sessionDir
    $renderer = $null; $chtpm = $null
    try {
        $scan = Resolve-Bin "ops/+x/start_scan.+x"
        if ($scan) { $null = Invoke-Bin $scan $sessionDir @("all") }
        $compose = Resolve-Bin "ops/+x/start_compose_frame.+x"
        if ($compose) { $null = Invoke-Bin $compose $sessionDir @() }

        $rendererBin = Resolve-Bin "system/renderer"
        $chtpmBin = Resolve-Bin "system/chtpm_parser_pal"
        if ($rendererBin) {
            $renderer = Start-Process -FilePath $rendererBin -WorkingDirectory $sessionDir -PassThru -NoNewWindow
        }
        if ($chtpmBin) {
            $chtpm = Start-Process -FilePath $chtpmBin `
                -ArgumentList "pieces/chtpm/layouts/home.chtpm" `
                -WorkingDirectory $sessionDir -PassThru -NoNewWindow
        }

        Set-Content -Path (Join-Path $da "history.txt") -Value "" -NoNewline
        $kbd = Resolve-Bin "system/keyboard_input"
        if ($kbd) { & $kbd } else { Write-Host "keyboard_input missing - run compile" -ForegroundColor Red; return $false }
    }
    finally {
        Pop-Location
    }

    $handoff = ""
    $hl = Join-Path $sp "handoff_launch.txt"
    if ((Test-Path -LiteralPath $hl) -and (Get-Item -LiteralPath $hl).Length -gt 0) {
        $handoff = (Get-Content -LiteralPath $hl -TotalCount 1)
    }

    foreach ($p in @($renderer, $chtpm)) {
        if ($p -and -not $p.HasExited) { try { Stop-Process -Id $p.Id -Force -ErrorAction Stop } catch { } }
    }
    Stop-SessionModule $sessionDir

    New-Item -ItemType Directory -Force -Path (Join-Path $SCRIPT_DIR "pieces\system") | Out-Null
    $lastHandoff = Join-Path $SCRIPT_DIR "pieces\system\last_handoff.txt"
    if ($handoff) {
        Set-Content -Path $lastHandoff -Value $handoff.Trim()
    } else {
        Remove-Item -LiteralPath $lastHandoff -Force -ErrorAction SilentlyContinue
    }

    Remove-Item -LiteralPath $sessionDir -Recurse -Force -ErrorAction SilentlyContinue
    return [bool]$handoff
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
elseif ($act -in @("check", "verify")) {
    $bins = @(
        "system/prisc+x", "system/keyboard_input", "system/renderer",
        "system/chtpm_parser_pal", "ops/+x/start_scan.+x",
        "ops/+x/start_compose_frame.+x", "ops/+x/start_menu_input.+x"
    )
    $missing = 0
    foreach ($b in $bins) {
        if (Resolve-Bin $b) { Write-Host "OK   $b" } else { Write-Host "MISSING $b"; $missing++ }
    }
    exit ([int]($missing -gt 0))
}
elseif ($act -in @("help", "h", "-h", "--help")) {
    Write-Host "START_BUTTON / HOUSE LOADER (Windows)" -ForegroundColor Cyan
    Write-Host ""
    Write-Host "Usage: .\button.ps1 <action>"
    Write-Host "  compile, c, build   - Build 7 binaries (MSYS2 MinGW64 gcc)"
    Write-Host "  run, r, start       - auto-compile, then category pre-screen"
    Write-Host "  kill, k, stop       - kill lingering loader processes"
    Write-Host "  check, verify       - verify all 7 binaries exist"
    Write-Host "  help, h             - this text"
    Write-Host ""
    Write-Host "Selected program runs in THIS terminal; exits back to loader."
    Write-Host "Ctrl+C quits a session (not 'q' - see keyboard_input.c header)."
    Write-Host ""
    Write-Host "Linux: use ./button.sh (unchanged)."
    Write-Host "Native Windows console only - arrows need a real conhost, not mintty."
}
else {
    Write-Error "Unknown: $Action"
    exit 1
}
