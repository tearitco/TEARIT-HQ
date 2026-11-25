# Tearit-map and the Grok planet/city/sandbox work: review and fix plan (2026-11-25)

Scope: commits `1fe81ab15..d4ba5962f` on `claude` (60 commits, 56 files, 3769 insertions, read with `git diff --stat`). Read-only review. Nothing was built, run live, or committed by this review.

Labels: **SAID** = in a doc or handoff by Grok or the owner. **READ** = read in the repo this session (file:line). **RAN** = a command run this session, output observed. Every number below is one of those; where I did not run something it says so.

House paths: `H` = `x0.parent-level-dev-env-04.04/yz.muchiverse/44.xyz.01.00`; `BOOK` = `x0.parent-level-dev-env-04.04/yz.muchiverse/#.#.calendar-dox/!.HQ-IQ-BOOK`.

## 1. What it was meant to do (SAID)

- ROADMAP 3.5 "TEARITIZATION sprints": a Civ-SNES shape on events, a human plus agents, a seeded map, 2D and 3D from one desk; each sprint is "one proof (a harness that can fail, plus a picture)". `BOOK/ROADMAP.md` (added section 3.5), `BOOK/16.game/TEARITIZATION-FORWARD-GUIDANCE-2026-11-23.md`.
- Decisions D124 to D135 in `BOOK/02-architecture/DESIGN-CHOICE-TEST-AND-DECISION-LOG.md`: seed map, eat/sleep, agent step, place, fog, step n/e/s/w, moods, checkpoint, two cities, emoji actors, fog button, second seat.
- Later in the same night the work changed direction: an ETOPO globe in Minecraft blocks (`earth-moth`), a shared ball formula (`planet_ball`) for moon, mars and a solar page, four real-city footprint books, and `rmmc`, "a small voxel sandbox with an RPG Maker body" (what the owner saw in `/home/no/Videos/rmc.mp4`).

## 2. What it actually does

### 2.1 tearit-map (READ, RAN where marked)

- `H/@.apps/tearit-map/ops/tearit_map_op.c`: 1440 lines (RAN `wc -l`), about 30 verbs behind one `main` (`gen check sabotage picture desk move step approach dig place draw checkpoint verify alter found eat sleep agent fog hut roam life`, lines 1388 to 1440).
- `H/@.apps/tearit-map/ops/tearit_driver.c`: 246 lines. Loop at :217 to :238.
- The map is integer-hash noise: `paint()` at tearit_map_op.c:41 (thresholds SEA 40, MOIST 30, HILL 70, MOUNT 85, :17). The driver makes the first ledger with `gen 1 16 16` (tearit_driver.c near :200). So the live book is always seed 1, 16 by 16.
- It never reads an RPG Maker map. The only RPG Maker thing in the op is a ground-tile sample from `Outside_A5.png` (`load_a5`, :570), opened by an absolute path `/home/no/Desktop/github/...` at :572.
- Game rules live as C functions: ration cap and eat/sleep (:1019 to :1067), found (:1001), dig (:1132), agent moods as two direction tables (:1099 to :1130), draw/water (:1219 to :1262), hut path (:1312), roam (:1349), fog sight (:519, :648 onward). Only six numbers reach `RULE` rows: sea, moist, hill, mount, ration, water (write_map :274 to :279). Everything else (what a mood is, sight radius 1, "tile 9..14" meanings in `desk()`) is C constants.
- No `events/` folder, no `state.pdl`, no registry row in tearit-map (RAN `ls`; RAN `find ... -name events` lists rta, teario, rpg-pet and others, not tearit-map). `H/@.apps/tearit-map/` has game.pdl, layouts.pdl, rooms.pdl, ops, and one desk folder.

### 2.2 earth-moth, planet_ball (READ, RAN)

