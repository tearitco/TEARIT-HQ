# STANDARD: rules as ops, designable and debuggable through Blueprintz (a reusable foundation design, 2026-11-18)

Written by Claude Sonnet 5.5 for the owner, to copy into any new project that has the same foundational needs: a thing with RULES (games, simulations, workflows, agents, markets) that must be **scored by a harness, reused by an FSM or an AI tool call, designed in a visual editor and debugged step by step**. Labels: **FACT** = read in the repo, **RAN** = I ran it and saw the result, **DOC** = a decision or proposal in this doc (not built unless it says BUILT). The decision-log entries behind it: D62, D63, D90, D92, D93 in `02-architecture/DESIGN-CHOICE-TEST-AND-DECISION-LOG.md`.

## 0. Why this exists (the mistake it prevents)
On 2026-11-18 Teario's enemies, stomp, fireball, power-up and patrol were written as C constants inside the shared engine `game_core.c`. The plan said "all mechanics as events", Blueprintz showed nothing, nothing could be scored alone or reused by an FSM or AI. The owner's ruling (D92): **no native rules, all ops, forever, always, no exceptions**: that is how harnesses and AI can score reuse in FSM and tool calls. Speed is not a reason to go native: build the ops, measure, and only after the behaviour is proven and approved consider the wraith-alpha in-memory DB.

## 1. The five rules (owner rulings; do not weaken)
1. **Every rule is an op**: a small reusable program invoked by name with data in and rows out. The engine is only the loop that calls ops and writes the ledger.
2. **The ledger is the only authority for state** (no ghost data, `02-architecture/NO-GHOST-DATA-THE-LEDGER-IS-THE-AUTHORITY-2026-11-07.md`): a rule op APPENDS rows; every other file is a declared PROJECTION, CONFIG or PREFERENCE.
3. **Numbers are data**: RULE rows, part of the rules hash, so a changed number is a different game and replay refuses a forged run.
4. **Events are the content, the op is the behaviour** (`PAL-VS-EVENTS.md`): the rule is authored as an event page that names the op; the page is visible and editable in Blueprintz.
5. **A harness case that can fail sits beside every op** (proven by sabotage), and anything a person must see is checked in the real window (open the picture).

## 2. The unit: one rule = four files (+ one page)
| # | File | Role | Example (BUILT) / (PROPOSED) |
|---|---|---|---|
| 1 | `_shared-lib/rule_<name>.c` | the PURE module: integer only, no files, no clock, no float, no environment; the ONE implementation; text-included | `phys_field.c` (BUILT, D90) / `rule_patrol.c` (BUILT 2026-11-18, case `rule_patrol` 27 checks, registry row, event page and Blueprintz proven) |
| 2 | `ops/rule_<name>_op.c` | thin CLI op that includes (1): `<op> <app> <state dir> <args>`; a PURE STATE -> STATE function over a scene: reads the scene rows and RULE numbers, prints the trace and a hash, exit code (0 ok, 2 a bad scene); ledger rows are written only for inputs and for consequences that are facts (a clear, a publish), because in an inputs-only run ledger tick state is derived by replay (refined 2026-11-18 by the first built rule); a `--describe` line (inputs, outputs, RULE keys) | `phys_field_op.c`, `rule_patrol_op.c` (BUILT) |
| 3 | one `COMMAND` row in `#.ref/menu/event_commands.registry.pdl` | `COMMAND <type> / LABEL / FIELDn / PARAMS / TEMPLATE exec "<op>" '{param}' / END`: makes the op appear in events-hq and Blueprintz with no manager recompile | the 172 existing rows are the pattern |
| 4 | `harness/cases/<name>.pdl` + `.pal` | runs the op ALONE with hand-computed expectations plus a sabotage proof | `phys_field.pdl` (23 checks, BUILT) |
| + | `@.apps/<app>/events/<rule>/event_pkg/pages/page_1/{event.ir.pdl,condition.pdl}` | the event page that names the op; edited in Blueprintz | the rpg-pet / rpg-space pages are the pattern |

**Why a pure module AND a CLI op (the key idea):** the tick loop must not start a process per tick (measured standard, section 4), so it calls module (1) IN-PROCESS; an event page, Blueprintz, a harness, an FSM node or an AI tool call calls op (2) by name. Same code, one authority, no divergence. This is the house sharing pattern: text-include a pure helper, thin op around it (memory: feedback-no-header-link-split).

