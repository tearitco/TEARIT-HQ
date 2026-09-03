# OpenCode Handoff — Events "6-rung ladder"

**Date:** 2026-09-03 · **For:** OpenCode · **Driver / reviewer:** Claude (this session)
**Chart of record:** `08-roadmap/design-docs/events-db-status-board.html` → the
"Plan going forward" section. **Background:** `sep-1-events-SOS.md`,
`EVENT-COMMAND-REGISTRY-ARCHITECTURE.md`, `EVENTS_RUNTIME.md`.

Media-suite work is a **separate track** (Grok) — see `media-suite.md`. Don't touch
`103.media-studio/` here.

---

## The one thing to understand first

The events system already has **44 working COMMAND blocks**. Most command types are
added by **editing one registry file** — `44.xyz.01.00/#.ref/menu/event_commands.registry.pdl`
— with **zero recompile**. A COMMAND block is compiled `event.ir.pdl → event.pal →
cmd_N.sh` by `compile_page()` in
`44.xyz.01.00/&.widgits/events-hq/ops/khtpm_events_hq_manager.c`.

Two block shapes:

- **Pure-template (no C):** `PAL li … / PAL ecall "{STATE_DIR}/<file>" "{key}"` — writes
  a `key=value` line to a flat kv file. `control_switch` / `control_variable` are the
  models (registry lines ~85–103). Prefer this route always.
- **C-op-backed:** `PAL ecall` into a small `mr_*.+x` built from
  `&.widgits/events-hq/ops/`. Model: `mr_character.c` (~55 lines — read it, it's the
  whole pattern: `argc>=4`, `snprintf` a `<dir>/<name>.pdl` path, `mr_kv_set()`,
  `mr_log()`). New ops go in the same dir + a line in that dir's build script.

The picker UI has **exactly 2 text fields** (`FIELD1`/`FIELD2`). A 3rd identifier is
compounded into one field as `"a:b"` — see the registry comment at ~line 312.

State files live under `{STATE_DIR}` (session root, entity-root fallback —
`resolve_session_root()` in the events-hq manager). Same flat kv convention as
`switches.txt` / `variables.txt` / `inventory.txt`.

The **Common-Events manager** (`common_events_manager.c`) is the runtime tick:
edge-triggered Autorun, cooldown-gated Parallel, watches a named switch. Any new
"tick consumer" reuses its cadence — don't invent a new loop.

---

## The ladder

Do these **in order**. Each rung: land the registry/C change, then tell the driver
so it gets a fresh build + a text-verified run before the next rung. **Check in
before Tier 6.**

### Rung 1 — Access toggles → the registry · *now, 0 C*

Append **4 COMMAND blocks** copied from `control_switch`, each pointed at a
`system_flags.txt` key: `menu_enabled`, `save_enabled`, `encounter_enabled`,
`formation_enabled` (RPG-Maker "Change Menu/Save/Encounter/Formation Access").
Use the `SELECT2 1:0` ON/OFF→1/0 route so no C is needed. Verifiable the moment the
manager re-polls: run the event, `cat {STATE_DIR}/system_flags.txt`.

### Rung 2 — Screen effects & actor-graphic ops · *1–2 ops*

- `mr_screen.c` — twin of `mr_character.c`. Modes `fadeout|fadein|tint|flash|shake`
  → `screen_state.pdl`. Registry blocks mirror `show_animation`.
- Add string modes to the actor-graphic op (`mr_actor_string.c` if present, else a
  new `mr_actor_graphic.c` same shape) → `actor_graphic_state.pdl`.
- **Write-only for now** — the consumer that *draws* this is Rung 5.

### Rung 3 — Audio: Play SE / BGM / BGS / ME · *1 op + dep check*

First: `which aplay paplay ffplay` on the target. If one exists → `mr_audio.c`
(~30 lines) that shells to it; 4 registry blocks. If **none** exist → report that and
**skip the rung** — do not ship a silent no-op.

### Rung 4 — Transfer Player / Movement Route / Scroll Map · *wiring job, not greenfield*

The position substrate is **already live**: `44.xyz.01.00/.../desks/<name>.pdl` rows
carry `px_x px_y grid_col grid_row … z`; `#.desktop/desk_grid.pdl` sets `cell_px`
and has commented-out `map_cols`/`map_rows` (the movement wall); the taskbar manager
already clamps & rewrites DESK rows.

1. **Pick "the player"** — simplest: a designated desk entity (user avatar / a
   player pal). Its `desktop_pos.txt` + `DESK|` row are the position of record.
2. `mr_transfer.c` (~48 lines, `mr_character.c` shape) — writes `x y map` to the
   player's pos file **and** rewrites its `DESK|` row so it visibly jumps. Registry
   `transfer_player`, params `x,y,map`.
3. `mr_move_route.c` — appends a step list (`up down left right wait`) to
   `move_route.pdl`. Registry `set_movement_route`, params `target,route`.
4. `mr_scroll_map.c` — writes a pan offset to `camera.pdl`.
5. `map_tick` — **the only genuinely new binary.** Reads `move_route.pdl`, applies
   one step per tick, clamped to `desk_grid.pdl`'s bound (uncomment/set
   `map_cols`/`map_rows`). Reuses the common-events manager tick cadence.
6. **Second substrate, same commands:** when the event runs inside a board-viewer
   game (`&.widgits/board-viewer/` — civ-txt / tactics-txt / piececraft-hq), point
   the same ops at board-viewer's grid/camera files instead. Registry + event API
   unchanged — only which coordinate file the op writes.

### Rung 5 — Make the write-only commands visible · *consumer*

One pass in the desktop tile renderer (khtpm entity mode) and/or board-viewer that
reads `screen_state.pdl` / actor-graphic state and reflects it. This is what turns
every Rung-2 "writes state nobody draws" command into a real effect. Do it **once**,
after Rungs 2 & 4 have state to show. Coordinate with the driver — this touches
`khtpm_core_render.c` which other agents also edit.

### Rung 6 — Shop & Battle · *real builds, check in first*

Genuinely new subsystems, each its own design pass. Shop = buy/sell UI + price math
+ gold (reuses `change_gold` + Items/Weapons/Armors tabs). Battle = the missing
subsystem the Enemies/Troops/States/Animations db-hq tabs all wait on. **Not on this
ladder's critical path** — do not start without a fresh design doc + owner sign-off.

---

## Rules of engagement

- **Never `git add -A`.** Stage explicit paths only. `khtpm_core_render.c` and
  `khtpm_taskbar_manager.c` are edited by other agents concurrently — for Rung 5,
  hand the driver a diff rather than pushing.
- Prefer a **registry-only** change every time it's possible before writing C.
- Every op writes **real files** and has a **non-op fallback** path.
- After each rung: stop, report, wait for the driver's build + text-verified run.
- Commit message footer:
  `Co-Authored-By: Claude Sonnet 5 <noreply@anthropic.com>` /
  `Claude-Session: https://claude.ai/code/session_01P4rAhi6a7TzLBZdcaqfHXN`

## Parallel, non-blocking — db-hq tab audit

Per `sep-1-grok.md`: for each of the 12 generic list tabs, confirm what its real
RPG-Maker equivalent should store and whether a row edits its sub-fields today. Does
not block the ladder.