- `H/@.apps/earth-moth/ops/earth_moth_op.c`: 633 lines. Builds a 200 by 200 desk (`#define N 200`, `R_BALL 92.0`) from ETOPO5. Every filled column is `floor = surface tile, wall = stone slot 6, depth`.
- `H/&.widgits/_shared-lib/harness/ops/planet_ball_op.c`: 68 lines. `formula <body>` only prints a table of radii (earth 92, moon 25, mars 49).
- The grey minimap: `board-viewer/ops/bv_render_3d.c:2825` paints `wl ? wl : fl`, wall first. That rule is from 2026-11-16 (owner: "why dont i see snow on mini map"). Grok's desk puts stone in the wall slot of every column, so every cell averages stone grey. Grok's own handoff says so (`BOOK/13.agent-coms/GROK/HANDOFF-2026-11-25-EARTH-MINIMAP-COLORS-AND-SOLAR.md`) and hands it to Claude. RAN `grep`: line 2825 is unchanged at the tip, so it is still open.

### 2.3 Four city books (READ, RAN)

- `chicago-loop`, `nyc-fidi`, `giza-plateau`, `jerusalem-old`: each is game.pdl + rooms.pdl + a generated desk. RAN `diff chicago-loop/game.pdl nyc-fidi/game.pdl`: only the name and title lines differ. Play is the shared `game_core` ("City rules are not wired yet").
- The painter change is tiny and good: `rta_city_op.c` gets one data row `SHORE | off` (+10 lines, `git diff`). The paint is building footprints only; streets are the grass gaps (FOUR-CITIES doc, SAID). Source caches are outside git (`#.NNEST_ASSETS/rta-*`), licences "NOT CHECKED" (SAID), and four PNGs, 930,136 bytes total (RAN `wc -c`), are committed into the BOOK.

### 2.4 rmmc "flat tile field" (READ, RAN)

- `H/@.apps/rmmc/ops/rmmc_field_op.c`: 138 lines. `cell_of()` (:~66 to :74) paints a 64 by 64 page by coordinate ranges: sand at x>=40 and y>=40, water in a corner, a snow corner, a trench strip, one pillar at 20,16, one tree at 44,22. Atlas blits are hard-coded tile offsets (:~100). The op exits 0 when its own counts of sand, water and trench are above zero (last line), which only checks its own constants.
- No harness case exists for it (RAN `ls cases | grep -c rmmc` = 0).
- Result on screen (SAID, the Codex handoff written by Grok): third-person view is sky or "a dark slab", the body is a flat cyan box not the charset, break and place are not built, "Do not narrate 9-pov2-field.png as the video".

## 3. Defects, with evidence