## 3. The layers (one direction, never skip a layer; `EVENT-ARCHITECTURE-ONBOARDING-2026-11-05.md`)
1. **When**: triggers: `ui`, `clock`, `on-click`, `on-touch`, `Autorun`, ... (FACT). Per-tick, contact, tile-hit and input-edge triggers are NOT built (DOC, "N2" in the Teario plan section 2.2).
2. **What happens**: event pages (IR nodes, trigger, compiled `event.pal`, one wrapper per node, a C op does the work).
3. **The record**: one append-only ledger per game: `EVENT|ts|actor|verb|target|at` rows, `ckpt` rows pin the state hash every N ticks; replay refuses a forged row (only with a ckpt row after it).
4. **What you see**: a projection (`state/ui.txt`, scene, picture) derived from the ledger; never read back as input.

## 4. The tick standard (RAN, `16.game/REALTIME-SPIKES-HELD-KEYS-TICK-RATE-PICTURE-RATE-2026-11-09.md`)
- 30 logical ticks a second, ONE thread, NO process per tick, 0.9 percent of one core on the weak dev machine, 0 dropped; idle 0.00 percent. `TUNE | tick_ms | N` (33 to 2000) in `tuning.pdl`, a preference, not in the rules hash.
- A late wake runs at most 4 steps and drops the rest (counted); a stall slows the game, it never bursts; ticks are logical so the ledger is identical.
- Held keys: X delivers presses only, never a release: use LATCH input (a press sets the direction) not hold.
- **Consequence:** per-tick rules are called in-process through the pure module; slow or rare rules may run as ops through events.

## 5. Determinism (DOC, all RAN for `phys_field`)
Integer fixed point (Q8: 1/256 of a cell, `long`), no float, no clock, no `rand()` (use a seeded die keyed by `seed` and `tick`), no iteration order that depends on memory. The module's output is hashed into the state hash; a replay of the inputs must give the same hash on every build.

## 6. Blueprintz: designable, debuggable, viewable (FACT; `08-roadmap/design-docs/BLUEPRINTZ-DESIGN.md` 12 and 13)
- **View / design:** window shows an event's commands (NODE rows); `Load <app> verbs` / request `LOAD:<app>` loads an app's `events/*` pages; `books.pdl` loads a book. Edit a command's parameters (`SETP`, one labelled field per parameter), add, delete, undo (16 deep), diff preview; only types that are `COMMAND`s in the registry are accepted. Every write goes through `bp_edit_op` -> the events-hq manager (the same path as events-hq).
- **Debug:** Play, Pause (triggers queue), Resume, Fire, Step ev, Step cmd, Run end, Clear queue. The prisc VM has a step hook (`PRISC_STEP_DIR`, one trace row per instruction, token file releases the next); `play_event.sh` has the pause gate; `bp_ctl` is the controller (`status pause step-event step-cmd resume abort run-to-end clear-queue`). **One VM step = one `exec cmd_N.sh` = one OP call**, so each rule op is exactly one steppable command. Harness `blueprintz_debugger` (34 checks) and `blueprintz_window`.
- **A game's buttons are event pages** (`16.game/EVENT-VERB-PAGES-DESIGN.md`), so a game is designable there too.
- **Game-level stepping:** the driver has a `step` verb and a Step button while paused.
- **Limits (FACT):** edits only `page_1`; each edit blocks about 2 to 3 s; no graph view (B5); a deleted command comes back at the END; the tracked VM binary may lack the step hook (command stepping then falls back to event stepping): CHECK `101.mutaclsym*/system/prisc+x` before promising Step cmd.

## 7. Checklist for a new project with the same foundations
1. Read this doc, D92, D93, `PAL-VS-EVENTS.md`, `BLUEPRINT-EVENT-PAGES-GUIDE.md`, `NO-GHOST-DATA...`, the tick spike doc. Answer the five design-choice questions in the decision log BEFORE building (`DESIGN-CHOICE-TEST-AND-DECISION-LOG.md`).
2. Name the state: declare every file in `state.pdl` (`FILE | name | LEDGER|PROJECTION|CONFIG|PREFERENCE | writer | regenerate_with`); the `audit` verb fails on a ghost.
3. List the rules; for each one build the four files and the page (section 2), smallest first; numbers in RULE rows.
4. Write the loop LAST and keep it dumb: gather inputs, call the pure modules in order, append ledger rows, `ckpt`, publish the projection.
5. Harness: a case per op alone (hand-computed numbers, hand-computed from the rules, not from the code), a replay case (same inputs, same hash), a forged-row case, a ghost-file case, a regenerate-projection case; SABOTAGE each (break the rule, watch the case fail, restore, `cmp` the source).
6. Registry rows, then `LOAD:<app>` in Blueprintz and photograph the pages; step one op with Step cmd.
7. Real window proof: run the real thing in the editor, open the PNG, assert what is on screen (a case that passes on files while the screen is empty is a failed test).
8. Commit only your paths (`commit_scoped.+x`), update the ONE doc a route row points to, add the decision-log row.

