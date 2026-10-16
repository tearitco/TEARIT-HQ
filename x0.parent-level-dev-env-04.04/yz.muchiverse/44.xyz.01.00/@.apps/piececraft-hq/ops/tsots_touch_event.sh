#!/bin/sh
# tsots_touch_event.sh <app_root> <x> <y> [expected_verb]
#
# Play mode only. The xelector just stepped onto x,y. If the open desk
# has a map event on that cell whose active page is on-touch or
# event-touch, run that package. Edit mode (play off) does nothing:
# a bar click or a right-click opens the same context menu as any
# other entity.
#
# Exit codes (2026-10-15, for the doom touch handler in pc_menu_input.c, which tries this first and falls back to its old direct verb path
# ONLY on 3): 0 = handled (an event ran through play_event.sh, or it was already killed, or play mode is off), 3 = this script does not
# apply (no bar.txt for the desk, no on-touch row on that cell, or play_event.sh ran nothing). Nothing is run twice: 3 means nothing ran.
# expected_verb (optional, from the caller's events.pdl `cmds=`): when given, the event page is only run if its own verb.txt says the same verb;
# otherwise exit 3 (the page would do something different from what the game does today; 2026-10-15 scan of the doom maps: 1,471 of 2,211
# pages carry a different verb than their tile's cmds=, e.g. Monster tiles whose page says nukage).
APP="$1"
X="$2"
Y="$3"
HOUSE="$APP"
while [ "$HOUSE" != / ] && [ ! -d "$HOUSE/xyzfs" ]; do HOUSE="$(dirname "$HOUSE")"; done
[ -n "$APP" ] || exit 0
MODE="$HOUSE/#.desktop/khtpm_play_mode.state.txt"
grep -q '^mode=on' "$MODE" 2>/dev/null || exit 0
W="$APP/pieces/world_01/state.txt"
MAP=$(sed -n 's/^map_id=//p' "$W" | head -1)
DESK=$(sed -n 's/^desk_id=//p' "$W" | head -1)
BAR="$APP/pieces/system/maps/$MAP/$DESK/bar.txt"
[ -f "$BAR" ] || exit 3
PLAY="$HOUSE/&.widgits/events-hq/ops/play_event.sh"
[ -f "$PLAY" ] || exit 3
# id name x y trigger
DP="$APP/pieces/system/maps/$MAP/deadpool.pdl"
ROW=$(awk -F '\t' -v x="$X" -v y="$Y" '$3==x && $4==y && ($5=="on-touch" || $5=="event-touch" || $5=="player-touch") { print $1, $5; exit }' "$BAR")
[ -n "$ROW" ] || exit 3
ID=${ROW% *}
TRIG=${ROW#* }
if [ -f "$DP" ] && awk -F'|' -v desk="$DESK" -v id="$ID" '
    { gsub(/^[ \t]+|[ \t]+$/, "", $2); gsub(/^[ \t]+|[ \t]+$/, "", $3) }
    $2=="desk" { d=$3 }
    $2=="ev" && $3==id && d==desk { hit=1 }
    END { exit hit ? 0 : 1 }
' "$DP"; then
    exit 0
fi
PKG="$APP/pieces/system/maps/$MAP/$DESK/ev/$ID"
if [ -n "$4" ]; then
    PV=$(head -1 "$PKG/verb.txt" 2>/dev/null | awk '{print $1}')
    [ "$PV" = "$4" ] || exit 3
fi
sh "$PLAY" "$PKG" "$HOUSE" "$TRIG" >/dev/null 2>&1 || exit 3
exit 0
