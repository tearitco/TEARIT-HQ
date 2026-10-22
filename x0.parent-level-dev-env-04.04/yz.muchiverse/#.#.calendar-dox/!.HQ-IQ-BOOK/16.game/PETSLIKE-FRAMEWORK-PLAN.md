# Pets-like boards as one framework (plan, 2026-10-21, claude)

Owner 2026-10-21: "did we ever figure out how to get the 'best of' all pets boards as a framework instead of writing separate managers, and all things can be data?" (and earlier: "i dont understand why all these pet / pc-hq like projects dont use the same layout / manager and just use some kind of framework to share the differences").

## Answer: not settled. Nothing here is built beyond the first data layer.

What exists:

- **Eight copies of one program.** rpg-pet, df-hq, tcg-rpgce, dsr-rpg, mcl-hq, cdda-hq, civ-rpgce and gta-hq each have their own `ops/<name>.c`, copied from `rpg_pet.c` (1126 lines). Measured 2026-10-21 with `diff` against `rpg_pet.c`: lines that differ (added plus removed) are df-hq 581 of 1419, tcg-rpgce 414 of 1276, dsr-rpg 307 of 1201, mcl-hq 575 of 1433, cdda-hq 546 of 1406, civ-rpgce 412 of 1272, gta-hq 638 of 1518. So roughly half to three quarters of every clone is the same code. rpg-edit-hq (`rpg_edit.c`, 456 lines) is a different program (a map painter plus a walker) and shares none of it.
- **Data layer, done the same way in all eight:** `party.pdl`, `rooms.pdl`, `catalog.pdl`, `time.pdl`, `tuning.pdl`, `keybinds.pdl`, `voices.pdl`, `side_style.pdl`, and since today `toolbar.pdl` (LyfeDesk shape, one shared reader). rpg-edit-hq has `party.pdl` too.
- **Layout tooling** (`@.apps/layout-studio`): `layout_check` (validator for .xhtpm), five starter templates, `layout_flow` (a game's layout, nav table, verb script, keybinds and views as one `flow.pdl`; first user pet-trainer), `pet_gen`. Strategy doc `08-roadmap/design-docs/LAYOUT-STUDIO-FOR-SIMPLE-AGENTS-STRATEGY-2026-10-09.md`.
- **House rule for sharing code** (GAMES-DECISIONS-2026-10-13.md): inline for one user; text-include for pure code with two or more users; op + fork + IPC for stateful code with two or more users; never a header plus a link step.
- **rpg-pet's own warning** (TWO-CODEBASES.md): when one window already does something, copy how it does it instead of inventing a second mechanism. Eight copies is the opposite of that: a fix in one is not in the others.

## What each board contributes (the "best of")

Known from the code and docs, not all checked line by line:

| Board | Brings |
|---|---|
| rpg-pet | the reference: INT (turn-based vs real time), clock events, chat and greetings, voices, relations, doors as events, bag hotbar, save / load slots |
| df-hq, cdda-hq, mcl-hq, gta-hq, civ-rpgce, tcg-rpgce, dsr-rpg | the same core plus a game rule set on top (designations and jobs, recipes and wounds, blocks and crafting, vehicles, cards, a city); the differing 300 to 640 lines per file are where those live |
| rpg-edit-hq | the island map painter: floor, wall and negative-z depth per cell, holes by depth, rock for hung layers, taper, a far island, a map-book with pages, a roster file |
| piececraft-hq / board-viewer | the engine: 2D and 3D board frames, xelector, camera, pages and books (`pchq_board_action.sh`), hotbar |
| LyfeDesk taskbar | menus as data, pre / post staging, the `_cmd` grammar |

## Proposed shape

1. **core + rules + data.** Move the identical code out of the eight copies into text-included shared files under `&.widgits/_shared-lib/` (prefix `pet_`): pdl and file helpers, the party / rooms / catalog loaders, the append-only ledger replay, the `ui.txt` writer, chat, the toolbar reader (`ptb_`, exists). Each game keeps one small file of its own: its verb table and AI step (the 300 to 640 lines that differ). The build script of a game includes the core plus its rules file.
2. **Prove it without a window.** Before and after each extraction, run the same verb script against every game and diff the `state/ui.txt` it writes (the layout-studio `layout_flow` already drives a game through its verbs). No extraction lands if a ui.txt changes.
3. **Verbs as data where they are simple.** A `verbs.pdl` row set (`VERB | name | effect`) for the ones that only change state (toggle chat, goto room, new room, new pal); C stays for the rules that need code. This is the concept-bank direction in GAMES-DECISIONS-2026-10-13.md; it needs that owner's say on bank layout.
4. **One template per window kind, generated from data.** The tab bar (done through toolbar.pdl), the left pet bar, the right chat bar and the footer are the same in all eight: one shared template body with the game's `toolbar.pdl`, `party.pdl` and `flow.pdl` filling it. Today there are sixteen .xhtpm files (board plus standalone) that I had to patch by script for every menu change.
5. **rpg-edit-hq joins as a front end, not as a ninth copy.** Its map painter is the one thing no other game has; its pets and chat should come from the same core (the roster is already a `party.pdl`).

## Order and risk

- Step 1 for the loaders and the `ui.txt` writer first (smallest, most shared), with step 2's diff as the gate. Then chat and clock. Rules files last.
- Risk: the eight copies have drifted (each has its own fixes); the diff gate will show every drift, and each one needs a decision (which fix wins). That decision list is the real work.
- Not decided: whether the core stays text-included per game (eight binaries, one source) or becomes one resident op that loads a game folder (`petslike.+x <game>`); the second is the "no separate managers" end state but needs the rules as loadable data or modules, which the house has no mechanism for yet except the concept bank.
- Nothing in this plan is started except the toolbar reader and the data files above.