## 8. Worked examples
- **BUILT, the right shape:** `phys_field` (D90): pure module + `phys_field_op` + case `phys_field` (23 checks, sabotage-proven); also reused by `game_core` MODE platform. It is the pattern to copy.
- **BUILT first rule (2026-11-18):** `rule_patrol` (one body, turn at a wall or ledge, fall out): module `_shared-lib/rule_patrol.c`, op `ops/rule_patrol_op.c`, case `rule_patrol` (20 checks, hand-computed, 5 sabotages), called in-process by `game_core`, speed = RULE `patrol_speed`. The registry `COMMAND` row, the `events/tm_patrol` page and `teario (book)` in Blueprintz are ALSO built and were driven in the real window (view, edit a parameter, Pause / Fire / Step ev, trace). `rule_fireball` is BUILT the same way (module, op, registry row, page, 27 checks, 9 sabotages; its targets are `rule_patrol` walkers: rules compose). `rule_stomp` (stomp / hit contact) is BUILT the same way (25 checks, 9 sabotages; two survived the first draft, see D93 progress 4: always sabotage every clause). `rule_pickup` and `rule_goal` are BUILT the same way (24 and 18 checks; every clause sabotaged and caught first time because the scenes were designed one per clause). `rule_spawn` is BUILT too (19 checks, 12 sabotages caught first time; one rule for load, hit and fall). `rule_fall` and `rule_input` are BUILT (17 and 20 checks). `rule_win` is BUILT too (18 checks). Score / counter totals are deliberately NOT a rule (bare `+=` of counts the rules already return; D93 progress 8). Next: MODE snake / track / walk.
- **The counter-example (do not copy):** Teario's enemies / stomp / fireball in `game_core.c` MODE platform (D92): behaviour that no harness can run alone, no page, nothing in Blueprintz.

## 9. Pitfalls that cost real runs (all hit 2026-11-18)
- Preferences persist between games (speed, human seat): a blind `speed_next` or `human_next` cycles them to the wrong value; press until the feed label says what you want.
- A harness `RUN` row: each field is ONE argv entry, no trailing `# comment`, `\p` is the pipe character, keep a row short; a scratch folder name used twice by two cases collides (`s2` was reused).
- An exit code you guess is a wrong pin: run the op once, read the code, then pin it (a refusal was 2, not 1).
- A pinned count that your change legitimately moves (`declared=4` after adding one declared file) is an expected edit of the pin, said so in the commit.
- `pkill -f` or `ps | grep` with the pattern in your own command line kills your own shell; kill by pid read from `awk` with an exclusion.
- A shared engine sleeps until woken: a changed events file does not redraw by itself (the engine needed a nudge).
- Never `git add -A`; never touch another agent's staged files; heavy work through `cpu_gate`; nothing under `xyzfs/users`.

## 10. Not built / open (honest)
Tick, touch, tile-hit and input triggers (N2); entity-pair and tile-contact vocabulary (N1); per-tick stepping through the VM (the tick standard says no process per tick, so game Step plus Blueprintz Step on the page); a Blueprintz graph view (B5); editing past page 1; the wraith-alpha in-memory DB (later, after the behaviour is proven and approved); and the actual unpacking of `game_core` MODE snake, track, walk and platform into ops (D92 fix order).

