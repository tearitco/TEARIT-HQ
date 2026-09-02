# build.ps1 - Windows twin of scripts/build.sh (piececraft-hq)
# ASCII only. Surgical Win shims live in sources (#ifdef _WIN32).
# Outputs system\*.exe; ops keep .+x suffix (MinGW PE without .exe is fine).

$ErrorActionPreference = "Continue"
$SCRIPT_DIR = Split-Path -Parent (Split-Path -Parent $MyInvocation.MyCommand.Path)
Set-Location -LiteralPath $SCRIPT_DIR

$MSYS = "C:\msys64\mingw64\bin"
$MSYS_LIB = "C:\msys64\mingw64\lib"
if (Test-Path $MSYS) {
    if ($env:Path -notlike "*$MSYS*") { $env:Path = "$MSYS;$env:Path" }
}

if (-not (Get-Command gcc -ErrorAction SilentlyContinue)) {
    Write-Error "gcc not found. Install MSYS2 MinGW64 (mingw-w64-x86_64-gcc, freeglut)."
    exit 1
}

# House root = parent of @.apps (two levels up from this project)
$HOUSE_DIR = Split-Path (Split-Path $SCRIPT_DIR -Parent) -Parent
if (-not (Test-Path -LiteralPath $HOUSE_DIR)) {
    $HOUSE_DIR = Split-Path $SCRIPT_DIR -Parent
    $HOUSE_DIR = Split-Path $HOUSE_DIR -Parent
}

New-Item -ItemType Directory -Path "ops\+x" -Force | Out-Null
New-Item -ItemType Directory -Path "system" -Force | Out-Null

$CFLAGS = @("-Wall", "-Wextra", "-O2", "-Iops")
$rc = 0

function Build-One([string]$Src, [string]$Out, [string[]]$Extra = @()) {
    if (-not (Test-Path -LiteralPath $Src)) {
        Write-Host "SKIP missing $Src"
        return
    }
    $outDir = Split-Path -Parent $Out
    if ($outDir -and -not (Test-Path -LiteralPath $outDir)) {
        New-Item -ItemType Directory -Path $outDir -Force | Out-Null
    }
    Write-Host "gcc $Src -> $Out"
    & gcc @CFLAGS $Src -o $Out @Extra
    if ($LASTEXITCODE -eq 0) { Write-Host "  OK $Out" }
    else {
        Write-Host "  FAIL $Out (exit $LASTEXITCODE)" -ForegroundColor Red
        $script:rc = 1
    }
}

Write-Host "--- system processes ---"
Build-One "system\prisc+x.c" "system\prisc+x.exe"
Build-One "system\keyboard_input.c" "system\keyboard_input.exe"
Build-One "system\renderer.c" "system\renderer.exe"
Build-One "system\chtpm_parser_pal.c" "system\chtpm_parser_pal.exe" @("-Wno-unused-result", "-Wno-stringop-truncation")
Build-One "system\chtpm_rgb_render.c" "system\chtpm_rgb_render.exe"
Build-One "system\orchestrator.c" "system\orchestrator.exe"

Write-Host "--- gl_mirror (prefer 014.wsr freeglut Win binary) ---"
$wsr = Get-ChildItem -LiteralPath $HOUSE_DIR -Directory -ErrorAction SilentlyContinue |
    Where-Object { $_.Name -like "014.wsr*" } | Select-Object -First 1
$glFlags = @("-L$MSYS_LIB", "-lopengl32", "-lglu32", "-lfreeglut", "-lwinmm", "-lgdi32", "-luser32")
$usedWsr = $false
if ($wsr) {
    Write-Host "  house wsr: $($wsr.FullName)"
    $wsrExe = Join-Path $wsr.FullName "system\gl_mirror.exe"
    $wsrBare = Join-Path $wsr.FullName "system\gl_mirror"
    $wsrGl = Join-Path $wsr.FullName "system\gl_mirror.c"
    if (Test-Path -LiteralPath $wsrExe) {
        Copy-Item -LiteralPath $wsrExe -Destination "system\gl_mirror.exe" -Force
        Write-Host "  OK gl_mirror.exe (copied from 014.wsr)"
        $usedWsr = $true
    } elseif (Test-Path -LiteralPath $wsrBare) {
        # PE without .exe extension
        Copy-Item -LiteralPath $wsrBare -Destination "system\gl_mirror.exe" -Force
        Write-Host "  OK gl_mirror.exe (copied bare PE from 014.wsr)"
        $usedWsr = $true
    } elseif (Test-Path -LiteralPath $wsrGl) {
        Write-Host "  compiling wsr gl_mirror.c..."
        & gcc @CFLAGS -o "system\gl_mirror.exe" $wsrGl @glFlags
        if ($LASTEXITCODE -eq 0) {
            Write-Host "  OK gl_mirror.exe (built from 014.wsr source)"
            $usedWsr = $true
        }
    }
}
if (-not $usedWsr) {
    Write-Host "  trying local gl_mirror.c (likely X11-only)..."
    & gcc @CFLAGS -o "system\gl_mirror.exe" "system\gl_mirror.c" @glFlags 2>&1 | Out-Null
    if ($LASTEXITCODE -eq 0) { Write-Host "  OK gl_mirror.exe" }
    else {
        Write-Host "  SKIP gl_mirror (need freeglut + Win source from 014.wsr)" -ForegroundColor Yellow
    }
}

Write-Host "--- aomorai ops ---"
Build-One "ops\pc_menu_input.c" "ops\+x\pc_menu_input.+x"
Build-One "ops\pc_compose_frame.c" "ops\+x\pc_compose_frame.+x"
Build-One "ops\pc_generate_chunk.c" "ops\+x\pc_generate_chunk.+x"
Build-One "ops\pc_phymoji_gen.c" "ops\+x\pc_phymoji_gen.+x" @("-lm")
Build-One "ops\pc_clock_daemon.c" "ops\+x\pc_clock_daemon.+x" @("-lm")

Write-Host "--- emoji tools from 014.wsr if present ---"
if ($wsr) {
    foreach ($bin in @("emoji_gen_atlas", "emoji_xtract")) {
        foreach ($ext in @(".+x.exe", ".exe", ".+x")) {
            $src = Join-Path $wsr.FullName "ops\+x\$bin$ext"
            if (Test-Path -LiteralPath $src) {
                Copy-Item -LiteralPath $src -Destination "ops\+x\$bin.+x" -Force
                Write-Host "  OK copied $bin"
                break
            }
        }
    }
}
New-Item -ItemType Directory -Path "pieces\registry\emoji_assets" -Force | Out-Null

# freeglut next to gl_mirror (MSYS2 ships libfreeglut.dll)
foreach ($glutDll in @("C:\msys64\mingw64\bin\libfreeglut.dll", "C:\msys64\mingw64\bin\freeglut.dll")) {
    if ((Test-Path $glutDll) -and (Test-Path "system\gl_mirror.exe")) {
        $dllName = Split-Path $glutDll -Leaf
        Copy-Item -LiteralPath $glutDll -Destination "system\$dllName" -Force
        Write-Host "  OK $dllName beside gl_mirror"
        break
    }
}

Write-Host "build ok (rc=$rc)"
exit $rc
