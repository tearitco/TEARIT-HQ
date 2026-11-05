# Handoff to Grok, 2026-11-04: the survivor slice, the clock, the coin shop, and WHERE THE TEAR-IT CO SOURCE PAGES ARE

From claude, end of the 2026-11-04 session. Branch `claude`, head `094fc3656` (pushed). `grok` and `main` are still at `183048f53`: `grok` is an ancestor of `claude`, 22 commits behind, fast-forwardable. I did NOT touch your branch this time (AGENTS.md: no merge or push unprompted); ask the owner, or merge `claude` into `grok` in a scratch worktree (`git worktree add /tmp/<x> grok`, never in the live checkout: `xyzfs/users` rule).

Read first: `../08-roadmap/GRAND-ROADMAP-CHECKLIST-2026-11-04.md` (every feature, done / partly / not, evidence tag per row, spec library) and `../09-appendix/HANDOFF-INDEX-2026-11-04.md` (what was OBSERVED vs only READ vs CLAIMED, with the harness name and count for each row). Your own design doc, `EARLY-SURVIVOR-AND-COMMON-EVENTS.md`, now has a status checklist: steps 1-6 are built in scratch.

## 1. The tear-it co source pages (the owner asked me to show you these)

The owner said: the two floating islands from the editor's own main space, plus the multi-planet pages from rpg-space / rpg-dust (spaceship / asteroid skiff). Both exist. All paths are under the house `44.xyz.01.00/`. Nothing was copied, converted or changed: they are exactly as they were.

