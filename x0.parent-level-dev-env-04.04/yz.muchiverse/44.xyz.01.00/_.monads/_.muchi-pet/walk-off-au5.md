# 🐉 walk-off-au5.md — MUCHI_RANCHER, session pause 2026-08-05

> **Superseded for current status:** house-root **`walk-off-au6.md`** (2026-08-06) — Play/Menu/qolq, Clear All, content-width menus, CPU ≤30fps. Keep this file for 08-05 history.

Written at a real pause point (a machine crash interrupted testing — recovered cleanly, no data lost, but a good moment to stop and hand off). Read this before touching anything in `@.apps/MUCHI_RANCHER/` again. Also read `MUCHI_RANCHER_DESIGN.md` (the full design) and `&.widgits/event-editor/visual-event-compiler-pal.md` (the event-compiler design this whole session's second half was proving out).

## ✅ What's real and working right now

1. **8 real monster sprites extracted** from `$BigMonster1.png`/`$BigMonster2.png` (`ops/mr_monster_extract.c`), all visually verified. `entities/{m1_ninjadragon..m8_redhorned}/sprite.csv`.
2. **Real 2×2-tile desktop footprint** — `tp_desktop_window.c`'s `WIN_PX` is a real runtime var now (`footprint_tiles` in `meta.pdl`), backward-compatible with pets/asa-ava (still default to the original 64px).
3. **`active_monster.pdl`/`roster.pdl`** — a single generic `pieces/monster/button.sh` spawns whichever monster `active_monster.pdl` names; switching monsters is a one-line file edit, not new code.
4. **KHTPM** (this session's name for the real, separate-from-CHTPM parser/convention built into `tp_desktop_window.c`):
   - Real per-entity `history.txt` + `interact_relay.txt` injection (`RUN_METHOD:<Label>`, `ACTIVATE_NAV:<N>`, `CLOSE`).
   - Real `objects.pdl` multi-page menus (`PAGE`/`OBJECT` rows), `GOTO:<page>`/`BACK` navigation, auto-appended `Cancel` row.
   - Real `[ ]`/`[>]` focus-cursor navigation (Up/Down/Enter), matching actual CHTPM visual convention (verified against a real captured frame).
   - Real shared, live nav-claim pool (`#.desktop/livedesk_nav_claims.txt`) — taskbar tabs and open-menu rows draw unique numbers from the same pool, never collide.
   - **Full design + build history**: `&.widgits/tile-picker/TILE_PICKER_DESIGN.md` §10-§13.
5. **`&.widgits/livedesk-taskbar/`** — real, its own top-level widget (a placement mistake happened first, corrected). Auto-launches once, singleton-checked; real `Nav > ` terminal input; jump-to-tab or remote `ACTIVATE_NAV` into another window's open menu.
6. **The event-compiler pipeline is real and proven for ONE command type** (`Change Gold`) — see next section. `Show Choices` (real branching) is designed but not built.

## 🎯 Where we are on the "visual compiler for .pal" work (the deep part)

This was explicit, direct framing: MUCHI_RANCHER is **the proving ground** for whether this house can do RPG-Maker-style event authoring correctly — a human clicks through a real GUI (event-ez), and the *output* is a real, executable `event.pal` that a real context-menu click runs.

**Built and proven this session, via genuine k3 key injection (not hand-authored files)**:
- A real Command Picker screen in event-ez (`event_ez_page_N_cmdpick.chtpm`) — currently lists just "Change Gold" (Show Choices deliberately not listed yet — no dead buttons).
- A real Change Gold parameter screen — one `cli_io` amount field, Save appends a real `NODE` row to `event.ir.pdl` and **recompiles `event.pal` fresh from scratch** every time (genuine compiler semantics — IR is the source of truth, `.pal` is always regenerated, never hand-patched).
- A real new op, `ops/mr_change_gold.+x` — reads/writes `<entity_dir>/inventory.txt`'s `qolq=` line (your game's "gold").
- **Two real, load-bearing bugs found and fixed only by actually running the compiled output** (not just reading it):
  1. `prisc+x`'s real `exec` opcode only supports ONE literal argument — a compiled `exec <op> <arg1> <arg2>` line silently no-ops (no crash, no error, just never runs). Fixed by generating a real per-command wrapper shell script (`page_dir/cmd_<id>.sh`) and having `event.pal` exec THAT with zero args. Full writeup: `!.HOUSE_STDS.md` §H.5.1.
  2. `${command_list_rows}` (dynamic content in the Page screen) was one render behind on first navigation — a real race between `chtpm_parser_pal`'s own href-commit-triggered reparse and the separate `ez_compose_frame.+x` process computing fresh content slightly later. Fixed by precomputing every reachable page's own `command_list_rows_N` (page-numbered key) at Gallery-compose time, same pattern already used for pre-generating the `.chtpm` files themselves. Full writeup: `!.HOUSE_STDS.md` §H.5.2, §H.5.3.
- **In progress, interrupted by the crash**: re-running the full authored flow through event-ez with the wrapper-script fix in place, to get `m8_redhorned`'s real page 1 (`Change Gold: 10`) actually executing via `prisc+x` end to end (money file created, real value change). The compiler-level fix is done and compiles clean; the live re-verification was mid-flight when the session paused.

## 🔜 Concrete next steps, in order

1. **Finish the interrupted verification**: launch one clean event-ez session (`EZ_PKG_DIR=.../m8_redhorned/event_pkg`), confirm page 1's `Change Gold: 10` command is listed, then run `page_1/event.pal` directly via `system/prisc+x` and confirm `entities/m8_redhorned/inventory.txt` really gets `qolq=10`. (Current on-disk state: `page_1/event.ir.pdl` and `event.pal` both hold ONE real `Change Gold: 10`/`amount=10` command, compiled with the wrapper-script fix — should just need one clean run to confirm.)
2. **Wire a real context-menu row** ("Faucet") on `m8_redhorned` whose action actually invokes `page_1/event.pal` via `prisc+x` (same real "Play"-style dispatch convention `dog`'s own `meta.pdl` already uses) — closes the loop from a real desktop click to a real money change.
3. **Show Choices** (the harder, "profound" one, per direct framing) — real branching command list, each choice spawning its own `branches/choice_N/` sub-command-list, rendered via a REAL KHTPM `objects.pdl` popup injected into the monster's own already-running window (a new `interact_relay.txt` command, `SHOW_PAGE:<objects.pdl>|<result_file>`, not yet built — design is written in `visual-event-compiler-pal.md` §7, confirmed by direct user answers: reuse KHTPM, real call/return via `prisc+x`'s blocking `exec` invoking a nested `prisc+x <branch's event.pal>`).
4. Real per-monster `event_pkg/` for the OTHER 7 monsters, once the pattern is fully proven on `m8_redhorned`.
5. `event-ez` still needs a real "load existing saved command list back into the form" step, and a real way to EDIT an already-saved command (today Save only ever appends — proven, not a bug, but a real, deliberately-deferred gap).

## ⚠️ Known gaps / honesty checks, don't assume these are done

- `condition.pdl`'s own trigger is recorded but **not evaluated** at runtime yet — no "highest-numbered-page-wins" real RPG Maker semantics. Every page is unconditionally available right now (matches pets' own current single-page state). Deferred by direct instruction earlier this session, revisit before it actually matters (e.g. `self_A`-gating Tournament/Errantry "currently away").
- `OPEN_USER` (a legacy submenu action) is not remote-dispatchable via `ACTIVATE_NAV`/relay — needs live popup-position context the relay path doesn't have.
- `tp_range_grid.c` has none of the KHTPM history/injection properties — untouched this whole pass.
- Pets/asa/ava are running the PRE-KHTPM binary — they'll get all of this for free the next time they're relaunched, but their currently-running processes predate it.
- The `objects.pdl` layout convention still has no official house-standard short name beyond "KHTPM" (informal, coined this session, not yet written into `!.HOUSE_STDS.md` as a formal naming decision — worth confirming if it should be).

## 🖥️ CPU/process safety — read before spinning up ANY event-ez/KHTPM session again

A real crash happened this session, very likely CPU-load-related given how many stray `gl_mirror`/`chtpm_parser_pal` processes accumulated across repeated test cycles (all sharing the literal window title `"mutaclsym RGB mirror"` — see `!.HOUSE_STDS.md` §H.5.4 for the full writeup). Before ANY future test:
```
ps aux | grep -E "chtpm_parser_pal|gl_mirror|prisc" | grep -v grep   # must be empty (or only unrelated processes)
xwininfo -root -tree 2>/dev/null | grep -c "mutaclsym RGB mirror"   # must be 0 before launch, 1 after
```
Always `timeout <N>` wrap the launch. Kill and re-verify after every test cycle, not just at the end.
