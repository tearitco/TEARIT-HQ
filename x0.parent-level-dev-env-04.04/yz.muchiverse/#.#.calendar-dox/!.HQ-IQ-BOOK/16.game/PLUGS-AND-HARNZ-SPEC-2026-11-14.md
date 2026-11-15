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
