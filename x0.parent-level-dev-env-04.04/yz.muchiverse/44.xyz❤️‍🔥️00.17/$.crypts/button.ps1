# button.ps1 - Windows launcher for $.crypts (parity with button.sh)
# Usage:
#   powershell -ExecutionPolicy Bypass -File .\button.ps1 <action>
# Actions: run|r|start|restart | on | off | status | compile|c|build | check | help
#
# ASCII only (no smart quotes / em-dashes).
# Does not hang on Get-Process.Path — kill uses ProcessName + taskkill.

param(
    [Parameter(Position = 0)]
    [string]$Action = "help"
)

$ErrorActionPreference = "Continue"
$SCRIPT_DIR = Split-Path -Parent $MyInvocation.MyCommand.Path
Set-Location -LiteralPath $SCRIPT_DIR

$MSYS_BIN = "C:\msys64\mingw64\bin"
if (Test-Path $MSYS_BIN) {
    if ($env:Path -notlike "*$MSYS_BIN*") {
        $env:Path = "$MSYS_BIN;$env:Path"
    }
}

$PDL = Join-Path $SCRIPT_DIR "autostart.pdl"
$BIN_DIR = Join-Path $SCRIPT_DIR "ops\+x"
$BIN = Join-Path $BIN_DIR "crypt_autostart.+x"
$BIN_EXE = Join-Path $BIN_DIR "crypt_autostart.+x.exe"
$SRC = Join-Path $SCRIPT_DIR "ops\crypt_autostart.c"
$HOUSE = Split-Path -Parent $SCRIPT_DIR

