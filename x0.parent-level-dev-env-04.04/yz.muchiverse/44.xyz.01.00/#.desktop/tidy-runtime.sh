#!/bin/sh
# tidy-runtime.sh — trim/delete disposable generated files so the tree
# (and the house .7z backup) doesn't bloat. Runtime-state / frame-mirror
# / log files only; NEVER touches real data (ledgers, blockchain.txt,
# per-pal history.txt, anything git-tracked).
#
#   tidy-runtime.sh          # do it
#   tidy-runtime.sh -n       # dry run — print what it would do
#
# Rules (house policy 2026-09-07): these categories never exceed 250 KB.
#   - framebuffers  rgb_frame*.raw / *_overlay.raw / canvas.raw / *.rgba32
#                   -> DELETED outright IF their owning session/app has
#                      no live process (regenerated on next launch).
#   - #.desktop/ascii_frames/*        -> emptied (headless render mirror)
#   - *frame_history.txt / *.log / gl_cli_out.txt  -> truncated to the
#                      last 250 KB (keep the tail).
# Skips any path git knows about (`git ls-files`) — a tracked file is
# somebody's deliberate checked-in state, not cruft.
set -u
CAP=256000                                  # 250 KB
DRY=0; [ "${1:-}" = "-n" ] && DRY=1
HOUSE="$(cd "$(dirname "$0")/.." && pwd)"   # .../44.xyz.01.00
cd "$HOUSE" || exit 1

# fast lookup of tracked files (relative to repo root)
TRACKED="$(git -C "$HOUSE" ls-files 2>/dev/null | sed 's#^#/#')"
is_tracked() { case "$TRACKED" in *"/$1"*) return 0;; esac; return 1; }
say() { echo "$1"; }
act() { if [ "$DRY" = 1 ]; then echo "would: $*"; else eval "$*"; fi; }

# --- framebuffers: delete when no owning process is alive -------------
# crude but safe: if ANY of the known session engines is running we skip
# the whole framebuffer sweep rather than guess which file is live.
if pgrep -f 'chtpm_parser_pal|/system/renderer|bv_compose_frame|bv_render_3d|mutaclsym|rtp-xyz|rpg-xyz|aomorai|agy-txt|editor-📄|media-studio|event-ez' >/dev/null 2>&1; then
    say "[skip] a session engine is running — leaving framebuffers alone"
else
    find . -type f \( -name 'rgb_frame*.raw' -o -name '*_3d_overlay.raw' \
        -o -name 'canvas.raw' -o -name '*.rgba32' \) 2>/dev/null | while IFS= read -r f; do
        rel=${f#./}
        is_tracked "$rel" && { say "[keep tracked] $rel"; continue; }
        [ "$(wc -c < "$f" 2>/dev/null || echo 0)" -le "$CAP" ] && continue
        act "rm -f \"$f\""
    done
fi

# --- headless ascii-frame mirror ------------------------------------
if [ -d "#.desktop/ascii_frames" ]; then
    act "find \"#.desktop/ascii_frames\" -type f -delete"
fi

# --- logs / frame-history: keep the last 250 KB --------------------
find . -type f \( -name '*frame_history.txt' -o -name '*.log' \
    -o -name 'gl_cli_out.txt' \) -size +250k 2>/dev/null | while IFS= read -r f; do
    rel=${f#./}
    is_tracked "$rel" && { say "[keep tracked] $rel"; continue; }
    if [ "$DRY" = 1 ]; then
        echo "would: tail -c $CAP \"$f\" -> \"$f\"  ($(wc -c < "$f") bytes now)"
    else
        tail -c "$CAP" "$f" > "$f.tidy" && mv "$f.tidy" "$f"
    fi
done

if [ "$DRY" = 1 ]; then say "done (dry run — nothing changed)"; else say "done"; fi