## 11. External references (paths from the repo root `NNEST-12.00/x0.parent-level-dev-env-04.04/yz.muchiverse/`; the BOOK is `#.#.calendar-dox/!.HQ-IQ-BOOK/`)
- Decisions: BOOK `02-architecture/DESIGN-CHOICE-TEST-AND-DECISION-LOG.md` (D62, D63, D90, D92, D93).
- Events: BOOK `02-architecture/EVENT-ARCHITECTURE-ONBOARDING-2026-11-05.md`, `BLUEPRINT-EVENT-PAGES-GUIDE.md`, `PAL-VS-EVENTS.md`, `PRISC-OPS-ARCHITECTURE.md`; `16.game/EVENT-VERB-PAGES-DESIGN.md`.
- Blueprintz: BOOK `08-roadmap/design-docs/BLUEPRINTZ-DESIGN.md` (12.12 to 12.22 and 13); code `44.xyz.01.00/&.hq-apps/blueprintz/`; registry `44.xyz.01.00/#.ref/menu/event_commands.registry.pdl`.
- Tick and realtime: BOOK `16.game/REALTIME-SPIKES-HELD-KEYS-TICK-RATE-PICTURE-RATE-2026-11-09.md`.
- State: BOOK `02-architecture/NO-GHOST-DATA-THE-LEDGER-IS-THE-AUTHORITY-2026-11-07.md`.
- Code examples: `44.xyz.01.00/&.widgits/_shared-lib/phys_field.c`, `.../ops/phys_field_op.c`, `.../harness/cases/phys_field.pdl`, harness README `.../harness/README.md`.
- Teario plan (the worked project): BOOK `16.game/TEARIO-MAKE-PLAY-LEVEL-EDITOR-AND-PLATFORMER-PLAN-2026-11-09.md` section 2 (all mechanics as events) and 2.7 (this contract).
- Wraith-alpha in-memory DB (future): memory `wraith-alpha-in-memory-db-plan`; not started.

## 12. How to put a rule into Blueprintz, step by step (RAN 2026-11-18 with `rule_patrol`; the repeatable recipe)
1. Registry: add one `COMMAND <rule>` block to `#.ref/menu/event_commands.registry.pdl` (`PARAMS`, `TEMPLATE exec "$D/<op>" "$D/<scene path>" '{param}'`). Allowed characters in a parameter value: letters, digits and `_ - . : , = | / space`: a path with `@` or `&` must be built by the TEMPLATE from `$D`.
2. Event: `mkdir -p @.apps/<app>/events/<name>/event_pkg/pages/page_1`, put the scene or data file beside it, then seed with the real manager: `BP_PAGE=page_1 bp_edit_op <house> <event_pkg> <name> append <type> '<k=v|k=v>'` and `... trigger ui`. NEVER hand-write the IR, the `.pal` or the wrappers. (Plain `new_page` on an empty `pages/` makes `page_2` and leaves `page_1` empty.)
3. Blueprintz list: add `BOOK | <id> | events | @.apps/<app>/events` to `&.hq-apps/blueprintz/books.pdl`; the window lists `<id> (book)` in its "Load verbs of a game" menu. Open the window with `cpu_gate ... sh &.hq-apps/blueprintz/button.sh run`, drive it with `relay_drive` (gentle mode, `tapid <id>`; ids are in `#.desktop/ascii_frames/<pid>.<seq>.objects.pdl`; pick a dropdown row with Down keys + Enter).
4. Proof in the real window: the page is listed with `no problems found`; edit a parameter field (clear it first); Pause, Fire, Step ev and read the trace rows; Detach; `button.sh stop`. Remove the runtime files the run leaves in the event folder (`inventory/`, `master_ledger.txt`, `.hq_manager/`) before committing.
5. Harness: registry row present (and a not-present negative), the page's command and trigger, the compiled wrapper prints the same line as the op alone; sabotage the registry row and the wrapper.

## 13. A rule is not proven until every clause has been SABOTAGED (lesson from `rule_stomp`, 2026-11-18)
Break each clause of the module on purpose (drop a test, flip `<=`, ignore a number, remove a `break`), rebuild the op, run the case, and require a FAIL; restore and `cmp` the source. For `rule_stomp` two of nine sabotages SURVIVED a case I thought complete: the "falling" test (my "not falling" scenes failed the depth test for another reason) and a hit that does not stop the scan (I had only put a stomp before a hit). Each survivor means a missing scene: find one input where ONLY that clause decides the result, hand-compute it, add it. A pin copied from the program's output is not hand-computed; a case nobody has tried to break proves nothing.

## 14. Design the scenes per clause BEFORE you sabotage (worked in `rule_pickup` / `rule_goal`, 2026-11-19)
List the clauses of the module (each test, each number, each loop exit, each quirk). For each write ONE scene in which only that clause changes the answer, hand-compute it, then sabotage the clause and confirm exactly that scene fails. Doing it in that order caught 15 of 15 sabotages the first time; doing it after the fact (`rule_stomp`) left two survivors.

## 15. A scene must make the sabotaged answer DIFFER (lesson from `rule_fall`, 2026-11-19)
When you design the scene for a clause, hand-compute the answer AND the answer the plausible-wrong version would give, and check they differ. `rule_fall`'s "far above the world" body at y -500 gave "not fallen" for the real rule, but also for an `abs()` mirror (500 is inside the 3584 limit), so the sabotage survived; y -4000 (mirror 4000 > 3584) makes them differ. Section 14 says one scene per clause; this says each scene must be able to fail.
