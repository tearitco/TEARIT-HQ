# run_khtpm_strip_win.ps1 - Windows twin of run_khtpm_strip.sh.
# ASCII only.
#
# WHY THIS FILE EXISTS (direct report, 2026-09-26): the bars had to be
# launched from an open CLI, and died when that CLI was closed. On Linux
# run_khtpm_strip.sh hides this with `setsid env ... & < /dev/null >>log`.
# PowerShell's Start-Process does NOT give that guarantee, because the
# child stays inside the launching shell's job object - when the parent
# goes away the whole job is torn down with it. So this script does not
# use Start-Process at all.
#
# THE ACTUAL FIX: launch through WMI (Win32_Process.Create). The new
# process's parent is the WMI service host (svchost), not this shell, so
# the child inherits no job object, no console and no parent at all.
# That is the Windows equivalent of setsid, and it is the only one of the
# three options that is bulletproof from a job-object parent:
#
#   Start-Process            NO  - stays in the parent's job object
#   CreateProcess+DETACHED   partial - DETACHED_PROCESS detaches the
#                                   console but a job object with
#                                   KILL_ON_JOB_CLOSE still reaps it
#   Win32_Process.Create     YES - created by the WMI service, fully
#                                   independent of who asked
#
# The renderer self-logs to #.desktop/khtpm_strip_parser.log, so unlike
# the Linux script this one does not need to redirect stdio - which is
# fortunate, because Win32_Process.Create exposes no way to set a child's
# std handles. Nothing is lost by that: the bars never printed to the
# terminal to begin with.
#
# Same contract as the Linux runner - boot|new|test|run, stop, status,
# build, help - and the same rule it exists to enforce: never trust a
# bare exit code for a backgrounded GUI launch, always confirm real PIDs.
#
# DIVERGENCE FROM LINUX, deliberate, documented not accidental:
# run_khtpm_strip.sh launches ONLY khtpm_strip_header.xhtpm. This one
# launches the header AND khtpm_strip_bottom.xhtpm, because the bottom
# bar is a real separate top-level window on this platform and nobody
# else starts it. If Linux later grows a bottom launcher, drop the second
# Spawn-Block here to match.
#
# See ..\..\..\#.#.calendar-dox\!.HQ-IQ-BOOK\09-appendix\
# WINDOWS-TASKBAR-PORT.md for the port notes this file belongs to.

$ErrorActionPreference = "Continue"

$SCRIPT_DIR = Split-Path -Parent $MyInvocation.MyCommand.Path

# ---- house discovery, mirroring khtpm_vars.sh's khtpm_find_house() ----
# Walks UP until it finds a dir holding both #.desktop/ and &.widgits/,
# so this works no matter which script or working dir invokes it.
function Find-House([string]$start) {
    $d = $start
    while ($d -and ($d -ne "/")) {
        if ((Test-Path -LiteralPath (Join-Path $d "#.desktop")) -and
            (Test-Path -LiteralPath (Join-Path $d "&.widgits"))) { return $d }
        $d = Split-Path -Parent $d
    }
    return $null
}

$HOUSE = Find-House $SCRIPT_DIR
if (-not $HOUSE) {
    Write-Error "house root not found (no dir above $SCRIPT_DIR has both #.desktop/ and &.widgits/)"
    exit 1
}

# Same three paths khtpm_vars.sh declares, so the shell runners and the
# C binaries still agree without editing C.
$STATE   = Join-Path $HOUSE "#.desktop"
$LOG     = Join-Path $STATE "khtpm_strip_parser.log"
$PIDFILE = Join-Path $STATE "livedesk_taskbar.pid"
$RENDER  = Join-Path $SCRIPT_DIR "+x\khtpm_core_render.exe"
$MANAGER = Join-Path $SCRIPT_DIR "+x\khtpm_taskbar_manager_main.exe"
$ENTITY  = Join-Path $SCRIPT_DIR "+x\khtpm_entity.exe"
$TASKBAR = Split-Path -Parent $SCRIPT_DIR
$HEADER  = Join-Path $TASKBAR "khtpm_strip_header.xhtpm"
$BOTTOM  = Join-Path $TASKBAR "khtpm_strip_bottom.xhtpm"

$ACTION = if ($args.Count -ge 1) { $args[0] } else { "help" }

