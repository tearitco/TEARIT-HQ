#!/bin/bash
# start_player.sh - start (or stop) the LyfeDesk UI sound player. BOOK 19.audio/UI-SOUNDS-AND-STARTUP-DESIGN.md.  Usage: start_player.sh [boot|run|stop|status]
#   boot / run : (re)build the two ops if missing, render the sound files if ui_sounds.pdl changed, start ONE player detached; the player plays the startup chime once per login session
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
setsid nohup nice -n 10 "$PLAY" --pdl "$PDL" --sounds "$SND" --settings "$DESK/hq_ui.pdl" --cues "$DESK/ui_cue.txt" --boot-stamp "$SND/startup_stamp.txt" >"$STATE/player.log" 2>&1 < /dev/null &
echo $! > "$PIDF"
echo "ui sound player started (pid $!)"
