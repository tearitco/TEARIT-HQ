# build.ps1 - Windows twin of build_lc_clock.sh (livedesk-clock)
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

# build_lc_clock.sh — build the livedesk-clock system (au11-hq/15.clock-design.md §5 P1).
#
# Two binaries:
#   +x/lc_clock.+x              headless daemon + control plane (no Xlib)
#   +x/lc_reminder_popup.+x     X11 RGB reminder window + CSS (house standard)
#
# Mirrors the khtpm strip build style (build_khtpm_strip.sh): cd to ops,
# mkdir +x, CC=gcc CFLAGS="-std=c11 -Wall -O2", Xft via pkg-config.
New-Item -ItemType Directory -Force -Path "+x" | Out-Null
$CC = "if ($env:CC) { $env:CC } else { "gcc" }"
$CFLAGS = @("-std=c11", "-Wall", "-O2")

# Sync the shared CSS parser from the single canonical source (same
# build-time-copy rule as build_khtpm_strip.sh — see
# &.widgits/_shared-lib/README.md). The popup is an X11 RGB window
# styled by this parser (house standard, NOT GL — see the header note
# in lc_reminder_popup.c).
$SHARED = Join-Path (Split-Path -Parent $SCRIPT_DIR) "../../_shared-lib"
Copy-Item -LiteralPath "$SHARED/khtpm_css_parser.c" -Destination "khtpm_css_parser.c" -Force
Copy-Item -LiteralPath "$SHARED/khtpm_css_parser.h" -Destination "khtpm_css_parser.h" -Force

Write-Host "-- lc_clock (daemon + control plane, headless) -> +x/lc_clock.+x"
& $CC @CFLAGS -o +x/lc_clock.+x lc_clock.c

Write-Host "-- lc_reminder_popup (X11 RGB window + CSS) -> +x/lc_reminder_popup.+x"
& $CC @CFLAGS $(pkg-config --cflags xft) -o +x/lc_reminder_popup.+x \
  lc_reminder_popup.c khtpm_css_parser.c -lX11 $(pkg-config --libs xft)

Write-Host "OK +x/lc_clock.+x and +x/lc_reminder_popup.+x"
