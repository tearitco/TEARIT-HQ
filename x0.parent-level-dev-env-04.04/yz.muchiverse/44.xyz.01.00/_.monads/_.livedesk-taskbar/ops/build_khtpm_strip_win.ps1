# build_khtpm_strip_win.ps1 - Windows twin of build_khtpm_strip.sh.
# ASCII only.
#
# WHY THIS FILE EXISTS (2026-09-26): ops/build.ps1 is a machine
# translation of the four Linux build scripts and cannot run. It is not
# merely unidiomatic, it is broken in ways that stop it dead:
#   - $CC = "if ($env:CC) { $env:CC } else { "gcc" }"  is a bash snippet
#     assigned to a PowerShell string, so `& $CC` tries to execute that
#     whole sentence as a program name.
#   - if (([ "$env:OS" = "Darwin")])  is bash if-syntax inside PowerShell.
#   - it passes $(pkg-config --cflags xft) and -lX11, neither of which
#     exists on this platform.
#   - its $SHARED path joins ../../../ from _.monads, which overshoots
#     the house root by one level.
# It also still builds the RETIRED khtpm_strip_parser.c stack. This file
# builds the CURRENT one: khtpm_core_render.c plus the Win32 X11 shim.
#
# WHAT ACTUALLY CHANGED ON WINDOWS, in one paragraph: the canonical
# renderer is portable by construction - it has no _WIN32 blocks and
# reaches for X11/Xft only through #include, so the source compiles
# UNMODIFIED. What it could not do was find those headers, because
# MinGW-w64 ships no X11. So this build points the include path at
# ops/win-compat/ first, and that directory funnels every X11/Xft/
# extension header to khtpm_strip_x11_win.c, the self-contained Win32
# backend that already existed. Three .c files come along for the ride:
# the X11 shim, a small POSIX shim, and khtpm_win_compat.c.
#
# Two flags here are load-bearing and were each found by a real failure:
#   -include win-compat/khtpm_win_compat_prelude.h
#       MinGW's headers trip over the POSIX identifiers the canonical
#       source uses before our own shims get a say.
#   NOT -municode
#       khtpm_core_render.c's entry point is `int main(int argc, char
#       **argv)`, the narrow one. -municode rewrites the CRT to expect
#       wmain, which then mismatch-reports every char* as LPCWSTR.
#
# See ..\..\..\#.#.calendar-dox\!.HQ-IQ-BOOK\09-appendix\
# WINDOWS-TASKBAR-PORT.md for the port notes this file belongs to.

$ErrorActionPreference = "Continue"
$SCRIPT_DIR = Split-Path -Parent $MyInvocation.MyCommand.Path

$MSYS = "C:\msys64\mingw64\bin"
if (-not (Get-Command gcc -ErrorAction SilentlyContinue)) {
    if (Test-Path -LiteralPath (Join-Path $MSYS "gcc.exe")) {
        $env:Path = "$MSYS;$env:Path"
    } else {
        Write-Error "gcc not found. Install MSYS2 MinGW64 (mingw-w64-x86_64-gcc)."
        exit 1
    }
}

# ---- paths, all absolute so the build cannot depend on the caller's cwd
$OPS      = $SCRIPT_DIR
$TASKBAR  = Split-Path -Parent $OPS

# House discovery by walking UP for a dir holding both #.desktop/ and
# &.widgits/ - the same test khtpm_vars.sh's khtpm_find_house() makes, and
# the reason this file does not hardcode a "how many levels up is the
# house root" number. Counting levels is how the first attempt at this
# script got .monads instead of the house root.
$HOUSE = $null
$d = $OPS
while ($d -and ($d -ne "/")) {
    if ((Test-Path -LiteralPath (Join-Path $d "#.desktop")) -and
        (Test-Path -LiteralPath (Join-Path $d "&.widgits"))) { $HOUSE = $d; break }
    $d = Split-Path -Parent $d
}
if (-not $HOUSE) {
    Write-Error "house root not found above $OPS (need both #.desktop/ and &.widgits/)"
    exit 1
}

$SHARED   = Join-Path $HOUSE "&.widgits\_shared-lib"
$WCOMPAT  = Join-Path $OPS "win-compat"
$OUTDIR   = Join-Path $OPS "+x"

if (-not (Test-Path -LiteralPath (Join-Path $HOUSE "#.desktop"))) {
    Write-Error "house root looks wrong: $HOUSE"
    exit 1
}
if (-not (Test-Path -LiteralPath $SHARED)) {
    Write-Error "shared lib dir missing: $SHARED"
    exit 1
}

