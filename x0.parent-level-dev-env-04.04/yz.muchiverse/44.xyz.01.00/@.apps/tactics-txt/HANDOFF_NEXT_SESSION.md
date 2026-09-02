# Handoff: tactics-txt — P1 Skeleton Built (2026-08-02)

**From:** Sonnet 5
**To:** Next agent picking up tactics-txt — assume ZERO context, read this whole doc first.

## ⚠️ NEWEST UPDATE (same day, later still): real per-unit entities + a real selection/possession mechanism now exist — read this first

Real progress beyond the board-widget section below: `pieces/battle_01/units/<unit_id>/` are now REAL directories (not the flat `units.txt` roster), each with a real `state.txt` (position, HP, owner) AND a real `piece.pdl` of its own (`Move`, `Attack` - stub effects, see below) - the first entity in this house's history to have its own methods at all (see `@.apps/PORTABLE_ENTITY_ARCHITECTURE.md`, written this same session). A new `roster.chtpm` screen lets you select one of the ACTIVE SIDE's own units and see/dispatch that unit's own real methods, merged in dynamically. Full build log, the real bugs found/fixed, and the exact test methodology: `ENTITY_MOVEMENT_PROGRESS.txt` (same directory) - **read that before touching any of this**, it also documents a real architectural constraint (`active_target_id` is layout-coupled here, not a free possession pointer) that explains why selection works the way it does rather than a more "obvious" approach that would have silently broken.

**Still stub, explicitly deferred, not a bug**: `MOVE_UNIT`/`ATTACK_UNIT` just set a "not yet implemented" message - real path-costing and combat resolution are separate, later work. No terrain-awareness in unit placement yet either (a unit could start on a `#` wall tile).

**Compile/build-verified AND functionally verified** (via direct standalone op invocation against a disposable scratch dir, not a live GL session - see `ENTITY_MOVEMENT_PROGRESS.txt` for exactly why and how) - NOT yet tested by the user in their own live terminal as of this writeup. State was reset to fresh for that.

## ⚠️ UPDATE 2026-08-02 (later): the board widget IS now wired up — read this before assuming the section below is still accurate

