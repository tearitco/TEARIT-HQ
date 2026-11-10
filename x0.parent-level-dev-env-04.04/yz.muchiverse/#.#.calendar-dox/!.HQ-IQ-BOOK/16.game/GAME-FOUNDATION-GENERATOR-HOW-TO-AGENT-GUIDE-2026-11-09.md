# Making a new game foundation with args: `game_new_op` (how-to for agents, 2026-11-09)

Owner, 2026-11-09: *"we should have harnesses for simple stuff like making new GAME FOUNDATIONS where we could just put in args, by now? document that for future agents and in our XO docs."*
Labels: **OBS** = observed this session with a number; **DOC** = reasoning, check before relying on it; **NOT VERIFIED** = nobody drove it.

## 1. What it is, in one paragraph

`game_new_op <house> new <name> --title "<User Name>" --kind ledger|realtime|editor [--players 2-4] [--seed N] [--into <dir>] [--dry-run]` copies a set of DATA templates (`&.widgits/_shared-lib/harness/templates/game/<kind>/`) into `@.apps/<name>/` and writes a harness case and pal. The generated app has **no C of its own**: its engine, its Play driver and its button op are ONE shared binary, `&.widgits/_shared-lib/ops/+x/game_core.+x` (source `ops/game_core.c`). A new game is therefore a folder of `.pdl` data plus rules in `rulesets/default/`. Verified absence (lead, 2026-11-09): there was no scaffolding for a Teropoly-style game before this; `new_game_module.pal` (other app families), `desk_copy_op`, `piece_mint` and `_template-hq` are different things.

## 2. Make a new game in 5 minutes

```
H=/home/no/Desktop/github/work/NNEST-12.00/x0.parent-level-dev-env-04.04/yz.muchiverse/44.xyz.01.00
cd "$H/&.widgits/_shared-lib/harness/ops" && cpu_gate ... sh build_harness_ops.sh     # once: builds game_core.+x and game_new_op.+x (cpu_gate rule: heavy work asks first)
G="$H/&.widgits/_shared-lib/harness/ops/+x/game_new_op.+x"
"$G" "$H" list                                                                          # the kinds
"$G" "$H" new demo-race --title "Demo Race" --kind ledger --players 3 --seed 7 --dry-run   # the files it WOULD write; nothing is written
"$G" "$H" new demo-race --title "Demo Race" --kind ledger --players 3 --seed 7           # 21 files
"$G" "$H" check demo-race                                                               # 11 checks, exit 0
cd "$H/&.widgits/_shared-lib/harness" && ./ops/+x/harness_case_op.+x cases/demo-race.pdl && ./ops/+x/harness_verdict_op.+x cases/demo-race.pdl   # 42 checks
"$H/&.widgits/_shared-lib/ops/+x/game_core.+x" "$H/@.apps/demo-race" /tmp/demo-race-state new 7     # play it from the CLI: join A, join B, start, roll / autoplay, replay, audit
```
Then: add the README row it prints (harness/README.md is shared, the generator does not edit it), edit `rulesets/default/board.pdl` and `rules.pdl`, relaunch the editor board, pick the book in the Books dropdown, Player > Play. **Do not generate into the live house to "try it"**: use a scratch house (`/tmp/x/{@.apps,&.widgits/_shared-lib/{ops/+x,harness/{cases,results}}}` plus a copy of `templates/`) or `--into /tmp/x` (then the harness case and pal are skipped, they name `$HOUSE/@.apps/<name>`).

## 3. The three kinds

| kind | rule it ships (a STUB, replace it) | ledger | verbs |
|---|---|---|---|
| `ledger` | turn based: a track of `TRACK length N` squares, seeded dice, `JUMP from to` rows (**chutes and ladders are exactly these rows**), first to the end wins | rows of `new join start seat roll ckpt` | `new join start seat roll tick autoplay ckpt status replay check ledger_hash audit ui help` |
| `realtime` | inputs only: a row per `input pN left\|right\|stay` with `round` = the logical TICK, and a `ckpt` row (state hash) every `ckpt_every` ticks. Ticks are never rows; replay simulates between rows with the same step function (a STUB: `x += direction`, first to the track end wins) and a ckpt whose hash differs refuses the ledger. **Pause writes no row** (the driver just stops asking for ticks). The tick INTERPRETER of a real platformer is NOT here | `new join start seat input ckpt` | the ledger verbs plus `input advance` |
| `editor` | a data document: `add set del` are the EDIT rows, `publish` is a rule stub (`rules.pdl` `REQUIRE \| id` rows name ids the document must have; the row records the document hash), `export` writes the projection `doc.txt` | `new add set del publish ckpt` | `new add set del publish export status replay check ledger_hash audit ui help` |

