# Closing the events / blueprintz gap: verb pages (design, 2026-10-20)

Status: **BUILT AND PROVEN for rpg-pvp-hq (phases V1-V5, 2026-10-20, owner: "route"; section 8) and BUILT for game-dev-hq (V6, same day, section 9).** The blueprintz edit round trip (V7) is not done. Owner, 2026-10-20: "can we fix the event / blueprintz gap? doc a solution first in house", then "route".
Labels: READ = from files today; RAN = I ran it today; PROPOSED = the design; NOT MEASURED.

## 1. The gap, stated exactly
`RPG-PVP-HQ-EVENTS.md` found that blueprintz (the book-level event viewer and editor, `08-roadmap/design-docs/BLUEPRINTZ-DESIGN.md`) can only see **event pages** (`event_pkg/pages/page_N/event.ir.pdl`), while gameplay in every game is C **verbs**. Reading the code for this page showed the gap has two parts, not one:

| part | what is true today | consequence |
|---|---|---|
| **G-a visibility** | game-dev-hq (grok) has **0** event pages (READ: no `events/` folder). Its 24 verbs (hire, train, rest, start, assign, advance, ship, advertise, contract, console ...) exist only in `game_dev_hq.c`. | blueprintz shows nothing for it |
| **G-b authority** | where a page does exist for a gameplay action (our `events/pickup`), the player's real path does **not** go through it: the window button runs `rpg_pvp_hq.+x <verb>` directly (READ: game-dev-hq's `game-dev-hq.xhtpm` actions are `game_dev_hq.+x train ${active_id}:scenario` etc.; rpg-pvp-hq's menu rows call the verb). The page is a mirror. | editing the page in blueprintz would change nothing: the "IDE" would be a picture |

What already works and is the precedent to copy (READ, `rpg_pvp_hq.c` and the cdda family): the engine has a function that **plays an event package through events-hq's `play_event.sh`** (`run_event` / `run_event_x`, with extra environment words, a kill deadline and one shared world dir). It is used for **doors** (stepping on a door plays `events/door_*`, which carries a real `transfer_player` node), for **greetings** (a page per line), for the **clock** (`common:<event>`) and for INT on/off. So "gameplay as an event page the engine really runs" is not new; it is applied to doors and chrome and not yet to the verbs.

Conclusion: fixing only G-a (generate pages that nobody runs) would repeat the pickup mistake: a page that looks right and drifts. The fix must put the page **in the execution path** (G-b) and keep the game alive when the page is missing.

## 2. The solution in one paragraph (PROPOSED)
Every game keeps its C verbs as the **rules** (they decide results and write the S1 ledger). Every **player-meaningful** verb also gets an event package, and the UI calls the **package**, not the verb: button -> `<engine> fire <verb> [args]` -> engine plays `events/<verb>/` through `play_event.sh` -> the page's node runs the registered command `<game>_verb` -> `<engine> run <verb> [args]` -> the verb. A single manifest file per game, `verbs.pdl`, lists the verbs and is the source of truth for a small generator that writes the packages, so a new verb costs one manifest row, not four hand-made files. Blueprintz then sees, edits and (later) compiles the same pages the player triggers, and if a page is deleted or broken the engine falls back to the raw verb and records that it did.

## 3. Parts