### A. The editor's floating islands (`@.apps/rpg-edit-hq`)
- **The room:** `@.apps/rpg-edit-hq/rooms.pdl` line 7: `ROOM | start | Islands | 0 | 0 | over | 56 | 17 | 24 | islands` (56 x 17 cells, 24 px tiles). It is the editor's START room. The five buildings on it lead to the interior rooms on lines 8-12: `bank`, `store`, `hotel`, `school`, `cabin` (each 24 x 17, biome `interior`). The doors are the `D` cells of the island map; the rooms are named after the door's building.
- **The painter (there is no page file for the islands; C paints them):** `@.apps/rpg-edit-hq/ops/rpg_edit_hq.c`: `#define SK_W 56` / `SK_H 17` (lines 59-60), `sk_isl_at(x, y)` (line 358, the glyph map of the island: grass, sand path, pines, building, door, pond, bridge, sky), `sk_isl_nb` (line 411, the 8-neighbour autotile shape, outside the page counts as "same"), `sk_ensure_map` (just after it: builds every room's tiles once per process from the RPG Maker sheets `Outside_A5`, `World_B`, `Outside_B`, `Inside_A5`, `Outside_A4`, `!Door1` under `#.NNEST_ASSETS/rmmv-www-img`).
- **Design doc (STALE, marked today):** `16.game/EDIT-ISLANDS-DESIGN.md`. It is a 24 x 17 paper design from 2026-10-21; what is built is 56 x 17. Use it for the intent (one building per island, the five teleport-door buildings), not for the numbers.
- **The same page as engine cells:** the editor board's page is `rpg-edit-hq-pchq/pieces/system/maps/` (engine reloads a page when `cells.marker` grows; trigger with `sh @.apps/piececraft-hq/ops/pchq_board_action.sh <session> load-map rpg-edit-hq`).
- **To get an island out as DATA** (what tear-it co needs): `legend_page_op` (`&.widgits/_shared-lib/harness/ops/legend_page_op.c`) writes a `cells.txt` from an ASCII legend; `gen_map_op` (below) shows the file format for a seeded map. Neither reads the island yet: dumping `sk_isl_at` into a legend file is the missing step.

### B. The multi-planet pages (`@.apps/rpg-space`, `@.apps/rpg-dust`)
- **The planets:** `@.apps/rpg-space/planets.pdl` and `@.apps/rpg-dust/planets.pdl` (15 lines each, identical rows): `PLANET | maker | Maker World | rmmv | 20261023`, `cata | Cata Prime | cdda | 20261107`, `cubia | Cubia | mcl | 20261121`, `hell | Hellrock | doom | 20261203` (id, name, tile family, universe seed). The current planet and any Regen are rows of `state/look_ledger.txt`, never edits of this file.
- **The spaceship / asteroid skiff:** `@.apps/rpg-space/space.pdl` line 9: `CRAFT | skiff_1 | 2 | 4 | Wren (skiff) | pip:12:0:0,nova:12:1:0 | iron:2,bread:3`; asteroids `ROCK | rock_1..3` (lines 6-8); the same objects on the planet map: `WCRAFT | skiff_1 | cata | 22 | 24 | Wren` and `WROCK | wr_1..3 | cata | ...` (lines 14-17, world cells of the 48 x 48 land -16..31). rpg-dust has its own `@.apps/rpg-dust/space.pdl` (same ROCK / CRAFT rows).
- **The pages as event packages:** `@.apps/rpg-space/events/planet/` (with its `master_ledger.txt`) and `events/newplanet/`; same two folders under `rpg-dust`.
- **Where the maps are built and tested:** harness phase tables `&.widgits/_shared-lib/harness/play/phases/rpg_space_planet_map.phases.pdl` and `rpg_dust_planet_map.phases.pdl`, compiled cases `harness/cases/play_rpg_space_planet_map.pdl` and `play_rpg_dust_planet_map.pdl` (commit `da81812e4`: every planet has its own asteroids). NOTE: both compiled cases are modified in the working tree and NOT committed by me (they were modified before this session; I do not know why).
- **Design docs:** `16.game/RPG-DUST-PETSLIKE-BOARD-DESIGN.md` D12 (planets), `16.game/RPG-SPACE-USER-ASTEROIDS-DESIGN.md`, `16.game/RPG-SPACE-ENGINE-PIECES.md`, `16.game/RPG-SPACE-COLLISION-AND-DESTRUCTION-DESIGN.md`.
- Driving the board by hand (launch, keys, screenshots, the traps): the `rpg-space-board-driving` skill.

## 2. What is built (all quarantined in the editor's own book unless the row says otherwise)

Book: `@.apps/rpg-edit-hq/books/ledger-lab/` (has a `QUARANTINE` file; every op refuses a folder without it). Ops in `@.apps/rpg-edit-hq/ops/` (compiled to `ops/+x/`, built by `ops/build_rpg_edit_hq.sh`, run it through `cpu_gate`). Harness pals in `&.widgits/_shared-lib/harness/` (run `prisc <name>.pal` from there; results in `results/`).

| Step | Op | Harness (PASS) |
|---|---|---|
| item ledger (append-only, hash-chained, conserved per id) | `item_ledger_op` | `item_ledger.pal` 39 |
| 1 smash / needs / eat / drink / wield / light | `survivor_op` | `survivor_slice.pal` 44 |
| 2 books / skills / read / practice / craft | `survivor_op` | `survivor_craft.pal` 37 |
| needs on the GAME clock | core `clock_event` + `survivor_clock.pdl` | `needs_clock.pal` 15 |
| 3 seeded house chunk + DF column | `gen_map_op` | `gen_map.pal` 21 |
| 4 flow / rain / fire | `world_common_op` | `world_common.pal` 38 |
| flow / rain / fire on the GAME clock | core + `world_clock.pdl` | `world_clock.pal` 14 |
| 5 MCL bench / 3x3 / furnace / bed / burst | `mcl_op` | `mcl_bench.pal` 49 |
| 6 the shop (coins = ledger item `coin.play`) | `shop_op` | `shop.pal` 54 |
| turn-driven clock events | (editor) | `play/phases/turn_clock_rpg_edit_hq.phases.pdl` 3 phases |
| coin shop verbs + popup state | core `lshop` | `coin_shop_ui.pal` 34 |

Every case has a negative or a mutation run (change an expectation, the case must fail). Keep that habit: the live run today found a real bug (`survivor_op tick` dropped the remainder, so six 10-minute ticks lost 40% of hunger and all fatigue) that the earlier scratch tests could not see because they used one 60-minute tick.

### The ONE released caller
The owner released exactly one caller from quarantine: the editor's coin shop. `books/shop/` has a `RELEASE` file (`shop_op`); `shop_op` accepts it and `item_ledger_op` accepts it only with env `ITEM_LEDGER_CALLER=shop_op` (a soft guard, not security). The other ops still know only `QUARANTINE`. The editor board has ONE tab `Coin shop <coins>c ▾` (7 item rows); picking a row opens a centred choices popup (`lshop-ov`: Buy 1 / Sell 1 / Close, prices, your coins, a short receipt), driven by the opt-in core verbs `lshop pick|close|list|wallet|buy|sell [item] [n]` (actor `player`; `buy`/`sell` without an item use the picked one). This is the Show Choices pattern of the rpg-space helm popup. History: it started as two tabs (a 16-row dropdown scrolls and hid rows), then the owner asked for a rework; nested dropdowns exist only for taskbar popup windows, not in-board tab dropdowns. Harness `coin_shop_ui.pal` (34). Known limit: a long text line in an overlay row wraps in width but the row does not grow, so lines can overlap (the popup text is kept short on purpose); whether the layout studio could own this popup was NOT checked. Do NOT lift quarantine for any other op without a new owner answer. Details: `SHOP-SLICE-SPEC-2026-11-04.md`, `books/shop/README.md`.

### Clock rows ship OFF
`survivor_clock.pdl` and `world_clock.pdl` both say `ENABLED | 0` (owner switches). The `time.pdl` rows (`needs_tick`, `world_flow`, `world_rain`, `world_fire`, `pet_say`) are installed only by `rpg_edit_hq clock reinstall`. The live daemon fired them once, on a backup, and everything was restored by hash (flow 12, fire 12, rain 6, needs 6 per actor in one game hour). They are NOT installed on the owner's clock now.

## 3. Rules and traps (from AGENTS.md and this session)
- NO Python, no new `.sh`. Harness = pal + case `.pdl` + compiled C op. `|` inside a case argument is `\p`; backslash is `\\`. Read `harness/README.md` first.
- Commit with an explicit pathspec (`git commit -m ... -- <paths>`), never `git add -A`; commit to YOUR branch (`grok`). `README.md` in the book has someone else's uncommitted line: stage only your own hunk (blob + `git update-index --cacheinfo`) or leave it.
- Heavy work (compiles, engine, screenshots, TTS) via `cpu_gate`, one at a time; `rm -rf /tmp/<glob>` is denied by the permission layer (delete exact paths only); never `pkill -f` from a tool shell.
- A dropdown that would run off the screen SCROLLS (window of rows + a pinned last row). Rows are not missing; keep a dropdown to about 12 rows or split it.
- Live window driving: `relay_drive <pid from board/module_parent.pid> tap X Y` (not `click`), then nudge the engine with `bv_keys <session> '~'` so it redraws; pointer-follows-focus means a tap elsewhere may not close a dropdown (Esc does). `debil` as a hostname did not resolve: use `debil@10.0.0.16`.
- exchange-hq is a PAPER currency order book (cones / mined / play); it knows nothing about game items; there is no cross-id item exchange anywhere. Across ids = two trades through coins.

## 3b. Views, layouts, the xelector look (owner, later the same day)
Read `../18.pc-hq/VIEWS-LAYOUT-STUDIO-AND-XELECTOR-LOOK-2026-11-04.md`: the owner's "Xcode views / MVC / blueprintz" vision (not built), the layout studio's real state, and a spec for a lighter/thinner green xelector while it controls an entity (it is a fixed green in both renderers today; `possessed_id` is already in `xelector_01/state.txt`). UPDATE 2026-11-05: BUILT with defaults of mine (the owner did not pick values): `_shared-lib/xelector_look.c` + `view.pdl` rows `xelector_free_*` / `xelector_ctl_*`; `pieces_2d_parity.pal` 58 PASS; 3D compiled and rebuilt but not run live.

## 3c. Event architecture, read this first
`../02-architecture/EVENT-ARCHITECTURE-ONBOARDING-2026-11-05.md` routes you through the four layers (clock, event pages, ledgers, projection) with the traps already hit.

## 3d. Added 2026-11-05 (all committed on `claude`, last commit c07dad28b)
- **Xelector look:** lighter/thinner green while possessing, data rows in view.pdl; `possessed_id=none` counts as free (a bug found from a live frame). 2D and 3D both observed; `pieces_2d_parity.pal` 59 PASS.
- **Wrapping popup text:** class `ov-wraptext` on an overlay `<text>` (khtpm_core_render.c, opt-in, cap 8 rows). One live frame; no harness case.
- **Possession:** Enter possesses any `pieces/<id>/state.txt` entity (bv_menu_input.c `entity_at_cell`; harness 70 PASS before the later request-path revert, see below). Pets/Harold are chosen by the daemon's existing `possess_at_xelector`; new verb `possess` + `possess_enter.pal` 11 PASS. The lock is per entity in `piece.pdl` (`de_possessible`, `possessable`). Owner decision: ALL possession is controlling; no special hero.
- **layout_check:** 44 -> 19 errors by passing every `board/state/*.txt` with repeated `--ui`; the 19 are explained (taskbar-published `load_*`, conditional keys, `toolbar.pdl` menu slots).
- **Event architecture guide:** `02-architecture/EVENT-ARCHITECTURE-ONBOARDING-2026-11-05.md`. **How to test fluid live behaviour:** `21.drive-doc-fix/TESTING-BOARD-BEHAVIOUR-DETERMINISTICALLY.md` (read it before driving the live editor).
- **Not proven / open:** the engine delivering Enter to the daemon and a live frame of Enter on a non-hero pet (needs pets frozen); an editor UI for `possessable`/`de_possessible`; lock-to-hero toggle (the per-piece flags are the existing lock); `layout_check` is still 19, not green; no harness case for the overlay wrap.
- **State of the live desk at the end:** the editor window and daemon run binaries built before the `possess` verb (relaunch to pick it up); the 3D engine process was not running at the last check and the CPU guard shows no action (cause not established); book `rpg-space` / page `bridge`. Foreign uncommitted files (hq_ui.pdl show_pointer, the play_rpg_* cases, README lines, .hai.bak files) are the owner's and were left alone.

## 4. What I would do next (your call, in this order)
0. The owner asked whether we need an onboarding guide for the event architecture. Existing pieces: `BLUEPRINT-EVENT-PAGES-GUIDE.md`, `PAL-VS-EVENTS.md`, `XHTPM-WINDOW-BUILD-GUIDE.md`, `EVENT-VERB-PAGES-DESIGN.md`, `S1-EVENT-LEDGER.md`; no single page connects the four layers touched this session (clock `common:` events from `time.pdl`, event pages / choice popups, S1 ledger events, board projection into dropdowns and popups). Proposed: one page written from the coin-shop rework. Not written.
1. Turn the island page and the planet pages into tear-it co sources (section 1): start by dumping `sk_isl_at` into a legend file `legend_page_op` accepts, with a determinism case like `legend_page.pal`.
2. Port the MCL rules (`mcl_op`: 2x2 vs 3x3 + bench, furnace, bed respawn, burst) into `mcl-hq`; they need an owner quarantine decision first.
3. A real caller for `gen_map_op` (generate on stepping onto a house) and a loot pickup verb.
4. The specs the checklist lists as "still to write" (CDDA import map, DF job rows, a pet-caller interface for the AI judge) before building those features.
5. Housekeeping the owner knows about: `show_pointer=1` left in `#.desktop/hq_ui.pdl`; stray Mac palnet node; `~/phase2-net` on debil and the Mac; three `.hai.bak` files under `events/phase_dawn`; the two modified `play_rpg_*_planet_map.pdl` cases.

Presentation video of all this: `XO/31.Nov/nov-4/8.deck-survivor-slice-and-roadmap/video.mp4` (149.5 s, progress cut from before the coin-shop release and the live clock run).

### 3e. Added 2026-11-05 (later): what changed since 3d

- **Lock to hero BUILT**: verb `lockhero [on|off]` (toggle without argument) + Menu row; it sets the per-piece flags and the daemon refuses pets while locked. `possess_enter.pal` 21 PASS. The editor window needs a relaunch to show the Menu row.
- **Daemon half of Enter PROVEN** in a scratch house: `possess_enter_daemon.pal` 4 PASS (mutation-tested). Still not shown live: a frame of Enter on a pet reached by arrow keys (xelector stopped one cell short of Ember twice, cause not found). Explanation for hand-placed xelectors: `follow_pchq` pins it to the controlled character.
- **layout_check on the editor board = 0 errors** with `board/layout_runtime_keys.txt` (every line names a writer); harness `layout_check_editor.pal` 5 PASS, a template for other boards.
- **Overlay wrap**: `overlay_wrap_wiring.pal` 6 PASS guards the wiring only; behaviour proof stays the live frame.
- **3D engine absence explained**: its own idle timeout (by design), not a crash or a guard kill. Side fix: `.loop_guard.log` had no cap (2774 lines in 9 h); now capped, `loop_guard_logcap.pal` 5 PASS.
- **Entity menus**: the owner reported Enter doing nothing in the older `menu.chtpm` windows (book-stack) vs newer overlay menus; could not reproduce (relay Enter works for every variant; whether a new menu window gets X focus varied per open, even for the same menu); owner later said it works. Difference to know: overlay menus focus their first item by rule (`ov-focus`, `kh_ov_finish`), the old windows rely on grab timing. No code changed.
- Harness traps recorded in the README: `EXPECT_HAS` reads 16 KB only; `|` inside `sh -c` splits a row; `int`/`running` are switch files.

### 3f. Added 2026-11-05 (night): the audit tools and what they found

- Built and pushed (all harness-proven, see the decision log `02-architecture/DESIGN-CHOICE-TEST-AND-DECISION-LOG.md`): D1 pet possession through `possessed_id` (live with physical keys), D2 harness `INCLUDE`, D3 `SNAPSHOT`/`RESTORE`, D5 `bv_session_sync`, D6 `commit_scoped`, D7 `cpu_gate --stop-engines`, D8 `xo_export`, D9 `growth_probe`. Standing rule in AGENTS.md: ask the design-choice test before building anything with two shapes.
- UNFIXED, found by `growth_probe`: the five event ledgers `events/*/master_ledger.txt` of the editor book are about half one repeated error (`Could not open ops file .../101.mutaclsym...+18.0G/system/default_op.txt`) and are uncapped; `state/relations.txt` and `state/world/messages.txt` are live and uncapped. Table in `03-pitfalls/RUNTIME-FILE-GROWTH-STANDARD.md`. Do not "fix" the missing `default_op.txt` blind: it changes how every event page starts.
- Still open from before: D4 (board_ctl `freeze`/`thaw`, skill made app-generic: the skill file has someone else's uncommitted edits), converting 5 live cases to `SNAPSHOT`/`RESTORE`, Tab/click selection does not update `possessed_id`.
