# rpg-space: the two ways to fly the ship (Galaxy and Direct), the choice popup, the map box

Written 2026-10-24. Branch `claude`. Built and run live (see RPG-SPACE-HELM-INTERACTION-TEST-PLAN.md for the evidence of the Galaxy mode; section 4 here for Direct).

## 1. The popup (owner: "a pop up event for the visual factor, not a standard")

Enter next to the bridge console (cell 8,4; stand on 7,4 or 9,4) shows a centred popup, an RPG Maker "Show Choices" box:

| choice | verb | what happens |
|---|---|---|
| Galaxy: planet to planet | `helm yes` (alias `helm galaxy`) | the pilot becomes the ship on the **space desk** (planets as islands, a 3x1 hull box, the minimap shows it); arrows move the hull one grid step (2 cells) per press; Esc lands over the planet under the hull |
| Direct: over this planet | `helm direct` | the ship **stays on its screen** (same bridge desk, same camera); the **planet slides away under it**; arrows move the planet page one cell per press; Esc just lets go |
| No: stay here | `helm no` | closes the popup |

Files: `events/helm/choice.pdl` (`CHOICE | title | question | galaxy | direct | no`; a row with 5 fields has no Direct choice; delete the file and Enter plays the page at once, Galaxy), row `ask-ov` in `board/pchq-board.xhtpm` (the window's real template, NOT `rpg-space.xhtpm`), flags `state/flag_ask.txt` (popup open) and `state/helm_mode.txt` (`galaxy` / `direct`, written by the verb, read by the event page's `helm on`). The same event page serves both modes; the mode file decides.

## 2. Direct mode, as the owner asked (2026-10-24)

"The ship should stay in view, while the planet fades away. Then it is the ship moving, not the camera." "The ship is moving as a vehicle through space away from the planet." Decided with the owner: the planet page under the hull is shifted; the planet itself is unchanged (same planet, same address); the ship rooms, the crew and the camera do not move.

- State `state/direct.txt` = `ox oy`, the shift of the planet page under the hull in cells (positive x = the ship is east of where it was). The skin copies page cell `(x+ox, y+oy)` under desk cell `(x,y)`; a cell off the page is open space (a hole).
- `tuning.pdl` `direct_max` (default 10): the ship stops when the planet is that far (of 16 cells) away ("the planet is almost gone behind the ship"). 0..15. To fly further the **planet map has to be bigger** (the engine allows 256 x 256 per board; a page is 16 x 16 today): that is the "expand the planet map limit" step, not done.
- A move rebuilds the ship rooms and the book and reloads the engine (about 5 s in 3D; the minimap updates with it). Log rows `DIRECT|time|ship|ox,oy|nx,ny` and `HELM|..|on-direct` / `off-direct` in `state/events.txt`.
- A new planet, Regen or `planet <id>` resets the shift to 0,0.
- The pilot is NOT moved (Galaxy puts the pilot on the space desk); the arrows are read from the xelector offset like Galaxy; Enter next to the console does nothing while at the helm.

## 3. The map box (owner: "draw a grey cube to show the map's 3D limit, for tests / presentations")

`tuning.pdl` `map_box` = 1 (default 1 for now): the outer ring of the **planet page** is drawn grey and its four corners are tall grey pillars (glyph `P`, extruded 8 above the hull floor), so while the planet slides away the limit of the map stays visible (3D: the pillars; minimap and 2D scene: the grey ring). It moves with the planet page. Set it to 0 to turn it off. Not a full wire cube (the engine's column model has one run per column plus one under it, so a top ring floating in the air is not possible yet); the ring and pillars are the edge of the map volume.

## 4. Evidence (live, 2026-10-24, `board_ctl` harness)

- Popup with three choices in a window dump; the Direct choice clicked: `helm_mode.txt = direct`, `flag_helm = 1`.
- Six Right keys: `direct.txt` 1 0 .. 6 0 (checked by `board_ctl expect` after each key), `DIRECT|` rows; the minimap shows the planet (brown) sliding to the top-right corner inside the grey ring while the bridge stays fixed; top-down 3D frame shows the bridge fixed and a grey corner pillar of the map box.
- Esc: `flag_helm = 0`; the ship holds its offset.
- NOT run: the refusal at `direct_max` (the code returns the "almost gone" message and keeps `direct.txt`; I flew only to offset 6 of 10).

## 5. Not done / open

- The camera presets (`tools/camera_presets.pdl`) drift after each book reload because the engine puts the xelector level back (-7); `board_ctl camera` now corrects the level with real z / x keys, but the 3D pictures of Direct frames 3-5 in the first harness run were dark or empty. The top-down preset shows it best.
- "Fades away": the planet leaves the screen by sliding; there is no fade effect (a fade needs per-cell dimming in the engine).
- The planet map is 16 x 16: Direct flight ends at `direct_max` cells; a bigger map and neighbour pages (the zoom ledger) are the next step. The owner chose "stop at the edge".
- Both modes at once (leave Direct for Galaxy without Esc) is not built: Esc, then Enter again.
- The grey box is only on the ship-over-planet desks, not on the space desk (its limit is the desk edge).