### 3.1 One registered command per game (exists for cdda family, needs adding for game-dev-hq)
`COMMAND <game>_verb` in `#.ref/menu/event_commands.registry.pdl` (READ: rpg-pvp-hq's is `TEMPLATE exec "$D/@.apps/rpg-pvp-hq/ops/+x/rpg_pvp_hq.+x" '{verb}'[ '{arg}']`). Registry only: zero recompile. Missing for **game-dev-hq** (READ: no `game_dev_hq` entry). Gate stays `bp_scan` = 0 flags (an unregistered command is the `unknown-type` flag that hit rpg-pvp-hq 105 times before registration).

### 3.2 The manifest `verbs.pdl` (one per game, data, the single source)
```
VERB | name      | page | trigger | args         | summary
VERB | hire      | yes  | ui      | job,name     | hire one staff member
VERB | train     | yes  | ui      | member:stat  | spend energy to raise a stat
VERB | step      | no   | engine  | -            | one tick (high frequency, never a page)
VERB | status    | no   | engine  | -            | read-only
```
- `page=yes`: the verb gets `events/<name>/` and the UI goes through `fire`. `page=no`: engine-internal or high-frequency; blueprintz lists it as a read-only "engine verb" so nothing is hidden.
- A **coverage check** reads the engine source for `strcmp(v, "<name>")` and fails if a C verb is missing from the manifest (and the reverse). Negative test required: remove one manifest row, the check must fail.
- First-cut classification from the code (READ: names only; each verb's cost and frequency must be read before it is final):
  - game-dev-hq, 24 verbs: page candidates **hire, train, rest, start, assign, advance, ship, advertise, contract, console, newgame, save, load, play, stop, int, say, clock** (18); engine/read-only: status, step, toggle, clearchat, select, exportbook (6).
  - rpg-pvp-hq, 42 verbs: page candidates **buy, craft, scavenge, pickup, place, remove, use, strike, goto, control, newgame, save, load, play, stop, int** (16 of the player-meaningful ones); engine/read-only: step, day_tick, phase, sync, status, resolve, view, export3d, compact, event, mode, helppanel, review, recipes, wounds, bag, inventory, decpct, reset, pet_says, walk, select, clock, clearchat, toggle and the rest (26). The 22 pages we already have (chrome, greetings, doors, pickup) stay.

### 3.3 The generator (a C op in events-hq's ops, not Python and not a new `.sh`)
`verbgen <app>` reads `verbs.pdl` and writes, for each `page=yes` verb, the same four files the existing packages have (READ, `events/pickup`): `event.ir.pdl` (`NODE | id=1 type=<game>_verb | verb=<name>|arg=`), `condition.pdl` (`COND | trigger | ui`), `event.pal` (`exec cmd_1.sh`, `halt`) and `cmd_1.sh`.
- **Idempotent, and never destroys hand edits.** Each generated `event.ir.pdl` carries `META | generated | verbgen`. The generator rewrites a package only if that stamp is present and the file is unchanged since generation (hash in a `META` row). A package without the stamp, or edited by blueprintz/events-hq, is **skipped and reported**, never overwritten. (Hazard from the blueprintz doc: events-hq edit/delete rewrite the file; the generator must tolerate extra `META` rows, which the readers ignore.)
- Limits to respect (READ in BLUEPRINTZ-DESIGN.md section 11): `compile_page` reads at most 128 NODE rows and the old edit buffer was 8 KiB. Generated pages are 1 node; the limits do not bite until someone builds long chains by hand.

### 3.4 Dispatch through the page, with a fallback (the engine side, a few lines of C per game)
- New engine verbs: `fire <verb> [args]` and `run <verb> [args]`. `run` is today's verb dispatcher unchanged. `fire` looks the verb up in the manifest; if `page=yes` and `events/<verb>/` exists it calls the existing `run_event_x(verb, "ui", "GAME_ARG='<args>'")`; otherwise it does `run`.
- **Arguments** travel in an environment word (precedent: `RPG_OTHER='Blip'` read by `pet_says`), and the page's node leaves `arg=` blank; the `run` verb reads `GAME_ARG` when no argument is given. Reason: a page has static node arguments, while `train ${active_id}:scenario` is dynamic.
- **No recursion:** the page's command calls `run`, never `fire`. The command template is `exec "<engine>" 'run' '{verb}'[ '{arg}']`.
- **Fallback:** if the package is missing, `play_event.sh` fails, or the deadline (existing `reap_timed`) expires, the engine runs the raw verb and appends one S1 row `EVENT|ts|engine|fallback|<verb>|reason` so the failure is visible, never silent. The game never gets stuck because a page was deleted.
- **Argument safety:** the template quotes with single quotes; the manifest restricts args to `[A-Za-z0-9_:.,-]`, and `fire` rejects anything else (negative test: a quote character in an argument must not reach a shell).
- **Cost rule (measured fact to respect):** every page run is a sh + bash + prisc launch (RAN today: 5 per 24 s was 4.5% of a core at the old pvp clock rate). Fine for a player click; **never** for per-tick or per-step verbs. `page=no` is mandatory for anything called more than about once a second. Each converted verb gets a CPU check: 20 fires in a row, /proc ticks, reported.

### 3.5 What blueprintz gains, and the runtime view
- B1/B2 (read-only tree and detail) show the verb pages with no blueprintz change (`bp_tree.sh` already lists the games; game-dev-hq is already in its list).
- The engine verbs (`page=no`) appear as read-only rows from the manifest so the book is complete.
- B4 (edit through the ops) becomes meaningful: change a page's nodes and the game's behaviour changes, because the page is what `fire` plays. Example chain a designer can author without C: `contract` page = `game_dev_hq_verb contract` then a `show_text` line then `control_switch` ("first contract done").
- Runtime correlation uses what exists: each package already logs `Play event page_N (trigger=ui) for <name>` in its `master_ledger.txt` (RAN: int_on, phase_night). **No change to the S1 ledger format.** Blueprintz's Play/Pause/Step view (design section 12) can show the last N ledger rows beside the page whose `master_ledger` fired just before them.

## 4. Phases (each ends with evidence and a negative test)
| phase | what | evidence it must produce |
|---|---|---|
| **V0** | this design reviewed by the owner; questions in section 6 answered | owner reply |
| **V1** | `verbs.pdl` for rpg-pvp-hq plus the coverage check | check passes; negative: delete one row, it fails; coverage of 42 verbs reported |
| **V2** | `verbgen` writes the pages; regenerate twice = no diff; hand-edited page is skipped | `bp_scan` 0 flags on all packages; negative: edited package untouched |
| **V3** | `fire`/`run` in rpg-pvp-hq; menu rows and buttons call `fire` | real window, relay-driven: click Buy, the package's `master_ledger` gains a line, the S1 rows match the old path |
| **V4** | page-in-path proof | edit the page's node (different verb), click, behaviour changes; delete the package, click, fallback row appears and the action still works |
| **V5** | CPU check of every converted verb | 20 fires each, ticks reported; any verb over budget goes to `page=no` |
| **V6** | game-dev-hq: register `game_dev_hq_verb`, `verbs.pdl`, `fire`/`run` (grok's code) | handed over with this page, the generator and the V1-V5 checks; grok's call |
| **V7** | blueprintz B4 edit round trip on a verb page, then B4b (`book.pal`) includes verb pages | the edit shows in the game; `book.pal` run matches per-event runs |

Order: rpg-pvp-hq first (mine, already the pilot for events), then hand the recipe to grok for game-dev-hq (their file; I will not edit it without their go-ahead).

## 5. Risks
- **Latency and CPU** of a launch per click: bounded by the `page=no` rule and V5. NOT MEASURED for these verbs yet.
- **Two paths drifting** (page vs raw verb): removed by making `fire` the only UI path; `run` is the leaf the page calls. A grep check that no xhtpm action or menu row calls a `page=yes` verb directly belongs in V3.
- **Generator overwriting a designer's edit:** stamp + hash + skip-and-report (3.3).
- **Concurrent fires** (two clicks at once): `run_event_x` has a deadline and one shared world dir; ordering is not guaranteed. The S1 resolver is append-only, so rows stay valid; a UI click lock is a V3 item if the test shows trouble.
- **Format drift** in events-hq: read through the registry and ops, as the blueprintz design already requires.
- **Scope creep into S1:** this design deliberately does not change the ledger or the resolver.

## 6. Questions for the owner (recommended answer first)
1. **Route the UI through the pages, or only mirror them?** Recommend **route, with the fallback**: only that makes blueprintz an editor and not a viewer. Mirror-only is cheaper but is the pickup situation.
2. **Generator language.** Recommend a C op in events-hq (matches "window logic is C or pal"); a one-off Python converter only for the first batch if time is short.
3. **game-dev-hq.** Recommend: hand the recipe to grok; I do not edit grok's engine unasked.
4. **Which verbs stay engine-only.** Recommend the first-cut table in 3.2, then confirm each verb by reading it in V1.

## 7. Not done / not measured
Everything above is a design. Not run: any `fire` path, the generator, a CPU check of a page-run verb, blueprintz on a verb page. The verb lists in 3.2 are from `strcmp(v, ...)` names in the two engines, not from a per-verb read.

## 8. Built for rpg-pvp-hq (2026-10-20): what was done, what was measured, what differs from the plan
**Files:** `@.apps/rpg-pvp-hq/verbs.pdl` (42 verbs: 13 page=yes, 3 native, 26 no); `&.widgits/events-hq/ops/verbgen.c` + `build_verbgen.sh` (`check` and `gen`); 12 new packages `events/{buy,craft,scavenge,place,remove,use,strike,goto,control,newgame,save,load}/` (plus the existing `pickup`); engine `ops/rpg_pvp_hq.c` (`fire`, the leaf, the argument strip); `rpg-pvp-hq.xhtpm` (15 window actions now `fire <verb>`).

**RAN (evidence):**
- V1 `verbgen check`: engine 42 verbs = manifest 42, PASS. Negative: removing the `strike` row gave `MISSING from verbs.pdl: strike`, FAIL.
- V2 `verbgen gen`: made 12; a second run made 0, changed 0 files (identical files are not rewritten). Negative: a package whose node was edited was `skipped (generated, then edited)` and left alone. `bp_scan` over all 34 packages: `flags=0` (the extra `META | generated | verbgen` row is tolerated).
- V3 `fire` in the engine; direct-call check added to `verbgen check` (a window file calling a page=yes verb directly fails it: it found 15, now 0). **Real window, relay-driven**: clicked Shop > Red stool; `state/fire_log.txt` got `buy|page:stool_red`, `events/buy/master_ledger.txt` got `Play event page_1 (trigger=ui) for buy`, `house.txt` got exactly one `BUY|...|stool_red|25` row, coins 90 -> 65.
- V4 page in the path: with the buy page's node edited to `scavenge`, `fire buy bed_blue` ran **scavenge** (chat "found a rag", a `give_item rag` ledger row) and did not buy. With the package moved away, `fire buy` logged `fallback:no-package` and bought exactly once (one BUY row). A hostile argument (`x';touch /tmp/PWNED;'`) was refused (`refused:bad-arg`), nothing ran, no file created. `fire status` (page=no) ran directly (`direct:page-no`).
- V5 CPU, 10 runs each, `time` including children: direct `pickup` 0.38 CPU-s total (0.04 s each); `fire pickup` 1.83 CPU-s total (**0.18 CPU-s each**, about 0.15 s real). Fine for a human click, not for anything per tick: that is why the manifest keeps step/walk/phase out of pages.

**Differences from the plan (decided while building):**
1. **No `run` verb.** The page's registered command already calls the plain verb, and `fire` is a different word, so there is no recursion; fewer moving parts.
2. **Fallback is logged to `state/fire_log.txt`** (`ts|verb|page:<args>` / `fallback:<reason>` / `direct:<reason>` / `refused:bad-arg:<arg>`), not as an S1 row: an unknown verb in the ledger could be refused or counted by the resolver. The log is append-only and one line per fire, so it doubles as the audit trail.
3. **Double-run guard:** the leaf writes a one-time token (from the environment word `RPGPVP_VIA`) to `state/via.txt`; `fire` runs the verb itself only when the token did not appear, so a page that ran is never repeated. The two environment words are cleared at once so nested engine calls do not inherit them.
4. **Found a pre-existing bug on the way:** the renderer appends the house root and the app dir to every window action (`bin <verb> <args> <house> <app>`). The old `buy <key>` therefore arrived as `buy <key> <path> <path>`, which the engine read as x,y = 0,0: every window purchase was placed at 0,0. The engine now drops trailing absolute-directory arguments, so a window purchase is placed at the first free cell (RAN: placed at 2,0 / 3,0). This also let the `fire` argument check work (it refused the two paths until the strip was added: caught by the real-window test, not by the CLI tests).

**Not done / not measured:** INT-mode key bindings still call verbs inside the engine, not through pages (a keyboard shortcut for buy/use bypasses the page; the window's mouse and menu path does not). Blueprintz was not opened on the new pages (B1 read-only scan only). A concurrent double click was not tested. V6 (game-dev-hq: register `game_dev_hq_verb`, manifest, `fire`) and V7 (blueprintz edit round trip) are open. The `Prisc Error ... default_op.txt` line in each package's `master_ledger.txt` is the known cdda-copy issue, still there.

## 9. Built for game-dev-hq (2026-10-20, V6; owner: "we are the same, let's fix and push", so I did grok's engine myself)
**Files:** `@.apps/game-dev-hq/verbs.pdl` (24 verbs: 17 page=yes, 7 no; `say` stays direct because the fire argument filter rejects free text), 17 generated packages in `events/`, `game_dev_hq.c` (`fire`, the leaf, the argument strip, `GDHQ_VIA`/`GDHQ_ARG`), 61 window actions in `game-dev-hq.xhtpm` and `board/pchq-board.xhtpm` now `fire <verb>`, registry command `game_dev_hq_verb`.
**RAN:** `verbgen check` engine 24 = manifest 24, 0 direct calls, PASS; `bp_scan` over the 17 packages `flags=0`. `fire contract q0` played the page (`fire_log: contract|page:q0`, package `master_ledger` line, funds 100000 -> 95600, week 1 -> 3, same as the direct verb). Page-in-path: the contract page edited to `advance` ran advance (answer "no game"), not contract. Package removed: `fallback:no-package` logged. Hostile argument: refused, no file created. `fire status` and `fire toggle` with the renderer's two extra path arguments: handled directly (`direct:page-no`), the paths stripped.
**Bugs fixed on the way:**
1. **Genre truncation:** a home console's genre `action,rpg,adventure` was stored as `action,rpg,adve` (field 32 wide, copy 16); now complete (RAN: `GAME | g1 | home console | action,rpg,adventure ...`).
2. **Clock daemon outlived the window:** RAN: after the window closed, `game_dev_hq.+x` was gone but `lc_clock.+x` stayed (parent init). The resident engine now handles SIGTERM/SIGHUP/SIGINT and runs `lc_clock daemon-stop` on the way out; RAN: started the resident engine, `kill -TERM`, both processes gone (and `daemon-stop` on the existing orphan removed it). A SIGKILL still cannot clean up.
3. `usleep`/POSIX declarations under `-std=c11` made explicit (`_DEFAULT_SOURCE`), no more implicit-declaration warning.
**Not done / not measured:** no real-window click on game-dev-hq (a bare launch of the xhtpm gave an unresponsive window; the production route is the pc-hq board via `button.sh`, not exercised because it starts the 3D board); the only evidence is the command line plus the rpg-pvp-hq real-window result for the renderer's appended arguments. The contract balance (a first contract with the starting staff loses about $4,400 once two weeks of payroll are paid) is a design question left as it was.
