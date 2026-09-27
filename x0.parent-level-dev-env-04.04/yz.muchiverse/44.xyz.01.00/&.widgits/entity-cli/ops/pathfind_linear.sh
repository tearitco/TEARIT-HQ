#!/bin/sh
# pathfind_linear.sh <old_x> <old_y> <new_x> <new_y> <step_px> <output_file>
#
# DESIGN PATTERN 2026-09-27: entity animation via sequential position
# updates (see entity-animation-pattern.md).
#
# Linear pathfinding op: writes waypoints from old_pos to new_pos,
# one per line: x=N y=N. Entity's game loop reads and renders each,
# creating smooth motion. Step size (px per frame) is configurable.
#
# Simple default implementation - can be replaced with A*, tile-
# constrained, avoidance, etc. - same interface, different algorithm.

set -u
OX="${1:-0}"
OY="${2:-0}"
NX="${3:-0}"
NY="${4:-0}"
STEP="${5:-8}"
OUT="${6:-}"

[ -z "$OUT" ] && { echo "pathfind_linear.sh: need all 6 args" >&2; exit 1; }
[ "$STEP" -le 0 ] && STEP=8

# Distance to travel
dx=$((NX - OX))
dy=$((NY - OY))

# Total distance (Manhattan for simplicity, can use Euclidean)
total=$(( (dx < 0 ? -dx : dx) + (dy < 0 ? -dy : dy) ))

[ "$total" -eq 0 ] && {
	printf 'x=%d\ny=%d\n' "$NX" "$NY" > "$OUT"
	exit 0
}

# Number of steps needed
steps=$(( total / STEP ))
[ "$steps" -lt 1 ] && steps=1

# Write each waypoint
x="$OX"
y="$OY"
i=0
while [ "$i" -le "$steps" ]; do
	if [ "$i" -eq "$steps" ]; then
		x="$NX"
		y="$NY"
	else
		# Linear interpolation
		x=$((OX + dx * i / steps))
		y=$((OY + dy * i / steps))
	fi
	printf 'x=%d\ny=%d\n' "$x" "$y" >> "$OUT"
	i=$((i + 1))
done
