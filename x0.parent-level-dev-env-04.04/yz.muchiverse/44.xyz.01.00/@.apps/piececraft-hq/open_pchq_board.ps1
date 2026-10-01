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

# Single-instance: clean-restart the board window (mirrors the .sh guard).
# Match only this board's own template so other khtpm_core_render windows
# are never touched.
Get-CimInstance Win32_Process -Filter "Name='khtpm_core_render.exe'" -ErrorAction SilentlyContinue |
    Where-Object { $_.CommandLine -and $_.CommandLine -like "*pchq-board.xhtpm*" } |
    ForEach-Object { Stop-Process -Id $_.ProcessId -Force -ErrorAction SilentlyContinue }
Start-Sleep -Milliseconds 400

Start-Process -FilePath $Bin -ArgumentList @($House, $Board) -WorkingDirectory $House
Write-Output "open_pchq_board: board window launched"
