# build.ps1 - Windows twin of build.sh (my-lawyer)
# ASCII only.

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

# scripts/build.sh - compile everything, warning-free where possible.
# Modeled directly on my-biotech's own scripts/build.sh.

New-Item -ItemType Directory -Force -Path "ops/+x system" | Out-Null

$CFLAGS = @("-Wall", "-Wextra", "-O2")

Write-Host "--- Building system processes ---"
& gcc @CFLAGS "system/prisc+x.c" -o "system/prisc+x"
& gcc @CFLAGS "system/keyboard_input.c" -o "system/keyboard_input"
& gcc @CFLAGS "system/renderer.c" -o "system/renderer"

Write-Host "--- Building chtpm_parser_pal ---"
& gcc @CFLAGS -Wno-unused-result -Wno-stringop-truncation "system/chtpm_parser_pal.c" -o "system/chtpm_parser_pal"

Write-Host "--- Building chtpm_rgb_render ---"
& gcc @CFLAGS "system/chtpm_rgb_render.c" -o "system/chtpm_rgb_render"

Write-Host "--- Building orchestrator ---"
& gcc @CFLAGS -o "system/orchestrator" "system/orchestrator.c"

Write-Host "--- Building my-lawyer ops ---"
& gcc @CFLAGS -o "ops/+x/mylawyer_menu_input.+x" "ops/mylawyer_menu_input.c"
& gcc @CFLAGS -o "ops/+x/mylawyer_compose_frame.+x" "ops/mylawyer_compose_frame.c"
& gcc @CFLAGS -o "ops/+x/mylawyer_case_worker.+x" "ops/mylawyer_case_worker.c"
& gcc @CFLAGS -o "ops/+x/mylawyer_judge_worker.+x" "ops/mylawyer_judge_worker.c"
& gcc @CFLAGS -o "ops/+x/connect_op.+x" "ops/connect_op.c"
& gcc @CFLAGS -o "ops/+x/json_parser.+x" "ops/json_parser.c"

Write-Host "build ok"
