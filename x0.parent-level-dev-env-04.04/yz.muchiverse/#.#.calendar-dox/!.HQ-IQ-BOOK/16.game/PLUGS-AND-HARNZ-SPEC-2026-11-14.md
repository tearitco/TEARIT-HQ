# Plugs, harnz and tests: the spec and its build record (2026-11-14; the owner said "ok, do it" to the build order and decided D74 to D76; sections marked DESIGN are still not built)

Status words: **BUILT** (code exists and a case passed, RAN 2026-11-14), **DESIGN** (my proposal, not built), **OPEN** (owner decides). Labels: SAID (owner told an agent), READ (I opened the file), DOC (my reasoning). Decision-log entries: D74 (names, rule, maker), D75 (the first plug, day/night).

## 1. Why this exists
The editor has one "harness" list today. It mixes proofs for agents (`CASE`) with one-click page tools (`ACTION`), and RPG Maker's plugin idea (optional, parameterised, shareable behaviour) has no home. The owner wants: RPG Maker-like plugs; page tools that run from a menu; both findable in one place; both creatable with the common tools (blueprintz, events-hq); events reused, not copied.

## 2. Vocabulary (SAID, with the judge)
| Word | What it is | RPG Maker analogy |
|---|---|---|
| **Event** | A built-in engine command or page (transfer/teleport, doors, message, tint screen, switches). Baked in, not customizable. | Events, common events, event commands |
| **Plug** | An optional, project-wide behaviour with parameters and an on/off switch; it may provide commands that events call. Shareable text file. | Plugins (Plugin Manager) |
| **Harnz** | A one-shot tool for the open page, run by a click, that changes the page and leaves a trace in its ledger. | (none; closest: a page event you trigger by hand) |
| **Test (CASE)** | A harness case for agents: pal + case + C ops, proves something. Keeps the name "harness". | (none) |

**The judge (SAID):** if RPG Maker has an event for it, or it is a very reasonable "should be" in any engine (for example the basic network or blockchain API), it is an event. Otherwise it is a plug (or, if it is one click on one page, a harnz). Teleport and doors are events. Day/night is a plug (a game need not have it; RPG Maker ships no cycle). The world clock is engine.

