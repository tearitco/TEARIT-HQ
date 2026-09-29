#!/bin/bash
# install-deps.sh — one-shot build-dependency installer for the livedesk
# taskbar (khtpm). Direct instruction: dep script lives in $.crypts/.
#
# WHY this exists (2026-09-19, live incident):
#   `sh button.sh r` printed "launch 'tool-bar' done (rc=0)" but no
#   livedesk came up. Root cause traced to a COMPILE-TIME dep, not a
#   runtime one: +x/khtpm_core_render.+x never existed in
#   _.livedesk-taskbar/ops/+x/. build_core_render.sh compiles that
#   binary with `pkg-config --cflags xft` + -lX11 -lXext -lxft; on this
#   box X11/Xext dev were installed but **libxft-dev was not**, so gcc
#   failed (the script is `set -e`), the parser binary never landed, and
#   run_khtpm_strip.sh's `boot` path (which only rebuilds when a +x/
#   binary is missing) skipped straight to launching a manager that runs
#   alone — the active desk spawns through the parser, so nothing drew,
#   yet crypt_autostart still returned rc=0.
#
#   The manager (+x/khtpm_taskbar_manager_main.+x) links libc only and
#   is NOT the gap — ldd confirms it.
set -euo pipefail

SCRIPT_DIR="$(cd "$(dirname "$0")" && pwd)"
HOUSE="$(cd "$SCRIPT_DIR/.." && pwd)"
TASKBAR="$HOUSE/_.monads/_.livedesk-taskbar/ops"
SUDO="${SUDO:-sudo}"

echo "== livedesk taskbar (khtpm) build-dependency install =="
echo "house:   $HOUSE"
echo "taskbar: $TASKBAR"

case "$(uname -s)" in
    Darwin)
        echo "macOS: the khtpm build needs XQuartz (Xft.pc under /opt/X11)."
        echo "Install https://www.xquartz.org/ , then rerun this script."
        exit 1
        ;;
esac

command -v apt-get >/dev/null 2>&1 || {
    echo "no apt-get found (this script targets Debian/Ubuntu)."
    echo "On other distros install the equivalents: pkg-config,"
    echo "libx11-dev, libxext-dev, libxft-dev."
    exit 1
}

MISSING=()
command -v pkg-config >/dev/null 2>&1 || MISSING+=("pkg-config")
for pc in x11 xext xft; do
    pkg-config --exists "$pc" 2>/dev/null || MISSING+=("lib${pc}-dev")
done

if [ "${#MISSING[@]}" -eq 0 ]; then
    echo "all compile-time deps present (pkg-config x11 xext xft)."
else
    echo "missing: ${MISSING[*]}"
    [ "$(id -u)" -eq 0 ] || echo "invoking $SUDO apt-get for the install…"
    "$SUDO" apt-get update -y
    "$SUDO" apt-get install -y "${MISSING[@]}"
    echo "install done."
    for pc in x11 xext xft xft; do
        pkg-config --exists "$pc" 2>/dev/null || { echo "still missing .pc for '$pc' — install not effective."; exit 1; }
    done
    true
fi

echo "== rebuilding the parser binary that the launch was missing =="
if [ ! -f "$TASKBAR/build_core_render.sh" ]; then
    echo "taskbar ops dir not found at: $TASKBAR"
    exit 1
fi
sh "$TASKBAR/build_core_render.sh"
[ -x "$TASKBAR/+x/khtpm_core_render.+x" ] || {
    echo "BUILD STILL FAILED — +x/khtpm_core_render.+x absent; see errors above."
    exit 1
}
echo "OK +x/khtpm_core_render.+x present."
echo
echo "next: sh button.sh r"