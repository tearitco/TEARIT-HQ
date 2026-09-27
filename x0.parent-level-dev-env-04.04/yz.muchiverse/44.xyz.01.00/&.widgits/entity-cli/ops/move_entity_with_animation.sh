#!/bin/sh
# move_entity_with_animation.sh <house_root> <entity_dir> <target_x> <target_y>
#
# DESIGN PATTERN 2026-09-27: entity animation via sequential position
# updates to the entity's own ledger (see entity-animation-pattern.md).
#
# Real, game-engine-native animation: instead of one-shot kill/relaunch,
# write a sequence of waypoints to the entity's position file. The entity's
# own game loop reads and renders each position smoothly, keeping the
# animation within game engine constraints (physics, collision, etc.).
#
# Pathfinding algorithm is pluggable (PATHFIND_OP env var or .pdl config);
# default is linear/straight-line. Each waypoint is rendered as the entity
# updates its own position from the shared ledger.

set -u
HOUSE="${1:-}"
ENT="${2:-}"
TX="${3:-}"
TY="${4:-}"

[ -n "$HOUSE" ] && [ -d "$ENT" ] && [ -n "$TX" ] && [ -n "$TY" ] || {
	echo "move_entity_with_animation.sh: need house_root, entity_dir, target_x, target_y" >&2
	exit 1
}

# Read current position
OX=$(grep '^x=' "$ENT/desktop_pos.txt" 2>/dev/null | head -1 | sed 's/^x=//')
OY=$(grep '^y=' "$ENT/desktop_pos.txt" 2>/dev/null | head -1 | sed 's/^y=//')
case "$OX" in ''|*[!0-9]*) OX=0 ;; esac
case "$OY" in ''|*[!0-9]*) OY=0 ;; esac

# Pathfinding op - default to linear, can override via env or .pdl
PATHFIND="${PATHFIND_OP:-}"
if [ -z "$PATHFIND" ]; then
	PATHFIND="$HOUSE/&.widgits/entity-cli/ops/pathfind_linear.sh"
fi
[ -x "$PATHFIND" ] || PATHFIND="$HOUSE/&.widgits/entity-cli/ops/pathfind_linear.sh"

# Step size (px per frame) - can be tuned in desk_grid.pdl or env
STEP="${ANIM_STEP:-8}"
case "$STEP" in *[!0-9]*) STEP=8 ;; esac

# Generate waypoints to a temp file
WAYPOINTS="/tmp/waypoints_$$.txt"
trap "rm -f '$WAYPOINTS'" EXIT
"$PATHFIND" "$OX" "$OY" "$TX" "$TY" "$STEP" "$WAYPOINTS" || exit 1

# Animate through waypoints: update entity's position file for each step,
# with a small delay between writes to let the entity's game loop render each
FRAME_MS=50  # ~50ms per frame (20fps) - tune for desired animation speed
count=0
while IFS= read -r line; do
	case "$line" in x=*|y=*) ;; *) continue ;; esac
	case "$line" in
	x=*) x_val="${line#x=}" ;;
	y=*) y_val="${line#y=}" ;;
	esac
	if [ -n "${x_val:-}" ] && [ -n "${y_val:-}" ]; then
		printf 'x=%s\ny=%s\nz=0\n' "$x_val" "$y_val" > "$ENT/desktop_pos.txt"
		count=$((count + 1))
		# Small delay to allow entity's game loop to render this frame
		# Not a blocking sleep (which would lock the entity) - just marks
		# time so the entity can read, render, and update display
		if [ "$count" -lt $(wc -l < "$WAYPOINTS") ]; then
			sleep 0.05  # ~50ms per waypoint
		fi
		x_val=""
		y_val=""
	fi
done < "$WAYPOINTS"

echo "animated: $count waypoints from ($OX,$OY) to ($TX,$TY)"
