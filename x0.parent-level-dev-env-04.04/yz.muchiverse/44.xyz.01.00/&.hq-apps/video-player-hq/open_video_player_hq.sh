#!/bin/sh
# open_video_player_hq.sh - launch the video-player-hq X11-HQ window.
# Shape = &.hq-apps/chat-hai/button-pal.sh: this script launches ONE
# process (the shared renderer on video-player-hq.xhtpm); the template's
# single <module> (ops/+x/video_player_manager.+x) is forked by the
# renderer's own kh_launch_window_modules() and SIGTERM'd on close.
#
#   sh open_video_player_hq.sh <house_root>
#
# No taskbar shim (shared taskbar files are off limits for this app); launch by hand. Design: README.md.
set -e

HOUSE_ROOT="${1:-}"
if [ -z "$HOUSE_ROOT" ] || [ ! -d "$HOUSE_ROOT" ]; then
    # not a directory (the Toys menu passes the literal "run", or nothing): walk up from this script to the dir holding #.desktop + &.widgits
    _d="$(cd "$(dirname "$0")" && pwd)"
    while [ "$_d" != "/" ] && { [ ! -d "$_d/#.desktop" ] || [ ! -d "$_d/&.widgits" ]; }; do _d="$(dirname "$_d")"; done
    HOUSE_ROOT="$_d"
fi
[ -d "$HOUSE_ROOT/#.desktop" ] || { echo "open_video_player_hq.sh: could not resolve the house root" >&2; exit 1; }
HOUSE_ROOT="$(cd "$HOUSE_ROOT" && pwd)"

HERE="$(cd "$(dirname "$0")" && pwd)"
XHTPM="$HERE/video-player-hq.xhtpm"
MGR="$HERE/ops/+x/video_player_manager.+x"
RENDER_OPS_DIR="$HOUSE_ROOT"/_.monads/_.livedesk-taskbar/ops
BIN="$(cd $RENDER_OPS_DIR && pwd)/+x/khtpm_core_render.+x"

[ -x "$BIN" ] || (cd $RENDER_OPS_DIR && sh build_core_render.sh) || true
[ -x "$MGR" ] || (cd "$HERE/ops" && sh build_video_player_manager.sh) || true
for f in "$BIN" "$MGR" "$XHTPM"; do
    [ -e "$f" ] || { echo "open_video_player_hq.sh: missing $f" >&2; exit 1; }
done

LOG_DIR="$HERE/audit"
mkdir -p "$LOG_DIR"

mine() { pgrep -f "khtpm_core_render\.\+x.*video-player-hq\.xhtpm" 2>/dev/null || true; }
strays() { pgrep -f "video-player-hq/ops/\+x/video_player_manager\.\+x" 2>/dev/null || true; }

pids="$(mine; strays)"
pids="$(echo "$pids" | grep -v '^$' || true)"
if [ -n "$pids" ]; then
    echo "open_video_player_hq.sh: killing existing instance(s): $(echo $pids | tr '\n' ' ')"
    echo "$pids" | xargs -r kill -TERM
    sleep 1
    pids="$(mine; strays)"
    pids="$(echo "$pids" | grep -v '^$' || true)"
    [ -n "$pids" ] && { echo "$pids" | xargs -r kill -KILL; sleep 1; }
fi

setsid nohup "$BIN" "$HOUSE_ROOT" "$XHTPM" \
    >"$LOG_DIR/video-player-hq.log" 2>&1 < /dev/null &
printf '%s %s 0 0 video-player-hq\n' "$!" "$!" >> "$HOUSE_ROOT/#.desktop/livedesk_proc_list.txt" 2>/dev/null || true
disown 2>/dev/null || true
sleep 1

n="$(mine | grep -c . || true)"
if [ "$n" -ge 1 ] 2>/dev/null; then
    echo "video-player-hq launched (renderer pid $(mine | tr '\n' ' '), log=$LOG_DIR/video-player-hq.log)"
else
    echo "open_video_player_hq.sh: FAILED to launch - check the log:" >&2
    cat "$LOG_DIR/video-player-hq.log" 2>/dev/null >&2
    exit 1
fi
