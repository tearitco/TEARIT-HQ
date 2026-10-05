# win_toy_button.ps1 - the Windows half of every toy project's button.sh.
# ASCII only.
#
# WHY THIS FILE EXISTS (direct report, toys dropdown opens nothing):
# livedesk_build_toys_menu() in khtpm_taskbar_manager.c builds every toys
# row as `livedesk:open-toy:<toy_dir>/button.sh`, and ktb_hq_activate()
# on _WIN32 rewrites that to button.ps1 and runs
# `powershell -File <toy_dir>\button.ps1 run`. Only four toy projects
# actually shipped a button.ps1, so every other row ran a non-existent
# file and silently did nothing - the row highlighted, the HQ closed,
# and no window ever appeared. That is the "Toys list ok, launch not"
# line in WIN-CONVERSION-STATUS.md.
#
# The canonical toy button.sh is nearly the same script fifteen times
# over. Every one of them does exactly four things:
#   1. derive HOUSE_ROOT as two levels up from the toy dir
#   2. locate the shared renderer at
#      $HOUSE_ROOT/_.monads/_.livedesk-taskbar/ops/+x/khtpm_core_render.+x
#   3. build-on-demand, then kill any existing instance of ITS OWN xhtpm
#   4. `setsid nohup "$BIN" "$HOUSE_ROOT" "$XHTPM" &`
# None of that differs per toy except the xhtpm name, and the xhtpm is
# discoverable by globbing the toy dir. So the per-toy button.ps1 files
# are three-line forwarders and the logic lives here once.
#
# The per-toy <module> managers are NOT launched from this script. That
# is deliberate and it is the Linux design: the renderer itself forks
# every <module src="..."> in the template, so closing the window stops
# the manager too. On Windows that needed khtpm_win_spawn_module() in
# khtpm_strip_posix_win.c, because fork()/execv() do not exist here.
#
# Usage (this is the shape ktb_hq_activate() uses):
#   powershell -NoProfile -ExecutionPolicy Bypass \
#     -File win_toy_button.ps1 <toy_dir> run
#
#   run|r|start   launch the toy window (default)
#   kill|k|stop   kill this toy's renderer instance
#   check|verify  report what would be launched, change nothing

$ErrorActionPreference = "Continue"

if (-not $args.Count) {
    Write-Error "usage: win_toy_button.ps1 <toy_dir> [run|kill|check]"
    exit 1
}
$ToyDir  = $args[0]
$Action  = if ($args.Count -gt 1) { $args[1] } else { "run" }

try { $ToyDir = (Resolve-Path -LiteralPath $ToyDir).Path } catch {
    Write-Error "toy dir not found: $ToyDir"
    exit 1
}

# --- house discovery, mirroring khtpm_vars.sh's khtpm_find_house() and
# run_khtpm_strip_win.ps1's Find-House(): walk UP until a dir holds both
# #.desktop/ and &.widgits/. Never count levels.
function Find-House([string]$start) {
    $d = $start
    while ($d -and ($d -ne "/")) {
        if ((Test-Path -LiteralPath (Join-Path $d "#.desktop")) -and
            (Test-Path -LiteralPath (Join-Path $d "&.widgits"))) { return $d }
        $d = Split-Path -Parent $d
    }
    return $null
}
$HOUSE = Find-House $ToyDir
if (-not $HOUSE) {
    Write-Error "house root not found above $ToyDir (need both #.desktop/ and &.widgits/)"
    exit 1
}

$RENDER_OPS = Join-Path $HOUSE "_.monads\_.livedesk-taskbar\ops"
$BIN        = Join-Path $RENDER_OPS "+x\khtpm_core_render.exe"

# --- pick this toy's template. Order matters and mirrors what each real
# button.sh names explicitly: the dir-name match first, then the -pal
# form (csv-hq, pdl-read, text-edit-hq, music-player-hq, file-explorer
# all use <name>-pal.xhtpm), then whatever single xhtpm is there.
function Find-Template([string]$dir) {
    $name = Split-Path $dir -Leaf
    foreach ($cand in @("$name.xhtpm", "$name-pal.xhtpm")) {
        $p = Join-Path $dir $cand
        if (Test-Path -LiteralPath $p) { return $p }
    }
    $rest = @(Get-ChildItem -LiteralPath $dir -Filter "*.xhtpm" -ErrorAction SilentlyContinue |
              Sort-Object Name)
    if ($rest.Count -eq 1) { return $rest[0].FullName }
    if ($rest.Count -gt 1) {
        # piececraft-hq is the only toy with two templates and it ships its
        # own real button.ps1, so it never reaches here. Pick the one that
        # is not a variant suffix rather than guessing silently.
        $plain = @($rest | Where-Object { $_.BaseName -notmatch '\.' })
        if ($plain.Count -eq 1) { return $plain[0].FullName }
        Write-Warning ("$name has $($rest.Count) templates: " +
                       (($rest | ForEach-Object Name) -join ", ") + " - using $($rest[0].Name)")
        return $rest[0].FullName
    }
    return $null
}
$XHTPM = Find-Template $ToyDir

