#!/bin/bash
# harness.sh — network-browser-normal presentation harness (K9 bash, not python)
# House: AIGENT-TESTING-K9.txt + PRESENTATION-VIDEO-PIPELINE.md
# Branch: opencode @ 4c652af7..ef70042f parity 0 0, make nbjs GREEN, wcs 12/12
# One self-contained bash harness per feature — copy-paste-and-adapt, not sourced.

set -euo pipefail
R="$(git rev-parse --show-toplevel 2>/dev/null || pwd)"
if [ -z "$R" ] || [ ! -d "$R/x0.parent-level-dev-env-04.04" ]; then
  R="/home/jb/Desktop/ele-desk+cal/0.opencode-desk/tearit-hq"
fi
FEATURE_DIR="$(cd "$(dirname "$0")" && pwd)"
SNAPDIR="$FEATURE_DIR/snapshots"
MANIFEST="$FEATURE_DIR/manifest.txt"
REPRODUCE="$FEATURE_DIR/REPRODUCE.md"

usage() { echo "usage: $0 [--dry-run] [--no-tts]"; exit 1; }
DRY_RUN=0; NO_TTS=""
for a in "$@"; do case "$a" in --dry-run) DRY_RUN=1;; --no-tts) NO_TTS="--no-tts";; *) usage;; esac; done

echo "==> check zero stray processes (K9 Rule 1)"
if pgrep -f "network_browser_manager" >/dev/null 2>&1; then
  echo "ERR: stray network_browser_manager still running — kill it first"; exit 1
fi

check_text_state() {
  local f="$R/x0.parent-level-dev-env-04.04/yz.muchiverse/44.xyz.01.00/&.hq-apps/network/tmp/page.state.txt"
  [ -f "$f" ] && head -20 "$f" || echo "no PAGE_STATE at $f"
}

RELAY="$HOME/.config/tearit-hq/network_browser_history.txt"
ALT_RELAY="$R/x0.parent-level-dev-env-04.04/yz.muchiverse/44.xyz.01.00/&.hq-apps/network/tmp/history.txt"
[ -f "$RELAY" ] || [ ! -f "$ALT_RELAY" ] || RELAY="$ALT_RELAY"

if [ "$DRY_RUN" = 1 ]; then
  echo "DRY RUN — would drive: relay=$RELAY manifest=$MANIFEST"
  cat "$MANIFEST"
  check_text_state
  exit 0
fi

echo "==> launch Network Browser (real X11 display required)"
echo "    Run: sh 44.xyz.01.00/&.hq-apps/network/button.sh run"
echo "    Then re-run this harness without --dry-run to capture"
echo "    Snapshots via: dump_frame_png_op --root snapshots/0X_*.png  # PIPELINE:35 never scrot"
echo "    Verify: page.state.txt contains TEXT/LINK/IMG|...|1|1|/tmp/nb_img_*.png"

if [ -f "$MANIFEST" ] && [ -d "$SNAPDIR" ]; then
  echo "==> manifest exists, snapshots dir exists ($(ls -1 "$SNAPDIR" 2>/dev/null | wc -l) files)"
  if ls "$SNAPDIR"/*.png 1>/dev/null 2>&1; then
    echo "    snapshots present — building video"
    if command -v make_presentation_video.py >/dev/null 2>&1; then
      make_presentation_video.py "$FEATURE_DIR" --width 1280 $NO_TTS || echo "video build failed (need ffmpeg/edge_tts)"
    else
      echo "    make_presentation_video.py not on PATH — copy from xyzfs/.../presentations/make_presentation_video.py"
    fi
  else
    echo "    no PNGs yet — fill snapshots/ via live capture first"
  fi
fi
echo "==> REPRODUCE.md at $REPRODUCE"
echo "done"
