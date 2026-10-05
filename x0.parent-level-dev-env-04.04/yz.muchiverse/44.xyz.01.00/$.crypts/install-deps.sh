#!/bin/bash
# install-deps.sh — one-shot build+runtime dependency installer for the
# livedesk taskbar (khtpm). Direct instruction: dep script lives in $.crypts/.
#
# WHY the font is in here too (2026-10-04, Debian 12 port):
#   Porting this house from Ubuntu to a minimal Debian install produced a
#   livedesk that built clean, launched clean, reported rc=0 for every LAUNCH
#   row, and drew every window correctly - with one exception: every CJK
#   glyph came out blank/tofu. Cause was not code and not the build: the
#   house hardcodes the literal family name "Noto Sans CJK SC" in at least
#   six places (khtpm_core_render.c reload_font_ui(), khtpm_entity.c,
#   khtpm_strip_parser.c, entity_menu_default.css, livedesk-clock,
#   board-viewer). Ubuntu ships fonts-noto-cjk by default; minimal Debian
#   does not, so XftFontOpenName() on that family silently fell back and
#   every CJK codepoint had no glyph to draw. Checked by family name below,
#   because that exact string is what the code asks Xft for - some *other*
#   CJK font being installed is not sufficient.
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

# WHY the video ops deps are in here too (2026-10-04, Debian 12 port):
#   The network-browser build script silently skips +x/nb_video_play.+x
#   (the V3 real-time libav video op) whenever the AV/ALSA dev packages
#   are absent - "skip nb_video_play (no libav/alsa dev)" scrolled by
#   with rc=0 and nobody noticed until playback just didn't work. Same
#   class of silent gap as the CJK font below: build "succeeds", feature
#   silently missing. Probing the same pkg-config set build.sh gates on.
# Maps each .pc to its Debian dev package (alsa's is libasound2-dev, not
# alsa-dev - that mismatch is exactly why a naive lib${pc}-dev loop can't
# be reused here).
VIDEO_PCS="libavformat libavcodec libavutil libswscale libswresample alsa"

MISSING=()
command -v pkg-config >/dev/null 2>&1 || MISSING+=("pkg-config")
for pc in x11 xext xft; do
    pkg-config --exists "$pc" 2>/dev/null || MISSING+=("lib${pc}-dev")
done
for pc in $VIDEO_PCS; do
    pkg-config --exists "$pc" 2>/dev/null && continue
    case "$pc" in
        alsa) MISSING+=("libasound2-dev") ;;
        *)    MISSING+=("${pc}-dev") ;;
    esac
done
# RUNTIME font dep, deliberately not a compile-time one: nothing here fails
# to build without it, so pkg-config/.pc probing cannot see it. See the
# header's WHY block - the symptom (blank CJK glyphs) is completely
# disconnected from the thing that fixes it (an apt package).
if ! fc-match "Noto Sans CJK SC" 2>/dev/null | grep -q "Noto Sans CJK SC"; then
    MISSING+=("fonts-noto-cjk")
fi

if [ "${#MISSING[@]}" -eq 0 ]; then
    echo "all compile-time deps present (pkg-config x11 xext xft $VIDEO_PCS)."
    echo "runtime font dep present (Noto Sans CJK SC)."
else
    echo "missing: ${MISSING[*]}"
    [ "$(id -u)" -eq 0 ] || echo "invoking $SUDO apt-get for the install…"
    "$SUDO" apt-get update -y
    "$SUDO" apt-get install -y "${MISSING[@]}"
    echo "install done."
    for pc in x11 xext xft $VIDEO_PCS; do
        pkg-config --exists "$pc" 2>/dev/null || { echo "still missing .pc for '$pc' — install not effective."; exit 1; }
    done
    # Re-verify by family name, the same way it was detected. The
    # fonts-noto-cjk postinst normally rebuilds the cache, but "the package
    # is installed" and "the exact family Xft will be asked for now
    # resolves" are different claims - only the second means glyphs draw.
    fc-match "Noto Sans CJK SC" 2>/dev/null | grep -q "Noto Sans CJK SC" || {
        echo "still no 'Noto Sans CJK SC' family - CJK glyphs will not render."
        echo "try: sudo fc-cache -f"
        exit 1
    }
    echo "OK Noto Sans CJK SC resolves."
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