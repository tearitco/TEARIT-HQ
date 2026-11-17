# Grok's work, inventoried (2026-11-10 to 11-11), and the "harness / plugs / harnz" naming question

Written 2026-11-14 by claude after merging. Labels: **READ** (I opened the file or diff), **RAN** (I ran it today and saw the verdict), **SAID** (the owner told an agent), **DOC** (my reasoning, not a fact). Git cannot tell the agents apart (every commit is by `tearitco`), so authorship below comes from the docs inside the commits and from the branch (`grok` worktree `.kilo/worktrees/grok`).

## 1. The merge state

- READ: 52 commits between `6ca6fc221` (my skyland blueprint, 11-10 00:55) and `59d6c353f` (Grok's last, 11-11 04:58): 81 files, +4406 / -83. No `xyzfs/users` paths in the range.
- `main`, `grok` and `claude` were fast-forwarded together (guarded `git update-ref`). Today's tip is `8092810d4` (my camera fix on top of Grok's last commit). Nothing is pushed.
- RAN: the board-viewer programs were stale. `bv_render_3d.+x` was built at 01:58 on 11-11, before Grok's minimap commit (03:36), so the owner saw "the minimap fix is gone". Rebuilt with `scripts/build.sh` under `cpu_gate --stop-engines`, then a full `button.sh reset` (exit 0).

## 2. Inventory by area

| Area | What Grok did (commit subjects, READ) | Proof I have |
|---|---|---|
| **Skyland crew and factory** (11-10) | `a51df7607` job list, `7261a1efc` first build FSM on the chapel job list, `945f1277a` chapel crew harness + dig marks, `65cdbb112` route a pet turn to an existing verb, `3b8d6c797` a scratch pet walks to one stone and places it; one factory (`mech_new_op`) raises every `BUILD` row: stage, hut, fountain, bridge, chapel, all 12 (`61f6ec97d`..`4cc65fe02`); underside verb for sky islands S3 and S6 | RAN: case `centroid_crew` 146 passed 0 failed; `play_centroid_crew` 11/0 |
| **Land generator and kits** (11-11) | `land_op` takes a seed and scale from a pdl (`148d37694`), levels, wider roads, snow band, creator kits (`6daa97492`; D73: biome, road, snow, level, tree as kit pdl files), floors use the terrain row (`d2e836942`), legends for rmmv, mineclonia, df, cdda, doom (`f17ebfeee`) | RAN: `land_gen` 12/0 |
| **Enter a house** | `enter_op`: one house, a shut door that refuses, an open door that climbs one stair (`0da03c1ac`) | RAN: `enter_house` 7/0 (the `_small` twin shares its result file) |
| **Castle from modules** | docs only: D71, a building is a count of five modules (`wall`, `window`, `door`, `roof`, `tower`); the modules are NOT yet in the parts file (the decision log says so) | READ only |
| **Pages and book menus** | `copy page` row and the page menu lists real desks (`n_desk_opts`) (`6f8942a60`), `small` page for the first island, `ark-age` page (a copy of live, `df8e9ea3d`) | READ; page menu not clicked by me |
| **Harness menu** | global rows in `#.desktop/harness_menu.pdl`; only confirmed-proof cases; `- cancel -` on every dropdown; `run all` = make, test, silent proof video, doc, continue (`proof_all_op`) | READ; `run all` NOT run (it rewrites `XO/31.Nov/nov-11/grok/0.proof-all`) |
| **2D mode 0 and wide pages** | RPG Maker atlas blit, 2D camera file, wasd/zxcv no longer eaten as a cell ref, z level, cursor crosses pages wider than 256, `bv_render_2d.c` +256 lines | owner saw "mode 0 uses the right colours"; I did not drive it |
| **3D and minimap** | pages up to 512 wide drawn whole, live fog to 160 (`3cdb12229`), corner map fits a page wider than 256 (`ad70266e2`, new `bv_minimap_fit.h`, `minimap_fit_op`) | RAN: `minimap_fit` 17/0; owner: "minimap size is fixed" |
| **Tools** | `map_json_op` page map to JSON + an HTML size page | READ; no case of its own seen |
| **Teropoly** | popup gets a `Close` button (before `Find me`); on exit the desk returns to `test` when the map is centroid | READ; see the failure below |
| **Docs** | `ONBOARD-2026-11-10-SKYLAND-AND-CREW.md`, `WEAKNESSES-2026-11-11.md`, `23.tools/maps/MAP-TO-PAGE.md`, `CENTROID-TEST-AND-LIVE-2026-11-06.md` (+92 lines: this is the reconciliation I asked for), decisions D69 to D73, pitfalls section 28 | READ headers; bodies not audited line by line |

## 3. What is wrong or unproven today (found 2026-11-14)

