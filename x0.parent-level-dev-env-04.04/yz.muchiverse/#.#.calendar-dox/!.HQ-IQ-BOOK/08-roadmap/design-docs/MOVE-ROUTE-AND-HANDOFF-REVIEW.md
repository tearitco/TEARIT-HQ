# Movement routes, and a review of the two handoffs — 2026-10-09

Checked this session against `rpg_objects.js` in the local
`rpg-maker-mv-og-mt` tree, `event_commands.registry.pdl`, and
`mr_world.c`. No executor was written.

## Tier 2, posts allowed

Tier 1 stays the default: the flat ledge and the lower wood rail.
Tier 2 keeps a skin when the middle's own band is at least 0.30 of the
block, even if the caps have a post or a brace. The label is drawn on
the middle tiles only, starting after the left cap.

Re-running the seam scan over every PNG in the tileset folder did not
find a new horizontal family. The passes are the same wood, beam,
stone, truss, and ledge already listed. Beams, arches, and trusses are
tier 2. The ledge and the lower wood rail stay tier 1.

Three hits asked for MIRROR because no right cell joined:

- `Dungeon_B` left 0,15 middle 4,15. The middle is the post cell, not
  the plank. Mirroring the left post would make a right post. That is
  acceptable only as a second wood rhythm, and I would keep it out of
  the default cycle.
- `SF_Outside_C` 12,2 with middles 13,2 and 14,2. Those cells are
  vertical red poles, not bars. The band test does not yet reject a
  shape that is taller than it is wide. Do not mirror them. Do not add
  them.

So mirroring is acceptable for an asymmetric end cap (a post on the
left becomes a post on the right) and unacceptable when the cell is not
a horizontal bar.

## Crop ids, corrected

B-sheet crop id is `row * 16 + col + 1` (1-based, row-major). Confirmed
by pixels: `Dungeon_b/225/sprite.csv` line 5 is `115,84,57,255`, and
`Dungeon_B.png` cell 0,14 pixel (0, 672) is the same color. Cell 1,14
matches `Dungeon_b/226`. The ids in the earlier rail table were short
by one. The primer table now says wood `225 / 226 / 229` and iron
middle `237`.

## Move types

`Game_Event.updateSelfMovement` switches on `_moveType`:

| Value | Behavior | House |
| --- | --- | --- |
| 0 | Fixed. No case. The event does not step on its own. | Missing. |
| 1 | Random. | Missing. |
| 2 | Toward the player (this is "approach"). | Missing. |
| 3 | Custom. Plays the event's move route. | Missing. |

These are a page setting. They are not route command codes.

## Route command codes

`Game_Character` lines 6841–6886. Every one of these is **missing** as
behavior. The registry command `set_move_route` calls
`mr_world move_route`, which only writes `move_target` and `move_route`
onto `map_state.pdl`. That is **state-only** for the whole string. It
does not read a code number.

| Code | Name | Class |
| --- | --- | --- |
| 0 | End | missing |
| 1–4 | Move down, left, right, up | missing |
| 5–8 | Move lower-left, lower-right, upper-left, upper-right | missing |
| 9 | Move random | missing |
| 10 | Move toward | missing |
| 11 | Move away | missing |
| 12–13 | Move forward, backward | missing |
| 14 | Jump | missing |
| 15 | Wait | missing |
| 16–19 | Turn down, left, right, up | missing |
| 20–22 | Turn 90 right, 90 left, 180 | missing |
| 23 | Turn 90 right or left | missing |
| 24 | Turn random | missing |
| 25–26 | Turn toward, turn away | missing |
| 27–28 | Switch on, off | missing |
| 29–30 | Change speed, frequency | missing |
| 31–34 | Walk anime on/off, step anime on/off | missing |
| 35–36 | Direction fix on/off | missing |
| 37–38 | Through on/off | missing |
| 39–40 | Transparent on/off | missing |
| 41 | Change image | missing |
| 42–43 | Change opacity, blend mode | missing |
| 44 | Play SE | missing |
| 45 | Script | missing |

The desk placer (`move_entity_init` / `move_entity_tick`) is a
different mechanism. It can step one entity across the desk. It is not
an implementation of any of these codes. Nothing in `mr_world.c` is
**supported** as an MV route step.

## Five things I would change in the handoffs

1. The readiness mapping says `transfer_player` goes to
   `mr_transfer_desk.+x`. The registry template runs
   `mr_world.+x transfer`. I read the template. I did not check whether
   a door entity calls the other binary on its own.
2. Gap 2 mixes move types with route steps. "Approach" is move type 2.
   Route code 10 is "move toward" as one step inside a custom route.
   A route string grammar has to keep those apart.
3. The handoff table says `tile_autotile.c` has the RMMV tables ported.
   The tables match the JS for the one floor row I compared. The C file
   does not implement A1's three-frame water index, the waterfall row
   shift, or A4's even/odd table switch. "Ported" is too strong.
4. `set_move_route` is marked as writing a route with no executor.
   That is true, and it is easy to over-read. The stored value is one
   opaque string, not a list of the codes above. An executor cannot
   start until the string has a grammar.
5. The handoff's "move events inside games" (section 2, item 4) points
   at the placer. That does not close the route gap. Marking it yellow
   as if MV movement were partly done will send the next build at the
   wrong file.

I am not challenging the order R1 then R2. A post-move hook before a
route executor matches how MV runs a touch check after a step. I did
not re-read the placer body, so I am not claiming the hook's exit test
is wired.
