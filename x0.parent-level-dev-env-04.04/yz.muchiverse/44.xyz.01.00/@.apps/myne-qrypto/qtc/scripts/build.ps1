# build.ps1 - Windows twin of build.sh (qtc)
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

# scripts/build.sh - compile everything, warning-free.
#
# LOCAL COPIES, NOT A LIVE SHARED_OPS REFERENCE: this project keeps its
# own real, local copy of every file below that also exists in
# yz.muchiverse/2.muchi-verse/shared-ops/ (system/prisc+x.c,
# system/keyboard_input.c, system/chtpm_parser_pal.c,
# system/chtpm_rgb_render.c, ops/palnet_peer.c) - same convention every
# other project in this family follows (see ../shared-ops-manifest.txt).
# To pull in an update from the canonical source, run (from
# yz.muchiverse/2.muchi-verse/):
#   bash sync_shared_op.sh <op_name> pal-chain/<target_dir>
#
# Real crypto: chain_create_wallet.c/chain_login.c/chain_miner.c/
# chain_inbox_watcher.c link OpenSSL's libcrypto for real SHA-256
# (PAL-CHAIN-STANDARD.txt sec. 0/1/2) - `-lcrypto` required on those
# four translation units.

New-Item -ItemType Directory -Force -Path "ops/+x system" | Out-Null

$CFLAGS = @("-Wall", "-Wextra", "-O2")

Write-Host "--- Building system processes ---"
& gcc @CFLAGS "system/prisc+x.c" -o "system/prisc+x"
& gcc @CFLAGS "system/keyboard_input.c" -o "system/keyboard_input"
& gcc @CFLAGS "system/renderer.c" -o "system/renderer"

Write-Host "--- Building chtpm_parser_pal (PERSISTENT process, -Wno-unused-result"
Write-Host "    -Wno-stringop-truncation required - see shared-ops/chtpm_parser_pal.c)"
& gcc @CFLAGS -Wno-unused-result -Wno-stringop-truncation "system/chtpm_parser_pal.c" -o "system/chtpm_parser_pal"

Write-Host "--- Building chtpm_rgb_render (local copy, PERSISTENT daemon) ---"
& gcc @CFLAGS "system/chtpm_rgb_render.c" -o "system/chtpm_rgb_render"

Write-Host "--- Building palnet_peer (local copy - reusable P2P op, see"
Write-Host "    ../PAL-NET-STANDARD.txt) ---"
& gcc @CFLAGS -o "ops/+x/palnet_peer.+x" "ops/palnet_peer.c"

Write-Host "--- Building pal-chain ops (real SHA-256 via OpenSSL) ---"
& gcc @CFLAGS -o "ops/+x/chain_create_wallet.+x" "ops/chain_create_wallet.c" -lcrypto
& gcc @CFLAGS -o "ops/+x/chain_login.+x" "ops/chain_login.c" -lcrypto
& gcc @CFLAGS -o "ops/+x/chain_balance.+x" "ops/chain_balance.c"
& gcc @CFLAGS -o "ops/+x/chain_send.+x" "ops/chain_send.c"
& gcc @CFLAGS -o "ops/+x/chain_miner.+x" "ops/chain_miner.c" -lcrypto
& gcc @CFLAGS -o "ops/+x/chain_inbox_watcher.+x" "ops/chain_inbox_watcher.c" -lcrypto
& gcc @CFLAGS -o "ops/+x/chain_menu_input.+x" "ops/chain_menu_input.c"
& gcc @CFLAGS -o "ops/+x/chain_compose_frame.+x" "ops/chain_compose_frame.c"

Write-Host "build ok"
