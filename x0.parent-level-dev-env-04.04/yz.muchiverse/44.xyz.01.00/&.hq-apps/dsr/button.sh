#!/bin/sh
# button.sh - launch DSR (Desk Street Raider): a real X11-HQ window
# (dsr.xhtpm) driven by a real, separate, compiled manager
# (dsr_manager.c, same house convention db-hq-pal/chat-hai/network
# already use - a real process owns state, publishes state/ui.txt, the
# generic renderer draws it, zero new per-project C in the renderer).
#   button.sh <house_root>
set -e
HERE="$(cd "$(dirname "$0")" && pwd)"
# The taskbar toys menu runs `button.sh run` (argv[1]="run", not a path),
# so fall back to the house root derived from this script's own location.
HOUSE_ROOT="${1:-}"
[ -n "$HOUSE_ROOT" ] && [ -d "$HOUSE_ROOT" ] || HOUSE_ROOT="$HERE/../.."
[ -d "$HOUSE_ROOT" ] || { echo "dsr: cannot resolve house_root" >&2; exit 1; }
HOUSE_ROOT="$(cd "$HOUSE_ROOT" && pwd)"
XHTPM="$HERE/dsr.xhtpm"
RENDER_OPS="$HOUSE_ROOT/*.monads/*.livedesk-taskbar/ops"
BIN="$RENDER_OPS/+x/khtpm_core_render.+x"
MGR="$HERE/ops/+x/dsr_manager.+x"

[ -x "$BIN" ] || (cd "$RENDER_OPS" && sh build_core_render.sh) || true
[ -x "$MGR" ] || sh "$HERE/ops/build_dsr_manager.sh" || true
[ -x "$BIN" ] || { echo "dsr: missing $BIN" >&2; exit 1; }
[ -x "$MGR" ] || { echo "dsr: missing $MGR" >&2; exit 1; }
[ -f "$XHTPM" ] || { echo "dsr: missing $XHTPM" >&2; exit 1; }
mkdir -p "$HERE/state"

for p in $(pgrep -f "khtpm_core_render\.\+x .*dsr\.xhtpm" 2>/dev/null || true) \
         $(pgrep -f "dsr_manager\.\+x" 2>/dev/null || true); do
    kill "$p" 2>/dev/null || true
done
sleep 0.3

setsid nohup "$MGR" "$HOUSE_ROOT" >/dev/null 2>&1 < /dev/null &
sleep 0.2
setsid nohup "$BIN" "$HOUSE_ROOT" "$XHTPM" >/dev/null 2>&1 < /dev/null &

# REAL, NEW 2026-09-14, direct live instruction ("the dsr desk should
# be opening when we open dsr x11-hq widgit"): the toy is one of DSR's
# two real front doors (see xyzfs/.../home/projects/dsr/NOTES.md §
# "two independent front doors") - by default, opening it ALSO opens
# its own desk, via the same real mr_transfer_desk op door_civ's own
# trigger uses (close whatever desk is active, spawn dsr's real
# buildings). A real, honest toggle: state/open_desk.state.txt
# (mode=on|off, same shape khtpm_play_mode.state.txt already uses) -
# missing file defaults to on, matching "defaults on, can be set off."
OPEN_DESK_STATE="$HERE/state/open_desk.state.txt"
OPEN_DESK_MODE="on"
if [ -f "$OPEN_DESK_STATE" ]; then
    OPEN_DESK_MODE="$(sed -n 's/^mode=//p' "$OPEN_DESK_STATE" | head -1)"
    [ -n "$OPEN_DESK_MODE" ] || OPEN_DESK_MODE="on"
fi
if [ "$OPEN_DESK_MODE" = "on" ]; then
    TRANSFER="$HOUSE_ROOT/&.widgits/events-hq/ops/+x/mr_transfer_desk.+x"
    [ -x "$TRANSFER" ] || sh "$HOUSE_ROOT/&.widgits/events-hq/ops/build_mr_event_ops.sh" >/dev/null 2>&1 || true
    if [ -x "$TRANSFER" ]; then
        # Resolve the real active session id, same convention door_civ's
        # own cmd_1.sh already uses (STATE | active_session | <id>).
        LOGIN_ROOT="$(find "$HOUSE_ROOT" -maxdepth 1 -name "0.user-pal*" -type d 2>/dev/null | head -1)/00.login-signup"
        USER_UUID="$(sed -n 's/^current_user_uuid=//p' "$LOGIN_ROOT/current_login.txt" 2>/dev/null | tr -d ' \r' | head -1)"
        if [ -n "$USER_UUID" ]; then
            SESS_ROOT="$HOUSE_ROOT/xyzfs/users/$USER_UUID/home/livedesk/sessions"
            SESSION_ID="$(awk -F'|' '/active_session/{gsub(/ /,"",$3); print $3}' "$SESS_ROOT/session.pdl" 2>/dev/null)"
            [ -n "$SESSION_ID" ] || SESSION_ID=s1
            # dsr desk renamed to dsr-dev 2026-09-22 (experimental
            # physical-layout sandbox); toy's front door now opens
            # that desk until a separate coded spawn-target desk exists.
            "$TRANSFER" "$HOUSE_ROOT" "$SESSION_ID" "dsr-dev" >/dev/null 2>&1 &
        fi
    fi
fi

echo "dsr launched (renderer + manager$([ "$OPEN_DESK_MODE" = "on" ] && echo " + desk"))"
