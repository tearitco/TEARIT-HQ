# run.ps1 - book-stack launcher (Windows parity with button.sh).
# Opens the book-stack desktop entity window and starts the reader
# dispatch, same as `sh button.sh run` on Linux.
$ErrorActionPreference = "Stop"
$Here = Split-Path -Parent $MyInvocation.MyCommand.Path
$BookStack = $Here
$House = (Resolve-Path (Join-Path $BookStack "..\..")).Path
$TpWin = Join-Path $House "&.widgits\tile-picker\ops\+x\tp_desktop_window.exe"
$Entity = Join-Path $House "*.monads\*.book-stack\entities\book-stack"
$Event = Join-Path $BookStack "pieces\reader\event_pkg\pages\page_1\event.pal"

# Resolve the prisc runner without literal emoji in source (wildcard)
$PriscDir = Get-ChildItem -LiteralPath $House -Directory |
    Where-Object { $_.Name -like "101.mutaclsym*" } | Select-Object -First 1
$Prisc = ""
if ($PriscDir) {
    foreach ($c in @("prisc.exe", "prisc+x.exe", "prisc+x")) {
        $p = Join-Path $PriscDir.FullName "system\$c"
        if (Test-Path -LiteralPath $p) { $Prisc = $p; break }
    }
}

# Entity window (background if not already running)
$running = Get-CimInstance Win32_Process -Filter "Name='tp_desktop_window.exe'" -EA SilentlyContinue |
    Where-Object { $_.CommandLine -like "*entities\book-stack*" }
if (-not $running) {
    if (-not (Test-Path -LiteralPath $TpWin)) { Write-Host "MISSING $TpWin"; exit 1 }
    Start-Process -FilePath $TpWin -ArgumentList $Entity -WindowStyle Hidden
    Start-Sleep -Milliseconds 800
}

# Reader dispatch
if ($Prisc) {
    & $Prisc $Event
} else {
    Write-Host "MISSING prisc runner under $House\101.mutaclsym*\system\ (reader cannot start)"
    exit 1
}
