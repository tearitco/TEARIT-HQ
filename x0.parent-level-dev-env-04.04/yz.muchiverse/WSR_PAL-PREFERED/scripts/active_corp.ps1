# active_corp.ps1 - Windows parity with active_corp.sh
# Resolves active_corp_index into corp_<TICKER> piece_id (sorted order).

$ErrorActionPreference = "SilentlyContinue"
$SCRIPT_DIR = Split-Path -Parent (Split-Path -Parent $MyInvocation.MyCommand.Path)
$state = Join-Path $SCRIPT_DIR "projects\wsr-pal\pieces\wsr_menu\state.txt"
$idx = 0
if (Test-Path $state) {
    $line = Get-Content $state | Where-Object { $_ -like "active_corp_index=*" } | Select-Object -First 1
    if ($line) { $idx = [int](($line -split "=", 2)[1]) }
}
$corps = Get-ChildItem (Join-Path $SCRIPT_DIR "projects\wsr-pal\pieces") -Directory -Filter "corp_*" |
    Sort-Object Name |
    Select-Object -ExpandProperty Name
if ($corps -and $idx -ge 0 -and $idx -lt $corps.Count) {
    Write-Output $corps[$idx]
}
