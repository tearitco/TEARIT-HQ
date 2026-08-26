#!/bin/bash
# bookmark_badge_contrast_fix_harness.sh - real proof-presentation harness
# for cursword's bookmark badge contrast fix, in the house's REAL
# harness shape (bash, same send_code/send_digits contract as
# #.desktop/harnesses/db-hq/nav.sh) - direct instruction 2026-08-25:
# "the harnesses aren't supposed to be in python (they should be in
# bash/pal c like the other harnesses)... we should use a typical
# harness of the kind that already exist." Ported from db-hq/nav.sh's
# own send_code/send_digits/send_char shape (reviewed in full before
# writing this), not reinvented - this house's own standing convention
# for harnesses is a self-contained per-feature script, not a sourced
# shared library (checked: no existing harness under %.harnesses/ or
# #.desktop/harnesses/ sources a common lib - each is copy-paste-and-
# adapt from the closest existing one, same as this file itself).
#
# What this does, end to end, zero manual steps:
#   1. Confirms zero stray processes for the bookmarks window.
#   2. Launches cursword's real bookmarks window (bm_menu.sh).
#   3. Dumps a real frame via the universal 'p' debug key.
#   4. Crops the nav badge for a close-up (python3 one-liner, PIL - same
#      library the presentation video tool already depends on, not a
#      new dependency).
#   5. Closes the window via its own real Close row + falls back to a
#      hard kill if it doesn't exit cleanly - zero stray processes after,
#      always, even on failure (trap-based).
#   6. Writes manifest.txt + REPRODUCE.md and calls the (Python,
#      unchanged - only the HARNESS/test-driver part moved to bash, per
#      the direct instruction) make_presentation_video.py to render the
#      final YouTube-ready .mp4 + yt-summary.txt.
#
# Usage: HOUSE=<house_root> bash bookmark_badge_contrast_fix_harness.sh
#        (HOUSE defaults to walking up from this file to the real
#        "44.xyz*" house root, same auto-detection the .py version used)

set -u

HERE="$(cd "$(dirname "$0")" && pwd)"

find_house_root() {
  local d="$HERE"
  while [ "$d" != "/" ]; do
    case "$(basename "$d")" in
      44.xyz*) echo "$d"; return 0 ;;
    esac
    d="$(dirname "$d")"
  done
  echo "bookmark_badge_contrast_fix_harness: could not find house root above $HERE" >&2
  exit 1
}

HOUSE="${HOUSE:-$(find_house_root)}"
PAL="$HOUSE/xyzfs/users/0a9558a7-7c74-4358-833c-2d5b21edc421/home/livedesk/pals/cursword"
RELAY="$HOUSE/#.desktop/db_hq_history.txt"
DUMP_PNG="/tmp/db-hq-frame.png"
PROC_PATTERN="khtpm_entity_menu_render\.\+x.*bookmarks\.chtpm"
MGR_PATTERN="bookmarks_manager\.\+x.*cursword"

FEATURE_DIR="$HERE/../presentations/bookmark-badge-contrast-fix-via-bash-harness"
SNAP_DIR="$FEATURE_DIR/snapshots"
mkdir -p "$SNAP_DIR"

send_code() {
  echo "$1" >> "$RELAY"
  sleep 0.2
}

