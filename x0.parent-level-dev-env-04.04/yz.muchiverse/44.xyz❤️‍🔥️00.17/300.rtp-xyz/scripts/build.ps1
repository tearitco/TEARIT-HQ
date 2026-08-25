# build.ps1 - Windows twin of build.sh (300.rtp-xyz)
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

# Builds every binary independently - no shared object files, no shared
# headers, matching cdda-tpm-std-fast.txt sec. 1. Each translation unit
# is compiled and linked on its own line.
#
# LOCAL COPIES, NOT A LIVE SHARED_OPS REFERENCE: this project keeps its
# own real, local copy of every file below that also exists in
# yz.muchiverse/2.muchi-verse/shared-ops/ (system/prisc+x.c,
# system/keyboard_input.c, system/chtpm_parser_pal.c,
# system/chtpm_rgb_render.c, ops/pdl_reader.c, ops/dump_rgb_png.c,
# ops/lib/) - direct user instruction: "i dont wanna use shared ops, in
# the classic sense... all code should be self independant and solo
# shippable." This build.sh never reaches outside this project's own
# directory. To pull in an update from the canonical source, run (from
# yz.muchiverse/2.muchi-verse/):
#   bash sync_shared_op.sh <op_name> <target_dir>
# (see that directory's own shared-ops-manifest.txt for available
# op_name values and this project's own consumer list) - a deliberate,
# explicit step, never automatic at build time.

# Resolve shared-lib canonical source (symlink-free)
_SS = Split-Path -Parent (Split-Path $SCRIPT_DIR -Parent)
$_SS = Join-Path $_SS "&.widgits/_shared-lib"
