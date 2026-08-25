#!/bin/sh
# open_cli.sh - launches the terminal ASCII mirror of the taskbar in a
# fresh gnome-terminal tab.
#
# REAL BUG FIX 2026-08-18, found live-testing the HQ menu's "cli" row:
# `gnome-terminal -- <path>` execve()s <path> DIRECTLY - it never invokes
# a shell for that argument, so this project's own real, literal directory
# names containing '*' characters (*.monads/*.livedesk-taskbar) were never
# resolved (confirmed live: "Failed to execute child process
# '*.monads/*.livedesk-taskbar/...': Failed to execve: No such file or
# directory"). Same real fix shape as run_khtpm_strip.sh already uses
# (SCRIPT_DIR via $0, not a glob relied on at the exec site) - this script
# itself IS still invoked the same glob-relative way "$.restart"'s own row
# already does (proven working, since sh -c genuinely DOES glob-expand
# before invoking a real script), it just resolves the FINAL binary path
# from its own $0 once running, so gnome-terminal never sees a wildcard.
#
# REAL ARCHITECTURE FIX 2026-08-18, direct user report ("its still
# staggered... it should display from a .txt frame file like tpmos"): the
# original single combined binary (khtpm_strip_renderer_ascii.+x, retired)
# did BOTH raw-termios keyboard input AND stdout rendering in one process -
# raw mode disables OPOST, breaking \n->\r\n translation for its own
# printf output (a real, visible staircase). TPMOS's real architecture
# never combines these: system/renderer.c never touches termios, system/
# keyboard_input.c never prints. Split into two binaries here to match -
# khtpm_strip_render_ascii.+x (backgrounded, no termios, writes+displays a
# real .txt frame file) and khtpm_strip_keyboard_ascii.+x (foreground, raw
# termios, never prints) - same real shape mutaclysm's own button.sh
# already uses (`./system/renderer &` then foreground `./system/
# keyboard_input`), not a new pattern.
set -u
SCRIPT_DIR="$(cd "$(dirname "$0")" && pwd)"
. "$SCRIPT_DIR/khtpm_vars.sh"
# macOS leg (2026-08-22): no gnome-terminal here. Same renderer-&-
# keyboard-fg split, hosted by Terminal.app via a throwaway .command
# wrapper (Terminal executes .command files directly; a temp copy keeps
# star-names and quoting out of the osascript/Tell layer entirely).
if [ "$(uname -s)" = "Darwin" ]; then
    TMP_SH="${TMPDIR:-/tmp}/khtpm_cli_$$.command"
    printf '%s\n' '#!/bin/sh' \
        "\"$SCRIPT_DIR/+x/khtpm_strip_render_ascii.+x\" \"$KHTPM_HOUSE\" &" \
        "\"$SCRIPT_DIR/+x/khtpm_strip_keyboard_ascii.+x\" \"$KHTPM_HOUSE\"" > "$TMP_SH"
    chmod +x "$TMP_SH"
    open -a Terminal "$TMP_SH"
    exit 0
fi
exec gnome-terminal -- sh -c "\"$SCRIPT_DIR/+x/khtpm_strip_render_ascii.+x\" \"$KHTPM_HOUSE\" & \"$SCRIPT_DIR/+x/khtpm_strip_keyboard_ascii.+x\" \"$KHTPM_HOUSE\""