send_digits() {
  local n="$1" i c
  for ((i = 0; i < ${#n}; i++)); do
    c="${n:$i:1}"
    echo -n "${c}" | od -An -tu1 | tr -d ' ' >> "$RELAY"
    printf '\n' >> "$RELAY"
    sleep 0.05
  done
}

dump_frame() {
  # universal 'p' debug key (ASCII 112) - every khtpm/-hq window's
  # dump_frame_png(), same contract as every other harness in this house
  send_code 112
  sleep 1.5
  if [ ! -f "$DUMP_PNG" ]; then
    echo "dump_frame: expected $DUMP_PNG but it doesn't exist" >&2
    exit 1
  fi
}

any_pids() {
  pgrep -f "$1" 2>/dev/null || true
}

cleanup() {
  # always runs, success or failure (trap) - the house's own
  # single-instance-guard standing rule applies even on our own errors
  send_code 53   # '5' - Close row (this window's own real Close index)
  sleep 0.3
  send_code 13   # Enter
  sleep 1
  local remaining
  remaining="$(any_pids "$PROC_PATTERN") $(any_pids "$MGR_PATTERN")"
  remaining="$(echo "$remaining" | tr ' ' '\n' | grep -v '^$' || true)"
  if [ -n "$remaining" ]; then
    echo "cleanup: force-killing leftover PIDs: $remaining"
    echo "$remaining" | xargs -r kill -TERM
    sleep 1
  fi
}
trap cleanup EXIT

echo "=== step 1: zero stray processes ==="
existing="$(any_pids "$PROC_PATTERN") $(any_pids "$MGR_PATTERN")"
existing="$(echo "$existing" | tr ' ' '\n' | grep -v '^$' || true)"
if [ -n "$existing" ]; then
  echo "bookmark_badge_contrast_fix_harness: stray process(es) already running: $existing" >&2
  echo "kill them first (house standing rule) - not doing it automatically since" >&2
  echo "another test may legitimately own them" >&2
  exit 1
fi

echo "=== step 2: launch bookmarks window ==="
setsid nohup sh "$HOUSE/&.widgits/bookmarks/bm_menu.sh" "$HOUSE" "$PAL" \
  >/tmp/bm-bash-harness-launch.log 2>&1 < /dev/null &
disown
sleep 1.5
shell_n="$(any_pids "$PROC_PATTERN" | grep -c . || true)"
mgr_n="$(any_pids "$MGR_PATTERN" | grep -c . || true)"
if [ "$shell_n" != "1" ] || [ "$mgr_n" != "1" ]; then
  echo "expected exactly 1 shell + 1 manager, got shell=$shell_n mgr=$mgr_n" >&2
  exit 1
fi

echo "=== step 3: dump real frame ==="
dump_frame
cp "$DUMP_PNG" "$SNAP_DIR/01_full_window.png"

echo "=== step 4: crop the badge close-up ==="
python3 - "$DUMP_PNG" "$SNAP_DIR/02_badge_zoom.png" <<'PYEOF'
import sys
from PIL import Image
src, dst = sys.argv[1], sys.argv[2]
im = Image.open(src)
im.crop((0, 135, 120, 160)).resize((720, 150)).save(dst)
PYEOF

echo "=== step 5: write manifest + REPRODUCE.md ==="
cat > "$FEATURE_DIR/manifest.txt" <<'EOF'
01_full_window.png | 6 | Cursword's real bookmarks window, live - captured by a real BASH harness (bookmark_badge_contrast_fix_harness.sh), same send_code/send_digits shape as db-hq/nav.sh, not a hand-run relay session.
01_full_window.png | 6 | The bug: the focused nav badge was hardcoded orange, rendered directly on the row's own gold background - almost unreadable.
02_badge_zoom.png | 7 | Zoomed crop of the badge after the fix - badge_focus_color() in khtpm_draw_core.c picks a dark contrasting color instead of orange when the row's own background is light.
02_badge_zoom.png | 6 | This entire presentation - launch, frame dump, crop, cleanup, video build - ran from ONE bash harness script, matching this house's real, existing harness convention.
EOF

cat > "$FEATURE_DIR/REPRODUCE.md" <<EOF
# Reproduce (real bash harness)

\`\`\`sh
HOUSE="$HOUSE" bash cursword/harnesses/bookmark_badge_contrast_fix_harness.sh
\`\`\`

Same house harness shape as \`#.desktop/harnesses/db-hq/nav.sh\` -
\`send_code\`/\`send_digits\` appending decimal ASCII lines to the real
relay/history file, a real frame dump via the universal 'p' debug key,
and a trap-based cleanup that always runs, success or failure.
EOF

echo "=== step 6: build video (python tool, unchanged) ==="
python3 "$HERE/../presentations/make_presentation_video.py" "$FEATURE_DIR"

echo "harness complete: $FEATURE_DIR"