# ---- process helpers -------------------------------------------------
# Liveness by pid, not by name: cheap and exact.
function Test-Alive([int]$pid_) {
    if ($pid_ -le 0) { return $false }
    return [bool](Get-Process -Id $pid_ -ErrorAction SilentlyContinue)
}

# Read the pid file into an array. One pid per line.
function Get-KhtpmPids {
    if (-not (Test-Path -LiteralPath $PIDFILE)) { return @() }
    $raw = Get-Content -LiteralPath $PIDFILE -ErrorAction SilentlyContinue
    $out = @()
    foreach ($l in $raw) {
        $t = ($l -replace "[^0-9]", "")
        if ($t -and (Test-Alive ([int]$t))) { $out += [int]$t }
    }
    return $out
}

# Launch one process, fully detached, via WMI. Returns the new pid, or 0.
# This is the whole point of the file - see the header comment.
# $rest is the argument list AFTER the exe, because the three binaries
# genuinely disagree about their invocation shape and gluing one fixed
# shape onto all of them is how you get a silent no-op:
#   khtpm_core_render.exe  <house_root> <template.xhtpm> [x] [y]
#   khtpm_taskbar_manager_main.exe <house_root>
#   khtpm_entity.exe      <package_dir>      <- NOT a house_root
function Start-Detached([string]$exe, [string[]]$rest) {
    if (-not (Test-Path -LiteralPath $exe)) {
        Write-Error "missing binary: $exe (run: run_khtpm_strip_win.ps1 build)"
        return 0
    }
    foreach ($r in $rest) {
        if ($r -and -not (Test-Path -LiteralPath $r)) {
            Write-Error "missing argument path: $r"
            return 0
        }
    }
    $quoted = @('"{0}"' -f $exe)
    foreach ($r in $rest) { $quoted += '"{0}"' -f $r }
    $cmd = $quoted -join " "
    try {
        $r2 = Invoke-CimMethod -ClassName Win32_Process -MethodName Create -Arguments @{
            CommandLine      = $cmd
            CurrentDirectory = $HOUSE
        }
    } catch {
        Write-Error "Win32_Process.Create failed: $($_.Exception.Message)"
        return 0
    }
    if ($r2.ReturnValue -ne 0) {
        Write-Error "Win32_Process.Create returned $($r2.ReturnValue) for $cmd"
        return 0
    }
    return [int]$r2.ProcessId
}

function Stop-Khtpm {
    $pids = @(Get-KhtpmPids)
    if ($pids.Count -eq 0) {
        Write-Host "khtpm was not running"
        return
    }
    Write-Host ("khtpm stopping (was PID(s): " + ($pids -join " ") + ")")
    # Same TERM-then-KILL escalation the Linux runner uses, in the same
    # spirit as its 2026-08-30 incident note: poll for real death rather
    # than trusting a fixed sleep, because the next launch's own liveness
    # guard would otherwise see the old process and refuse to start.
    foreach ($p in $pids) {
        Stop-Process -Id $p -ErrorAction SilentlyContinue
    }
    $i = 0
    while ($i -lt 30) {
        $alive = @($pids | Where-Object { Test-Alive $_ })
        if ($alive.Count -eq 0) { break }
        Start-Sleep -Milliseconds 100
        $i++
    }
    $alive = @($pids | Where-Object { Test-Alive $_ })
    if ($alive.Count -gt 0) {
        foreach ($p in $alive) { Stop-Process -Id $p -Force -ErrorAction SilentlyContinue }
        Start-Sleep -Milliseconds 300
    }
    Remove-Item -LiteralPath $PIDFILE -Force -ErrorAction SilentlyContinue
}

