# pc-hq Move, the shared range library, and TAKE — status and roadmap

Written 2026-10-05, end of the session that built pc-hq Move. Read this
before touching Move, the range finder, the placer, or adding Take /
Place / Mine / Build. It says what is built, what was only checked by
reading, what is still open, and in what order to do it.

## 0. The rule that shaped this work

**The desk (livedesk) is the functional parent.** Direct instruction,
2026-10-05: "the desk was working first / better than pc-hq. we should
have used it as the functional parent." When pc-hq and the desk do the
same thing, start from the desk's code and data, lift the pure logic into
a text-included `_shared-lib` file, and let each environment keep only
what is specific to it (how it draws, where it stores the result). If
they disagree, the desk wins.

This session did it in the wrong order: pc-hq's Move range was built
first (a hardcoded, always-on diamond), then reconciled to the desk. The
cost was two reworks and one regression (§3, z levels). Do not repeat.

Related standard: `@.apps/piececraft-hq/RENDER-STANDARD.md` — **3D is the
real thing, 2D is only a render of it.** State is world-coordinate files;
both renderers read them; 2D never owns state.

## 1. What is built (all committed on branch `claude`)

| Commit | What |
|---|---|
| `83d858dc4` | Survey of why the Move range was always on (`@.apps/piececraft-hq/MOVE-RANGE-SURVEY.md`) |
| `3296936fc` | Range finder is a real flow: one switch file, Esc closes, Enter places, placer armed on open, redraw trigger |
| `6c5fcb9ad` | Typed cell-jump, HUD label, animated move, hero-centred range, Cli-io on pc-hq context menus |
| `db527e435` | Desk and pc-hq consume one shared library: `_shared-lib/khtpm_move_range.c` |
| `f390997eb` | Z levels restored (true 3D diamond), thin external-style range, bold placer, click select/place, board-edge clamp, per-step ledger line, step-race fix |

Behaviour, as verified live through the relay (bottom-taskbar hero →
`3` + Enter on Act → Enter on Move):

- Move from the hero's Act menu opens the range finder. It is **open
  while `pieces/display/move_range_matrix.txt` exists** and nothing is
  drawn when it does not.
- The range is a **true 3D diamond** centred on the entity being moved
  (the hero), drawn in 3D as thin dim wire cells; 2D renders the
  hero-level slice of the same data.
- The green placer is armed on the hero. **Arrows (relay codes
  1000–1003) move it in x/y; `z`/`x` move it down/up a level.** Letters +
  digits type a cell ref ("jump: g15_"), Enter jumps, Enter again places,
  Esc cancels. The HUD shows `move <entity> -> <ref> (x,y,z)` and adds
  `OUT OF RANGE` when the selector is off the field.
- Place validates (x,y,z) against the range and the board edge, then the
  hero **animates** to the target through x, y and z together at one step
  per 90 ms, the desk's cadence.
- Each step appends `entity_move` to `data/master_ledger.txt`.
- Context menus for pc-hq entities now have the same **Cli-io** field as
  a desk entity, wired to the shared `entity_cli_commit.sh`.

## 2. Architecture and file map

Shared (pure, text-included, no I/O policy) — `&.widgits/_shared-lib/`:

- `khtpm_move_range.c` (`mvr_*`): range matrix load + in-range test (with
  a depth map so the 2D matrix has a consistent 3D form), straight stepped
  path planner, waypoint queue (`animation_queue.txt` + `.cursor`, an
  append-only ledger plus cursor).
- `khtpm_grid_jump.c`: the ref parser/buffer (already shared with csv-hq's
  `<grid>`).

Consumers:

| Environment | Files | What is its own |
|---|---|---|
| Desk | `tile-picker/ops/tp_arm_placer_rmmv.c`, `entity-cli/ops/move_entity_init.c`, `move_entity_tick.c` | X11 overlay, pixel coordinates (step 8), `desktop_pos.txt`, `move_entity.pal` ticks it |
| pc-hq | `board-viewer/ops/bv_move_range.c` (glue), `bv_render_3d.c`, `bv_render_2d.c`, `bv_menu_input.c`, `bv_dispatch.c` | Wire cells in the 3D board, cell coordinates (step 1), `pieces/<id>/state.txt`, `bv_dispatch` ticks it |

Shared writers/entry points that both already used: `tp_gen_range_matrix.+x`
(shape, radius from `desk_grid.pdl` `move_view_range`),
`move_entity_on_desk.sh` (the Move verb), `entity-cli/skills.pdl` +
`act_row.sh` (verbs), `entity_cli_commit.sh` (Cli-io).

