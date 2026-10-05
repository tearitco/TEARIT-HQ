# scripts/build.ps1 - Windows compile for wsr-pal (parity with build.sh)
# Uses MSYS2 MinGW64 gcc. LOCAL COPIES ONLY - never reaches outside this project.
# Linux remains on scripts/build.sh.

$ErrorActionPreference = "Continue"
$SCRIPT_DIR = Split-Path -Parent (Split-Path -Parent $MyInvocation.MyCommand.Path)
Set-Location $SCRIPT_DIR

$MSYS_BIN = "C:\msys64\mingw64\bin"
$MSYS_LIB = "C:\msys64\mingw64\lib"
if (Test-Path $MSYS_BIN) {
    if ($env:Path -notlike "*$MSYS_BIN*") {
        $env:Path = "$MSYS_BIN;$env:Path"
    }
}

if (-not (Get-Command gcc -ErrorAction SilentlyContinue)) {
    Write-Error "gcc not found. Install MSYS2 MinGW64 and add C:\msys64\mingw64\bin to PATH."
    Write-Host "  pacman -S --needed mingw-w64-x86_64-gcc mingw-w64-x86_64-freeglut"
    exit 1
}

New-Item -ItemType Directory -Force -Path (Join-Path $SCRIPT_DIR "ops\+x") | Out-Null

$CFLAGS = @("-Wall", "-Wextra", "-O2")
# freeglut on MinGW (same link order as 1.TPMOS compile_all.ps1)
$GL_FLAGS = @("-L$MSYS_LIB", "-lopengl32", "-lglu32", "-lfreeglut", "-lwinmm", "-lgdi32", "-luser32")

function Compile-One {
    param(
        [string]$Src,
        [string]$Out,
        [string[]]$Extra = @()
    )
    if (-not (Test-Path $Src)) {
        Write-Warning "missing $Src"
        return $false
    }
    $outDir = Split-Path -Parent $Out
    if ($outDir -and -not (Test-Path $outDir)) {
        New-Item -ItemType Directory -Force -Path $outDir | Out-Null
    }
    Write-Host "  $Src -> $Out"
    & gcc @CFLAGS $Src -o $Out @Extra
    return ($LASTEXITCODE -eq 0)
}

Write-Host "--- Building system processes ---" -ForegroundColor Cyan
Compile-One "system\prisc+x.c" "system\prisc+x" | Out-Null
Compile-One "system\keyboard_input.c" "system\keyboard_input" | Out-Null
Compile-One "system\renderer.c" "system\renderer" | Out-Null
Compile-One "system\orchestrator.c" "system\orchestrator" | Out-Null

Write-Host "--- Building chtpm_parser_pal ---" -ForegroundColor Cyan
# MinGW may not need the same -Wno flags; keep best-effort
& gcc @CFLAGS -Wno-unused-result "system\chtpm_parser_pal.c" -o "system\chtpm_parser_pal" 2>$null
if ($LASTEXITCODE -ne 0) {
    & gcc @CFLAGS "system\chtpm_parser_pal.c" -o "system\chtpm_parser_pal"
}

Write-Host "--- Building chtpm_rgb_render ---" -ForegroundColor Cyan
Compile-One "system\chtpm_rgb_render.c" "system\chtpm_rgb_render" | Out-Null

Write-Host "--- Building gl_mirror (best effort, freeglut) ---" -ForegroundColor Cyan
$glLog = Join-Path $env:TEMP "wsr_gl_mirror_build.log"
& gcc @CFLAGS -o "system\gl_mirror" "system\gl_mirror.c" @GL_FLAGS 2>$glLog
if ($LASTEXITCODE -eq 0) {
    Write-Host "    ok"
} else {
    Write-Host "    skipped (GLUT/GL not available - see $glLog)"
}

Write-Host "--- Building dump_rgb_png ---" -ForegroundColor Cyan
& gcc @CFLAGS -I"ops\lib" -o "ops\+x\dump_rgb_png.+x" "ops\dump_rgb_png.c" -lm 2>$null
if ($LASTEXITCODE -ne 0) {
    & gcc @CFLAGS -I"ops\lib" -o "ops\+x\dump_rgb_png.+x" "ops\dump_rgb_png.c"
}

Write-Host "--- Building ops ---" -ForegroundColor Cyan
Get-ChildItem "ops\*.c" | ForEach-Object {
    $name = $_.BaseName
    if ($name -eq "dump_rgb_png") { return }
    $out = "ops\+x\$name.+x"
    Write-Host "  Compiling $name..."
    & gcc @CFLAGS $_.FullName -o $out -lm
    if ($LASTEXITCODE -ne 0) {
        Write-Warning "FAILED $name"
    }
}

Write-Host "--- Build Complete ---" -ForegroundColor Green
Get-ChildItem "system\prisc+x*","ops\+x\*" -ErrorAction SilentlyContinue |
    Select-Object Name, Length | Format-Table -AutoSize
