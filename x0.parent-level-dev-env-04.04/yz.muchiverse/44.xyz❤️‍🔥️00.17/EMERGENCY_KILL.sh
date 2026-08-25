#!/bin/sh
# EMERGENCY_KILL.sh - FAST nuclear kill of project binaries.
#
# HISTORY / BUG FIX (2026-07-30, !.xyzos-pitfalls+1.txt PITFALL 55):
# the original version used `pkill -x`/`pgrep -x` (exact match against
# /proc/[pid]/comm). Linux truncates comm to 15 characters (TASK_COMM_LEN),
# so TWO of this script's own nine target names were SILENTLY UNKILLABLE
# by their own exact-match check:
#   chtpm_parser_pal (17 chars) -> kernel stores "chtpm_parser_pa"
#   chtpm_rgb_render  (16 chars) -> kernel stores "chtpm_rgb_rende"
# `pkill -x "chtpm_parser_pal"` can NEVER match a real running process by
# that name - verified live (started a dummy 17-char-named process,
# confirmed pgrep -x misses it). Worse: the OLD end-of-script verification
# used the identical broken check, so it could print "All project
# processes terminated." while these two - exactly the daemons most prone
# to runaway CPU in this house's own GL/RGB pipeline - kept running.
#
# FIX: switched to `pkill -f` with an ANCHORED regex - `(^|/)NAME( |$)` -
# matching against the untruncated /proc/[pid]/cmdline instead of the
# truncated comm field, while still requiring the name to appear as its
# own path-final-component or whole token (never a bare substring). This
# keeps the original "safe vs Chrome" property (chrome's own cmdline only
# ever contains these strings glued to "=", e.g. "--type=renderer", which
# the anchor deliberately does NOT match - verified live, zero false
# positives against a real running Chrome) while actually being able to
# kill every name on the list, not just the ones under 15 characters.

echo "EMERGENCY KILL..."

# REAL ADDITION 2026-08-04, direct instruction ("clean up cpu emergency
# add to EMERGENCY_KILL.sh"): tp_desktop_window.+x (tile-picker's own
# live desktop-entity GL windows - long-running by design, same class
# of process as egg_window above) and tp_arm_placer.+x (holds a real
# GLOBAL X11 pointer+keyboard grab while armed - a stuck one would lock
# input for the whole desktop, not just one window, making it a real
# emergency-kill candidate specifically). Confirmed this session:
# chtpm_parser_pal/chtpm_rgb_render (already on this list) are the ones
# that actually ran away with real CPU/memory when left unattended -
# root cause was a real bug in tp_menu_input.c (fixed), not these
# binaries themselves, but keeping them on this list is still correct
# defense-in-depth.
#
# REAL ADDITION 2026-08-05, direct instruction ("all those borderless
# windows u make need to have a... script to emergency kill them, pls
# make that now. or they will stay on screen like a virus"):
# tp_range_grid.+x - a real, small, standalone override_redirect X11
# popup (the range-finder grid) with NO titlebar and NO window-manager
# close button, only closable via its own click/Escape handler inside
# its own event loop. If that process somehow hangs (stuck in
# XNextEvent with a lost grab, say), there is NO other way to close it
# short of this. gl_mirror itself (already on this list above) also now
# has a real GL_MIRROR_BORDERLESS=1 mode (same binary/process name, no
# separate entry needed - this list already covers it).
# REAL ADDITION 2026-08-18, direct instruction (chat-hai window kept
# running after being closed): khtpm_entity_menu_render.+x is the ONE
# shared binary behind FIVE separate apps (chat-hai, db-hq, events-hq,
# taskbar-settings, entity-menu) - each launch forks/execs its own real,
# separate PID running this same executable file, mode-selected via a
# class= arg at launch (e.g. class=chat-hai vs class=db-hq). They are
# NOT one shared running process - each open window really is its own
# live PID. But pkill -f here matches by the executable's PATH in
# /proc/[pid]/cmdline, which is identical across all five - it has no
# way to see the class= arg and tell "this PID is chat-hai" apart from
# "this PID is db-hq". So adding this name here kills EVERY currently
# running instance of the binary, whichever of the five apps each one
# happens to be - there is no way to target just one of the five by
# name alone.
NAMES='orchestrator keyboard_input chtpm_parser_pal chtpm_rgb_render gl_mirror egg_window avatar_window renderer prisc\+x agy_browser_manager rtp_manager yahoo_menu_input yahoo_compose_frame broker_menu_input broker_compose_frame deposit_withdraw tp_desktop_window tp_arm_placer tp_range_grid khtpm_strip_parser\.\+x khtpm_taskbar_manager_main\.\+x khtpm_entity_menu_render\.\+x'

