#!/bin/bash
# move.sh - v1 move op. Moves on a simple grid (pos_x/pos_y in state).
# Usage: move.sh <dx> <dy>
. "$(cd "$(dirname "$0")/../brain" && pwd)/oplib.sh"

DX="${1:-0}"
DY="${2:-0}"
[ "$DX" -lt -1 ] && DX=-1
[ "$DX" -gt 1 ] && DX=1
[ "$DY" -lt -1 ] && DY=-1
[ "$DY" -gt 1 ] && DY=1

x="$(read_state pos_x)"
y="$(read_state pos_y)"
write_state pos_x $((x + DX))
write_state pos_y $((y + DY))

ledger_append "Move" "moved ($DX,$DY) to ($((x+DX)),$((y+DY)))" "move.sh"
echo "Move: now at ($((x+DX)),$((y+DY)))"