# ---- actions ---------------------------------------------------------
switch ($ACTION) {

    { $_ -in @("boot", "new", "test", "run") } {

        # ORDER DIFFERS FROM LINUX, deliberately. run_khtpm_strip.sh builds
        # first and calls kill_khtpm afterwards, which is fine on POSIX:
        # an open executable can be unlinked and rewritten underneath a
        # running process. Windows cannot - ld.exe fails with "Permission
        # denied" on a .exe that is currently mapped, so on this platform
        # the old bars MUST be stopped before the rebuild or the whole
        # `new` fails at the link step. (Hit for real, 2026-09-26.)
        #
        # `boot` = launch-only, no rebuild, so the desktop start button is
        # snappy - same split the Linux runner makes. `new`/`run`/`test`
        # are the explicit "build fresh" verbs. This is the Windows
        # equivalent of the Linux single-restart lock: without it, N
        # concurrent clicks race on the same +x/ output and the same PIDs.
        $need_build = ($ACTION -ne "boot")
        if (-not $need_build) {
            # NOTE: khtpm_entity.exe is deliberately NOT in this check.
            # It does not compile yet (khtpm_entity.c needs a much wider
            # Xlib slice than the strip renderer), so testing for it here
            # would make "is anything missing?" permanently true and every
            # `boot` - i.e. every desktop-start-button click - would run a
            # full ~40s rebuild and then spew the entity's compile errors
            # at the user. That is exactly what happened on 2026-09-26
            # before this was fixed. Only the two bar binaries gate boot;
            # the entity's status is reported as a warning, not treated
            # as a missing prerequisite.
            if (-not (Test-Path -LiteralPath $RENDER) -or -not (Test-Path -LiteralPath $MANAGER)) {
                $need_build = $true   # first-ever boot with no binaries
            } else {
                Write-Warning "khtpm_entity.exe absent - khtpm_entity.c is not ported to Windows yet."
                Write-Warning "  The taskbar starts, but with NO entities, so the bottom bar has no cells."
                Write-Warning "  This is a known gap, not a build failure. See WINDOWS-TASKBAR-PORT.md."
            }
        }

        if ($need_build) {
            $lock = Join-Path $STATE ".khtpm_restart.lock"
            $held = $false
            try {
                New-Item -ItemType Directory -Path $lock -ErrorAction Stop | Out-Null
            } catch {
                if (Test-Path -LiteralPath $lock) {
                    $holder = (Get-Content -LiteralPath (Join-Path $lock "pid") -ErrorAction SilentlyContinue) -replace "[^0-9]", ""
                    if ($holder -and (Test-Alive ([int]$holder))) {
                        Write-Host "khtpm restart already in progress (pid $holder) - ignoring this click"
                        exit 0
                    }
                    # dead holder (crashed mid-build): reclaim it
                    Remove-Item -LiteralPath $lock -Recurse -Force -ErrorAction SilentlyContinue
                    New-Item -ItemType Directory -Path $lock -ErrorAction SilentlyContinue | Out-Null
                    $held = $true
                } else {
                    Write-Host "khtpm restart already in progress - ignoring"
                    exit 0
                }
            }
            if (-not $held) {
                Set-Content -LiteralPath (Join-Path $lock "pid") -Value $PID -Encoding ASCII
            }
            try {
                # stop BEFORE building - see the order note above
                Stop-Khtpm
                & (Join-Path $SCRIPT_DIR "build_khtpm_strip_win.ps1")
                if ($LASTEXITCODE -ne 0 -and $null -ne $LASTEXITCODE) {
                    Write-Error "BUILD FAILED - not launching"
                    exit 1
                }
            } finally {
                Remove-Item -LiteralPath $lock -Recurse -Force -ErrorAction SilentlyContinue
            }
        } else {
            Stop-Khtpm
        }

        # cd into HOUSE first - the renderer's children inherit this cwd,
        # and relative menu commands (strip_relay.sh) depend on it being
        # house root, not wherever this script was invoked from. Win32_
        # Process.Create's CurrentDirectory is what does the `cd` here.
        $new = @()

        # The manager writes #.desktop/strip_ui.txt, which the templates
        # name as their vars= input. The bars have nothing to draw until
        # that file exists and is non-empty, so wait for it - exactly the
        # Linux runner's poll loop.
        if (Test-Path -LiteralPath $MANAGER) {
            $mp = Start-Detached $MANAGER @($HOUSE)
            if ($mp) { $new += $mp }
        }

        $strip = Join-Path $STATE "strip_ui.txt"
        if (Test-Path -LiteralPath $strip) { Remove-Item -LiteralPath $strip -Force -ErrorAction SilentlyContinue }
        $i = 0
        while ($i -lt 50) {
            if ((Test-Path -LiteralPath $strip) -and (Get-Item -LiteralPath $strip).Length -gt 0) { break }
            Start-Sleep -Milliseconds 100
            $i++
        }

        $hp = Start-Detached $RENDER @($HOUSE, $HEADER)
        if ($hp) { $new += $hp }
        $bp = Start-Detached $RENDER @($HOUSE, $BOTTOM)
        if ($bp) { $new += $bp }

        # Entities. The bottom bar's cells come from these, so with none
        # running the bottom is correctly-drawn-but-empty (n_hqwins=0).
        # Each pal is its own khtpm_entity process, invoked with the pal's
        # own package_dir - khtpm_core_render.c's main() explicitly
        # refuses to be the pal process and says so in its argc==2 usage
        # text. Names come from the command line:
        #     .\run_khtpm_strip_win.ps1 new cursword dsr_bank_a1
        # Anything after the action verb is a pal name; with none given,
        # a small default set is launched so the bar is never empty on a
        # fresh boot. A pal with no package dir is reported, not skipped
        # silently.
        $pals = @()
        if ($args.Count -ge 2) { $pals = $args[1..($args.Count - 1)] }
        if ($pals.Count -eq 0) {
            $pals = @("cursword", "dsr_bank_a1", "dsr_bank_b1", "book-stack")
        }
        if (-not (Test-Path -LiteralPath $ENTITY)) {
            Write-Warning "no $ENTITY - khtpm_entity.c is not ported to Windows yet, so NO entities can run."
            Write-Warning "  The bars will still start; the bottom bar's cells come from entities, so it stays empty."
            Write-Warning "  See WINDOWS-TASKBAR-PORT.md for the full missing-Xlib list."
            $pals = @()
        }
        $palsRoot = Join-Path $HOUSE "xyzfs\users"
        $userDir = $null
        if (Test-Path -LiteralPath $palsRoot) {
            $ud = @(Get-ChildItem -LiteralPath $palsRoot -Directory -ErrorAction SilentlyContinue)
            if ($ud.Count -ge 1) { $userDir = $ud[0].FullName }
        }
        foreach ($pal in $pals) {
            if (-not $userDir) { Write-Error "no xyzfs/users dir - cannot resolve pals"; break }
            $pkg = Join-Path $userDir "home\livedesk\pals\$pal"
            if (-not (Test-Path -LiteralPath $pkg)) {
                Write-Error "no such pal: $pal (looked in $pkg)"
                continue
            }
            # khtpm_entity takes <package_dir>, not <house_root>, and has no
            # template argument at all - a different shape from the bars.
            $ep = Start-Detached $ENTITY @($pkg)
            if ($ep) { $new += $ep }
        }

        Start-Sleep -Seconds 2

        # Never trust the return value alone - confirm the processes are
        # really up, and only then publish the pid file.
        $up = @($new | Where-Object { Test-Alive $_ })
        if ($up.Count -gt 0) {
            Set-Content -LiteralPath $PIDFILE -Value $up -Encoding ASCII
            Write-Host ("OK - khtpm running, PID(s): " + ($up -join " "))
            Write-Host "log: $LOG"
            Write-Host "detached: closing this terminal will NOT stop the bars"
        } else {
            Write-Error "FAILED to launch - no bar stayed alive"
            if (Test-Path -LiteralPath $LOG) { Get-Content -LiteralPath $LOG -Tail 40 }
            exit 1
        }
    }

    "stop" { Stop-Khtpm }

    "status" {
        $pids = @(Get-KhtpmPids)
        if ($pids.Count -gt 0) {
            Write-Host ("khtpm: RUNNING (PID(s): " + ($pids -join " ") + ")")
        } else {
            Write-Host "khtpm: stopped"
        }
    }

    "build" { & (Join-Path $SCRIPT_DIR "build_khtpm_strip_win.ps1") }

    default {
        Write-Host @"
run_khtpm_strip_win.ps1 - Windows runner for the khtpm taskbar

  .\run_khtpm_strip_win.ps1 new       # build fresh, stop any running khtpm, launch
  .\run_khtpm_strip_win.ps1 boot      # launch only, no rebuild (autostart)
  .\run_khtpm_strip_win.ps1 stop      # stop khtpm
  .\run_khtpm_strip_win.ps1 status    # is it running
  .\run_khtpm_strip_win.ps1 build     # build only, no process changes

Bars are launched through WMI, so they survive closing this terminal.
Always ends with a real PID check, never a bare exit code.
"@
    }
}
