# rpg-pet roadmap (2026-10-10)

Status words: DONE = built and seen working / PARTIAL / TODO / PLAN = documented only. Design + how-to: `RPG-PET-SPRINT.md` and the house copy `@.apps/rpg-pet/HOW-TO-RPG-PET.md`. Story: `NIGHT_50_THE_HOUSE_OF_MONSTERS.txt/.mp3`.

## Done
- Toy `rpg-pet` in the toys menu (a `toy.pdl` file is enough), old pet-trainer untouched. Commits `9bfbee969`, `d0f153a5f`, `13ca84c78`.
- A 13x8 room drawn only from RPG Maker MV sheets (Inside_A5 floor/wall, Inside_B furniture, !Door1 door, Monster.png pets); a 14-item shop (`catalog.pdl`); append-only ledger `state/house.txt` (BUY/PLACE/MOVE/REMOVE; `reset` archives); placement rules (floor/wall, collisions, door and pet cells).
- Two rooms with teleport doors (`rooms.pdl`, tile units, same ROOM/DOOR rows as pet-trainer); pets walk through `auto=1` doors by themselves.
- The six pets are RPG Maker DB entries (db-hq PetParty + ACTOR stats) shown as Monster.png monsters; `select` and `goto` verbs; pets block each other.
- INT mode (relay arrows move the pet, Esc leaves) and autonomous one-tile event steps with the 3-frame walk cycle, every step a row in `state/events.txt`.
- pet-trainer GUI: INT | book | page | Shop | Menu, stats sidebar, six pet buttons in the footer. CPU: renderer ~6%, daemon 0.2% (class `fixed-size managed user-resizable` was the fix; 40-47% without).
- One C op (`ops/rpg_pet.c`, stb_image), no Python.

- (later 2026-10-10) All six pets walk and meet by themselves; INT controls the selected pet or the player (Tab); roguelike turns (clock moves only when you act, every other pet steps); the old game's clock mechanism (lc_clock, own root) shown in the top bar with a Time dropdown; day/night tint.

- (2026-10-10, latest) **Own cast**: six NEW DB monsters Blip, Fang, Gruk, Draxa, Gargo, Bonz (db-hq ACTOR 19-24 + SYSTEM RpgPetParty, additive) with their own entity_uid / wallet ids and own voices that SPEAK Chinese, Japanese and Korean (and English); **Harold** (DB actor 1) is the hero/player, first in the bottom bar, owner of the six; chat and bag overlays open (the renderer drops an overlay row with no item child); an outside room with day/night sky; INT arrows (codes 1000-1003, window must be focused); roguelike turns + the lc_clock top-bar clock.

## Next (in order, each ends with a before/after PNG)
0. **Language support (owner: serve Chinese, Japanese, Korean audiences)**: `lang/<code>.pdl` label tables so the whole window (menus, labels, chat) can switch to zh / ja / ko; check the renderer's font path for CJK window text (board-viewer already draws CJK with `bv_cjk_glyph.c`); then pet chat in the pet's own language with the English shown on request.
1. Shop pictures: RMMV `IconSet.png` for DB items, the furniture's own Inside_B tile for furniture (find the renderer's item `sprite=` format first). DB items as shop stock.
2. Chat + bag hotbar overlays actually drawn (compare pet-trainer; footer buttons already work).
3. The xlector (placement cursor): arrows move it, Enter places/picks up, z/x levels.
4. Smooth walking: pixel tween between tiles using RPG Maker move speed from `rpg_core.js` (ask the owner for the code/animation ops if the house has them).
5. Scene data (planes) so 2D is drawn from data; harness `rpg_pet` (pal + cases) for the verbs (`--once`-style verbs already exist: `rpg_pet.+x <verb>`).
6. TSOTS interior statistics (`reference/tsots_interior_corpus.py` is the first cut: 36 of 94 maps are interiors, 13x15 to 56x50) -> `data/house_stats.pdl` -> a house sampler checked by `check_place`.
7. 3D: export the house in the TSOTS desk format, call board-viewer `bv_render_3d`; POV keys 1-4, 0, q/e, r/t, c/v, f; CPU measured. Then the backtick Chinese-glyph view (`bv_cjk_glyph`).
8. The village (the person/Actor1 trainer walks it; pets live in houses), same setup as pet-trainer.

## Later (owner direction)
- **Every pet gets its own room; some share because of relationships** (PLAN, documented in HOW-TO: owners column in `rooms.pdl`, append-only share ledger replayed into owner lists, owner-only furniture edits, visiting through doors).
- Item functions as events (sleep, eat, read, play); purse <-> chain bridge (coins are paper today, see CHAIN-ECONOMY-DESIGN); pets driving the relay (the ecosystem soul); doors shared with pet-trainer's `rooms.pdl`.

## Known bugs / honesty
- Voices are generated with edge-tts (online); the agent could not listen - the owner confirms by ear. Only Japanese and Chinese samples were synthesized in testing; Korean phrases exist but no Korean sample was checked.
- INT arrows only reach the game while the window has focus (click it first); real-hardware check still wanted.
- Chat and hotbar overlays do not draw over the canvas (cause not found).
- The DB pet profiles still describe animals; the sprites are monsters (owner's data, untouched).
- Only the active pet walks by itself; the other five stand.
- Screenshots on this Wayland session: `ffmpeg x11grab` is black; use `&.widgits/_shared-lib/ops/+x/dump_frame_png_op.+x 0x<hexid> out.png`.

- 2026-10-10 handoff for the next agent: see HANDOFF-NEXT-AGENT-2026-10-10.md (pets stacked/not moving, camera follows pc-hq xelector not Harold, POV not in the pet window, CPU).
