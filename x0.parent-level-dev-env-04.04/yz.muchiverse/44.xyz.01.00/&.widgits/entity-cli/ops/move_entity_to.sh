#!/bin/sh
# move_entity_to.sh <house_root> <entity_dir> <x> <y>
#
# REAL FIX 2026-09-26, direct instruction ("the agent just needs
# inject position, doesn't need to use mouse"): move_entity_on_desk.sh
# (same dir) reuses the real interactive full-screen placing grid
# (tp_arm_placer_rmmv.+x) - correct for a human, but real live testing
# just showed it is disruptive for normal desktop use (grid covers the
# whole screen, blocks clicking anything else) and synthetic
# mouse/keyboard delivery to it is NOT reliable for an automated caller
# (confirmed live: a synthetic xdotool key crashed the placer process
# outright - HOUSE_CODE_PITFALLS.md #23, no custom X error handler).
#
# This script is the real, reliable, NO-UI alternative for exactly
# that caller: an agent (or any automated script) that already knows
# the target REFERENCE px (desktop_pos.txt's own unit - identity on
# the reference screen, khtpm_ui_scale.c converts for any other real
# screen) writes it directly - no grid, no click, no keyboard, nothing
# that depends on real/synthetic input delivery at all. This is the
# path an in-game AI agent (or a test) should use to move an entity,
# not the interactive grid.
#
# The interactive grid + its real UX problems (full-screen, no click-
# vs-double-click distinction, no range limit) are real, scoped,
# NOT fixed here - see 00-compact/compact-fix-guide.md's own
# "PLACE_RANGE placing grid" section for the deferred redesign notes.
set -eu
HOUSE="${1:-}"
ENT="${2:-}"
X="${3:-}"
Y="${4:-}"
[ -n "$HOUSE" ] && [ -d "$ENT" ] || { echo "move_entity_to.sh: need house_root and entity_dir" >&2; exit 1; }
case "$X" in *[!0-9-]*|"") echo "move_entity_to.sh: x must be a real integer" >&2; exit 1 ;; esac
case "$Y" in *[!0-9-]*|"") echo "move_entity_to.sh: y must be a real integer" >&2; exit 1 ;; esac

printf 'x=%s\ny=%s\n' "$X" "$Y" > "$ENT/desktop_pos.txt"

# Kill this entity's own currently-live process (showing at the OLD
# position right now) before relaunching it at the new one - same real
# kill-then-relaunch idiom this house already uses elsewhere
# (palettes_menu.sh's launch_cat(), launch_khtpm_menu(),
# move_entity_on_desk.sh's own copy of this same block).
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
echo "moved to x=$X y=$Y"