An earlier version of this doc said no board widget UI existed yet and that it should wait for real P2 movement data. **That plan changed**: civ-txt's own board-viewer widget (`&.widgits/board-viewer`) was built out fully this session (real camera controls, real 3D raymarching, real voxel textures — see `&.widgits/BOARD_WIDGET_PROGRESS.txt` for the whole story), and the SAME shared widget is now wired up for tactics-txt too:
- `pieces/system/board.txt` — a REAL, generated 10×10 terrain grid (not a placeholder), written by `CONFIRM_START` in `ops/tactics_menu_input.c`. Terrain glyphs: `.`=grass, `^`=high-ground, `~`=water/mud, `#`=wall (a real, TALL obstacle — board-viewer's own terrain tables were extended to support this glyph specifically for this project, since it didn't exist in civ-txt's own glyph set).
- `OPEN_BOARD_WIDGET` — a real numbered METHOD row on `main.chtpm` ("View Board"), spawns `&.widgits/board-viewer/button.sh run-widget <this project's real root>` as a genuinely separate, detached process (`setsid`) with its own GL window — this project's own screen never changes. Exact same mechanism as civ-txt's own (`ops/civ_menu_input.c`'s `OPEN_BOARD_WIDGET`).
- `button.sh` now writes `pieces/system/house_root.txt`/`real_project_root.txt` (needed for the widget spawn to find itself) and symlinks `board.txt` into every session (the same "real persistent data must be explicitly symlinked or it silently writes into the ephemeral session dir" bug class already found/fixed elsewhere — see `!.HOUSE_STDS.md` §F.7).

**Still true, not yet done**: no unit x/y positions exist anywhere in tactics-txt's own data yet (P1's `units.txt` is still just two flat rosters, no board coordinates) — the widget shows real TERRAIN now, but no units on it. That's real P2 (movement) work per `TACTICS_TXT_DESIGN.md` §10, still not started. Once units have real positions, the widget's own selector/command-bus hookup (`BOARD_WIDGET_ARCHITECTURE.md` §5) becomes meaningful the same way it will for civ-txt's own cities/units later.

**Status of everything else**: P1 skeleton (per `TACTICS_TXT_DESIGN.md` §10's own build order) is built and **live-verified through the real `button.sh run` entry point**: a real setup screen (Classic mode only), two fixed 3-unit rosters displayed correctly, real navigation into battle, and a real shared-turn/actions-remaining loop (side alternates, turn increments only on full round-trip) with ledger logging. The user is doing a human walkthrough after this handoff (`user-walkthru.txt`, same directory) — **do not assume anything about current game state until you read `pieces/system/config.txt` fresh.**

**Read `TACTICS_TXT_DESIGN.md` in full before extending this** — it has the complete data model (registries, professions/skills, Classic-vs-Collection modularity boundary), screen list, and P1-P10 build order this handoff only summarizes the P1 slice of.

---

## 1. WHAT'S BUILT (P1 only — this is a skeleton, not a battle yet)

- `system/` — copied verbatim (source) from `@.apps/my-chara-txt/system/`, zero project-specific changes.
- `ops/tactics_menu_input.c` — METHOD-table dispatcher, modeled line-for-line on `mychara_menu_input.c`. Commands implemented: `SET_MODE:classic`, `CONFIRM_START`, `END_TURN` (alternates `active_side` 1↔2, resets `actions_remaining_this_turn` to 5, increments `turn` only when wrapping back to side 1 — i.e. after a full round).
- `ops/tactics_compose_frame.c` — per-screen renderer. Two screens: `setup` (mode pick + a static preview of both fixed rosters) and `main` (turn/side/actions-remaining + BOTH full rosters read live from `units.txt`).
- `pieces/system/units.txt` — the two fixed Classic-mode rosters (3 units/side: Side 1 = warrior/chef/farmer, Side 2 = warrior/clown/lawyer), each with `profession` + `hp` fields. **Read-only in this P1 pass** — nothing mutates it yet (no combat exists).
- `pieces/chtpm/layouts/setup.chtpm` + `main.chtpm`, `pal/setup_module.pal` + `main_module.pal` — same per-screen-module shape as my-chara-txt/civ-txt.
- `pieces/system/config.txt` — `battle_id`, `mode`, `turn`, `active_side`, `actions_remaining_this_turn`, `game_state` (`setup`→`playing`).
- `button.sh` / `scripts/build.sh` / `default_op.txt` — direct structural copies, renamed for tactics-txt.

## 2. LIVE VERIFICATION PERFORMED

Same CPU-safety rigor as civ-txt/my-chara-txt this session: every test wrapped in `timeout N bash button.sh run`, CPU%-checked (stayed low, 0-2%), killed and re-checked between tests.

**One real false alarm during testing, worth knowing about if you see it again:** the very first test run showed a stuck `[Map Loading...]` / `[No Methods]` placeholder frame that never updated, even though `view.txt` and the `setup` screen's `piece.pdl` both had correct real content on disk at the same moment. Root cause: a **stale `chtpm_parser_pal` process from an earlier test** was still alive and being matched by a loose `pgrep` pattern, so the CPU/env checks were inspecting the WRONG (old, orphaned) process's environment — the actual current session's `chtpm_parser_pal` was a different PID entirely and was rendering correctly the whole time. **Lesson: always verify you're checking the CURRENT session's PID** (cross-check against the session directory path in `/proc/<pid>/environ`'s `PRISC_PROJECT_ROOT`, not just a name-based `pgrep` match) before concluding something is broken — this cost real debugging time chasing a non-bug. Once verified against the correct PID, everything rendered correctly on the very first try.

Confirmed live (once checking the right process):
- Setup screen renders mode picker + static roster preview correctly.
- `SET_MODE:classic` + `CONFIRM_START` work, `game_state` flips `setup`→`playing`.
- Real navigation `setup.chtpm`→`main.chtpm` via `<button href>`.
- Main screen renders BOTH full rosters live from `units.txt` (all 6 units, correct professions/HP).
- `END_TURN` alternates side 1→2→1 correctly, increments `turn` only on the 2→1 wrap (confirmed: turn stayed 1 across the first End Turn, became 2 on the second), ledger appends correctly with side attribution on each line.
- Same digit+Enter buffered-keypress quirk as civ-txt/my-chara-txt (documented in `user-walkthru.txt` §2) — not a bug.

**Game state was reset to fresh (`game_state=setup`, mode unset) and the ledger cleared at the end of this session.**

## 3. WHAT'S NOT BUILT (honest, large gap — this is P1 of 10 phases)

Per `TACTICS_TXT_DESIGN.md` §10, everything from P2 onward: **no board exists at all** (units have no x/y position — this P1 skeleton doesn't even have a 10×10 grid yet, just two flat rosters), no movement, no terrain/registry, no Attack/Skill/Defend actions (the "actions_remaining" pool is wired but nothing currently spends from it — every End Turn shows 5/5 because there's no action yet that costs anything), no combat resolution, no `board.chtpm` widget, no professions/skills registry (the profession NAMES shown are just static strings in `units.txt`, not backed by a real `skills.txt` registry yet), and **Collection mode doesn't exist at all** (only Classic's fixed rosters).

**Recommended next step, per the design doc's own P2:** "Movement: `move_unit.c`, `roster.chtpm` unit selection + move-target list, real path-cost." This is the first phase that needs the 10×10 grid to actually exist as real data (even before terrain variance in P3) — read that doc's §6 (Combat & Movement) and §9 (directory layout, `pieces/battle_01/board/tile_<x>_<y>/` + `pieces/battle_01/units/<unit_id>/state.txt`) before starting, since P1's flat `units.txt` will need to be replaced/supplemented by the real per-unit piece-directory shape that design doc specifies.

## 4. FILES (for a fast diff/audit)

