#!/bin/sh
# open_window_cli.sh - attach a terminal ASCII mirror to an ALREADY-
# RUNNING khtpm window, by its renderer PID. The generic sibling of
# open_cli.sh (which is strip-only). See TERMINAL-MIRROR-PARITY-all-
# windows.md.
#
# The running khtpm_core_render.+x process for that window already
# writes #.desktop/ascii_frames/<pid>.frame.txt + .pulse.txt every
# repaint (kh_write_ascii_frame()), and already polls
# #.desktop/entity_menu_history/<pid>.txt for input. This just opens a
# gnome-terminal running the two generic binaries against that PID -
# same renderer/keyboard split as the strip (never combined: raw termios
# clears OPOST and staircases; see khtpm_strip_render_ascii.c header).
#
# Usage: open_window_cli.sh <renderer-pid>
set -u
SCRIPT_DIR="$(cd "$(dirname "$0")" && pwd)"
. "$SCRIPT_DIR/khtpm_vars.sh"

PID="${1:-}"
if [ -z "$PID" ]; then
    echo "usage: $0 <renderer-pid>" >&2
    echo "  running khtpm windows:" >&2
    pgrep -af 'khtpm_core_render\.\+x' | grep -v ' --dump-and-exit' >&2 || true
    exit 1
fi
if ! kill -0 "$PID" 2>/dev/null; then
    echo "$0: no process $PID" >&2
    exit 1
fi

R="$SCRIPT_DIR/+x/khtpm_render_ascii.+x"
K="$SCRIPT_DIR/+x/khtpm_kbd_ascii.+x"

if [ "$(uname -s)" = "Darwin" ]; then
    TMP_SH="${TMPDIR:-/tmp}/khtpm_win_cli_$$.command"
    printf '%s\n' '#!/bin/sh' \
        "\"$R\" \"$KHTPM_HOUSE\" \"$PID\" &" \
        "\"$K\" \"$KHTPM_HOUSE\" \"$PID\"" > "$TMP_SH"
    chmod +x "$TMP_SH"
    open -a Terminal "$TMP_SH"
    exit 0
fi
exec gnome-terminal -- sh -c "\"$R\" \"$KHTPM_HOUSE\" \"$PID\" & \"$K\" \"$KHTPM_HOUSE\" \"$PID\""
