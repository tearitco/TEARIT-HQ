# button.ps1 - Windows launcher for csv-hq (parity with button.sh).
#
# The taskbar's livedesk_build_toys_menu() builds this toy's row as
# livedesk:open-toy:@.apps\csv-hq/button.sh, and ktb_hq_activate() on _WIN32
# rewrites that to button.ps1 and runs
#   powershell -NoProfile -ExecutionPolicy Bypass -File <this file> run
# Linux stays on button.sh; this file exists only because Windows has no
# sh. The logic every toy shares (find the house, find the shared
# renderer, kill this toy's own previous window, launch it detached) lives
# once in win_toy_button.ps1 next to the renderer, rather than copied here
# seventeen times.
param(
    [Parameter(Position = 0)]
    [string]$Action = "run"
)

$ErrorActionPreference = "Continue"
$here = Split-Path -Parent $MyInvocation.MyCommand.Path
$house = Split-Path (Split-Path $here -Parent) -Parent
$shared = Join-Path $house "_.monads\_.livedesk-taskbar\ops\win_toy_button.ps1"
if (-not (Test-Path -LiteralPath $shared)) {
    Write-Error "missing $shared"
    exit 1
}
& $shared $here $Action
exit $LASTEXITCODE