# Linux house dirs are named "*.monads", "*.livedesk-taskbar", ...
# Windows forbids "*" in a path component; this checkout uses "_.monads", ...
# Alias at resolve time so PDL/Linux spelling can stay "*.foo".
function ConvertTo-WinHousePath {
    param([string]$Path)
    if ([string]::IsNullOrEmpty($Path)) { return $Path }
    $parts = $Path -split '[\\/]+'
    $aliased = foreach ($p in $parts) {
        if ($p.Length -ge 2 -and $p[0] -eq [char]'*' -and $p[1] -eq [char]'.') {
            '_' + $p.Substring(1)
        } else {
            $p
        }
    }
    $arr = @($aliased)
    if ($arr.Count -eq 0) { return $Path }
    if ($arr[0] -match '^[A-Za-z]:$') {
        if ($arr.Count -eq 1) { return ($arr[0] + '\') }
        return ($arr[0] + '\' + ($arr[1..($arr.Count - 1)] -join '\'))
    }
    return ($arr -join '\')
}

function Get-CryptBin {
    $plain = Join-Path $BIN_DIR "crypt_autostart.exe"
    if (Test-Path -LiteralPath $plain) { return $plain }
    if (Test-Path -LiteralPath $BIN_EXE) { return $BIN_EXE }
    if (Test-Path -LiteralPath $BIN) { return $BIN }
    return $null
}

function Invoke-CompileCrypt {
    if (-not (Test-Path -LiteralPath $BIN_DIR)) {
        New-Item -ItemType Directory -LiteralPath $BIN_DIR -Force | Out-Null
    }
    # Windows: plain .exe (ld rejects ".+x" under emoji house paths)
    $out = "crypt_autostart.exe"
    Write-Host "gcc crypt_autostart.c -> ops/+x/$out"
    Push-Location -LiteralPath $BIN_DIR
    try {
        & gcc -Wall -O2 -mwindows -o $out "..\crypt_autostart.c"
        if ($LASTEXITCODE -ne 0) {
            Write-Host "FAIL crypt_autostart"
            return $LASTEXITCODE
        }
    } finally {
        Pop-Location
    }
    Write-Host "OK crypt_autostart"

    # Desktop starter (parity with ops/livedesk-start-button.c on Linux)
    $srcBtn = Join-Path $SCRIPT_DIR "ops\livedesk-start-button.c"
    if (Test-Path -LiteralPath $srcBtn) {
        Write-Host "gcc livedesk-start-button.c -> ops/+x/livedesk-start-button.exe"
        Push-Location -LiteralPath $BIN_DIR
        try {
            & gcc -Wall -O2 -mwindows -o "livedesk-start-button.exe" "..\livedesk-start-button.c"
            if ($LASTEXITCODE -ne 0) { Write-Host "FAIL livedesk-start-button" }
            else { Write-Host "OK livedesk-start-button" }
        } finally { Pop-Location }
    }
    return 0
}

function Invoke-InstallDesktop {
    $exe = Join-Path $BIN_DIR "livedesk-start-button.exe"
    if (-not (Test-Path -LiteralPath $exe)) {
        $null = Invoke-CompileCrypt
    }
    if (-not (Test-Path -LiteralPath $exe)) {
        Write-Host "FAIL: no livedesk-start-button.exe"
        return 1
    }
    $desk = [Environment]::GetFolderPath("Desktop")
    $dest = Join-Path $desk "livedesk-start-button.exe"
    $side = Join-Path $desk "livedesk-house-root.txt"
    Copy-Item -LiteralPath $exe -Destination $dest -Force
    $utf8 = New-Object System.Text.UTF8Encoding $false
    [System.IO.File]::WriteAllText($side, $HOUSE, $utf8)
    Write-Host "installed: $dest"
    Write-Host "house sidecar: $side"
    Write-Host "Double-click livedesk-start-button.exe on the Desktop to start the strip."
    return 0
}

function Invoke-CompileKhtpm {
    # Current taskbar lives in *.monads/*.livedesk-taskbar/ops (Win: _.monads\...).
    # Linux build_khtpm_strip.sh is unchanged. We compile:
    #   khtpm_taskbar_manager_main.exe  (shared C, already has _WIN32 shims)
    #   khtpm_strip_parser.exe          (same khtpm_strip_parser.c as Linux + x11_win shim)
    # Do NOT resurrect archived tp_taskbar_win.c / khtpm_taskbar_plat_win.c.
    $tbOps = ConvertTo-WinHousePath (Join-Path $HOUSE "*.monads\*.livedesk-taskbar\ops")
    $tbOutDir = ConvertTo-WinHousePath (Join-Path $tbOps "+x")
    Write-Host "  taskbar ops: $tbOps"

    if (-not (Test-Path -LiteralPath $tbOutDir)) {
        New-Item -ItemType Directory -LiteralPath $tbOutDir -Force | Out-Null
    }

    $rc = 0
    $mgrMain = Join-Path $tbOps "khtpm_taskbar_manager_main.c"
    $mgrCore = Join-Path $tbOps "khtpm_taskbar_manager.c"
    if ((Test-Path -LiteralPath $mgrMain) -and (Test-Path -LiteralPath $mgrCore)) {
        Write-Host "gcc khtpm_taskbar_manager_main.c + manager.c -> khtpm_taskbar_manager_main.exe"
        Push-Location -LiteralPath $tbOutDir
        try {
            & gcc -Wall -O2 -mwindows -o "khtpm_taskbar_manager_main.exe" `
                "..\khtpm_taskbar_manager_main.c" "..\khtpm_taskbar_manager.c"
            if ($LASTEXITCODE -ne 0) { $rc = 1; Write-Host "FAIL khtpm_taskbar_manager_main" }
            else { Write-Host "OK khtpm_taskbar_manager_main" }
        } finally { Pop-Location }
    } else {
        Write-Host "MISS khtpm_taskbar_manager sources"
        $rc = 1
    }

    $parserCore = Join-Path $tbOps "khtpm_strip_parser.c"
    $parserLay = Join-Path $tbOps "khtpm_strip_layout.c"
    $parserShim = Join-Path $tbOps "khtpm_strip_x11_win.c"
    if ((Test-Path -LiteralPath $parserCore) -and (Test-Path -LiteralPath $parserLay) -and (Test-Path -LiteralPath $parserShim)) {
        Write-Host "gcc khtpm_strip_parser.c + layout + x11_win shim -> khtpm_strip_parser.exe"
        Push-Location -LiteralPath $tbOutDir
        try {
            & gcc -Wall -O2 -mwindows -o "khtpm_strip_parser.exe" `
                "..\khtpm_strip_parser.c" "..\khtpm_strip_layout.c" "..\khtpm_strip_x11_win.c" `
                -lgdi32 -luser32
            if ($LASTEXITCODE -ne 0) { $rc = 1; Write-Host "FAIL khtpm_strip_parser (shared core + win shim)" }
            else { Write-Host "OK khtpm_strip_parser (shared core + win shim)" }
        } finally { Pop-Location }
    } else {
        Write-Host "MISS khtpm_strip_parser.c / layout / x11_win shim"
        $rc = 1
    }

    $rgb = Join-Path $tbOps "tp_desktop_window_rgb.c"
    if ((Test-Path -LiteralPath $rgb) -and (Test-Path -LiteralPath $parserShim)) {
        Write-Host "gcc tp_desktop_window_rgb.c + x11_win shim -> tp_desktop_window_rgb.exe"
        # gcc -o into the emoji house path can fail (Invalid argument); compile to TEMP then copy.
        $rgbTmp = Join-Path $env:TEMP "tp_desktop_window_rgb.exe"
        Push-Location -LiteralPath $tbOps
        try {
            & gcc -Wall -O2 -mwindows -o $rgbTmp `
                ".\tp_desktop_window_rgb.c" ".\khtpm_strip_x11_win.c" `
                -lgdi32 -luser32
            if ($LASTEXITCODE -ne 0) { $rc = 1; Write-Host "FAIL tp_desktop_window_rgb" }
            else {
                Copy-Item -LiteralPath $rgbTmp -Destination (Join-Path $tbOutDir "tp_desktop_window_rgb.exe") -Force
                Write-Host "OK tp_desktop_window_rgb (shared entity + win shim)"
            }
        } finally { Pop-Location }
    } else {
        Write-Host "MISS tp_desktop_window_rgb.c"
        $rc = 1
    }
    return $rc
}

function Invoke-KillLivedesk {
    Write-Host "Stopping KHTPM processes (name-only)..."
    $names = @("tp_desktop_window", "tp_desktop_window_rgb", "tp_taskbar",
               "khtpm_strip_parser", "khtpm_taskbar_manager_main", "crypt_autostart",
               "tp_desktop_window.+x", "tp_taskbar.+x", "crypt_autostart.+x")
    foreach ($n in $names) {
        Get-Process -ErrorAction SilentlyContinue |
            Where-Object { $_.ProcessName -eq $n -or $_.ProcessName -like "$n*" } |
            ForEach-Object {
                try { Stop-Process -Id $_.Id -Force -ErrorAction Stop } catch { }
            }
        & taskkill /F /IM "$n.exe" 2>$null | Out-Null
        & taskkill /F /IM $n 2>$null | Out-Null
    }
    Write-Host "done"
}

function Set-AutostartEnabled([int]$On) {
    if (-not (Test-Path -LiteralPath $PDL)) {
        Write-Host "MISSING $PDL"
        return 1
    }
    $lines = Get-Content -LiteralPath $PDL
    $out = foreach ($line in $lines) {
        if ($line -match '^\s*STATE\s*\|\s*enabled\s*\|') {
            if ($On -eq 1) {
                ($line -replace '\|\s*[01]\s*$', '| 1')
            } else {
                ($line -replace '\|\s*[01]\s*$', '| 0')
            }
        } else {
            $line
        }
    }
    $out | Set-Content -LiteralPath $PDL -Encoding utf8
    if ($On -eq 1) { Write-Host "autostart: ON" } else { Write-Host "autostart: OFF" }
    return 0
}

function Invoke-Run {
    Write-Host "=== $.crypts Windows launcher ===" -ForegroundColor Cyan
    Write-Host "[1/3] Ensure crypt_autostart binary..."
    $bin = Get-CryptBin
    if (-not $bin) {
        $rc = Invoke-CompileCrypt
        if ($rc -ne 0) { exit $rc }
        $bin = Get-CryptBin
    }
    if (-not $bin) {
        Write-Host "FAIL: no crypt_autostart binary after compile"
        exit 1
    }

    Write-Host "[2/3] Ensure KHTPM Win stubs (if missing)..."
    $tbParser = ConvertTo-WinHousePath (Join-Path $HOUSE "*.monads\*.livedesk-taskbar\ops\+x\khtpm_strip_parser.exe")
    $tbMgr = ConvertTo-WinHousePath (Join-Path $HOUSE "*.monads\*.livedesk-taskbar\ops\+x\khtpm_taskbar_manager_main.exe")
    $needK = -not (Test-Path -LiteralPath $tbParser) -or -not (Test-Path -LiteralPath $tbMgr)
    if ($needK) {
        $null = Invoke-CompileKhtpm
    } else {
        Write-Host "  KHTPM bins present"
    }

    Write-Host "[3/3] Run crypt_autostart (CWD=house, relative pdl)"
    # CWD = house root: emoji-safe relative paths (no long abs Unicode in argv)
    Push-Location -LiteralPath $HOUSE
    try {
        # Relative path from house root avoids CreateProcessA/ACP issues
        $relPdl = "$.crypts\autostart.pdl"
        & $bin $relPdl
        $rc = $LASTEXITCODE
    } finally {
        Pop-Location
    }
    if ($null -eq $rc) { $rc = 0 }
    Write-Host "crypt_autostart exit=$rc"
    return $rc
}

$act = $Action.ToLowerInvariant()

if ($act -eq "run" -or $act -eq "r" -or $act -eq "start" -or $act -eq "restart") {
    exit (Invoke-Run)
}
elseif ($act -eq "on") {
    exit (Set-AutostartEnabled 1)
}
elseif ($act -eq "off") {
    exit (Set-AutostartEnabled 0)
}
elseif ($act -eq "status") {
    if (Test-Path -LiteralPath $PDL) {
        Select-String -LiteralPath $PDL -Pattern "enabled" | ForEach-Object { $_.Line }
    } else {
        Write-Host "MISSING $PDL"
        exit 1
    }
    exit 0
}
elseif ($act -eq "compile" -or $act -eq "c" -or $act -eq "build") {
    $rc = Invoke-CompileCrypt
    $rc2 = Invoke-CompileKhtpm
    if ($rc -ne 0) { exit $rc }
    if ($rc2 -ne 0) { exit $rc2 }
    exit 0
}
elseif ($act -eq "check") {
    $bin = Get-CryptBin
    if ($bin) { Write-Host "OK $bin" } else { Write-Host "MISSING crypt_autostart" }
    if (Test-Path -LiteralPath $PDL) { Write-Host "OK $PDL" } else { Write-Host "MISSING $PDL" }
    $tb = ConvertTo-WinHousePath (Join-Path $HOUSE "*.monads\*.livedesk-taskbar\ops\+x")
    Get-ChildItem -LiteralPath $tb -Filter "khtpm_strip_parser*" -ErrorAction SilentlyContinue |
        ForEach-Object { Write-Host "OK $($_.FullName)" }
    Get-ChildItem -LiteralPath $tb -Filter "khtpm_taskbar_manager_main*" -ErrorAction SilentlyContinue |
        ForEach-Object { Write-Host "OK $($_.FullName)" }
    exit 0
}
elseif ($act -eq "kill") {
    Invoke-KillLivedesk
    exit 0
}
elseif ($act -eq "install-xdg") {
    Write-Host "install-xdg is Linux-only. On Windows: .\button.ps1 install-desktop"
    exit 0
}
elseif ($act -eq "install-desktop") {
    exit (Invoke-InstallDesktop)
}
else {
    Write-Host @'
$.crypts — house-wide autostart control (Windows)

  .\button.ps1 run            # quit livedesk, then launch autostart.pdl
  .\button.ps1 restart        # same as run
  .\button.ps1 on | off       # toggle STATE|enabled in autostart.pdl
  .\button.ps1 status         # show enabled state
  .\button.ps1 compile         # rebuild crypt_autostart + strip + desktop starter
  .\button.ps1 check           # verify binary + pdl + KHTPM bins
  .\button.ps1 kill            # stop strip/parser/manager by name
  .\button.ps1 install-desktop # copy livedesk-start-button.exe to the Desktop

Linux: sh button.sh ...
PDL paths must be house-relative (see !.linux-absolute-FIXME-a6.txt).
'@
    exit 0
}
