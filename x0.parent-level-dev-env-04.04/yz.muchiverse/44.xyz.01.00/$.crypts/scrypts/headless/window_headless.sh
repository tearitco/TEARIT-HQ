#!/bin/sh
# window_headless.sh - launch ONE khtpm window with no human display,
# for agents / CI / driving IRC+chain from many ports at once.
#
# WHERE THIS LIVES: $.crypts/scrypts/headless/ - $.crypts is the house's
# session/lifecycle-script area (button.sh, harness-runner.sh,
# EMERGENCY_CLOSE.sh). Per-window runtime helpers
# (open_cli.sh / open_window_cli.sh) stay in the taskbar ops/ dir; this
# is the headless *entry point* that ties them together.
#
# HOW HEADLESS ACTUALLY WORKS RIGHT NOW
#   khtpm_core_render.+x still needs an X connection to build its window
#   (a true no-X `--headless` build is designed but NOT done - see
#   TERMINAL-MIRROR-PARITY-all-windows.md step 4). So "headless" here is
#   one of:
#     a) a real DISPLAY is set (e.g. :0)  -> the window is drawn on it
#        but you never look at it; you read/drive it purely via files.
#     b) no DISPLAY, `xvfb-run` present   -> we wrap in a throwaway
#        virtual X server; genuinely no screen needed (SSH / CI).
#        Install once:  sudo apt-get install -y xvfb
#     c) no DISPLAY, no xvfb              -> we stop and tell you (b).
#
# WHAT YOU GET (whichever path)
#   frame : #.desktop/ascii_frames/<pid>.frame.txt      (cat it)
#   pulse : #.desktop/ascii_frames/<pid>.pulse.txt      (size grows/frame)
#   input : #.desktop/entity_menu_history/<pid>.txt     (append to drive)
#           one line per event:
#             KEY_PRESSED: 200|201|202|203   Up|Down|Left|Right
#             KEY_PRESSED: 13                Enter      27  Escape
#             KEY_PRESSED: <32..126>         a literal typed char
#             MOUSE_EVENT: <btn> <x> <y> 1   a click
#   The PID is printed on stdout as `PID=<n>` and written to
#   <house>/#.desktop/ascii_frames/last_headless.pid
#
#   --attach : also run the presenter+keyboard in THIS terminal
#              (foreground) so you can watch/type live. Ctrl+C detaches
#              the viewer; the window keeps running. Omit for pure
#              file-driven / scripted use.
#
# Usage:
#   window_headless.sh <house_root> <xhtpm-or-chtpm-path> [x] [y] [--attach]
#   window_headless.sh <xhtpm-or-chtpm-path> [--attach]   (house auto-found)
set -u

SELF_DIR="$(cd "$(dirname "$0")" && pwd)"

# --- resolve house + chtpm ---------------------------------------------
ATTACH=0
ARGS=""
for a in "$@"; do
    if [ "$a" = "--attach" ]; then ATTACH=1; else ARGS="$ARGS $a"; fi
done
# shellcheck disable=SC2086
set -- $ARGS

find_house() {
    d="$1"
    while [ -n "$d" ] && [ "$d" != "/" ]; do
        if [ -d "$d/#.desktop" ] && [ -d "$d/&.widgits" ]; then echo "$d"; return 0; fi
        d="$(dirname "$d")"
    done
    return 1
}

if [ $# -ge 2 ] && [ -d "$1" ]; then
    HOUSE="$(cd "$1" && pwd)"; CHTPM="$2"; shift 2
elif [ $# -ge 1 ]; then
    CHTPM="$1"; shift
    HOUSE="$(find_house "$(cd "$(dirname "$CHTPM")" && pwd)")" || HOUSE=""
else
    echo "usage: $0 [<house_root>] <xhtpm-or-chtpm-path> [x] [y] [--attach]" >&2
    exit 1
fi
[ -n "${HOUSE:-}" ] && [ -d "$HOUSE" ] || { echo "$0: cannot find house root (pass it explicitly)" >&2; exit 1; }
[ -f "$CHTPM" ] || { echo "$0: no such file: $CHTPM" >&2; exit 1; }
CHTPM="$(cd "$(dirname "$CHTPM")" && pwd)/$(basename "$CHTPM")"
X="${1:-80}"; Y="${2:-80}"

# The house uses literal '*' characters in these dir names, so this is a
# real path, not a glob to expand - quote it everywhere.
OPS="$HOUSE/*.monads/*.livedesk-taskbar/ops"
BIN="$OPS/+x/khtpm_core_render.+x"
[ -x "$BIN" ] || { echo "$0: missing $BIN (run build_khtpm_strip.sh)" >&2; exit 1; }

# --- pick the X strategy ----------------------------------------------
WRAP=""
if [ -n "${DISPLAY:-}" ]; then
    MODE="DISPLAY=$DISPLAY (window drawn there, ignore it; drive via files)"
elif command -v xvfb-run >/dev/null 2>&1; then
    WRAP="xvfb-run -a"
    MODE="xvfb-run (virtual X server, no screen)"
else
    cat >&2 <<EOF
$0: no DISPLAY and no xvfb-run.
  Either:  export DISPLAY=:0    (use the running desktop's X server)
  Or:      sudo apt-get install -y xvfb   (then re-run; genuinely screenless)
EOF
    exit 2
fi

# --- launch ----------------------------------------------------------
# shellcheck disable=SC2086
$WRAP "$BIN" "$HOUSE" "$CHTPM" "$X" "$Y" &
PID=$!
# under xvfb-run, $! is the wrapper; the real renderer is its child.
sleep 1
REAL_PID="$PID"
if [ -n "$WRAP" ]; then
    REAL_PID="$(pgrep -P "$PID" -f 'khtpm_core_render' | head -1)"
    [ -n "$REAL_PID" ] || REAL_PID="$PID"
fi

FRAME="$HOUSE/#.desktop/ascii_frames/$REAL_PID.frame.txt"
RELAY="$HOUSE/#.desktop/entity_menu_history/$REAL_PID.txt"
echo "$REAL_PID" > "$HOUSE/#.desktop/ascii_frames/last_headless.pid" 2>/dev/null || true

# wait for first frame
i=0
while [ ! -f "$FRAME" ] && [ $i -lt 100 ]; do sleep 0.1; i=$((i+1)); done

cat <<EOF
PID=$REAL_PID
mode : $MODE
frame: $FRAME
relay: $RELAY
  read : cat "$FRAME"
  drive: printf 'KEY_PRESSED: 201\\n' >> "$RELAY"   # Down
  stop : kill $REAL_PID
EOF

if [ "$ATTACH" = "1" ]; then
    R="$OPS/+x/khtpm_render_ascii.+x"
    K="$OPS/+x/khtpm_kbd_ascii.+x"
    "$R" "$HOUSE" "$REAL_PID" &
    VPID=$!
    trap 'kill $VPID 2>/dev/null' INT TERM
    "$K" "$HOUSE" "$REAL_PID"
    kill $VPID 2>/dev/null
fi