```
@.apps/tactics-txt/
├── TACTICS_TXT_DESIGN.md         (pre-existing, from earlier this session)
├── HANDOFF_NEXT_SESSION.md       NEW (this file)
├── user-walkthru.txt             NEW
├── button.sh                     NEW
├── default_op.txt                 NEW
├── scripts/build.sh               NEW
├── system/*.c                     NEW (copied from my-chara-txt)
├── ops/tactics_menu_input.c       NEW
├── ops/tactics_compose_frame.c    NEW
├── pal/setup_module.pal           NEW
├── pal/main_module.pal            NEW
├── pieces/chtpm/layouts/setup.chtpm      NEW
├── pieces/chtpm/layouts/main.chtpm       NEW
├── pieces/system/config.txt       NEW (reset to fresh setup state)
├── pieces/system/units.txt        NEW (the two fixed Classic rosters)
└── data/master_ledger.txt         NEW (empty, reset for walkthrough)
```

---

## 5. ADDENDUM (same day, later): GL mirror, test harness, and a HIGH-PRIORITY confirmed bug shared with the whole house

**GL/RGB mirror**: `system/gl_mirror.c` (real GLUT window, ported from `101.mutaclsym🧟‍♂️️+18.01`) + `chtpm_rgb_render.c` + the ASCII glyph font registry are all now real and wired into `button.sh run` (gated on `NO_GL`/`DISPLAY`). **User directly confirmed seeing the window open** (for my-chara-txt; the same wiring here is structurally identical and build-verified, not yet independently re-confirmed visually for this specific project). A pre-existing nested-comment bug in mutaclysm's own `gl_mirror.c` (line 364, breaks compilation) was found and fixed in this copy. Note per direct user instruction: this basic text-mirror is separate from — and does NOT replace — the future mutaclysm-style ASCII/emoji "View Board" GL window (its own persistent session, opened on demand once `board.chtpm` exists) discussed with the user but explicitly deferred past this pass.

**Test harness**: `test-harn-same/scenarios/demo_setup_and_battle.sh` — covers setup→Confirm&Start→Enter Battle→End Turn×2, verifying the shared 5-action pool, side alternation, and turn-increment-only-on-full-round logic. All checks passing. Run: `cd test-harn-same && bash scenarios/demo_setup_and_battle.sh` (or `bash button.sh demo`).

**HIGH-PRIORITY CONFIRMED BUG, shared with the whole CHTPM family (my-chara-txt, civ-txt, and almost certainly every other project using this house's shared `chtpm_parser_pal.c`)**: navigating between screens via `<button href>` causes stale `interact_relay.txt` entries to be re-dispatched against the new screen's own `piece.pdl` — and this is confirmed to COMPOUND with repeated navigation (my-chara-txt's own testing saw `day` inflate from ~4 to 55+ within 8 round-trip loop iterations). **This project hit it directly and visibly**: navigating setup→main causes exactly one phantom `END_TURN` to fire automatically, flipping `active_side` before any real key press — confirmed reproducible, NOT a scenario bug, and now explicitly asserted-as-expected in `demo_setup_and_battle.sh` rather than hidden or worked around silently. Full writeup and root-cause hypothesis: see `@.apps/my-chara-txt/HANDOFF_NEXT_SESSION.md` §6c — **read that before writing any new tactics-txt scenario or building P2's movement/board screens**, since every future screen this project adds (`roster.chtpm`, `board.chtpm`) will be subject to the same compounding issue on its own navigation paths.

Game state reset to fresh (setup screen, mode not picked, empty ledger) again after this addendum's own testing.

---

## 6. ADDENDUM 2 (same day, later still): real board widget wiring, real 3D view

See the top-of-file update note for the full summary. Additional detail:

- **Board glyph set differs from civ-txt's own** by design: `.`/`^`/`~` are shared/reused (same real height/color meaning in board-viewer's own terrain tables — grass≈plains, high-ground≈hills, water/mud≈water), but `#` (wall) is new to this project — a real, tall (1.8 world-units, taller than civ-txt's own capital marker) obstacle, added specifically because tactics-txt needed genuinely "extrudable" terrain the design doc's own combat model (cover/line-of-sight around obstacles) will eventually depend on. It currently renders as a flat color in 3D (no curated voxel texture asset yet — falls back cleanly, same as any untextured cell) and a real emoji (🧱 brick) in 2D via board-viewer's own generic on-demand texture-generation path.
- **Compile-verified, NOT live-tested this pass** — `ops/tactics_menu_input.c`/`ops/tactics_compose_frame.c` and `scripts/build.sh` all compile/build clean, but the actual spawn-the-widget-and-see-a-real-board flow has not been run live for tactics-txt specifically yet (civ-txt's own identical mechanism has been, extensively). **Recommended first step for whoever picks this up next**: `bash button.sh run`, pick Classic mode, Confirm & Start, Enter Battle, select "View Board" — confirm a real GL window opens showing the generated 10×10 grid with visible walls/high-ground/water variety, same as civ-txt's own walkthrough.

---

*End of handoff. Game state was left fresh (setup screen, mode not picked) for the user's own walkthrough — check `pieces/system/config.txt` before assuming anything about progress.*
