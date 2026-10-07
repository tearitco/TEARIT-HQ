#!/bin/sh
# move_entity_on_desk.sh <house_root> <entity_dir>
#
# REAL FIX 2026-09-27, direct instruction ("sh should only be used for
# launching .pal and ops, for the rest ... no logic"): this used to
# own grid-snap arithmetic, an awk-parsed desk_grid.pdl read, and a
# branching animate/instant-move/fallback block - all real logic that
# PRISC-OPS-ARCHITECTURE.md's "Bootstrap vs. Production" section
# explicitly forbids in shell. That logic now lives entirely in
# move_entity_init.+x (grid-snap, clamp, waypoint pathfinding, pal-
# loop launch). This script is a pure launcher: arm the interactive
# placer (an op), read its raw click coords (a data read, not a
# computation), hand off to move_entity_init.+x.
#
# Older history (still true): supersedes the first Move wiring
# (commit 6e2a58d7, the stamp/brush path, which created a new tile
# instead of relocating the entity). Uses FE_PLACE_CLICK, the same
# short-circuit File Explorer's own drag-and-drop already relies on -
# tp_arm_placer_rmmv.+x writes the clicked/jumped cell's reference-px
# x/y to that file and returns without stamping any tile at all.
set -u
HOUSE="${1:-}"
ENT="${2:-}"
[ -n "$HOUSE" ] && [ -d "$ENT" ] || { echo "move_entity_on_desk.sh: need house_root and entity_dir" >&2; exit 1; }

# Single-instance hygiene: kill any stray placer already open for this
# entity (same idiom palettes_menu.sh's launch_cat() uses elsewhere).
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

# REAL, 2026-09-30, direct instruction ("placer should read placement
# layout from an external matrix.txt... an op can write that based on
# range of character, like a writer/renderer architecture"): regenerate
# the range matrix (a plain data read/write, done by an op, not shell)
# fresh before every arm - tp_gen_range_matrix.+x is the writer,
# tp_arm_placer_rmmv.+x (below) is the renderer and has no shape logic
# of its own, it only reads TP_RANGE_MATRIX.
GEN="$HOUSE/&.widgits/tile-picker/ops/+x/tp_gen_range_matrix.+x"
MATRIX="$ENT/move_range_matrix.txt"
rm -f "$MATRIX"
if [ -x "$GEN" ]; then
    "$GEN" "$HOUSE" "$MATRIX" || true
fi
[ -f "$MATRIX" ] && export TP_RANGE_MATRIX="$MATRIX"

# A piececraft / pc-hq entity lives under pieces/. Its range is the
# same '#' matrix, drawn as voxels inside the 3D window by
# bv_render_3d.c. The desk X11 diamond must not open for that entity.
case "$ENT" in
  */pieces/*)
    proj=${ENT%%/pieces/*}
    disp="$proj/pieces/display"
    mkdir -p "$disp"
    if [ -f "$MATRIX" ]; then
      # move_range_matrix.txt PRESENT = range finder open (see
      # board-viewer/ops/bv_move_range.c). The sidecar names who Enter
      # relocates. Esc / a confirmed pick delete both. The entity-dir
      # copy is scratch - drop it so it can't go stale.
      printf 'entity=%s\n' "$(basename "$ENT")" > "$disp/move_range_entity.txt"
      cp "$MATRIX" "$disp/move_range_matrix.txt"
      rm -f "$MATRIX"
    fi
    echo "pc-hq range stays in the 3D window: $disp/move_range_matrix.txt"
    exit 0
    ;;
esac

ARM="$HOUSE/&.widgits/tile-picker/ops/+x/tp_arm_placer_rmmv.+x"
[ -x "$ARM" ] || { echo "move_entity_on_desk.sh: no placer binary" >&2; exit 1; }
"$ARM" "$ENT" "$HOUSE" || exit 1
[ -f "$CLICK" ] || exit 0

x=$(grep '^x=' "$CLICK" | sed 's/^x=//')
y=$(grep '^y=' "$CLICK" | sed 's/^y=//')
rm -f "$CLICK"
[ -n "$x" ] && [ -n "$y" ] || exit 1

INIT="$HOUSE/&.widgits/entity-cli/ops/move_entity_init.+x"
[ -x "$INIT" ] || { echo "move_entity_on_desk.sh: no move_entity_init op" >&2; exit 1; }
"$INIT" "$HOUSE" "$ENT" "$x" "$y"
