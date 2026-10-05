# open_pchq_board.ps1 - Windows launcher for piececraft-hq's board window.
#
# Parity with open_pchq_board.sh's TASKBAR role: the toys dropdown row for
# Piececraft-HQ is `livedesk:open-toy:<pkg>/open_pchq_board.sh` and the
# manager's own Windows toys dispatch rewrites `.sh` -> `.ps1` and runs it
# (`powershell -File open_pchq_board.ps1 run`). Before this file existed
# that rewrite targeted a script that was never ported, so the row ran
# NOTHING at all - the real reason "pc-hq doesn't open from toys".
#
# What it does: resolve the house root, then launch the shared
# khtpm_core_render against pchq-board.xhtpm (the same `<house> <template>`
# shape every other HQ app's Windows branch uses). open_pchq_board.sh's own
# engine-session/projector guards are POSIX-only (pgrep/readlink /proc/
# setsid) and have their own port; the board WINDOW is this row's deliverable.
#
# Usage: open_pchq_board.ps1 [<house_root>|run]
#   - HQ-menu dispatch passes <house_root>.
#   - toys-menu dispatch passes the literal "run" -> walk up to house root.

param([string]$Arg = "run")

$ErrorActionPreference = "Continue"
$PKG = Split-Path -Parent $MyInvocation.MyCommand.Path

function Test-House([string]$d) {
    return ($d -and (Test-Path -LiteralPath (Join-Path $d "#.desktop")))
}

# Resolve house root: an explicit directory arg wins; otherwise ("run" or
# nothing) walk up from this package to the dir holding #.desktop.
$House = $null
if ($Arg -and (Test-Path -LiteralPath $Arg -PathType Container) -and (Test-House $Arg)) {
    $House = (Resolve-Path -LiteralPath $Arg).Path
} else {
    $d = $PKG
    while ($d -and -not (Test-House $d)) {
        $parent = Split-Path -Parent $d
        if (-not $parent -or $parent -eq $d) { break }
        $d = $parent
    }
    if (Test-House $d) { $House = $d }
}
if (-not $House) {
    Write-Error "open_pchq_board.ps1: could not resolve house root (arg='$Arg')"
    exit 1
}

$Bin   = Join-Path $House "_.monads\_.livedesk-taskbar\ops\+x\khtpm_core_render.exe"
$Board = Join-Path $PKG "pchq-board.xhtpm"
if (-not (Test-Path -LiteralPath $Bin))   { Write-Error "open_pchq_board.ps1: missing $Bin";   exit 1 }
if (-not (Test-Path -LiteralPath $Board)) { Write-Error "open_pchq_board.ps1: missing $Board"; exit 1 }

# Single-instance: clean-restart the board window (mirrors the .sh guard at
# open_pchq_board.sh:133-148). Match only this board's own template so other
# khtpm_core_render windows are never touched.
#
# The .sh guard kills the PROJECTOR too (proj_pids(), sh:137) - this did not,
# which let a stale pchq_board_projector.exe survive a relaunch and keep
# writing state/ui.txt alongside the fresh one. Two projectors racing on one
# ui.txt is its own source of "the board looks broken".
Get-CimInstance Win32_Process -Filter "Name='khtpm_core_render.exe'" -ErrorAction SilentlyContinue |
    Where-Object { $_.CommandLine -and $_.CommandLine -like "*pchq-board.xhtpm*" } |
    ForEach-Object { Stop-Process -Id $_.ProcessId -Force -ErrorAction SilentlyContinue }
Get-CimInstance Win32_Process -Filter "Name='pchq_board_projector.exe'" -ErrorAction SilentlyContinue |
    ForEach-Object { Stop-Process -Id $_.ProcessId -Force -ErrorAction SilentlyContinue }
Start-Sleep -Milliseconds 400

# Launch in the "standard x11-hq shape" (sh:206):
#     "$BIN" "$HOUSE_ROOT" "$BOARD_TPL" piececraft-hq
# FOUR arguments. The fourth ("piececraft-hq") was MISSING here - the .ps1
# passed only two. khtpm_core_render.c reinterprets argv[3]: an existing
# directory becomes g_arg3_dir, an "instance dir" whose ui.txt is appended as
# an extra UI source (khtpm_core_render.c:1143-1158, :1565, :11826-11829).
# With argc==4 and a non-directory value it is currently inert, but Linux
# passes it, it is part of the documented launch contract, and the .ps1 runs
# with a different working directory than the .sh's setsid+inherit, so a
# relative "piececraft-hq" could stat differently. Match the reference.
$proc = Start-Process -FilePath $Bin -ArgumentList @($House, $Board, "piececraft-hq") `
                      -WorkingDirectory $House -PassThru

# Record the PID in the proc-ledger exactly like sh:208-209, so a taskbar quit
# (ktb_reap_launched, khtpm_taskbar_manager.c:1477) can actually reap this
# board window instead of orphaning it.
$ledger = Join-Path $House "#.desktop\livedesk_proc_list.txt"
try {
    Add-Content -LiteralPath $ledger -Value ("{0} {0} 0 0 pchq-board" -f $proc.Id) -ErrorAction Stop
} catch {
    Write-Warning "open_pchq_board.ps1: could not write $ledger : $($_.Exception.Message)"
}

Write-Output "open_pchq_board: board window launched (pid $($proc.Id))"