pc-hq data files (real project, `@.apps/piececraft-hq/pieces/`):

| File | Meaning |
|---|---|
| `display/move_range_matrix.txt` | **The switch.** Present = range finder open |
| `display/move_range_entity.txt` | `entity=<id>` Move was chosen for |
| `<id>/animation_queue.txt` + `.cursor` | Waypoints, same format the desk uses |
| `display/move_active.txt` | Which entity has a pending queue (removed when drained) |
| `system/move_range_style.pdl` | **Look, tunable live, no rebuild**: `STYLE \| range_edge`, `range_dim`, `range_color`, `placer_edge`, `placer_color` |

Session files (per engine session, not the real project):
`display/placer.txt` (armed, x, y, z), `display/move_jump.txt` (typed ref).

## 3. Why the range had no z levels for a while (regression, fixed)

The original always-on diamond looped over dx, dy and dz. My first
matrix-driven replacement (`3296936fc`) drew it **flat at one level**,
even writing "flat at oz" in the comments — a regression I introduced.
Fixed in `f390997eb`: the matrix gets a depth map (Manhattan distance to
its rim), and a cell is in range at height dz when its depth > |dz|, i.e.
|dx|+|dy|+|dz| ≤ R for the diamond. Any shape the writer emits gets a
consistent 3D form.

## 4. Verified vs only compiled

Verified live this session: range open/close, arrows, `z`/`x`, typed
ref + jump, Enter place with z animation, Esc, hero z 17→18, queue files
created and removed, ledger line written, the desk move of
`terumon_001_ember` through the refactored ops (11 waypoints, cursor
advanced, queue cleaned, position restored), Cli-io text reaching
`cli_commands.txt`.

**Compiled but not exercised live:** mouse click-select / click-again
place (the relay's `MOUSE_EVENT` does not go through the real X button
path, so no canvas click is published; to test, write
`#.desktop/pchq_canvas_click.txt` as "cx cy cw ch" the way the renderer
does), the 2D-view range, the desk placer's click and labelled-grid
overlay after its matrix code was swapped for the shared one.

**Seen once, fix unverified:** a duplicated `entity_move` ledger line for a
single waypoint. `bvr_step` now claims the tick and advances the cursor
before side effects; it was not re-observed afterwards.

## 5. Roadmap, in order

### R1. Where the hero's position lives (biggest open item)

`18.pc-hq/PAGE-FILE.md` (2026-09-30) is the agreed direction: the hero is
one `DESK | name | path | x_px | y_px | cell_x | cell_y | glyph | n` row in
the livedesk page file, a move in either window writes **that row**, and
`pieces/hero_01/state.txt` is listed as the old private position that
pc-hq stops using. **pc-hq Move currently reads and writes `state.txt`.**
Consequences seen: on a page-bound view the displayed hero (and the
xelector that follows it) can sit a few cells from the range, because the
renderer draws from the page rows while Move centred on `state.txt`.

To do: make the origin and the write go through the same row the renderer
draws. **Open question:** the `DESK` row has no z field, but Move now has
z levels — where does z live (a new field, the existing page `cz` that
`page_row_meta` returns, or per-entity state)? Needs an owner decision
before the write is moved.

### R2. Post-move hook (documented, not wired)

Documented behaviour after a move: `civ-vs-piece.md` (2026-08-03) —
"movement ends a tick AND a manual End Turn button stays available";
`human-dev.md` — `tick_animals()` runs after every `advance_tick()` at
MOVE/JUMP/MINE/BUILD/END_TURN; `EVENT-TRIGGER-LAYER-PLAN.md` (2026-09-14)
— the POST-move hook is `pc_menu_input.c`'s `MOVE` handler, which checks
`player-touch` triggers at the new position and appends `touched_npc`.

Move placement currently does **none** of these. Plan: when a queue
drains (arrival), send the existing `MOVE` action to the host inbox
(`send_action_to_host`) so the tick, NPC step, `move` ledger line and
touch trigger all run through the path the docs already name. Earlier in
this session I called ending the turn "a game-design call"; the docs say
otherwise.

### R3. Z-falloff shape — owner decision

`design-docs/PCHQ-3D-MOVE-OVERLAY-DESIGN.md` §3 leaves open whether the z
falloff is spherical/octahedral or "the same shape at every z". I
implemented **octahedral** (via the depth map) without having read that
doc. That doc also planned per-z matrix files and a shared
`_shared-lib/range_matrix.c`; the depth map made per-z files unnecessary
and the shared file is `khtpm_move_range.c`. Confirm or change.

### R4. Desk parity leftovers

