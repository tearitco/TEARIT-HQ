# Input / relay / dispatch pipeline

*Condensed from `HOUSE_FAQ.md` ("NAV / INPUT" section) and
`TESTING_STRATEGY.md`, 2026-09-02.*

## How a click/keypress actually reaches the renderer

Real capture-then-consume (since 2026-08-28, Phase 3a/3b), the same
format mutaclysm's own `pieces/keyboard/history.txt` already used:

```
MOUSE_EVENT: <button> <x> <y> <is_press>
KEY_PRESSED: <decimal>
```

written to `<mode>_history.txt` (per-PID as of 2026-08-29 — see
03-pitfalls), consumed same-tick by `poll_agent_history()`. An agent
and a real human produce byte-identical lines; the dispatcher cannot
tell which wrote them — this is the basis for relay-only testing (see
`06-testing/TESTING_STRATEGY.md`).

🔄 **CORRECTION (2026-09-18)**: the line below described a per-mode
focus gate as the collision-avoidance mechanism. That's stale — fixed
2026-08-29 (`03-pitfalls/OPERATIONAL-LANDMINES.md`): history files are
now strictly **per-PID**, not per-mode, so two windows of the same
type never share a file at all (no focus gate needed — there's nothing
to gate). Also: relay dispatch does **not** check X11 focus state at
all, by design — see `RELAY-WINDOW-TARGETING-DESIGN.md` §1-2 for the
real current mechanism and why that's a deliberate property, not a gap.
~~One per-mode focus gate: if two windows of the SAME mode share one
history file, only the X-focused one reads/dispatches — the other
skips to EOF (matches wraith-alpha's "one file, one reader" shape).~~

## The relay files

🔄 **CORRECTION (2026-09-18)**: this section originally described
`livedesk_agent_relay.txt` as a live parser-layer path. **It is
dead.** `khtpm_strip_parser.c` (its only consumer, via
`poll_agent_relay()`) was folded into `khtpm_core_render.c` on
2026-09-01, and that function did not survive the merge —
`khtpm_strip_keyboard_ascii.c`'s own header comment documents this
retarget directly. Nothing reads `livedesk_agent_relay.txt` today.
Full writeup: `04-bugs/BUG-LOG.md`'s "`nav.sh`'s primary test commands
... are silent no-ops" entry (2026-09-18). Corrected list below.

- ~~`#.desktop/livedesk_agent_relay.txt` — parser-layer~~ **DEAD, do
  not use.** ✅ 2026-09-19: `nav.sh` no longer writes here — `nav`/`row`/
  `key`/`esc`/`type` now target `strip_history.txt` (default) or
  `entity_menu_history/<pid>.txt` (`NAV_PID=<pid>`), plus `click`/`string`.
  See the FIXED note in `04-bugs/BUG-LOG.md`.
- `#.desktop/strip_history.txt` — manager-layer, already-resolved
  decimal action codes (`KSC_HQ_HEADER_BASE`+n for a header cell,
  `KSC_HQ_ITEM_BASE`+n for a submenu row). **This is the real, live
  path** — read by `khtpm_taskbar_manager_main.c`'s
  `poll_strip_history()` → `dispatch_code()`. Reach it via `nav.sh
  hqcell <n>` / `nav.sh mgrcode <n>` (needs `HOUSE=<house_root>` set —
  defaults to `$PWD` otherwise, an easy footgun if you `cd` first).
- `#.desktop/entity_menu_history/<pid>.txt` — per-window relay, one
  file per `khtpm_core_render.c` process (keyed by its own `getpid()`),
  `KEY_PRESSED:`/`MOUSE_EVENT:` per line — this is the mechanism for
  driving an individual HQ window (not the taskbar strip itself). See
  `08-roadmap/design-docs/RELAY-WINDOW-TARGETING-DESIGN.md` for how an
  agent resolves which PID/window this actually targets.
- Prefer relay-file injection over `xdotool`/screenshots for driving
  or testing a taskbar/khtpm window; reach for `xdotool`/XTest only
  when the above are genuinely insufficient (e.g. real mouse-drag
  physics).

## nav_index numbering across windows

`nav_index` restarts at 1 in every window/header/footer independently
— this is not a bug to "fix" by sharing a global counter. Resolved
design: each window keeps its own unchanged local `nav_index`; a
separate window-level `Tab<N>` address is cycled with the literal Tab
key. `^` marks which window currently has Tab-cycle focus (moves only
on Tab); `>` stays the existing local cursor (moves on digit-jump
inside whichever window has `^`) — one level above LayDoc's own
`active_index`/`focus_index` split. Tab is agent-drivable for free
through the same `KEY_PRESSED:` file mailbox as any other key.

## Two tree/render systems, not one

- **LayDoc** (`khtpm_strip_layout.h`/`.c`) — the taskbar's own engine:
  flat-array tree with `parent_index`, `${var}` substitution at render
  time, a real ACTIVATE-scope nav mechanism.
- **Elem/CSS** (`khtpm_render_core.c`/`khtpm_draw_core.c`) — every HQ
  window's engine: pointer tree, concrete strings after parse, CSS box
  model + `hit_test()`.

LayDoc was found ahead of Elem for some real capabilities (var
substitution, ACTIVATE scope, `cli_io` tag); as of 2026-08-28, 6 of 8
gaps were ported into Elem/CSS so the two systems converge over time.
The taskbar itself has not been retargeted onto Elem yet — a separate,
later step. See `LAYDOC-ELEM-PORT-IMPLEMENTATION-PLAN.md` (folded into
08-roadmap) for status.
