# build.ps1 - Windows twin of build_chtpm_rgb_render.sh (_shared-lib)
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

# build_chtpm_rgb_render.sh — the real, canonical chtpm_rgb_render.c
# source (2026-08-17, khtpm-merge-how2.md §5c.7, legacy-shared-fix.md
# §2). This is a SOURCE consolidation, not a shared-binary one: the 9
# real projects that had a byte-identical copy now symlink their own
# system/chtpm_rgb_render.c here and still build/launch their OWN local
# binary as before - only the .c file is shared, no launcher changes
# needed. This build script exists for reference/standalone compile
# checks only; each project's own scripts/build.sh or button.sh still
# does the real per-project compile.
New-Item -ItemType Directory -Force -Path "+x" | Out-Null
$CC = "if ($env:CC) { $env:CC } else { "gcc" }"
$CFLAGS = @("-std=c11", "-Wall", "-O2")

Write-Host "-- chtpm_rgb_render (canonical, symlinked by 9 real projects) -> +x/chtpm_rgb_render.+x"
& $CC @CFLAGS -o +x/chtpm_rgb_render.+x chtpm_rgb_render.c
Write-Host "OK +x/chtpm_rgb_render.+x"

# ===== NEXT SCRIPT =====

# build.ps1 - Windows twin of build_dump_frame_png_op.sh (_shared-lib)
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

# build_dump_frame_png_op.sh — build the real, standalone dump_frame_png_op
# binary (2026-08-16 TPMOS-standard correction). Real op binary, invoked
# via fork/exec or system() by any khtpm app - never text-included.
New-Item -ItemType Directory -Force -Path "+x" | Out-Null
$CC = "if ($env:CC) { $env:CC } else { "gcc" }"
$CFLAGS = @("-std=c11", "-Wall", "-O2")
$LIBS = @("-lX11", "-lm")

Write-Host "-- dump_frame_png_op -> +x/dump_frame_png_op.+x"
& $CC @CFLAGS -o +x/dump_frame_png_op.+x dump_frame_png_op.c @LIBS
Write-Host "OK +x/dump_frame_png_op.+x"

# ===== NEXT SCRIPT =====

# build.ps1 - Windows twin of build_x11_mirror.sh (_shared-lib)
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

# build_x11_mirror.sh — build the real, shared x11_mirror binary
# (2026-08-17, khtpm-merge-how2.md §5c.6, legacy-shared-fix.md §3).
# ONE compiled binary, launched by every legacy-GL project
# (mutaclysm/piececraft-xyz/my-chara-txt so far) in place of each
# project's own separate gl_mirror.c copy - see x11_mirror.c's own
# header comment for the real parameterization approach (window title
# derived from basename(project_root), no new config needed).
New-Item -ItemType Directory -Force -Path "+x" | Out-Null
$CC = "if ($env:CC) { $env:CC } else { "gcc" }"
$CFLAGS = @("-std=c11", "-Wall", "-O2")
$LIBS = @("-I/usr/include/freetype2", "-lX11", "-lXft")

Write-Host "-- x11_mirror -> +x/x11_mirror.+x"
& $CC @CFLAGS -o +x/x11_mirror.+x x11_mirror.c @LIBS
Write-Host "OK +x/x11_mirror.+x"
