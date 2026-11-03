#!/bin/sh
# start_player.sh - start (or stop) the LyfeDesk UI sound player. BOOK 19.audio/UI-SOUNDS-AND-STARTUP-DESIGN.md.  Usage: start_player.sh [boot|run|stop|status]
#   boot       : (re)build the two ops if missing, render the sound files if ui_sounds.pdl changed, start ONE player detached; the player plays the startup chime once per HQ start (autostart uses this; stamp = boot_id + 90 s bucket)
#   run        : the same but WITHOUT the startup chime (khtpm_core_render starts the player this way when it writes a cue and none is running)
#   stop       : stop the player recorded in state/player.pid   status : say whether it is running
# Files: sounds are rendered into <house>/#.desktop/sounds/<theme>/<id>.wav (generated, not tracked); cues are read from <house>/#.desktop/ui_cue.txt; settings are the sound_* keys of <house>/#.desktop/hq_ui.pdl.
# Heavy work is niced. No sound server or a failed build => the desktop just starts without sound (this script never fails the autostart).
ACTION="${1:-boot}"
HERE="$(cd "$(dirname "$0")" && pwd)"
HOUSE="$(cd "$HERE/../.." && pwd)"
DESK="$HOUSE/#.desktop"; SND="$DESK/sounds"; STATE="$HERE/state"
PDL="$HERE/ui_sounds.pdl"; REND="$HERE/ops/+x/ui_sound_render.+x"; PLAY="$HERE/ops/+x/ui_sound_player.+x"
PIDF="$STATE/player.pid"
mkdir -p "$STATE"
alive() { [ -f "$PIDF" ] && p="$(cat "$PIDF" 2>/dev/null)" && [ -n "$p" ] && kill -0 "$p" 2>/dev/null && grep -q 'ui_sound_player' "/proc/$p/cmdline" 2>/dev/null; }
case "$ACTION" in
    stop)
        if alive; then kill "$(cat "$PIDF")" 2>/dev/null; echo "ui sound player stopped"; else echo "ui sound player not running"; fi
        rm -f "$PIDF"; exit 0 ;;
    status)
        if alive; then echo "running pid $(cat "$PIDF")"; else echo "not running"; fi; exit 0 ;;
    boot|run|start) ;;
    *) echo "usage: start_player.sh [boot|run|stop|status]" >&2; exit 2 ;;
esac
if alive; then echo "ui sound player already running (pid $(cat "$PIDF"))"; exit 0; fi
for b in render player; do
    [ -x "$HERE/ops/+x/ui_sound_$b.+x" ] || nice -n 15 sh "$HERE/ops/build_ui_sound_$b.sh" >/dev/null 2>&1 || true
done
if [ ! -x "$REND" ] || [ ! -x "$PLAY" ]; then echo "ui sounds: build failed, starting without sound" >&2; exit 0; fi
SUM="$(cksum < "$PDL" | cut -d' ' -f1,2) $(cksum < "$REND" | cut -d' ' -f1,2)"
if [ ! -d "$SND" ] || [ "$(cat "$SND/.rendered" 2>/dev/null)" != "$SUM" ]; then
    if nice -n 15 "$REND" "$PDL" "$SND" >"$STATE/render.log" 2>&1; then echo "$SUM" > "$SND/.rendered"; else echo "ui sounds: render failed (see $STATE/render.log), starting without sound" >&2; exit 0; fi
fi
# The chime plays once per HQ START (owner 2026-11-02: a restart in the same login session was silent). `boot` runs from the autostart row on every start;
# the stamp id is <boot_id>:<90 s bucket>, so a second call inside the same start (login autostart + launcher) stays quiet, a restart a minute or more later plays.
[ "$ACTION" = "boot" ] && set -- --boot-stamp "$SND/startup_stamp.txt" --boot-id "$(cat /proc/sys/kernel/random/boot_id 2>/dev/null):$(( $(date +%s) / 90 ))" || set --
# Started through the spawn funnel (AGENTS.md: every process the house starts is recorded in #.desktop/proc_registry.pdl before it does any work): lifetime `house` (it lives
# with the desktop, not with one window). The funnel sends output to /dev/null, so the command is wrapped to keep player.log; every `exec` keeps the pid, so the row names the
# real player. (khtpm_core_render also starts the player, through `start_player.sh run`, so this one site covers both.)
# the sound server socket lives under XDG_RUNTIME_DIR; a launcher with a stripped environment (a detached autostart) has none: default it (2026-11-03)
[ -n "${XDG_RUNTIME_DIR:-}" ] || { XDG_RUNTIME_DIR="/run/user/$(id -u)"; export XDG_RUNTIME_DIR; }
FUNNEL="$HOUSE/&.widgits/_shared-lib/kh_spawn.sh"
[ -f "$FUNNEL" ] || { echo "ui sounds: missing the spawn funnel $FUNNEL, starting without sound" >&2; exit 0; }
NEWPID="$(KH_ROLE=sound KH_APP=ui-sounds KH_LIFETIME=house sh "$FUNNEL" "$HOUSE" 0 ui-sound-player -- sh -c 'log="$1"; shift; exec "$@" >"$log" 2>&1 < /dev/null' ui_sound_launch "$STATE/player.log" nice -n 10 "$PLAY" --pdl "$PDL" --sounds "$SND" --settings "$DESK/hq_ui.pdl" --cues "$DESK/ui_cue.txt" "$@")"
echo "$NEWPID" > "$PIDF"
echo "ui sound player started (pid $NEWPID)"
