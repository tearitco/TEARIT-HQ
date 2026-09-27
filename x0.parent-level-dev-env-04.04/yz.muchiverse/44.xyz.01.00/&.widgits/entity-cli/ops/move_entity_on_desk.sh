#!/bin/sh
# move_entity_on_desk.sh <house_root> <entity_dir>
#
# REAL FIX 2026-09-26, direct instruction ("agent should be able to
# move entity using placer reliably"): the first Move wiring
# (act_row.sh commit 6e2a58d7) reused the STAMP/brush placement path
# (tp_set_brush_rmmv.+x + tp_arm_placer_rmmv.+x's default click ->
# tp_place_desktop_rmmv.+x hand-off), which creates a brand-new tile
# entity at the clicked cell - it never relocated the original entity,
# a real, documented, honest limitation at the time.
#
# This version reuses a DIFFERENT, already-proven real mechanism
# instead: FE_PLACE_CLICK, the same short-circuit File Explorer's own
# drag-and-drop placement already uses (fe_place_on_desk.sh, in
# &.widgits/file-explorer/ops/) - when this env var is set,
# tp_arm_placer_rmmv.+x writes the real clicked/jumped cell's
# reference-px x/y to that file and returns WITHOUT stamping any tile
# at all (see that binary's own FE_PLACE_CLICK branch, right before its
# default tp_place_desktop_rmmv.+x hand-off). This script then does the
# real "move" itself: snap to the real desk grid cell (same convention
# fe_place_on_desk.sh uses), rewrite the ENTITY'S OWN desktop_pos.txt
# (not a new entity's), kill its current live process (same real
# kill-then-relaunch idiom palettes_menu.sh's launch_cat() and
# launch_khtpm_menu() both already use), and relaunch it at the new
# position - a real, reliable, single-entity relocation, not a stamp.
#
# No PLACE_RANGE/brush arming needed here at all - FE_PLACE_CLICK
# bypasses that whole path.
#
# UPDATE 2026-09-26, direct instruction ("i still need some kind of
# grid, i just think its range should be reduced and i want the arrow
# controlled placer... we can do it now"): the placer itself now
# supports a real, range-limited view + move-then-confirm clicking
# (see tp_arm_placer_rmmv.c's own 2026-09-26 header addendum and
# desk_grid.pdl's move_view_range/place_confirm keys). This script
# passes the entity's OWN current position as TP_ORIGIN_X/TP_ORIGIN_Y
# so the grid centres on it instead of covering the whole screen.
set -u
HOUSE="${1:-}"
ENT="${2:-}"
[ -n "$HOUSE" ] && [ -d "$ENT" ] || { echo "move_entity_on_desk.sh: need house_root and entity_dir" >&2; exit 1; }

# REAL FIX 2026-09-26, direct live report ("it places 2 green placers
# on grid now"): two concurrent tp_arm_placer_rmmv.+x instances for the
# SAME entity (each drawing its own overlapping grid) - nothing
# stopped Move being triggered twice (e.g. clicking again because the
# target used to not show immediately - now fixed separately) from
# spawning a second placer alongside the first. Same real single-
# instance kill-before-relaunch idiom this house already uses
# elsewhere (palettes_menu.sh's launch_cat(), launch_khtpm_menu()).
for p in /proc/[0-9]*; do
    [ "$p" = "/proc/$$" ] && continue
    [ -r "$p/cmdline" ] || continue
    cl=$(tr '\0' ' ' < "$p/cmdline" 2>/dev/null) || continue
    case "$cl" in *"tp_arm_placer_rmmv"*"$ENT"*) kill "${p#/proc/}" 2>/dev/null || true ;; esac
done

CLICK="$ENT/move_click.txt"
rm -f "$CLICK"
export FE_PLACE_CLICK="$CLICK"

ox=$(grep '^x=' "$ENT/desktop_pos.txt" 2>/dev/null | head -1 | sed 's/^x=//')
oy=$(grep '^y=' "$ENT/desktop_pos.txt" 2>/dev/null | head -1 | sed 's/^y=//')
case "$ox" in ''|*[!0-9]*) ox="" ;; esac
case "$oy" in ''|*[!0-9]*) oy="" ;; esac
if [ -n "$ox" ] && [ -n "$oy" ]; then
    export TP_ORIGIN_X="$ox" TP_ORIGIN_Y="$oy"
fi

