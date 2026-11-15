# Plugs and harnz: the spec for owner review (2026-11-14, DRAFT, nothing here is approved)

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
| 2 Tools dropdown groups with on/off marks | **BUILT** (editor tab); the LyfeDesk cell mirrors the layout but shows only the global group | case `tools_menu` 45/0; a mutation (the toggle does not flip) failed it; `harness_menu` updated to 60/0 (heading rows counted). READ: the taskbar builder passes no book or page, so it lists no plugs; the page filter is `visible()` in `harness_menu_op.c` and works |
| 3 Generic plug header (`PARAM` types and bounds, `AFFECTS`, `USES`, `VERSION`, `REQUIRES`, `CONFLICTS`), multi-plug loader, collision and trust checks | NOT BUILT (typed `PARAM` rows and the plug name check exist; the rest does not) | the day-night file is the only plug |
| 4 Harnz kind `EVENT`; 4b `TRIGGER` and verdict rows | NOT BUILT | |
| 5 The maker | NOT BUILT | |
Known limits of step 2: a toggle is live for every plug (the hybrid view/outcome rule is designed, not built); plug rows appear only when a book with a `plugs/` folder is open (day-night lives in `@.apps/rpg-edit-hq`); the toggle writes `ENABLED` through the op and logs `TOGGLE | epoch | plug | from | to` in `<app>/state/plug_log.txt` (capped at 32 KB); `[x]` and `[ ]` are plain text because the dropdown draws labels only.
