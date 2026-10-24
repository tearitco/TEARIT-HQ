# Events that touch a volume: the area an event affects, and the selection window (design, written 2026-10-24)

Owner 2026-10-24: "in the future when we make a multi voxel affecting event, we will have a selection window for 2d/3d area affected (when we leave that for rpg editing) so you can use a similar model now since you are trying to put this in rpg maker events format." And: "document about future event size selection, that may be new to our house engine."

Status: **half built**. The data format and the first consumer exist (the helm). The selection window, the outline of an area and the rigid move of voxels do not.

## 1. The problem

An RPG Maker event sits on **one cell**; its page has a condition (trigger, switches) and a list of commands. Our events can touch **many voxels**: the helm moves the whole hull (four rooms, each 16 x 16 x 4), a door frame is a small box, a lava pool is a patch of floor, a gravity switch is a room. So an event page needs one more thing next to *condition* and *commands*: **the area it affects**. Where the event stands (its trigger cell: the console at bridge 8,4) and what it changes (the hull) are two different things.

## 2. What is built today (rpg-space, 2026-10-24)

Files of the helm event, `@.apps/rpg-space/`:

| File | Role |
|---|---|
| `hotspots.pdl` `HOT \| room \| x \| y \| z \| label \| event` | the trigger cell and the page to play (the 7th field is new; rows without it are only outlined) |
| `events/helm/event_pkg/pages/page_1/condition.pdl` `COND \| trigger \| action` | trigger `action` is new in this house: "Enter next to the cell". The old triggers are `ui` (menu verb), `clock`, `int_on`, `int_off`. |
| `events/helm/event_pkg/pages/page_1/cmd_1.sh` | the command: the verb `helm on` |
| `events/helm/area.pdl` `AREA \| name \| mode \| room \| x0 \| y0 \| z0 \| x1 \| y1 \| z1` | **the area**: boxes of voxels. `mode` 2d = cells of one desk (z ignored), 3d = a box (z in levels above the floor top) |

Today `area.pdl` is **read for its voxel count** (the helm logs `HELM|time|on|4 boxes, 4096 voxels`). The helm's flight is a move over a galaxy grid (planets.pdl `POS` rows) that swaps the planet under the hull; no voxel is actually moved yet.

## 3. What the selection window has to be (future, with rpg editing)

A window of the rpg editor (rpg-edit-hq) that writes `AREA` rows for the event being edited, in two modes:

- **2D**: drag a rectangle on one desk; a second drag adds another rectangle (a union), shift-drag removes. Output: `AREA | name | 2d | room | x0 | y0 | 0 | x1 | y1 | 0`.
- **3D**: the board engine's xelector marks corner A, a key sets corner B, `z` / `x` stretch the height one level at a time (the xelector already has `z`/`x` and a level readout), Enter closes the box. Output: `AREA | name | 3d | room | x0 | y0 | z0 | x1 | y1 | z1`.
- **Several rooms**: the hull is four desks, so an area is a list of boxes, one per desk. A name groups them (`hull`).
- **Anchor**: an area can be written relative to the event's trigger cell (`anchor=trigger`), so moving the event moves its area. Today the boxes are absolute.
- **Preview**: the engine outlines every box of the selected event. It already outlines hotspots orange while In: is off (`hotspots.txt`); areas get a second colour (cyan) and are drawn as a wire box.
- **Kind**: what the commands do with the area, because the same box means different things: `move` (rigid body: the helm, a lift), `paint` (change tiles: a flood), `toggle` (show/hide: a force field), `zone` (a trigger region: stepping inside plays the page). The kind is a column of the page's command, not of the box.

## 4. What is new to the house engine

1. **Events with a volume.** Everything so far is a cell (doors, hotspots) or the whole world (the clock). A box that the commands act on needs: a stored shape (above), a replayable record (append-only `AREA` rows, same rule as every ledger), and a lock: two events may not claim the same voxel for `move`.
2. **A rigid move of voxels.** Today a "move" is simulated by swapping what is under the hull (the planet). A real move translates a box of the 3D export (`cells.txt` columns, both runs) by a vector. The engine's second solid run (2026-10-23) is the first piece: a hull can float over a planet without removing it.
3. **Selection in 3D.** The engine has cell picking (xelector, camera aware) but no box. A box selection is two xelector positions plus a height; the new part is drawing the wire box and a way to read it back (a state file the editor reads, like `pieces/xelector_01/state.txt`).
4. **Trigger `action`.** Facing / adjacency rules are new: the daemon asks "is the controlled character within one cell of a HOT row that names an event" on Enter. Moving this rule into the core (`petslike_core.c` section 77 hook) lets every pets-like board have interactive objects.

## 5. Limits to decide before building the window

- Maximum box: a desk is 16 x 16 and the ship hull is about 5 levels; a planet page is 16 x 16 x 99. Proposal: at most 4096 voxels per box, 16 boxes per area; refuse (and say so) above that.
- Areas across **books** (ship plus planet): not allowed in the first version; a helm that moves the ship over a planet moves only the ship's boxes.
- Undo: area edits are rows, so undo is a `DROP` row, never an edit in place.

## 6. How it will be tested

1. Round trip: select a box in the window, read `area.pdl`, count the voxels (`(x1-x0+1)(y1-y0+1)(z1-z0+1)`), compare with the window's own readout.
2. A deliberately wrong box (corner B before corner A, outside the desk) is refused, and a check that cannot fail is not a check: run each negative once.
3. The outline: screenshot of the box on a known desk, and `worldscope` for the voxels the box covers.
4. A `move` event: the voxel set before and after differs by exactly the vector (hash of the exported cells).

## 7. What is NOT done

No selection window. No wire-box outline. No voxel is moved by the helm. The area is not anchored. The `action` rule is adjacency (not facing). Two events cannot yet be refused for overlapping areas.
