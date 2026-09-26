# open_event_ez.ps1 — Windows only (no bash).
# Called from KHTPM context menu with package_dir (Linux uses open_event_ez.sh).
param(
    [Parameter(Position = 0, Mandatory = $true)]
    [string]$PkgDir
)

$ErrorActionPreference = "Continue"
if (-not (Test-Path -LiteralPath $PkgDir)) {
    $try = Join-Path (Get-Location) $PkgDir
    if (Test-Path -LiteralPath $try) { $PkgDir = $try }
    else { Write-Error "open_event_ez: package missing: $PkgDir"; exit 1 }
}
$PkgDir = (Resolve-Path -LiteralPath $PkgDir).Path
$Name = Split-Path -Leaf $PkgDir

$House = (Get-Location).Path
if (-not (Test-Path -LiteralPath (Join-Path $House "&.widgits"))) {
    # entities/name -> rancher -> @.apps -> house
    $House = Split-Path (Split-Path (Split-Path (Split-Path $PkgDir -Parent) -Parent) -Parent) -Parent
}
$Ez = Join-Path $House "&.widgits\event-ez"
$EventPkg = Join-Path $PkgDir "event_pkg"
New-Item -ItemType Directory -Path (Join-Path $EventPkg "pages\page_1") -Force | Out-Null

$ps1 = Join-Path $Ez "button.ps1"
if (-not (Test-Path -LiteralPath $ps1)) {
    Write-Error "open_event_ez: missing $ps1 (Windows uses button.ps1, not bash)"
    exit 1
}

$env:EZ_PKG_NAME = $Name
$env:EZ_PKG_DIR = $EventPkg
$env:Path = "C:\msys64\mingw64\bin;" + $env:Path

Write-Host "event-ez (ps1) name=$Name"
Write-Host "  EZ_PKG_DIR=$EventPkg"
Write-Host "  button=$ps1"

Start-Process -FilePath "powershell.exe" -ArgumentList @(
    "-NoProfile", "-ExecutionPolicy", "Bypass", "-File", $ps1, "run"
) -WorkingDirectory $Ez -WindowStyle Normal

Write-Host "launched event-ez button.ps1 run"
exit 0
