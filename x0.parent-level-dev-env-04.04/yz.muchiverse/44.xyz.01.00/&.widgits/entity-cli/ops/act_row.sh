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
    # REAL FIX 2026-09-26, direct instruction ("agent should be able to
    # move entity using placer reliably") - supersedes this same file's
    # first Move wiring (commit 6e2a58d7, the stamp/brush path), which
    # created a brand-new tile instead of relocating the original
    # entity - a real, honest limitation flagged at the time and fixed
    # here. Reuses move_entity_on_desk.sh (same dir), which itself
    # reuses FE_PLACE_CLICK, the exact same real short-circuit File
    # Explorer's own drag-and-drop already relies on - no stamping, a
    # real single-entity relocation. This call is already backgrounded
    # by the renderer's own dispatch_action() (every action= gets a
    # trailing `&` appended before exec, see that function's own
    # system(cmd) call) - safe to block here waiting for the real
    # click/keyboard-jump-and-place, same as fe_place_on_desk.sh does.
    sh "$house/&.widgits/entity-cli/ops/move_entity_on_desk.sh" "$house" "$ent"
    ;;
  use)
    echo "$cmd recorded"
    ;;
  *)
    echo "act: $cmd"
    ;;
esac