function Get-ToyRenderers([string]$leaf) {
    if (-not $leaf) { return @() }
    $out = @()
    foreach ($p in @(Get-CimInstance Win32_Process -Filter "Name='khtpm_core_render.exe'" `
                              -ErrorAction SilentlyContinue)) {
        if ($p.CommandLine -and $p.CommandLine.Contains($leaf)) { $out += $p }
    }
    return $out
}
$leaf = if ($XHTPM) { Split-Path $XHTPM -Leaf } else { $null }

switch -Regex ($Action.ToLowerInvariant()) {

    "^(kill|k|stop)$" {
        $mine = @(Get-ToyRenderers $leaf)
        if ($mine.Count -eq 0) { Write-Host "no running instance"; exit 0 }
        foreach ($p in $mine) {
            Stop-Process -Id ([int]$p.ProcessId) -Force -ErrorAction SilentlyContinue
        }
        Write-Host ("killed PID(s): " + (($mine | ForEach-Object ProcessId) -join " "))
        exit 0
    }

    "^(check|verify)$" {
        Write-Host ("toy    : " + $ToyDir)
        Write-Host ("house  : " + $HOUSE)
        Write-Host ("xhtpm  : " + $(if ($XHTPM) { Split-Path $XHTPM -Leaf } else { "(none)" }))
        Write-Host ("binary : " + $(if (Test-Path -LiteralPath $BIN) { $BIN } else { "MISSING $BIN" }))
        $mine = @(Get-ToyRenderers $leaf)
        Write-Host ("running: " + $(if ($mine.Count) { ($mine | ForEach-Object ProcessId) -join " " } else { "no" }))
        exit 0
    }

    default {
        # "run", "r", "start" and anything else, matching button.sh, which
        # treats a non-"run" ACTION as a no-op exit rather than an error.
        if (-not (Test-Path -LiteralPath $BIN)) {
            Write-Error "missing renderer: $BIN (build it: build_khtpm_strip_win.ps1)"
            exit 1
        }
        if (-not $XHTPM) {
            Write-Error "$(Split-Path $ToyDir -Leaf): no *.xhtpm template in $ToyDir"
            exit 1
        }

        # Same single-instance guard every real toy button.sh has: kill the
        # previous window for THIS template only, so two toys can be open at
        # once but clicking the same toy twice does not stack windows.
        $mine = @(Get-ToyRenderers $leaf)
        if ($mine.Count -gt 0) {
            foreach ($p in $mine) {
                Stop-Process -Id ([int]$p.ProcessId) -Force -ErrorAction SilentlyContinue
            }
            Start-Sleep -Milliseconds 600
        }

        # Detached via WMI, exactly like run_khtpm_strip_win.ps1: the
        # renderer must outlive this powershell, which the taskbar manager
        # spawned from a CreateProcessW it does not wait on. Start-Process
        # would leave the child in a job object tied to that manager.
        $cmd = '"{0}" "{1}" "{2}"' -f $BIN, $HOUSE, $XHTPM
        try {
            $r = Invoke-CimMethod -ClassName Win32_Process -MethodName Create -Arguments @{
                CommandLine      = $cmd
                CurrentDirectory = $HOUSE
            }
        } catch {
            Write-Error "Win32_Process.Create failed: $($_.Exception.Message)"
            exit 1
        }
        if ($r.ReturnValue -ne 0) {
            Write-Error "Win32_Process.Create returned $($r.ReturnValue) for $cmd"
            exit 1
        }
        # Never trust a bare exit code for a backgrounded GUI launch.
        Start-Sleep -Milliseconds 800
        $alive = @(Get-ToyRenderers $leaf)
        if ($alive.Count -eq 0) {
            Write-Error "$(Split-Path $ToyDir -Leaf): renderer did not stay alive after launch (pid $($r.ProcessId))"
            exit 1
        }
        Write-Host ("{0} launched (PID {1}, template {2})" -f `
            (Split-Path $ToyDir -Leaf), $r.ProcessId, (Split-Path $XHTPM -Leaf))
        exit 0
    }
}