1. **Wrong ground truth: noise, not RPG Maker or the existing levels (READ).** The tearit map has no link to an RPG Maker map, to the petslike boards, to the sp levels, or to the rta city desks. `rmmc` copies the look of the RPG Maker tileset but not a map. The owner's picture is an RPG Maker world; the build is a hash. The two cases where the house already had a real map source (rta city desks with `rta_stand_op`, and the imported RPG Maker maps named in skill `tsots-game-map`) were used only for the city books.
2. **Rules are hard-coded in C, not event pages (D92) (READ).** Eat, sleep, ration, found, dig, moods, draw, fog, hut are functions in one op, called by a driver and bar buttons. `H/@.apps/rta/rules.pdl` already has `hunger_start`, `eat_amount`, `sleep_max_turns`, `walk_turns`, and `H/@.apps/rta/events/rta_eat`, `rta_sleep`, `rta_walk` exist as event pages. tearit-map re-invented hunger as `RATION_CAP` (an enum default, :17, overridden by one RULE row). That is a second authority for "how a person eats". The verbs are ops and write ledger rows (good, see section 4), but no event page calls them, no registry row, no harness case per rule in the standard's shape. Decisions D124 to D135 answer the five questions "House shapes: op", and none of them mention D92 or the events folder.
3. **One 1440-line file with two jobs and a mixed authority (READ).** The driver calls `desk` with the ledger as the map input (`{op, "desk", ledger, dir, ledger}`, tearit_driver.c `desk()`), and `gen` writes the map header and `CELL` rows into the same file as the ledger rows. So the seed projection (256 `CELL` rows) sits inside the ledger, and `checkpoint`'s hash covers it. Map and ledger are one file; one fact (the seed) has two homes (the `gen` args and the rows).
4. **The driver costs idle CPU and breaks the marker rule (READ, not measured).** Every 250 ms (`usleep(250000)`, :238) it rewrites `game_ui.txt` and `stamps_ui.txt` unconditionally (`publish`, `stamps`, :205 to :237 area), stats the ledger by `st_mtime` (`led_m`) to decide when to rebuild the desk (AGENTS: change detection is marker size growth, never mtime), and forks `roam` every fourth beat when life is on. No commit message states an idle or active cost, which AGENTS requires. I did not measure; the next agent must (CPU-OPTIMIZATION-PITFALLS 30 to 32).
5. **No state.pdl, so the ghost-data audit cannot run (RAN).** `ls` of `@.apps/tearit-map`, `earth-moth`, `rmmc` finds no `state.pdl` (count 0). Preferences `roam.txt`, `fog.txt`, `action.txt`, `stamps_ui.txt` are declared nowhere. `fog.txt` is reasonably a preference (D134 says so); that is exactly the kind of file `state.pdl` exists to name. `earth-moth/state/ledger.txt` starts with `MAP | earth-moth | etopo5 | 72 | 72` (RAN `head`) while page earth is 200 wide: a stale file, not tracked, not declared.
6. **Tests prove files, not the screen (READ, RAN).** `tearit_map.pdl` is 255 lines, 1 CASE, 138 `EXPECT_*` rows, 19 refusal or writes-nothing rows (RAN `grep -c`). It contains no `see`, no PNG check, no `relay_drive` row (RAN `grep -c 'relay_drive\|dump_frame\|see'` = 9 matches, all the substring "see" in "seed 1"; none is a step). `earth_moth.pdl` (16 rows) and `planet_ball.pdl` (33 rows) likewise. `picture` writes a PPM no case opens. Every "on the board" claim in ROADMAP is prose ("Hand-run", "Not on the open window", "photo"). AGENTS: "A case that passes on ledgers and files while the screen is empty is a failed test". Also `harness/results/{tearit_map,earth_moth,planet_ball}.txt` do not exist (RAN `cat`), so no run is on disk to read.
7. **The earth harness pins the bug (READ).** `earth_moth.pdl` case `scratch-desk` asserts `grep -F 2,6,` ("grass-top-stone-wall") and `formula` asserts `slots floor=surface wall=stone`. That is exactly the data shape that makes the minimap grey. The test has no assertion on a colour. `planet_ball` is a tautology: the op prints constants and the case greps for the same constants (`FORMULA body=earth R=92 ...`); a wrong radius in `earth_moth_op.c` would pass.
8. **Radii have two authorities (READ).** `planet_ball_op.c` table (92, 25, 49) and `earth_moth_op.c` `#define R_MOON 25.0`, `R_MARS 49.0`, `R_BALL 92.0` ("Radii match planet_ball_op", a comment is the only link).
9. **Not portable, not rebuildable from the repo (READ).** Absolute `/home/no/...` paths in `earth_moth_driver.c:135-136`, `tearit_map_op.c:572`, `earth_moth.pdl`, `planet_ball.pdl`. `planet_ball.pdl` and `earth_moth.pdl` use prebuilt gitignored `+x` binaries (`*.+x` is in `.gitignore:9`, RAN `git check-ignore`), so a fresh clone fails them; `tearit_map.pdl` compiles in-case, which is the right shape. City desks need caches that are not in git.
10. **Shared renderer widened without a case (READ).** `bv_render_3d.c`: depth limits 16 to 255, `MAX_VOXEL_Z` 64 to 256. `MAX_BOARD_DIM` is already 256 (:80), so the static `board3d` goes from 64*256*256 = 4,194,304 bytes to 256*256*256 = 16,777,216 bytes for every board (arithmetic from the two defines). The commit adds no case in `minimap_fit` or a renderer case.
11. **Copy-paste books (READ).** Four city `game.pdl` files differ by two lines. Under the "zero per-project C, data only" standard these should be one template plus a `CITY` row each.
12. **rmmc is a hard-coded picture (READ).** See 2.4. It reproduces a look with magic coordinates instead of reading a map, and nothing can fail on it.
13. **Hand-off pattern.** Three handoffs in one night (earth minimap to Claude, rmmc to Codex, sunset/promote doc) each say "not done". The honest status is in them; the ROADMAP rows still read as sprint proofs (each says "Not on the open window").