## 3. UX (DESIGN, with the owner's constraint: no new top-bar tab)
- One tab, **Tools ▾** (BUILT: the label changed in the editor board and in LyfeDesk's top taskbar cell 11; internal ids stay `harness`). Both read one list from `harness_menu_op`, so they mirror each other by construction.
- Dropdown groups (DESIGN):
  - `this page`: harnz rows for the open book and page (run now).
  - `plugs`: one row per plug with an on/off mark, then `plug manager...` (parameters, order).
  - `tests`: the CASE rows, collapsed, for agents.
  - `+ new...`: opens the maker (section 6).
- Every dropdown keeps its `- cancel -` row (BUILT by Grok).

## 4. Data (BUILT unless marked)
**Harnz rows** (`#.desktop/harness_menu.pdl` global, `@.apps/<book>/harness.pdl` per book): `HARNESS | id | page | kind | label | target | args | writes | confirm | note`; `page` is `*` or a page id (the book file; READ in the op's header; I did not read the filter code). Kinds today: `CASE`, `ACTION`. **DESIGN: a third kind `EVENT | id | event or verb | args`**: fires an event or verb through the same `fire` path a click uses, so a person, a driver, an AI and a schedule run the one thing.

**Plug file** `@.apps/<app>/plugs/<name>.plug.pdl` (BUILT for the first plug): `PLUG | name | ...`, `ENABLED | 1/0`, then the plug's own rows. Day/night uses `PHASE | name | from_hour | outdoor r,g,b | indoor r,g,b | sky`. **DESIGN, generic header rows:** `PARAM | name | default | type | help` and `COMMAND | name | event or verb | args`, so any plug is described the same way and the Tools menu and the maker can list it. Rule: the plug file is config (an input, never written by the game); anything a plug changes in a game goes through a ledger verb (the no-ghost-data rule).

## 5. Worked example: day/night (BUILT, RAN: case `day_night_plug` 35/0)
One visible file holds the switch and the cycle; `pet_sky.c` reads it with the old hard-coded values as defaults (parity with the legacy rule for all 24 hours is a check; a moved hour and a changed tint break it); the shared core installs one clock reminder per phase at its hour, so the hour lives only in the plug; `ENABLED | 0` = always day, no tint, no phase reminders; a book without the file is unchanged. The engine events (`tint_screen`, `pet_says`, `lamps`, `phase_*` pages) were not changed. Guide: `16.game/PLUG-DAY-NIGHT-THE-FIRST-PLUG-2026-11-14.md`.

## 6. The maker (DESIGN, owner request)
- Lives in the existing editors (blueprintz and events-hq), reached from `+ new...` in Tools. No new tab.
- Form: a **kind switch (Plug / Harnz / Both)**, name, description, scope (project or this page).
- Parameters (`PARAM` rows) and commands (`COMMAND` rows). **Pick-lists, as blueprintz does (READ: its "Load verbs of a game" menu, "Load common events", event list, page-switch field, "who calls whom" graph, problem scan):** the game's `verbs.pdl` (every verb with argument help), common events, the event folders under `@.apps/<app>/events/`, switches, and other plugs' parameters.
- **Reuse by reference, never a copy** (one authority per fact). Test, driver, AI and automation harnesses are harnz rows that name events.
- `Plug` writes the plug file; `Harnz` writes one `HARNESS` row; `Both` writes the plug plus one harnz button per command.
- One shared op writes the files (the `game_new_op` pattern: data only, no per-plug C), validated by `harness_menu_op check` plus a new plug check.
- **Limit:** a maker-made plug composes parameters, scheduled or triggered events and commands. A new engine rule still needs C.
- **Prerequisite:** event pages must read plug parameters (for example `${day-night.dusk_hour}`). Today the four `phase_*` pages hold their tint numbers, greeting and lamps toggle themselves.

## 7. Plug candidates (RPG Maker's own stock plugins as the guide; READ the example game's `js/plugins`)
popup position (`TitleCommandPosition`, parameters only), collection book (`ItemBook` and `EnemyBook`: commands plus ledger-backed entries), per-item override by note tag (`WeaponSkill`), alternate menu layout (`AltMenuScreen`), screen options (`Community_Basic`). Not plugs: teleport, doors (events); movement rules such as step height (engine, a reasonable "should be").

## 8. Build order (DESIGN, owner decides)
1. Event pages read plug parameters; convert the four `phase_*` pages (finishes day/night).
2. The Tools dropdown groups with on/off marks (reads one list; both bars update).
3. The generic plug header (`PARAM`, `COMMAND`) and a multi-plug loader; then the second plug (popup position).
4. The harnz kind `EVENT`.
5. The maker.
Each step: a harness case with a negative control first, then a person looks at the editor (nothing in this spec has been looked at live yet).

## 9. Open for the owner
1. Is a plug on or off for the whole project, or can a page override it? (My default: project-wide first.)
2. For `Both`, do plug commands appear as buttons on every page or on the pages you pick?
3. Should a harnz script be able to call a plug's command (like a common event), or only the Tools menu?
4. Does a plug load once at project start with parameters (RPG Maker style) or run on click?
5. Should CASE tests stay in an agent-facing list only?
6. Order of section 8: maker before the dropdown?

## 10. External references
Repo (`x0.parent-level-dev-env-04.04/yz.muchiverse/`): BOOK `02-architecture/DESIGN-CHOICE-TEST-AND-DECISION-LOG.md` (D74, D75), `16.game/PLUG-DAY-NIGHT-THE-FIRST-PLUG-2026-11-14.md`, `16.game/GROK-WORK-INVENTORY-AND-HARNESS-PLUG-NAMING-2026-11-14.md`; code `44.xyz.01.00/&.widgits/_shared-lib/ops/harness_menu_op.c`, `&.widgits/_shared-lib/pet_sky.c`, `&.widgits/_shared-lib/petslike_core.c`, `@.apps/rpg-edit-hq/plugs/day-night.plug.pdl`, `&.hq-apps/blueprintz`, `&.widgits/events-hq`; commits `f5c5f444e` (day/night plug), `d3901a82c` and `af9f7ce38` (Tools label). Outside: the RPG Maker MV example game's `js/plugins` (ItemBook, EnemyBook, TitleCommandPosition, WeaponSkill, AltMenuScreen, Community_Basic) on the owner's machine.

## 11. Outside review of this spec (2026-11-14; a reviewer who read only the spec, pasted by the owner) and what I did with it
The reviewer approved the split, the judge, the default-parity check, config-versus-ledger, and the order dropdown before maker. Their ten gaps, each checked against the repo where I could. Verdicts are mine (DOC); the owner decides.

| # | Reviewer's point | Verdict | Evidence or reason |
|---|---|---|---|
| 1 | A plug setting can change outcomes, so an old replay can diverge; record each plug and a parameter hash in the first ledger row and refuse a replay mismatch | **ADOPT** for any plug whose parameters affect ledger outcomes | READ: Deskopoly already does this for rules (`deskopoly.c`: an FNV-1a rules hash recorded in `lobby` and `start`, re-checked by every replay, spec 5.2 F7). Reuse that pattern. Day/night is mostly a projection (tint, sky), but phase also reaches pet chat rules, so it must declare whether it is outcome-affecting |
| 2 | Conflicts: duplicate command names, duplicate parameters, two plugs installing reminders at one hour; header rows `VERSION`, `REQUIRES`, `CONFLICTS` | **ADOPT** (DESIGN) | RPG Maker's known plugin-order bug. One plug exists today, so no collision is possible yet; the check belongs with the multi-plug loader (section 8, step 3) |
| 3 | A shared text file that composes events can run anything events can: each plug declares the verbs it uses, undeclared calls are refused, an outside plug needs the owner's `/apply` | **ADOPT** | READ: `hai_op.c` already has propose, then the owner types `/apply` (old file kept as `.hai.bak`). Reuse for installing a plug from outside. `COMMAND` and `USES` rows make the declared set checkable by the same validator |
| 4 | A plug command that calls an event page launches processes each time: refuse per-tick triggers for page-based plugs | **ADOPT** | READ: the code already says a page run is a sh+bash+prisc launch, never per tick (`verbs.pdl`), and `clock_feature_off` records a measured 1 CPU-second per firing. The plug check must refuse a page-based command on a high-frequency trigger |
| 5 | Automation is missing: triggers (a commit, a nightly run, a ledger row) with a required verdict row for tests and automations | **ADOPT as a new open item** | The spec only covered click-run tools. The clock `AT` rows are already a trigger mechanism for plugs. Add a `TRIGGER` field and a required verdict row for `CASE` and automation rows. Not designed yet; the owner has not asked for commit or nightly triggers |
| 6 | `PARAM` needs types and bounds (hour 25 refused); the maker picks from the same lists | **ADOPT** | BUILT in part: the day-night reader already refuses an hour outside 0..23 and a colour outside 0..512 and ignores that row (a harness case covers bad rows). Generalise to `PARAM | name | default | type | min | max | help` |
| 7 | Generate a plug's help from `PARAM` and `COMMAND`; flag any event that calls a removed plug's command in blueprintz's problem scan | **ADOPT** | blueprintz already has a problem scan and a call graph (READ) |
| 8 | "harness", "harnz", "harnx" sound alike; show kinds as test, page tool, plug in the UI | **PARTLY**: agree the UI should say the kind in plain words | The owner chose the names plug and harnz. I said earlier that harnz is close to harness; the plain-word kind label in the menu groups (`this page`, `plugs`, `tests`) already does what the reviewer asks |
| 9 | Tiebreak: when unsure, make it a plug (promoting a plug to an event later is easy, the reverse breaks things) | **ADOPT as the stated tiebreak**, owner to confirm | Matches the owner's judge for the clear cases |
| 10 | A plug-making AI is safe only if the maker's shared op is its target, with a whitelisted shape and `/apply` | **ADOPT** | Same op, same `/apply` flow as #3. The existing h-ai proposal path is the model |

**The reviewer's answers to the section 9 questions** (theirs, not the owner's): 1 project-wide first, page overrides later; 2 opt-in pages; 3 yes, through the `EVENT` kind; 4 load once at start, toggling takes effect on restart and the change is logged; 5 tests in a collapsed list visible to the owner; 6 no, the dropdown before the maker. These match my defaults except **4**: the day-night plug already re-reads its file when its size or mtime changes and reinstalls the schedule by itself (RAN, `day_night_plug`), so a toggle takes effect live, not on restart. Whether live toggling is right for an outcome-affecting plug (point 1) is a real question for the owner: it argues for "restart or a logged row".

**New open questions from the review:**
7. Triggers (commit, nightly, ledger row) for harnz and tests: wanted now, or later?
8. Live toggle or restart-only for a plug that changes game outcomes?
9. Who may install a plug from outside: only through `/apply` by the owner?
10. Tiebreak "when unsure, make it a plug": confirm?

## 12. Owner answers to the review questions (2026-11-14, SAID) and how they become design
| Q | Owner said | Design (DOC, mine; the owner has not reviewed this wording) |
|---|---|---|
| 7 Triggers | "triggers is fine" | In scope. A harnz or a test row may carry a `TRIGGER` (click, a clock reminder, a ledger row, a commit, a nightly run). Every automated run writes a verdict row; the existing clock `AT` rows are the schedule mechanism. Built after the Tools dropdown (section 8 gets step 4b) |
| 8 Live or restart | "hybrid?" | Yes. A plug declares `AFFECTS | view` or `AFFECTS | outcome`. A **view** plug (day/night's tint and sky) toggles live, as it does today. An **outcome** plug changes only at a safe boundary (the next session or game) through a logged ledger row, and its parameter hash goes into the first ledger row so a replay refuses a mismatch (the Deskopoly pattern). Day/night must declare which it is: its tint and sky are view; its phase reaches pet chat rules, so it is `outcome` until that link is cut. Not built |
| 9 Outside plugs | "safety but also dev abilities" | Two tiers. **Local dev plugs** (made in this house, by the owner or an agent, through the maker or by hand) are validated by the plug check and must declare the verbs they use (`USES` rows), but need no `/apply`. **Outside plugs** (imported, shared, pasted) are always proposed and the owner types `/apply` (old file kept as `.hai.bak`). A plug that tries an undeclared verb is refused in both tiers. Not built |
| 10 Tiebreak | "u judge" | **Adopted: when unsure, make it a plug** (promoting a plug to an event later is easy; the reverse breaks games). One exception, stated so it is not abused: anything that guards ledger integrity (appending rows, replay checks, the rules hash itself) is engine, never a plug |

Resulting build order (section 8, revised): 1 event pages read plug parameters (finishes day/night); 2 Tools dropdown groups; 3 generic plug header (`PARAM` with types, `AFFECTS`, `USES`, `VERSION`, `REQUIRES`, `CONFLICTS`) and the multi-plug loader with the collision and trust checks; 4 harnz kind `EVENT`; 4b `TRIGGER` and verdict rows; 5 the maker.

## 13. Build status (2026-11-14, after the owner said "do it")
| Step | Status | Evidence (RAN) |
|---|---|---|
| 1 Event pages read plug parameters; the four `phase_*` pages converted | **BUILT** | case `plug_ref` 60/0; a mutation (the OFF check removed) failed it; the real `phase_dusk` page follows an edit of the plug, in a scratch house. Commit `573f56a85` |
| 2 Tools dropdown groups with on/off marks | **BUILT** (editor tab); the LyfeDesk cell now lists for the open book and page too (wiring proved by source checks, not clicked) | case `tools_menu` 45/0; a mutation (the toggle does not flip) failed it; `harness_menu` updated to 60/0 (heading rows counted). READ: the taskbar builder passes no book or page, so it lists no plugs; the page filter is `visible()` in `harness_menu_op.c` and works |
| 3 Generic plug header (`PARAM` types and bounds, `AFFECTS`, `USES`, `VERSION`, `REQUIRES`, `CONFLICTS`), multi-plug loader, collision and trust checks | PARTLY BUILT 2026-11-14 (section 16): the header and typed rows are VALIDATED (`harness_menu_op check`, case `plug_header` 26/0) and shown in the plugs window; NOT BUILT: the multi-plug loader, undeclared-verb refusal (needs the engine), the live/outcome boundary | the day-night file is the only plug |
| 4 Harnz kind `EVENT`; 4b `TRIGGER` and verdict rows | 4b BUILT 2026-11-14 (section 17, case `plug_triggers` 41/0); the `EVENT` kind and timer sources NOT built | |
| 5 The maker | CORE BUILT 2026-11-14 (section 17, case `plug_maker` 29/0): the data-writing verb and the `+ new plug` row; the form with pick-lists NOT built | |
Update 2026-11-14 (SUPERSEDED by section 14): the LyfeDesk Tools cell first mirrored the editor tab for the open book and page. Its book and page are the ones the BAR shows (book:<session> page:<desk>), never the editor board state (a first version read that: wrong source). The dropdown is now a launcher, see section 14.
Known limits of step 2: a toggle is live for every plug (the hybrid view/outcome rule is designed, not built); plug rows appear only when a book with a `plugs/` folder is open (day-night lives in `@.apps/rpg-edit-hq`); the toggle writes `ENABLED` through the op and logs `TOGGLE | epoch | plug | from | to` in `<app>/state/plug_log.txt` (capped at 32 KB); `[x]` and `[ ]` are plain text because the dropdown draws labels only.

## 14. Three kinds, three windows, a launcher dropdown (2026-11-14, owner decision D76; BUILT, seen live)
Owner: "tests and harnz are the same to me, but to you they are not, so give it its own distinction... so 3 windows". The three kinds and what each screen offers:

| kind | rows | control | where the data is |
|---|---|---|---|
| plugs | `@.apps/<book>/plugs/*.plug.pdl` | on/off switch (flips `ENABLED`, logged in `state/plug_log.txt`) | the plug file |
| harnz | the book's ACTION rows (`@.apps/<book>/harness.pdl`) | click (runs the op; a row with `confirm = yes` asks first) | `harness.pdl` |
| tests | CASE rows (global `#.desktop/harness_menu.pdl` and the book's) | run (behind `cpu_gate`, one at a time), the last verdict is in the row | the harness cases |

- **Dropdown** (the LyfeDesk top bar, Tools cell): `plugs`, `harnz`, `tests`, then the bar's own `notes` and `cancel`. Each opens that kind's window. The editor tab still lists everything (its `[x]` toggles work); making it the same launcher is open.
- **Window** = `&.hq-apps/plugz-hq/`: ONE static layout (`plugz-hq.xhtpm`) for all three kinds, served through the renderer's generic instance-dir hook (`khtpm_core_render <house> <xhtpm> <instance dir>`, one instance dir per kind under `instances/`). A C projector (`ops/plugz_projector.c`, the `<module>`) writes `<instance>/.hq_manager/ui.txt` only when it changes; a C action op (`ops/plugz_act.c`) is the only write path (`select`, `control`, `confirm`, and a detached registered worker so a test run never blocks the window). The rows, details, listing rules and the pick stay in `harness_menu_op` (verbs `rows`, `describe`, and `pick open:<kind>`): one authority.
- **Layout**: left = the list (a plug shows `[x]`/`[ ]`, a test its last verdict); right = the selected row's title, status, the one control button, the result line, the LINKS block and a scrolling details list.
- **LINKS** is read-only: "fires: (none) fired by: (none)". Triggers (a plug fires a harnz and the reverse) are step 4b: `TRIGGER` rows, a loop guard (a chain that returns to its start is refused), a depth cap, one ledger row per fire. Nothing is faked in the window.
- **Evidence**: case `plugz_hq` 47/0 (real ops in a scratch house); live 2026-11-14: the plugs window for the book `rpg-edit-hq` was opened, the real `turn OFF` button was clicked through the window's relay (the plug file went to `ENABLED | 0`, a `TOGGLE` row was logged, the window redrew `[ ]`, `OFF`, `turn ON` and the result line), then switched back (file byte-identical, sha256 prefix `4bd49e19e8f5180a`); the harnz and tests windows were opened for `centroid:live` and photographed. NOT done: a test `run` or a confirm click on the live window; the bar's dropdown opening a window by a real click (the running taskbar manager predates the launcher, it needs a restart).
- **Known gaps**: the three windows open at the same screen position (stacked: FIXED 2026-11-15, they now open side by side through the shared window-placement op, see PLUS-PHONE-DESIGN section 21); the details list scrolls inside a fixed panel; `+ new` (the maker, step 5) is not in these windows yet; the generic plug header and loader (step 3) and the `EVENT` kind (step 4) are not built.

## 15. Local examples for the office page, a `dir` row, and `GLOBAL | no` (2026-11-14, owner requests; BUILT, seen live)
- **Local examples for `pre-design:office`** (`@.apps/pre-design/`, the first book with all three kinds, meant to be copied): the **plug** `plugs/office-example.plug.pdl` (ENABLED, `stamp_limit` int 5..200, `stamp_label` word); the **harnz** `stamp this page` (page `office`; `ops/office_stamp.c`, built with `ops/build_office_stamp.sh`) appends a line to `state/office_stamps.txt`, and reads its limit and label through plug references, so it only runs while the plug is ON (OFF = refused with the reason, nothing written); the **test** `check office tools` (case `office_tools`, 12/0). The three kinds visibly work together: the plug's parameters change what the harnz does.
- **`dir`** (owner: "so a human can easily open the tools dir for that book, in edit also"): a `-- folder --` group with a `dir` row in the shared list (the editor Tools tab and the full list) and a 4th row in the bar launcher (`plugs`, `harnz`, `tests`, `dir`, then notes and cancel). `pick open:dir` opens `@.apps/<book>/` with `xdg-open` (the HQ menu's own `dir` row does the same); a book with no folder prints `DIR-MISSING <book>` and the folder is never created by the pick. `HARNESS_MENU_OPENER` replaces the opener (a test hook so a case opens no file manager). NOT clicked live (it would open a real file manager window on the owner's screen).
- **`GLOBAL | no`** (owner: "book shows enter house?"): the global rows of `#.desktop/harness_menu.pdl` (the centroid generators, `enter house`, `crew`) are shown on every book that has tools of its own. A book may now say `GLOBAL | no` in its own `harness.pdl` to hide them (the office book does); without that row nothing changes for other books. `check` accepts the row.
- **Seen live**: the bar (after the restart) listed the 4 launcher rows; the plugs window opened by a real pick of `plugs`; in the harnz window a real click wrote `state/office_stamps.txt` and the window showed `stamp (exit 0)`; in the tests window a real click ran `office_check` (result `VERDICT|PASS|passed=12`, row `[12 ok]`) and, after `GLOBAL | no`, the list shows only `check office tools`.

## 16. Step 3, the safe half: the plug header is validated and shown (2026-11-14; BUILT, seen live)
Chosen for being data-only (no engine behaviour changes): `harness_menu_op check <house> <book>` validates every plug file of a book, and the plugs window shows the header and the problems.
- **Header rows** (all optional except `PLUG | name | <file name>` and `ENABLED | 0 or 1`): `PLUG | version | <1..9999>`, `AFFECTS | view or outcome` (at most one), `REQUIRES | <plug>`, `CONFLICTS | <plug>`, `USES | <verb>`. A value takes the whole rest of the row: a trailing `# comment` on the same row makes the value wrong (put comments on their own line; the check caught this in my own edit).
- **Typed rows**: `PARAM | key | value | type | min | max | help` are checked with the SAME function the engine uses at run time (`plug_ref_value_ok` in `plug_ref.c`, now one authority): the type (int, tone, word, text), the bounds, a min above its max, a key that is not a plain word, a key declared twice, a row with fewer than 4 fields.
- **Cross-plug rules inside one book**: a `REQUIRES` must name an installed plug and that plug must be ON while this one is ON; a `CONFLICTS` must not be ON while this one is ON; a plug cannot require or conflict with itself. `USES` is checked as a plain name only: refusing a verb a plug did not declare needs the engine (step 4, not built).
- **Window**: the plug detail lists `version N   affects view`, `requires`, `conflicts with`, `uses`, then up to four `PROBLEM:` lines first, so a long parameter list never cuts them off. `day-night` and `office-example` now declare `AFFECTS | view`.
- **Three bugs found and fixed while building it** (each has a test that fails without the fix): (1) `plug_ref.c` split `PARAM` rows with `strtok`, which collapses empty fields, so a blank min with a max (`| int |  | 200 |`) made 200 the MINIMUM in the engine; (2) a plug with a ONE-letter name was invisible to the list and the check (`plug_names` needed a file name longer than 10 characters); (3) the window clamped a stale selection to row 0 for display but the action op did not, so after the list shrank the button said "run this test" and then "nothing is selected". Also: the worker's result line no longer shows the absolute case path.
- **Evidence**: `plug_header` 26/0 (a passing control for every rule; the shipped plugs pass the stricter check), `plug_ref` 66/0 (6 new, run against the ORIGINAL `plug_ref.c` they fail), `plugz_hq` 64/0 (the stale-selection case fails against the previous action op), `tools_menu` 82/0, `harness_menu` 60/0, `taskbar_harness_menu` 30/0, `office_tools` 12/0, `day_night_plug` 35/0, `spawn_lint` 19/0, `py_lint` 11/0; the editor and the three games still compile with the changed `plug_ref.c`; live: one click ran the office test with the stale selection and showed `VERDICT|PASS|passed=12|failed=0`. NOT done: rebuilding the running engine binaries from the changed `plug_ref.c` (they behave the same for every valid file; the blank-min fix reaches them at their next build).

## 17. Loader, triggers, maker, editor launchers (BUILT 2026-11-14, owner: "triggers, plug loader, maker, editor do all that"; D78 to D81)
All four live in `harness_menu_op.c` (one authority) and are covered by scratch cases that use the REAL op. Honest scope is stated per item.
- **Plug loader (D78, step 3 second half).** `harness_menu_op load <house> <book>` orders the ON plugs (a plug after every ON plug it `REQUIRES`; a cycle is reported), reports a COLLISION when two ON plugs declare the same `COMMAND` name or install a clock reminder (`PHASE`) at the same hour, and writes the projection `<app>/state/plugs_active.txt` (ORDER and PROBLEM rows; regenerates byte for byte; nothing reads it as authority). Exit 2 when there is a problem. A click or trigger that would switch a plug ON is REFUSED when the loader's result for the would-be state names that plug (nothing changes, no log row); switching OFF is always allowed. Case `plug_loader` 28/0. NOT built: undeclared-verb refusal (needs the engine to report which verb a plug calls), and the live/outcome hybrid boundary.
- **Triggers (D79, step 4b).** `@.apps/<app>/triggers.pdl`: `TRIGGER | id | on | run | note`. `on` = `toggle:<plug>:on|off`, `done:<id>`, `failed:<id>`; `run` = `harnz:<id>`, `test:<id>`, `plug:<plug>:on|off`. A trigger runs through the SAME `run_row` code a click uses. Guards, all tested with controls: a target already in the chain is refused (loop, also found statically by `check`), depth cap 4, 12 fires per run, a row that asks for confirmation is never confirmed by a trigger, a plug switched on by a trigger meets the loader refusal, `done:` does not fire after a failure. Every fire or refusal is one row of `<app>/state/trigger_log.txt` (capped at 32 KB, older half dropped). The plugs/harnz windows' Links panel now shows `fires` and `fired by` from this file. Case `plug_triggers` 41/0. Only click-like sources exist (a plug switch, a finished row): there is NO timer, commit or nightly source yet, so the per-tick refusal rule (review item 4) has nothing to guard and is not built. A bug found on the way: `load_all` appended rows each call, so a nested call inside a run made every id "declared twice"; it now resets.
- **Maker (D80, step 5).** `harness_menu_op make <house> <book> plug <name>` writes a starter plug (version 1, OFF, `AFFECTS | view`, one typed example PARAM). `make ... harnz <name> <target> [args]` appends one `HARNESS` row. Refusals (each tested): existing name (nothing overwritten, byte-identical), name not a plain word, no book folder (no folder created), target not an executable under `ops/+x`, `..` or `|` in args. The book is checked after writing; a made file that adds a problem is undone (this undo path is NOT covered by a test: no valid input reaches it). In the Tools list a click on `+ new plug` makes `new-plug-N` in the open book; `+ new harnz` says what it needs (a target) and changes nothing. Case `plug_maker` 29/0. NOT built: a form with pick-lists (verbs, events, switches) as sketched in section 6; the maker is the data-writing core those forms would call.
- **Editor launchers (D81).** The editor's Tools list (`list`) now starts with a `-- windows --` group (`plugs`, `harnz`, `tests`) for a named book, above the unchanged rows, so the editor opens the same three windows as the bar without losing its one-click and confirm rows. `pal_side` rebuilds the tab after `new:`. Cases `harness_menu` 60/0 and `tools_menu` 86/0 (row counts and line numbers updated). The running editor keeps its old binary until relaunched: NOT relaunched, so not seen live.
