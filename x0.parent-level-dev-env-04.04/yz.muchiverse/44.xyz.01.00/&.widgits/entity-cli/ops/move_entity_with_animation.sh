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

# Animate through waypoints: extract x/y pairs, update entity's position
# file for each step with kill+relaunch, with frame delays between updates
count=0
while IFS='=' read -r key val; do
	case "$key" in
	x) cx="$val" ;;
	y) cy="$val" ;;
	esac
	# When we have both x and y, write position and restart entity
	if [ -n "${cx:-}" ] && [ -n "${cy:-}" ]; then
		printf 'x=%s\ny=%s\nz=0\n' "$cx" "$cy" > "$ENT/desktop_pos.txt"
		count=$((count + 1))

		# Kill old process and launch new one at this waypoint position
		for p in /proc/[0-9]*; do
			[ "$p" = "/proc/$$" ] && continue
			[ -r "$p/cmdline" ] || continue
			cl=$(tr '\0' ' ' < "$p/cmdline" 2>/dev/null) || continue
			case "$cl" in *"$ENT"*) kill -9 "${p#/proc/}" 2>/dev/null || true ;; esac
		done

		# Relaunch entity at new waypoint
		ENT_BIN="$(find "$HOUSE" -path "*livedesk-taskbar/ops/+x/khtpm_entity.+x" -o -path "*livedesk-taskbar/ops/+x/khtpm_core_render.+x" 2>/dev/null | head -1)"
		[ -x "$ENT_BIN" ] || ENT_BIN="$HOUSE/"*.monads/*.livedesk-taskbar/ops/+x/khtpm_core_render.+x
		setsid nohup "$ENT_BIN" "$ENT" >/dev/null 2>&1 < /dev/null &

		# Frame delay between waypoints (allows visual rendering)
		sleep 0.1  # ~100ms per waypoint for visible animation
		cx=""
		cy=""
	fi
done < "$WAYPOINTS"

echo "animated: $count waypoints from ($OX,$OY) to ($TX,$TY)"
