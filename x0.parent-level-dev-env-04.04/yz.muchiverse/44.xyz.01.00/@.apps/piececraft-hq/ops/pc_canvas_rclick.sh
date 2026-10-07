#!/bin/sh
# pc_canvas_rclick.sh <click_rx> <click_ry> <win_rx> <win_ry> <win_w> <win_h> <cx> <cy> <cw> <ch>
# Right-click on the pc-hq view. Opens the entity menu at the click,
# or at the window center when the camera is first person.
set -u
SELF=$(cd "$(dirname "$0")" && pwd)
HOUSE=$(cd "$SELF/../../.." && pwd)
PCHQ="$HOUSE/@.apps/piececraft-hq"
RX=${1:-0}; RY=${2:-0}; WX=${3:-0}; WY=${4:-0}; WW=${5:-0}; WH=${6:-0}
CX=${7:-0}; CY=${8:-0}; CW=${9:-1}; CH=${10:-1}
MAP="$PCHQ/pieces/display/view_map.txt"
HX=$(sed -n 's/^pos_x=//p' "$PCHQ/pieces/hero_01/state.txt" | head -1)
HY=$(sed -n 's/^pos_y=//p' "$PCHQ/pieces/hero_01/state.txt" | head -1)
HZ=$(sed -n 's/^pos_z=//p' "$PCHQ/pieces/hero_01/state.txt" | head -1)
KIND=hero; ID=hero_01; SX=${HX:-0}; SY=${HY:-0}; SZ=${HZ:-0}
ox=$(sed -n 's/^ox=//p' "$MAP" | head -1)
oy=$(sed -n 's/^oy=//p' "$MAP" | head -1)
cell=$(sed -n 's/^cell=//p' "$MAP" | head -1)
FW=$(sed -n 's/^W=//p' "$MAP" | head -1)
FH=$(sed -n 's/^H=//p' "$MAP" | head -1)
cell=${cell:-80}; FW=${FW:-640}; FH=${FH:-480}; ox=${ox:-0}; oy=${oy:-0}
if [ "$CW" -gt 0 ] && [ "$CH" -gt 0 ]; then
    px=$((CX * FW / CW)); py=$((CY * FH / CH))
    scx=$((px / cell)); scy=$((py / cell))
    bx=$((ox + scx)); by=$((oy + scy))
    hit=$(awk -v x="$bx" -v y="$by" '$2==x && $3==y { print $1; exit }' \
        "$PCHQ/pieces/display/synched_entities.txt" 2>/dev/null || true)
    if [ -n "$hit" ]; then KIND=entity; ID=$hit; SX=$bx; SY=$by; SZ=0
    elif [ "$bx" != "$HX" ] || [ "$by" != "$HY" ]; then KIND=air; ID=""; SX=$bx; SY=$by
    fi
fi
mkdir -p "$PCHQ/pieces/display"
printf 'kind=%s\nid=%s\nsel_x=%s\nsel_y=%s\nsel_z=%s\n' "$KIND" "$ID" "$SX" "$SY" "$SZ" \
  > "$PCHQ/pieces/display/pick.txt"
CAM=$(sed -n 's/^camera_mode=//p' "$PCHQ/pieces/system/board_state.txt" 2>/dev/null | head -1)
MX=$RX; MY=$RY
if [ "$CAM" = 1 ] && [ "$WW" -gt 0 ]; then
    MX=$((WX + WW / 2)); MY=$((WY + WH / 2))
fi
CTX_AT_X=$CX CTX_AT_Y=$CY MENU_X=$MX MENU_Y=$MY sh "$PCHQ/ops/pc_entity_ctx.sh" "$PCHQ" "$SX" "$SY" "$SZ" "$KIND" "${ID:-_}"