pat() {
    # $1 = bare name (unescaped except prisc+x's own literal backslash
    # above, which is already regex-safe) -> anchored search pattern
    printf '(^|/)%s( |$)' "$1"
}

# TERM first (quick), then KILL — no long grace if nothing was up
HIT=0
for name in $NAMES; do
    if pgrep -f "$(pat "$name")" >/dev/null 2>&1; then
        pkill -f -TERM "$(pat "$name")" 2>/dev/null || true
        HIT=1
    fi
done

if [ "$HIT" = "1" ]; then
    sleep 0.1
    for name in $NAMES; do
        pkill -f -KILL "$(pat "$name")" 2>/dev/null || true
    done
fi

SCRIPT_DIR="$(cd "$(dirname "$0")" && pwd)"

# Targeted stale-file cleanup only (NOT find over whole tree)
for rel in \
    "101.mutaclsym🧟‍♂️️+18.01/debug.txt" \
    "101.mutaclsym🧟‍♂️️+18.01/pieces/display/gl_key_debug.log" \
    "101.mutaclsym🧟‍♂️️+18.01/pieces/os/proc_list.txt"
do
    : > "$SCRIPT_DIR/$rel" 2>/dev/null || true
done

# Common project roots for window.pid / quit_flag (shallow, fixed paths)
for root in \
    "$SCRIPT_DIR/0.user-pal👤️/01.avatar-creation👤️" \
    "$SCRIPT_DIR/01.muchi-pals-🥚️-13.01" \
    "$SCRIPT_DIR/101.mutaclsym🧟‍♂️️+18.01"
do
    [ -d "$root" ] || continue
    rm -f "$root/pieces/world_01/map_lobby"/*/window.pid 2>/dev/null || true
    rm -f "$root/pieces/system/quit_flag.txt" 2>/dev/null || true
    rm -f "$root/pieces/system/gl_focus.lock" 2>/dev/null || true
    : > "$root/pieces/system/avatar_window_pids.txt" 2>/dev/null || true
done

# BLANKET SESSION SWEEP (added 2026-07-30, PITFALL 55): the newer
# session-per-launch architecture (pieces/sessions/<ts>-<pid>/, used by
# 102.editor-📄️00.00, &.widgits/file-menu, @.apps/text-editor-xyz, and
# every project following this house pattern) accumulates throwaway
# session directories that the old hardcoded 3-project cleanup above
# never touched at all. Bounded find (maxdepth, directory-name match
# only, not a content walk) - stays fast, still comprehensive across
# every project rather than a fixed list that goes stale as new
# projects get added. This IS a nuclear/emergency option: it does not
# check whether a session is still legitimately in progress before
# removing it - by design, matching this script's own stated purpose.
find "$SCRIPT_DIR" -maxdepth 4 -type d -path '*/pieces/sessions' 2>/dev/null | while read -r sdir; do
    rm -rf "${sdir:?}"/* 2>/dev/null || true
done
# Also clear the equivalent /tmp session dirs (@.apps/*/button.sh's own
# pattern: /tmp/.<app-name>-editor-<id>/ - not under pieces/sessions/ at
# all, so the find above never reaches these).
rm -rf /tmp/.text-editor-xyz-editor-* 2>/dev/null || true
rm -rf /tmp/.yahoo-app-bank-* 2>/dev/null || true

left=""
for name in $NAMES; do
    if pgrep -f "$(pat "$name")" >/dev/null 2>&1; then
        left="$left $name"
    fi
done
if [ -n "$left" ]; then
    echo "WARNING still running:$left"
else
    echo "All project processes terminated."
fi
echo "Done."