1. **RAN, FAIL: `dsk_camera` 72 passed, 3 failed.** Failing labels: `possess/CONTROL-not-possessing-it-moves`, `possess/a-fresh-editor-launch-possesses-hero_01-the-snap-lets-go-of-it-and-moves`, `tuning_off/CONTROL-and-it-moves-the-cursor` (cursor never moved within 6 s). Cause not found. Suspects (DOC): the new cursor limit that reads `cells.txt`, the `Close` button shifting popup indexes, or the stale-binary state when I ran it. Not fixed (owner said: test everything, fix nothing yet).
2. **The minimap lost its tile colours.** `ad70266e2` replaced the per-cell colour arrays with three flat colours (water blue, wall brown, floor green). Snow and roads cannot show. Owner noticed. **FIXED 2026-11-16 (claude): the corner map samples the atlas tile colour per cell (`bv_minimap_tile_avg`, harness `minimap_fit` 25), see `CENTROID-TEST-AND-LIVE-2026-11-06.md` "Minimap colours".**
3. **FIXED today (`8092810d4`): the `\` skin swap snapped the 3D camera home.** Every reload bumps the cell-set id and the camera baseline was keyed on it. It is keyed on the desk folder now. RAN: camera values and `cam_home.txt` unchanged across two swaps.
4. **Live page uses the "gen" sheet by default.** `live/skin.txt` is `gen` (red and blue swapped: blue grass) in the working tree; HEAD says `rmmv`. The owner toggles with `\`. Left on `rmmv` after my test. These live-page files are uncommitted data.
5. `In:` toggled by mouse four times in a row for me (off, on, off, on) with two clicks lost to timing; the owner saw it working. Nothing in Grok's diff touches it. Treat the earlier report as a fluke, not proven.
6. Grok's own `WEAKNESSES` doc says commits go from its worktree with `--branch grok`; the 52 commits are on `claude` through that branch, which is why `claude` was already at Grok's tip. Fine, but the "shared checkout" worry is partly solved: Grok has a worktree.
7. My own mistake: I relaunched the editor (`open_board.sh`) while the owner had it open and was pressing `\`. The owner had said "have u drive it", but not while they were using it. Ask first next time.

## 4. The naming question: harness, plugs, harnz (owner thinking, 2026-11-14)

**Facts (READ).**
- The top taskbar's cell 11 used to be the inert "plugins" cell. On 11-07 (`73de0cc69`) it became "harness", and the editor's Harness tab shares one list (`66a36a8a3`).
- A row is `HARNESS | id | page | kind | label | target | args | writes | confirm | note`. `page` is `*` (every page) or a page; global rows are in `#.desktop/harness_menu.pdl`, a book's own rows in `@.apps/<book>/harness.pdl` (six exist: centroid, chutes, deskopoly, snake, teario-make, teario-play). `kind` is `CASE` (run a test case, no arguments) or `ACTION` (does something to the page, can take arguments, for example the `land` row).
- The word "harness" already means something else in this house: the test harness (pal + case + C ops, 120+ cases, `AGENTS.md`). The menu reuses it for creator actions as well as proof cases.
- RPG Maker analogy (DOC, from the owner's message): plugins are project-global code with parameters, set up once in a Plugin Manager, and are not clicked from a menu. Events are per page. Common events are project-global and callable. The owner's rows behave like page events that you can click, not like plugins.

**The owner's leaning (SAID): a "Tools" button with two lists.** `harnz` (or similar) = scripts that belong to this page: they run from a click and change the map or entities. `plugs` = the project's global plugins. The owner likes this most.

**My reading (DOC, for the owner to overrule).**
- It matches the existing data. One row format with a scope column already exists (`page`), so `harnz` and `plugs` can be two filtered views of one list or two files of one format, not two systems. That is the design-choice test: one authority per fact, the 10th consumer is another row.
- Keep the two jobs apart: **tests** (`CASE`, agents, the existing "harness") and **creator actions** (`ACTION`, what the owner means by "runnable script that does stuff to a map"). A creator-facing list that mixes proof cases with "grow the island" is part of why the name feels wrong.
- A name should say its scope. "harnz" ties to "harness" (so agents who know the old word find it) and the `z` marks it as the page-level kind; "plugs" ties to RPG Maker plugins. The cost of changing the user-facing label is small (policy from the LyfeDesk rename: user-visible text changes, internal ids such as `harness`, `HARNESS` rows and file names stay until a deliberate rename).
- Unverified: whether the `page` column really filters the dropdown by the open page today. I read the header comment, not the filter code.

**Questions for the owner** (decision-log entry D74, status OPEN):
1. Names: `Tools` button, `harnz` (page) and `plugs` (project)? Or keep "harness" for the test list only?
2. Do proof/test cases (`CASE`) stay in a separate, agent-facing list, with only `ACTION` rows in `harnz`?
3. Should `plugs` be callable from a harnz script (like a common event from an event), or only run by the Tools menu?
4. Is a plug loaded at project start with parameters (RPG Maker style), or run on click?

## 5. External references

Repo (`x0.parent-level-dev-env-04.04/yz.muchiverse/`): `#.desktop/harness_menu.pdl`, `@.apps/<book>/harness.pdl`, `@.apps/rpg-edit-hq/board/pchq-board.xhtpm` (tabs `tb-harness`, `tb-page`), BOOK `13.agent-coms/GROK/WEAKNESSES-2026-11-11.md`, `ONBOARD-2026-11-10-SKYLAND-AND-CREW.md`, `16.game/CENTROID-TEST-AND-LIVE-2026-11-06.md`, `02-architecture/DESIGN-CHOICE-TEST-AND-DECISION-LOG.md` (D69 to D74). Commits: `73de0cc69`, `66a36a8a3` (harness cell), `ad70266e2` (minimap), `8092810d4` (camera fix).