ARM="$HOUSE/&.widgits/tile-picker/ops/+x/tp_arm_placer_rmmv.+x"
[ -x "$ARM" ] || { echo "move_entity_on_desk.sh: no placer binary" >&2; exit 1; }
"$ARM" "$ENT" "$HOUSE" || exit 1
[ -f "$CLICK" ] || exit 0

x=$(grep '^x=' "$CLICK" | sed 's/^x=//')
y=$(grep '^y=' "$CLICK" | sed 's/^y=//')
rm -f "$CLICK"
[ -n "$x" ] && [ -n "$y" ] || exit 1

# Snap to the real desk grid cell (reference px) - same convention
# fe_place_on_desk.sh uses (was a hardcoded 64 there once too; read the
# real value, default 80 only if the config row is missing/malformed).
g=$(awk -F'|' '/cell_px/ { gsub(/[ \t]/, "", $3); print $3; exit }' "$HOUSE/#.desktop/desk_grid.pdl" 2>/dev/null)
case "$g" in ''|*[!0-9]*) g=80 ;; esac
[ "$g" -gt 0 ] 2>/dev/null || g=80
x=$(( (x / g) * g ))
y=$(( (y / g) * g ))

# REAL FIX 2026-09-27, direct instruction ("im still not seeing animated
# move. lets do that"): instead of a direct one-shot kill/relaunch, animate
# the entity sliding from its current position to the target via
# move_entity_with_animation.sh. Writes sequential waypoints, entity
# relaunches at each waypoint, creating smooth visible motion.
ANIM_SCRIPT="$HOUSE/&.widgits/entity-cli/ops/move_entity_with_animation.sh"
if [ -x "$ANIM_SCRIPT" ]; then
	# Get current position (old pos, to animate FROM)
	ox=$(grep '^x=' "$ENT/desktop_pos.txt" 2>/dev/null | head -1 | sed 's/^x=//' || echo "0")
	oy=$(grep '^y=' "$ENT/desktop_pos.txt" 2>/dev/null | head -1 | sed 's/^y=//' || echo "0")
	case "$ox" in ''|*[!0-9]*) ox=0 ;; esac
	case "$oy" in ''|*[!0-9]*) oy=0 ;; esac
	# Animate from old to new position
	ANIM_STEP="${ANIM_STEP:-8}"  # 8px per frame
	"$ANIM_SCRIPT" "$HOUSE" "$ENT" "$x" "$y" || {
		# Fallback to instant move if animation script fails
		printf 'x=%s\ny=%s\n' "$x" "$y" > "$ENT/desktop_pos.txt"
		for p in /proc/[0-9]*; do
			[ "$p" = "/proc/$$" ] && continue
			[ -r "$p/cmdline" ] || continue
			cl=$(tr '\0' ' ' < "$p/cmdline" 2>/dev/null) || continue
			case "$cl" in *"$ENT"*) kill "${p#/proc/}" 2>/dev/null || true ;; esac
		done
		sleep 0.2
		ENT_BIN="$HOUSE/"*.monads/*.livedesk-taskbar/ops/+x/khtpm_entity.+x
		[ -x "$ENT_BIN" ] || ENT_BIN="$HOUSE/"*.monads/*.livedesk-taskbar/ops/+x/khtpm_core_render.+x
		setsid nohup "$ENT_BIN" "$ENT" >/dev/null 2>&1 < /dev/null &
	}
else
	# No animation script; fall back to instant move
	printf 'x=%s\ny=%s\n' "$x" "$y" > "$ENT/desktop_pos.txt"
	for p in /proc/[0-9]*; do
		[ "$p" = "/proc/$$" ] && continue
		[ -r "$p/cmdline" ] || continue
		cl=$(tr '\0' ' ' < "$p/cmdline" 2>/dev/null) || continue
		case "$cl" in *"$ENT"*) kill "${p#/proc/}" 2>/dev/null || true ;; esac
	done
	sleep 0.2
	ENT_BIN="$HOUSE/"*.monads/*.livedesk-taskbar/ops/+x/khtpm_entity.+x
	[ -x "$ENT_BIN" ] || ENT_BIN="$HOUSE/"*.monads/*.livedesk-taskbar/ops/+x/khtpm_core_render.+x
	setsid nohup "$ENT_BIN" "$ENT" >/dev/null 2>&1 < /dev/null &
fi
echo "moved to x=$x y=$y"
