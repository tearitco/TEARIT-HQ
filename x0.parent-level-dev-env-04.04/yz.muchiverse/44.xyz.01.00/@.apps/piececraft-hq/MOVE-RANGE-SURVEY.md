# pc-hq Move range: why it is always on, and a recommended fix

Surveyed 2026-10-05. Code-reading only, nothing changed yet.

## Intended behaviour

On the desk, Move is a context-menu action. The range diamond appears only
while the placer is armed, and it goes away on a click or Escape
(`tp_arm_placer_rmmv.+x` exits). pc-hq should mirror that: pick **Move** from
the hero's context menu, see the range, pick a cell (or Escape), range gone.

## What happens now

There are two independent things drawing a range in pc-hq, and neither is
tied to Move being active.

1. **3D diamond is unconditional.** `bv_render_3d.c` ~line 3255, the block
   commented "3D diamond, range 2": `if (g_xelector_present || g_hero_present)`
   draws every cell within Manhattan distance 2 as yellow wireframe, every
   frame. Radius is the literal `int rad = 2;`. It was added for testing in
   `a59f0852d` / `609902d8d` and never gated. This is the one you see
   perpetually in the 3D window.
2. **2D diamond follows a stale file.** `bv_render_2d.c` ~line 1078 ("Desk
   diamond") draws one tile per `#` from
   `pieces/display/move_range_matrix.txt` whenever the file exists and the
   hero is on screen. `move_entity_on_desk.sh` writes that file for pc-hq
   entities (`*/pieces/*` branch, copies `$MATRIX` to `pieces/display/`) and
   exits. **Nothing ever deletes it.** Both
   `pieces/display/move_range_matrix.txt` and
   `pieces/hero_01/move_range_matrix.txt` exist now (11:29 today), so the 2D
   view keeps drawing it too.

Related mismatches:
- The 3D diamond ignores the matrix. The matrix is the designed source of
  truth (`tp_gen_range_matrix.+x` is the writer, radius from
  `desk_grid.pdl`'s `move_view_range`, 2026-09-30 instruction), but 3D has a
  hardcoded radius 2 and a 3D (dz) shape. The matrix is 2D.
- The pc-hq branch of `move_entity_on_desk.sh` stops after publishing the
  matrix. There is no confirm step that turns a picked cell into a move, and
  nothing that ends the "armed" state. Escape handling exists for the green
  selector (`placer.txt`, `armed=0` in `bv_menu_input.c` ~line 560) but
  not for the range.

## Recommended fix

Make `pieces/display/move_range_matrix.txt` the one switch: **present = Move is
active, absent = nothing drawn.** It already works that way for 2D, so this
reuses the existing writer/renderer split.

1. **3D reads the matrix, not a hardcoded diamond.** In `bv_render_3d.c`
   replace the unconditional block with the same read 2D uses: skip entirely
   if the file is missing, otherwise draw one wire cell per `#` centred on the
   hero/xelector cell (same `cx0/cy0 = dim/2` centring as 2D). Keep dz = 0 for
   v1 (a flat diamond at the entity's level) and decide on a vertical shape
   later by teaching `tp_gen_range_matrix.+x` a `--shape`.
2. **Something must delete the file.** Cleanest is `bv_menu_input.c`, which
   already owns Escape and the placer: on Escape, or on a confirmed pick,
   `unlink` `pieces/display/move_range_matrix.txt` (and the entity-dir copy),
   next to the existing `armed=0` write, then `bump_screen_changed`.
3. **Confirm step.** When the pc-hq placer is armed by Move and the user picks a
   cell, validate it against the matrix, write the click file Move already
   expects (`FE_PLACE_CLICK` style `x=`/`y=`) or call `move_entity_init.+x`,
   then delete the matrix. This is the part that makes it a mirror of the desk
   flow and not just a display toggle.
4. **Remove the leftover files now** as a one-off so the current session
   stops showing it: `rm` both `move_range_matrix.txt` files.

Smaller alternative (stop-gap, ~10 lines): do only steps 2 and 4 plus gate the
3D block on file existence. Range then appears only after Move is chosen, and
disappears on Escape, but picking a destination is still unimplemented.

## Not yet verified

- How the pc-hq context menu's Move verb reaches `move_entity_on_desk.sh`.
  `ctx_menu/append.sh` posts `CTX_<VERB>` lines to
  `pieces/system/widget_cmds/inbox.txt`; I did not trace who consumes that
  inbox for Move. Step 1-2 do not depend on it; step 3 does.
- Whether the xelector (cyan cursor) or the hero should be the range origin
  when both exist; the 3D code currently prefers the xelector.
- Not run live: the board viewer is a stale session copy until relaunched, so
  any fix needs `scripts/build.sh`, `bash button.sh kill`, relaunch.

## Status (2026-10-05, later): implemented

Full fix landed (see the commits after `83d858dc4`). What exists now, all
verified live through the relay (bottom-taskbar hero, Act, Move):

- Range = `pieces/display/move_range_matrix.txt` (present = open), drawn in
  3D and rendered in 2D from the same shared helper `bv_move_range.c`.
- Origin = the entity Move was chosen for (the hero), not the xelector.
  (The first version centred on the xelector; corrected on review.)
- Opening Move arms the green placer on the hero. Arrows (1000-1003) move it.
  Letters+digits type a cell ref ("jump: g15_", shared `khtpm_grid_jump.c`),
  Enter jumps, Enter again places, Esc cancels. z/x change the placer z when
  no ref is being typed.
- Place animates: a waypoint ledger (`move_path.txt`) stepped every 90 ms by
  `bv_dispatch.c`, same cadence as the desk's `move_entity.pal`.
- HUD label: `move <entity> -> <ref> (x,y,z)` plus `jump:` / key hints.
- pc-hq context menus have the desk's Cli-io field (shared
  `entity_cli_commit.sh`, via a generated `cli.sh` shim).

Known limits: the path is a straight cell path (no obstacle pathfinding like
the desk's waypoint pathfinder); the range is a flat diamond at the entity's z;
raw arrow/letter keys only reach the engine while Interact is ON, as before.