New-Item -ItemType Directory -Force -Path $OUTDIR | Out-Null

$CFLAGS = @("-std=c11", "-O2", "-w")

# -mwindows is load-bearing, not cosmetic (direct report 2026-09-26:
# "the cli's are still opening on screen"). A MinGW binary is CONSOLE
# subsystem by default, so Windows allocates a console for it the moment
# it starts - no spawn flag can prevent that, and WMI cannot even set a
# child's std handles. -mwindows sets the PE subsystem to Windows, so no
# console is ever allocated. It also makes the CRT look for WinMain
# instead of main, which is what khtpm_gui_entry_win.c provides; the
# canonical sources keep their own narrow main() and stay unedited.
$GUI = @("-mwindows")
$GUISRC = (Join-Path $OPS "khtpm_gui_entry_win.c")

# The include order IS the port. win-compat comes first so that
# <X11/Xlib.h> resolves to our shim rather than to any real (absent)
# X11, and the bare shared-module names resolve to the one canonical
# copy in &.widgits/_shared-lib.
$INCS = @(
    "-I", $OPS,
    "-I", $WCOMPAT,
    "-I", $SHARED,
    "-I", (Join-Path $OPS "lib"),
    "-include", (Join-Path $WCOMPAT "khtpm_win_compat_prelude.h")
)

# Win32 libraries the shim actually needs. -lX11/-lXft/-lXext are the
# Linux set and have no meaning here. -lm is for the cos()/sin() in the
# shim's XDrawArc, which places X11's arc end angles onto the ellipse.
# Windows gives a thread 1 MB of stack by default; Linux gives 8 MB. The
# manager never noticed the difference on Linux because its own address-book
# arrays are larger than a Windows stack: khtpm_taskbar_manager.h sets
# KTB_PATH_BUF 4352 and KTB_LIVEDESK_MAX_OPEN 64, and six functions each hold
# a `char paths[64][4352]` on the stack - 272 KB apiece, ~280 KB with `ents`.
# The spawn path NESTS three of them:
#     livedesk_spawn_active_desk -> livedesk_spawn_desk -> livedesk_ensure_cursword
# which is ~840 KB before main()'s own frames, so the manager died with
# 0xC00000FD (STACK_OVERFLOW) on this platform - before it could write a
# single byte of #.desktop/strip_ui.txt, which is why the bottom bar stayed
# empty. Confirmed by gdb backtrace (chkstk_ms -> livedesk_ensure_cursword).
#
# The fix is the linker's stack reservation, not a rewrite of six canonical
# call sites onto the heap: 8 MB, matching the Linux default, so the code
# keeps the stack footprint it was written against.
$STACK = @("-Wl,--stack,0x800000")

$LIBS = @("-lgdi32", "-luser32", "-lshlwapi", "-lshell32",
          "-ladvapi32", "-lcomctl32", "-lcomdlg32", "-lm")

$SOURCES = @(
    (Join-Path $OPS "khtpm_core_render.c"),
    # Linked, not included: khtpm_core_render.c includes khtpm_css_parser.h
    # for its declarations but calls css_load()/css_compute_style(), which
    # are defined here. The Linux link line does the same.
    (Join-Path $SHARED "khtpm_css_parser.c"),
    (Join-Path $OPS "khtpm_strip_x11_win.c"),
    (Join-Path $OPS "khtpm_strip_posix_win.c"),
    (Join-Path $OPS "khtpm_win_compat.c")
)

$RENDER_OUT = Join-Path $OUTDIR "khtpm_core_render.exe"
$MANAGER_OUT = Join-Path $OUTDIR "khtpm_taskbar_manager_main.exe"
$ENTITY_OUT = Join-Path $OUTDIR "khtpm_entity.exe"