## 4. What is fine and reusable

- Verbs write append-only ledger rows and refuse bad moves with exit 4 and nothing written (harness: "a refused step writes nothing" via `cmp`). This is the ledger-is-authority shape.
- Determinism proofs: same seed same bytes; desk deleted and rebuilt gives equal `cells.txt` (`cmp`); `checkpoint` and `verify` reject an edited ledger. These are real negative tests (19 refusal rows).
- `tearit_map.pdl` compiles the op in-case into `$T`, uses a scratch dir, never the live house.
- `rta_city_op` `SHORE | off`: a data row, 10 lines, correct shape. Reuse it.
- The 2D renderer fix (`cells.marker` size reload in `bv_render_2d.c`) follows the marker rule; keep it.
- Emoji actor csvs (`emoji/<hex>/voxels_16.csv`) and `entities.pdl` rows are a data-only way to draw a body in 2D.
- The ETOPO to column formula (surface, three dirt, stone) is a good data pipeline once the slot choice is fixed.
- Grok's handoffs are candid about what is not done; the Codex handoff lists the real causes (camera home, xelector reset, event Y).

## 5. Ranked fix plan (smallest first)

Each item: the five design-choice answers in one line, in order: one authority / modular / scalable / long term / house shape. Add a D-row to the decision log before building any of them.