- Obstacle-aware pathfinding (both sides use a straight stepped path today).
- Verify the mouse path live (§4) and the desk placer after the refactor.
- Keys reach the engine only while Interact is on, as before.
- The desk's world-manager ledger append has a pc-hq equivalent only as the
  `entity_move` line; whether other pc-hq systems should read it is open.

### R5. TAKE (design only, nothing built)

Idea (owner, 2026-10-05): a **Take** verb that fires events turning an
entity into an item in the taker's inventory, reusing the desk's inventory
logic (the one used by desktop drag-and-drop).

How the desk's inventory works (read, not changed): an entity's inventory
is its `inventory/` directory, and an item is the same object as an
entity, just nested (`event_auto_clacker.sh`: "a pal and an inventory
item are the SAME real object/directory"). Putting one in is a plain `mv`
(`file-explorer/ops/fe_drop.sh`; `fe_place_on_desk.sh` for a click in
another window's drop zone). Taking out is the placer
(`tp_arm_placer_rmmv` + `FE_PLACE_CLICK`).

Plan, desk first (rule §0):

1. Add `SKILL | Take | take` to `entity-cli/skills.pdl` and a `take)` case
   to `act_row.sh`. The desk's Act menu and pc-hq's both read that table,
   so the menu entry appears in both.
2. New `entity-cli/ops/take_entity.+x`: range from the shared
   `khtpm_move_range.c`, target picked with the same placer, `mv` the
   target's directory to `<taker>/inventory/<id>`, append a `take` ledger
   event, and despawn the target. **Unverified:** how a live desk entity's
   window is closed and its page row removed on despawn.
3. pc-hq: the same op on `pieces/<id>` + `data/master_ledger.txt`.

Open questions, with the defaults to use if not answered:
reach = same range as Move (not adjacent-only); events = ledger line only
for a first cut (a `taken` hook through events-hq `play_event.sh` later);
build order = desk first, then mirror into pc-hq.

### R6. Place / Mine / Build / Inventory (documented, stubbed)

`PIECECRAFT_XYZ_DESIGN.md` §7 (2026-08-03): Break reads the target cell,
looks up its drop, overwrites it with air, adds the drop to
`pieces/hero_01/inventory.txt` and bumps the tick; Place is the inverse
with a validity check (target is air and adjacent to a solid cell);
inventory is slot key-values (`slot_0=dirt:12`). `civ-vs-piece.md`: Mine
is `g`, Build is `h`, as raw keys. `ops_bank` names `PLACE_BLOCK`,
`BREAK_BLOCK`, `CRAFT`, `INSPECT`. Today `MINE` and `BUILD` in
`pc_menu_input.c` are honest stubs and `CTX_PLACE` says "pick a block
palette (todo)". TAKE (R5) overlaps with the inventory half of this; do
them together so there is one inventory format.

### R7. Known limits and debt

- The GPU scene holds at most **128 boxes** (`BV_GPU_MAX_BOX`). A radius-3
  diamond is about 60 wire cells on top of entities; if the range ever
  looks cut off at the edges, raise it (the shader arrays must match).
- The range is thin wire only; there is no real glow.
- Engine sessions copy `ops/` at launch: rebuild, `bash button.sh kill`,
  relaunch (`open_pchq_board.sh`) or the old binaries keep running.
- Test runs moved `hero_01` and `xelector_01` in their `state.txt` files
  (the hero's original cell was 2,5,17); they show as modified in git.

## 6. How to drive it through the relay (test recipe)

1. Relaunch pc-hq (see R7), find the board's `khtpm_core_render` pid
   (command line contains `pchq-board`).
2. Append `MOUSE_EVENT: 1 110 1093 1` then `... 0` to
   `#.desktop/entity_menu_history/<pid>.txt` (the bottom-taskbar hero
   button). The first click may only focus; repeat once if no menu appears.
3. On the context menu's pid file send `KEY_PRESSED: 51` and `13` (Act),
   then `KEY_PRESSED: 13` on the Act menu's pid file (Move is first).
4. Engine keys go to the session's
   `pieces/apps/player_app/interact_relay.txt`, one code per line: arrows
   `1000` L, `1001` R, `1002` U, `1003` D; `120` = `x` (up a level),
   `122` = `z` (down); letters/digits as ASCII for a typed ref; `13` Enter;
   `27` Esc.
5. Capture with `&.widgits/_shared-lib/ops/+x/dump_frame_png_op.+x
   <window-id> <out.png>` (the board window is the 1447x1114 one).
6. Do not send keys to a session someone else is driving; they land in
   the same game.