Write-Host "-- khtpm manager driver (pure logic, no Xlib) -> +x/khtpm_taskbar_manager_main.exe"
# -I $SHARED because khtpm_taskbar_manager.c #includes kh_proc_registry.h,
# which lives in &.widgits/_shared-lib (PROC-LIFECYCLE-ORCHESTRATOR-
# TEARDOWN.md). Same reason the Linux line at build_khtpm_strip.sh:141
# carries it. No X11 and no win-compat on this leg: the manager never
# opens a window, it only reads and writes plain files. It is also the one
# leg that needs $STACK - see the note there.
& gcc $CFLAGS @GUI $STACK -I $SHARED -o $MANAGER_OUT `
    $GUISRC `
    (Join-Path $OPS "khtpm_taskbar_manager_main.c") `
    (Join-Path $OPS "khtpm_taskbar_manager.c")
if ($LASTEXITCODE -ne 0) {
    Write-Error "manager build FAILED (exit $LASTEXITCODE) - not launching the bars"
    exit 1
}

Write-Host "-- khtpm core renderer (canonical source + Win32 X11 shim) -> +x/khtpm_core_render.exe"
& gcc @CFLAGS @GUI @INCS -o $RENDER_OUT $GUISRC @SOURCES @LIBS
if ($LASTEXITCODE -ne 0) {
    Write-Error "renderer build FAILED (exit $LASTEXITCODE) - not launching the bars"
    exit 1
}

# khtpm_entity is a SEPARATE binary from the renderer, and this is not a
# porting convenience - it is what the canonical source itself insists
# on. khtpm_core_render.c's main() rejects argc==2 outright and prints
# "pal/tile process is khtpm_entity.+x <package_dir>". So the entities
# you see on the desktop are one khtpm_entity process per pal, invoked
# with the pal's own package_dir, and each renders its own window.
#
# STATUS: this leg now COMPILES and LINKS. It did not before, and for a
# reason worth writing down: the entity leg was missing the whole Win32 X11
# shim from its link line. It listed only khtpm_entity.c, so every Xlib
# symbol the entity calls - all 40-odd of them, XCreateWindow, XftFontOpenName,
# XShapeCombineMask, the lot - came back "undefined reference" and the leg
# was written off as "needs a wider Xlib slice". The slice was not the
# problem: khtpm_strip_x11_win.c already implements the entity's calls, it was
# simply never passed to the linker. That shim grew the missing pieces
# (XReparentWindow, XMatchVisualInfo, XCreateBitmapFromData, XDrawArc,
# XQueryPointer, the extra keysyms, ShapeUnion) as part of this port, and
# khtpm_win_compat.c grew flock/readlink/sigaction/win_package_rel.
#
# So the entity gets the same three shim sources the renderer does. It does
# NOT get khtpm_core_render.c or the CSS parser: the entity needs neither,
# matching the Linux line (build_core_render.sh).
#
# A failure here is still reported loudly but does NOT block the taskbar from
# launching: a broken entity build is a smaller problem than a taskbar that
# refuses to start.
$ENTITY_SOURCES = @(
    (Join-Path $OPS "khtpm_entity.c"),
    (Join-Path $OPS "khtpm_strip_x11_win.c"),
    (Join-Path $OPS "khtpm_strip_posix_win.c"),
    (Join-Path $OPS "khtpm_win_compat.c")
)
Write-Host "-- khtpm entity pal renderer -> +x/khtpm_entity.exe"
& gcc @CFLAGS @GUI @INCS -o $ENTITY_OUT $GUISRC @ENTITY_SOURCES @LIBS
if ($LASTEXITCODE -ne 0) {
    Write-Warning "ENTITY BUILD FAILED (exit $LASTEXITCODE) - bars will launch with NO entities."
    Write-Warning "  Continuing so the taskbar still starts. See WINDOWS-TASKBAR-PORT.md."
    $script:EntityBuilt = $false
} else {
    $script:EntityBuilt = $true
}

# Report size, because "it compiled" is not evidence it produced a real
# binary - a truncated or stub output still exits 0 on some toolchains.
foreach ($o in @($RENDER_OUT, $MANAGER_OUT)) {
    if (Test-Path -LiteralPath $o) {
        $len = (Get-Item -LiteralPath $o).Length
        Write-Host ("   OK {0} ({1} bytes)" -f (Split-Path -Leaf $o), $len)
        if ($len -lt 20000) {
            Write-Error "  suspiciously small output - treat as a failed build"
            exit 1
        }
    } else {
        Write-Error "  missing output: $o"
        exit 1
    }
}

# The entity binary is optional-but-reported (see its own block above).
if (Test-Path -LiteralPath $ENTITY_OUT) {
    Write-Host ("   OK {0} ({1} bytes)" -f (Split-Path -Leaf $ENTITY_OUT), (Get-Item -LiteralPath $ENTITY_OUT).Length)
}

Write-Host "OK +x/khtpm_core_render.exe and +x/khtpm_taskbar_manager_main.exe"
exit 0