1. **Declare state (state.pdl for tearit-map, earth-moth, rmmc; delete the stale earth-moth ledger or regenerate it).** One authority: state.pdl names each file's kind. Modular: data only. Scalable: the audit verb then covers every app. Long term: the ghost-data standard needs it. House shape: `.pdl` + the `audit` verb.
2. **One radii table.** Make `earth_moth_op` read `planet_ball_op formula <body>` output (or a `BODY` row file both read); delete the `#define`s. Authority: one table. Modular: planet_ball stays the only place. Scalable: a fourth body is one row. Long term: solar page grows. Shape: pdl rows + op.
3. **Fix the grey minimap by choosing the top face in one place.** Add one helper (for example `bv_cell_top_slot(fl, wl)`: floor when set and wall is a side-only material) next to `bv_minimap_tile_avg`, used by both the shader loader and the minimap; keep the 2026-11-16 snow rule working (a cell with only a wall still uses the wall). Add a `minimap_fit` case that reads the minimap pixels of page earth and asserts blue and green exist and are not equal. Authority: one top-face rule. Modular: shared renderer, zero per-project C. Scalable: every board. Long term: parity of 2D/3D. Shape: C helper + harness case that opens pixels. Risk: other boards that rely on wall-first (ark-age snow); run `minimap_fit` before and after.
4. **Make the harness look at the screen.** For tearit_map, earth_moth, rmmc add one case each using `relay_drive` `see`/dump and a PNG pixel check op (small C op if none exists), in a scratch house, gentle mode, not the owner's window. Replace the planet_ball tautology with a desk-level check (count of cells per tile slot on page solar). Remove absolute paths via `$HOUSE`/`NEEDENV`; compile ops in-case. Authority: the PNG. Modular: shared check op. Scalable: every game. Long term: the compendium rule. Shape: pal + case + C op.
5. **Measure and cut the driver.** Write the two UI feeds only when content changes; poll a marker file size, not `st_mtime`; put an idle and active cost line in the commit. Authority: ledger size marker. Modular: one `feed_write_if_changed` shared by `publish` and `stamps` (they duplicate the bar). Scalable: other book drivers. Long term: `cpu_guard`. Shape: marker file, capped runtime files.
6. **Collapse the four city books to one template + rows.** Authority: one `CITY` row per city (box, kind rows, SHORE). Modular: no per-city files. Scalable: the 5th city is a row. Long term: licence flag is a column, so unchecked cities cannot be published. Shape: `.pdl` rows + `rta_city_op`/`rta_stand_op`.
7. **Split tearit_map_op.c along the rules and move rules to event pages (D92).** Order: eat/sleep first (reuse rta `rta_eat`/`rta_sleep` pages and the RULE names `hunger_*`, instead of `RATION_CAP`), then found, dig/place, fog, agent moods. Each becomes: pure module + CLI op + registry row + event page under `@.apps/tearit-map/events/` + a case. Separate the seed (map file) from the ledger so `CELL` rows leave the ledger. Authority: RULE rows and the ledger. Modular: one op per rule family, not one file. Scalable: the 10th rule is a page. Long term: harnesses and AI score reuse. Shape: events + op + ledger. This is the largest item; do it in the order above, one rule per commit, harness green after each.
8. **Replace the noise and the hard-coded field with a real map source.** Read the answer to "which RPG Maker or existing level should this mirror" from the owner first (I did not locate the imported RPG Maker map store on disk; a `find` to depth 6 for `Map071*` found nothing, so the reader must check the `tsots-game-map` skill's path). Then generate the desk from that map with the existing importer path (the same one `bay-slice`/`centon` use with `rta_stand_op`), and keep the seeded generator only as a "new world" option behind a `GEN` row. For rmmc: read the chosen map instead of `cell_of()`. Authority: the source map. Modular: importer is shared. Scalable: all 98 maps. Long term: the owner's stated goal (RPG Maker body in a RPG Maker world). Shape: importer + desk + case. Needs the owner's answer before building.
9. **Put break/place on `vox_op` events for rmmc (do not add a branch in game_core).** Per the Codex handoff and the minecraft-events handoff. Authority: ledger. Shape: event pages + `vox_op`.

## 6. Why it is "jacked up", in two sentences

It was built as a chain of small proofs on a toy (a 16 by 16 noise map), each proof a file check, so the work grew a private game engine in one C file instead of reusing the house's event pages and real maps; then the rendering work (globe, minimap, sandbox) was built the same way and never checked on a screen, which is why the minimap is grey and the sandbox is sky.

## 7. Not checked

I did not run the harness cases, the editor, or any engine. I did not open the PNGs in the four-cities folder. I did not measure CPU. I did not find the RPG Maker map store; item 8 needs it.

## External references

- `H/@.apps/tearit-map/ops/tearit_map_op.c`, `tearit_driver.c`, `game.pdl`, `rooms.pdl`, `layouts.pdl`
- `H/@.apps/earth-moth/ops/earth_moth_op.c`, `earth_moth_driver.c`; `H/&.widgits/_shared-lib/harness/ops/planet_ball_op.c`
- `H/@.apps/rmmc/ops/rmmc_field_op.c`; `H/@.apps/rta/ops/rta_city_op.c`; `H/@.apps/rta/rules.pdl`, `rta/events/rta_eat`
- `H/&.widgits/board-viewer/ops/bv_render_3d.c` lines 80, 952, 2825
- Harness: `H/&.widgits/_shared-lib/harness/cases/tearit_map.pdl`, `earth_moth.pdl`, `planet_ball.pdl`, `minimap_fit.pdl`, `README.md`
- `BOOK/13.agent-coms/GROK/HANDOFF-2026-11-25-EARTH-MINIMAP-COLORS-AND-SOLAR.md`, `HANDOFF-2026-11-25-CODEX-RPG-MAKER-BODY-SANDBOX.md`
- `BOOK/02-architecture/STANDARD-RULES-AS-OPS-FOUNDATION-DESIGN-2026-11-18.md`, `NO-GHOST-DATA-THE-LEDGER-IS-THE-AUTHORITY-2026-11-07.md`, `DESIGN-CHOICE-TEST-AND-DECISION-LOG.md` (D92, D124 to D135)
- `BOOK/16.game/presentations/FOUR-CITIES-2026-11-25.md`
- Owner report copy: `/home/no/Desktop/github/work/XO/31.Nov/nov-25/4.tearit-map-grok-review-2026-11-25.md`
