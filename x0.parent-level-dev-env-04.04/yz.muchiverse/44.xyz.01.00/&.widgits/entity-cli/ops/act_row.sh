#!/bin/sh
# argv: command entity_dir
set -u
cmd="${1:-}"
ent="${2:-}"
house=$(cd "$(dirname "$0")/../../.." && pwd)
printf '%s\n' "$cmd" >> "$ent/cli_commands.txt"
case "$cmd" in
  attack)
    sh "$house/#.desktop/harnesses/two-facts/apply_range.sh" a1 b2 2 6 \
      "$house/#.desktop/db_hq_actors.state.txt" \
      "$house/&.widgits/db-hq/data/actors.pdl"
    ;;
  move)
    # REAL FIX 2026-09-26, direct instruction ("its supposed to happen
    # when user clicks Move" - referring to the PLACE_RANGE placing
    # grid, which had ZERO real UI trigger anywhere in the house before
    # this, confirmed via a house-wide grep). Wires Move to the SAME
    # real arm-brush + arm-placer chain palettes_menu.sh's own
    # arm_rmmv() already uses (identical &.widgits/palettes/state
    # STATE_DIR convention - this is not a new mechanism, just a new
    # caller of the existing one). PLACE_RANGE default (2) matches the
    # only other real range value in the house (this same file's own
    # `attack` case, apply_range.sh's a1/b2/2/6 demo). The brush
    # (World_a2/003) is a real, existing rmmv sprite used only as a
    # placeholder "move target" marker so a click actually places
    # something instead of silently failing ("no armed rmmv brush") -
    # a dedicated move-marker asset is a separate, not-yet-made design
    # decision. Real, honest limitation: clicking a cell after this
    # currently STAMPS A NEW TILE via the existing placement mechanism
    # (same as any palette pick) - it does not yet relocate/delete the
    # original entity. That's real follow-up work, not silently
    # invented here.
    state_dir="$house/&.widgits/palettes/state"
    desk_dir="$house/#.desktop"
    sprite_dir="$house/#.desktop/sprites/rmmv/World_a2/003"
    mkdir -p "$state_dir" "$desk_dir/tiles"
    "$house/&.widgits/tile-picker/ops/+x/tp_set_brush_rmmv.+x" "$state_dir" "$sprite_dir" "World" "A2" "move-target" >/dev/null 2>&1 || true
    printf 'move ARMED: World/A2 "move-target" - click desktop to place, Esc to cancel\n' > "$state_dir/rmmv_armed.txt"
    PLACE_RANGE="${MOVE_RANGE:-2}" setsid "$house/&.widgits/tile-picker/ops/+x/tp_arm_placer_rmmv.+x" "$state_dir" "$desk_dir" >/dev/null 2>&1 < /dev/null &
    echo "move recorded, grid armed (range ${MOVE_RANGE:-2})"
    ;;
  use)
    echo "$cmd recorded"
    ;;
  *)
    echo "act: $cmd"
    ;;
esac