Template per kind: `templates/game/<kind>/MANIFEST.pdl` (`KIND`, `INCLUDE`, `FILE`, `NOTE` rows; the grammar is in the header of `ops/game_new_op.c`), shared files in `templates/game/_common/`.

## 4. What is generated (ledger kind, `demo-race`: 21 files)

| Path under `@.apps/demo-race/` | What | Kind of fact |
|---|---|---|
| `game.pdl` | `GAME` rows: name, title, kind, ruleset, `driver` (the shared binary with `{house} {book} {editor}`), `clock \| pause`. Read by the editor's Play (`petslike_core.c book_driver()`) | CONFIG |
| `state.pdl` | `FILE \| name \| kind \| writer \| regenerate_with`: `ledger.txt` LEDGER, `ui.txt` PROJECTION (editor adds `doc.txt`) | CONFIG |
| `verbs.pdl` | the verbs as data (`help` reads it; `audit` checks that every PROJECTION names a verb listed here) | CONFIG |
| `rulesets/default/board.pdl`, `rules.pdl` | the track and the numbers. **The default folder is where a new game's data goes** (e.g. Chutes and Ladders fills `JUMP` rows). Their hash is in the first ledger row: editing them after a game began refuses that ledger | CONFIG |
| `rooms.pdl`, `layouts.pdl`, `tuning.pdl`, `nav.pdl`, `harness.pdl` | what the editor needs to resolve the book (`rooms.pdl`), bars (empty), pace notes, the driving notes, the `HARNESS` row that shows the case in the editor's Harness tab | CONFIG |
| `demo-race-pchq/pieces/system/maps/demo-race/game.pdl` + `board/{map.txt,cells.txt,cells.rgba,skin.txt,events.txt,extrusion.pdl}` | the book package skeleton: one empty 15 x 15 desk `board` with the Teropoly tile atlas (110 KB, copied verbatim from `deskopoly`'s board desk; the template is a snapshot) | CONFIG |
| `tests/demo-race-play.pdl`, `tests/demo-race-drive.pdl` | STEP scripts for `relay_drive` (Play pressed; the kind's buttons). **UNTESTED LIVE when generated**, the first line says so | CONFIG |
| (harness) `cases/demo-race.pdl`, `demo-race.pal` | the generated case: 42 / 33 / 28 checks for ledger / realtime / editor, all relative (no pinned hash), every check with a NEGATIVE or a vacuity control | test |
| **runtime** `<editor>/state/games/demo-race/game/ledger.txt`, `ui.txt` | made by the engine when a game starts | LEDGER, PROJECTION |

## 5. What the shared engine does and does NOT do

**Does (OBS, case `game_new`, 225 checks since 2026-11-09, 193 at the first version):** a seven-field row `EVENT|round|actor|verb|target|at|result` (the REAL Teropoly shape, so the exporters and the master ledger read it; the lead's request text had a `seq` field, the real ledgers do not, see `CHUTES-LADDERS-AND-SNAKE-GAME-PLANS` section 2); one function (`apply`) makes a row when a verb runs and checks it again on replay; replay refuses (exit 1, `ledger line N: ...`) a hand-edited result, a deleted row, a changed seed, a forged appended row, a first row naming another kind, a rules file edited after the game began; a refused action appends nothing; a seeded `die(seed, idx, k)` with the splitmix64 of `deskopoly.c`; `ckpt` rows pin the state hash; `audit` fails on any undeclared file in the state folder; `ui`/`export` projections regenerate byte for byte; the driver (`game_core.+x driver <house> <book> <editor>`) locks itself, ticks only while the editor's `running=1`, publishes `state/game_ui.txt` (`gm_on`, `gm_pid`, `gm_title`, popup text and buttons), takes presses from `action.txt`, and ends with its owner. **No process is started by the driver** (verbs run in-process), so the process-registry rules have nothing to register; the core registers the driver itself.
**Does NOT (updated 2026-11-09: see section 11b for what the first four games added):** any rule of Teropoly (properties, money, cards), collisions between entities, physics; the 3D figures (`events.txt` walking), the board picture of the game (the `play` desk and `page` verb), the table-talk pane, the chat hook (`GAME \| chat`), the camera snap, personalities, trading. Pets only roll; the pet intelligence is a die. The editor kind is a document with text rows, not a typed level. **A game that needs real rules** copies the shape of this file into its own app (as `deskopoly` did) or extends the data first; it does not edit the shared engine for one game (decision D62).
**Limits found:** (a) in an inputs-only ledger the ticks after the last `ckpt` row are not part of the game (a restart resumes from the last row; the driver keeps its in-memory ticks while it runs, a button press does not rewind them); a deleted ckpt row is not detected, an edited input is (the next ckpt hash differs). (b) `--title` is limited to letters, digits, space and `- _ . ' , : ! & ( )`. (c) `--players` is 2 to 4 at generation time; the engine itself accepts `max_players` 2 to 8 in `rules.pdl`.

## 6. How a generated game gets onto Play (verified half and unverified half)

**Chain (read from the code):** `rpg-edit-hq/board/open_board.sh` links every `@.apps/*/*-pchq/pieces/system/maps/*/game.pdl` into the editor's maps, so "a new book appears without a code change" at the next **editor board relaunch**; the Books dropdown then offers it; picking it writes `state/last_book.txt` (`edit_last.sh save`: a `ROOM` row in `@.apps/<book>/rooms.pdl` or a `maps/<book>/<page>/map.txt`); Player > Play runs `petslike_core.c book_driver()`, which reads `@.apps/<last_book>/game.pdl` `GAME \| driver` and starts it with `{house} {book} {editor}`. `edit_open.sh <house> <book> <page>` is the editor's own read-only opener.
**Verified (OBS):** `game_new_op check` runs the editor's real `edit_open.sh` against the new book and it answers `book=<name> page=board`; the real driver of each kind runs in a scratch editor (case `game_new`). **NOT VERIFIED:** the Books dropdown listing it in the live window, Player > Play starting it, the popup drawn on screen (no live window was driven: another agent was using the editor). **No shared file needs editing for registration**: everything is per-game data. If the live editor does not list the book, the first suspects are an un-relaunched board and `rooms.pdl` (it is a copy of Teropoly's row, whose numbers nobody explained).
**Not made openable:** nothing is missing in the files; what is missing is the live run. The generated Play shows the authored empty `board` desk (no `play` desk, no figures) and the popup.

## 7. How to add a kind or a template

* A **template** is a text file with `{{name}} {{Title}} {{kind}} {{seed}} {{players}}` and a `FILE` row in a `MANIFEST.pdl` that lists exactly the placeholders it uses (a contract: an undeclared one, a declared unused one, a misspelt one, an unclosed `{{x}` and any `{{` left in the output are all refused, case `game_new`). A binary file is `bin` with `-`.
* A **kind** is a folder `templates/game/<kind>/MANIFEST.pdl` (a `KIND` row, `INCLUDE \| _common/MANIFEST.pdl`, the kind's own files). `game_new_op list` finds it. The engine must also know the kind (`game_core.c`: `K_*`, `apply`, the driver publish): a kind that is only data cannot exist yet (DOC).
* A new **placeholder** needs one `Var` row in `game_new_op.c` `cmd_new`.
* Every change to `game_core.c` must keep `game_new` (193), the three generated cases (42 / 33 / 28) and, for a deskopoly user, nothing: deskopoly has its own engine and is not touched.

## 8. Run the case

`cd &.widgits/_shared-lib/harness; rm -f results/game_new.txt; cpu_gate <house> run game_new --wait 900 -- ./ops/+x/harness_case_op.+x cases/game_new.pdl > /tmp/game_new.log 2>&1; ./ops/+x/harness_verdict_op.+x cases/game_new.pdl | head -1` : about 25 s, `VERDICT|PASS|passed=225|failed=0` (OBS, twice, 2026-11-09; it was 193 before the finish rules, talk and export rows). It compiles both programs from source into a scratch folder first, so it tests the source, and it starts three real drivers in a scratch editor (fake owner = a sleeper, fake `switches.txt`). Run also `py_lint` (new=0), `spawn_lint` (new=0) and the case `spawn_lint`; `cpu_lint` on the whole house is red for reasons that were there before (none of its hits is in `game_core.c`); the case runs `cpu_lint` and `spawn_lint` on a scratch copy of `game_core.c` with an empty baseline (0 hits) and proves both can fire.

## 9. Design-choice test (decision D62 in `02-architecture/DESIGN-CHOICE-TEST-AND-DECISION-LOG.md`)

| Question | A: C file copied into each game and built there | B (chosen): ONE shared binary taking the game dir |
|---|---|---|
| one authority per fact | N copies of the same rules, each editable | one source; the game's facts are its data and its ledger |
| modular | per-game C in a game folder (what the house wants to stop) | zero per-game C; a game's differences are `.pdl` |
| scalable (10th game) | 10 copies drift; a bug fix is 10 edits | a fix reaches all, the generated cases are the regression gate |
| long term | each game starts with a fork it has to maintain | the games that outgrow it fork it on purpose (as deskopoly did) |
| house shapes | copy-and-edit is the pattern the audit counts as duplicates (83 groups) | op + data + marker files; no header-link split, no linking across binaries |
| cost and risk | no build path exists for per-game C (the deskopoly binaries have no `build_*.sh` today) | one build line; **a shared binary change touches every game**, mitigated by `game_new` and the generated cases |

## 10. Pitfalls and lessons (each cost a run)

* A bar inside a case field is `\p` even without spaces around it (`VERDICT|PASS` is two fields); the case op reports `row has extra fields`.
* `grep -rl '{{'` through the case op matched everything (the op expands `{{`): write the pattern `[{][{]`. `grep -rI` skips the binary atlas.
* `COPYDIR` copies from `$HOUSE`, not from `$T`: copy a scratch tree with `RUN cp -r`.
* A tamper test must change something only the verification can see: changing the seed of the first row was caught by accident through the game-over rule under the mutant; the check now edits the `start` row's `players=` field.
* A harness-run process that exits is a zombie until reaped: `kill` the fake owner, then `EXPECT_DEAD` it, then wait for the driver (copied from `dsk_driver`).
* `sed` replacement text containing `&` (the house path `&.widgits`) is the matched text: escape it.
* `SUMTREE` hashes files, not empty folders: a vacuity control must add a FILE.
* The setup screen defaults to a full table (`--players N`); a button press reloads the ledger only when the file is not what this process wrote, otherwise a real-time press rewound the ticks since the last ckpt.
* Another agent's data-hq took decision D61 while this was being built: read the tail of the decision log at the moment you append.

## 11. Status

| Item | State |
|---|---|
| `game_new_op` new / list / check, `--dry-run`, `--into` | BUILT, case `game_new` 193 checks, run twice |
| `game_core` engine + driver + ctl, three kinds | BUILT; generated cases 42 / 33 / 28; mutants `GC_MUT_NO_VERIFY`, `GC_MUT_NO_GHOST` caught |
| real driver of each kind in a scratch editor | VERIFIED (setup is a preference, pause writes no row, a human's turn costs no row, owner death clears the feed) |
| idle cost of the driver | OBS once, no browser running: setup screen and stopped 0 CPU ticks in 10 s, 4 wakeups/s; live at the slow pace 2 ticks in 10 s, 12 wakeups/s. Not measured next to a browser; not measured at the fast pace |
| book resolves for the editor (`edit_open.sh`) | VERIFIED read-only |
| Books dropdown / Play / popup in the live editor | **NOT VERIFIED** |
| generated STEP scripts `tests/*-play.pdl`, `*-drive.pdl` | UNTESTED LIVE |
| `build_harness_ops.sh` builds `game_core` into `_shared-lib/ops/+x/` | BUILT (a harness script building a shared op is a stopgap; removal condition: a proper `_shared-lib/ops/build_game_core.sh` when new `.sh` files are allowed again) |
| Chutes and Ladders, Snake, Teario-Make, Teario-Play on top of it | **BUILT as foundations (2026-11-09, D63)**: cases `chutes` 175, `snake` 117, `teario_make` 64, `teario_play` 57; finish rules, talk, export, `MODE snake`, `MODE walk`, the level document and `publish_needs_clear` are generic engine options now; NOT built: the Chutes page (serpentine numbers need a generic `page` verb and the label baker extracted from `deskopoly.c`), figures, physics, a 30 ticks a second loop; see section 11b |

## 11b. Lessons from the first four real games (Chutes, Snake, Teario-Make, Teario-Play; 2026-11-09, decision D63)

**What the generator did well (OBS).** `new` + `check` took about one second per app and left an openable book package, a state declaration, a verbs manifest, a harness case and pal, and the editor Play wiring; each of the four apps was a working, replaying, auditable game BEFORE any data was edited (the stub rule), so every later step was a diff against a green baseline. The `ledger`, `realtime` and `editor` kinds were the right three shapes: Chutes is the ledger kind with data, Snake and Teario-Play the realtime kind with a new MODE, Teario-Make the editor kind with a typed document.
**How long, from args to working (honest).** The apps themselves: generation about a second each; the Chutes board and rules as data, about ten minutes including the sanity of the table. The time went into the GENERIC engine work the games needed (finish rule, talk, export, the snake step, the level document, `clear`), a few hours, plus the cases (every pinned number is observed in a scratch run first, then written): Chutes 175 checks, Snake 117, Teario-Make 64, Teario-Play 57. A new game that needs no new engine option is minutes of data plus its case.
**What had to change in the generator or engine (each has a case row in `game_new`, now 225, or in the game's own case).**
* `RULE finish reach|exact_stay|bounce` and the `TALK` words and the table-talk sentences, the feed keys `last_*` / `talk_0..4`, the verb `talk` (ledger kind).
* `export <out_dir>`: `game.csv` (one row, `game_id` = the ledger digest), `events.csv`, `moves.csv` (ledger kind), `schema.pdl`; the template `_common/game.pdl` now writes a `GAME | export` row, so every new game is found by `data_op provider`. An out folder inside the state folder (checked by real path, a relative state folder had slipped past the first version), one that holds a `ledger.txt`, or a ledger that does not replay is refused and nothing is written.
* `MODE snake` (grid, seeded food, solid or wrap walls, growth), `MODE walk` + `LEVEL` (a hero along the START row of a level file; the file's text is part of the rules hash), the realtime consequence rows (`eat` `die` `over`) that replay must remake, `turn` with the house key codes 1000-1003, the verb `keys <file> <offset>` and the driver's own byte-cursor tail of `state/interact_relay.txt`.
* the editor `doc_format level` (+ `level.pdl` projection), `clear <play app> <play state>` (a short-lived child process, 10 s watchdog) and `publish_needs_clear`.
**Limits of the generator found (not fixed; each is a one-line change when someone needs it).** (a) `--players` is 2 to 4: a one-hero game (Snake, Teario-Play) is generated with 2 and the rules edited to `min_players` / `max_players` 1. (b) `game_new_op check` is the ledger acceptance check: it joins two players and, for an editor, publishes an empty document, so it FAILS on a game customised on purpose (Snake, Teario-Play: "the table is full (1 players)"; Teario-Make: "no row name"). The game's own case is the real gate; `check` is for fresh apps. (c) The generated case's checks that edit `TRACK length 30` or the `players=2,` tamper target break when the data changes: edit them with the data (Chutes: 30 to 100). (d) `GAME | export` ran only through `data_op provider`; the editor's Play ignores it (D61). (e) A case that edits an app copy must copy the shared folder too when the app reads `../teario/...`.
**Pitfalls that cost a run (add to section 10).**
* The case op keeps at most 32 variables; a case with many `CAPTURE` / `CALC` runs out and later `$x` stay literal ("cannot compute '$sim_m4'"). Reuse a small pool of names.
* A `|` inside a RUN field splits it, also inside a `grep -E` pattern or a `3|4`-style argument: write `\p`.
* `SCRATCH` makes a NEW `$T`; a block placed after a `CLEAN` has no scratch house. Add rows before the last `CLEAN`.
* `data_op provider` needs the exporter binary under `{house}`; a scratch copy of an app has the row but no binary there, so run the provider on the LIVE app against a scratch state folder.
* `sed -i s/x/y/` on a row with bars: the case op turns `\p` into `|` before the sed sees it; count the fields of the row you edit (the `add` row is `EVENT|round|actor|add|id|rows|text`).
* A replay of a ledger written under one board with another board's rules is refused at row 1 (the rules hash): that is the rule file's protection and a good negative control, but it also means a scratch `COPYDIR` of an app that was edited changes the hash of every ledger made under the original.

## 12. External references

Repo: `&.widgits/_shared-lib/harness/ops/game_new_op.c`, `&.widgits/_shared-lib/ops/game_core.c`, `&.widgits/_shared-lib/harness/templates/game/`, `harness/cases/game_new.pdl`, `harness/README.md` (row `game_new.pal`), `@.apps/deskopoly/{game.pdl,state.pdl,verbs.pdl,ops/}` (the working reference), `@.apps/rpg-edit-hq/board/open_board.sh`, `ops/edit_open.sh`, `ops/edit_last.sh`, `&.widgits/_shared-lib/petslike_core.c` (`game_row`, `book_driver`). Book: `16.game/TEROPOLY-ONBOARDING-HARNESSES-DRIVING-AND-CPU-LESSONS-2026-11-08.md`, `16.game/CHUTES-LADDERS-AND-SNAKE-GAME-PLANS-2026-11-09.md`, `16.game/TEARIO-MAKE-PLAY-LEVEL-EDITOR-AND-PLATFORMER-PLAN-2026-11-09.md` (section 5, shared core), `02-architecture/NO-GHOST-DATA-THE-LEDGER-IS-THE-AUTHORITY-2026-11-07.md`, `02-architecture/DESIGN-CHOICE-TEST-AND-DECISION-LOG.md` (D62).
