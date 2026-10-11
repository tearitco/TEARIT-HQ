#!/bin/sh
# tsots_touch_event.sh <app_root> <x> <y>
#
# Play mode only. The xelector just stepped onto x,y. If the open desk
# has a map event on that cell whose active page is on-touch or
# event-touch, run that package. Edit mode (play off) does nothing:
# a bar click or a right-click opens the same context menu as any
# other entity.
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
[ -f "$BAR" ] || exit 0
PLAY="$HOUSE/&.widgits/events-hq/ops/play_event.sh"
[ -f "$PLAY" ] || exit 0
# id name x y trigger
DP="$APP/pieces/system/maps/$MAP/deadpool.pdl"
awk -F '\t' -v x="$X" -v y="$Y" '$3==x && $4==y && ($5=="on-touch" || $5=="event-touch" || $5=="player-touch") { print $1, $5; exit }' "$BAR" |
while read -r ID TRIG; do
    [ -n "$ID" ] || exit 0
    if [ -f "$DP" ] && awk -F'|' -v desk="$DESK" -v id="$ID" '
        { gsub(/^[ \t]+|[ \t]+$/, "", $2); gsub(/^[ \t]+|[ \t]+$/, "", $3) }
        $2=="desk" { d=$3 }
        $2=="ev" && $3==id && d==desk { hit=1 }
        END { exit hit ? 0 : 1 }
    ' "$DP"; then
        exit 0
    fi
    PKG="$APP/pieces/system/maps/$MAP/$DESK/ev/$ID"
    sh "$PLAY" "$PKG" "$HOUSE" "$TRIG"
done
