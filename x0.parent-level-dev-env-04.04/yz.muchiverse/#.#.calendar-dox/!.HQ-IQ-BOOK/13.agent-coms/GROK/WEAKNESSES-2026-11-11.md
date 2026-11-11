# Grok weaknesses on the skyland lane (2026-11-11)

Read this after `ONBOARD-GROK.md` and before you touch centroid. It is the list of turns that burned tokens on 2026-11-10 and 2026-11-11, and the stop that ends each one. The owner can see two islands on `book:centroid` `page:live`. Do not rebuild that from zero.

## Do this, and stop

1. The factory already exists. `mech_new_op` raises one blueprint row. The harness case `centroid_crew` printed `VERDICT|PASS|passed=146|failed=0`. All 12 `BUILD` ids are in it. Do not re-run that case unless you changed the factory. Do not compile it once per building.
2. The plan grid is `GRID | 72 | 52` in `skyland.blueprint.pdl`. `SCALE | cell_blocks | 4` is recorded and not applied. The live page is that grid: 1940 land cells, 216 wall cells, each island and each building at its plan x,y. Files: `@.apps/centroid/centroid-pchq/pieces/system/maps/centroid/live/cells.txt` and `map.txt`. Grow `cells.marker` by one byte after a write.
3. Proof is a dump of the open editor, then look at the PNG. Window was `0x1c00003` (confirm with the X tree; do not guess). Wake with one line `126` appended to the session `interact_relay.txt`. Do not send camera keys. Do not `board_ctl recover`. Do not `board_ctl camera`.
4. Book names. The Islands picture is book `rpg-edit-hq`, page `start`. There is no book named `edit`. Centroid pages are `test` and `live`. The page menu must list `n_desk_opts`, not `n_room` (that list is Bank, Store, Hotel).

## What looked done and was not

| What was said | What was true |
|---|---|
| Harness passed, so the island is on screen | The harness raises one slice. Until 2026-11-11 every slice was parked at page 8,6 on a 16 by 16 sheet |
| The green square is the footprint | `EXTRUDE \| W \| 4` on grass cells lifts grass. The test building uses wall slot `1,36` and no `EXTRUDE` row |
| The brown strip is the cathedral | The camera is high. Most of a small sheet sits below the frame. Open the PNG |
| Two minimaps means the HUD draws twice | Test `cells.txt` rows 24–32 were a second grass island in sky. One widget, two land shapes. Cropped to height 24 |
| `book:edit` vanished from the menu | The row is `rpg-edit-hq`. The toolbar title `.edit` is the editor, not the book id |
| Chrome is why the CPU is hot | Measure house pids over a few seconds (`/proc/<pid>/stat` fields 14+15, and 16+17 for children). A hot browser can be real. The owner rule is: fix our strays. Do not kill the editor, the board window, or the whole browser. Do not `pkill -f` |

## Do not do these

- A new generator per `BUILD`. `bp_op gen` and `crew_op raise` are the generator and the placer.
- Hand-edit `plan.ppm` or replace it. Scratch plans stay. They are not the editor.
- Apply scale 4 so the page "looks bigger".
- Claim the big lobes were crew-raised. `under:S6` and `under:S3` were. On 2026-11-11 the five big lobes and the small sky islands were written onto `page:live` as ellipses from the blueprint, not as a `mech_new_op` earth walk. Say that when you report it.
- Commit on `claude`. The shared checkout stays there. Commit from `/home/no/Desktop/github/work/NNEST-12.00/.kilo/worktrees/grok` with `commit_scoped --branch grok` and an explicit path list. This worktree is sparse. A path it does not list will not add. Do not `git add -A`.
- Move the camera to hide a miss. On the land dump the camera was still `pos 8,8,1`, which is sky next to the lobes. The 3D view showed the near plates and dirt edges. The minimap showed the whole campus. Both facts go in the report.

## Pictures that are the current proof

- `XO/31.Nov/nov-10/grok/27.test-one-minimap.png` — one minimap, page test, the building still there.
- `XO/31.Nov/nov-10/grok/28.live-land.png` — page live, two lobes in the 3D view, one minimap of the campus. Toolbar `book:centroid` `page:live`.
- Older dumps `24` and `25` are the rejected green band and the brown strip. Do not cite them as the look.

Pitfall writeup, same facts: `03-pitfalls/HOUSE_CODE_PITFALLS.md` section 28.

The owner's 2026-11-11 view of the island is the lobes plus one 5 by 5 house (roof, door, windows) and several flat orange slabs. That page is now `small`. Page `live` is a copy, and the castle is built there from five modules (D71): `wall`, `window`, `door`, `roof`, `tower`. Slot 35 is the orange slab. Slots 12–15 are the castle drawings and are caps only. The steps and the tile list are in `16.game/CENTROID-TEST-AND-LIVE-2026-11-06.md` under "Castle scale". Do not move the camera they set. Do not restamp `small`. Do not apply scale 4 to `small`. The palace with wings waits for scale 4 on `live`.

The harness menu lists the confirmed-proof cases plus `run all`, and every toolbar dropdown including Harness has a cancel row. `run all` has not been clicked. It would run the generator proofs and the crew. Key `126` opens that menu. Do not send it to wake a frame.

A CASE harness row cannot take a size. The land row on page `live` is an ACTION so it can. Size and seed are `data/skyland/land.seed.pdl` (`SEED | land`, `SCALE | land`). `land_op` starts from a blank page. Do not hand-edit `live/cells.txt` to grow the island. Change the seed file and run the row. Page `small` is not that row.
