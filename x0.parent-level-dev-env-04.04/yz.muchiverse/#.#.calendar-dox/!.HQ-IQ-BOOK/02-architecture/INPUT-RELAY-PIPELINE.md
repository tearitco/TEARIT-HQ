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

One per-mode focus gate: if two windows of the SAME mode share one
history file, only the X-focused one reads/dispatches — the other
skips to EOF (matches wraith-alpha's "one file, one reader" shape).

## The relay files

- `#.desktop/livedesk_agent_relay.txt` — parser-layer, raw
  digit/Enter/Escape/printable ASCII, resolved the same way real human
  input is. Driven via
  `#.desktop/harnesses/khtpm-livedesk-taskbar/nav.sh` (needs
  `HOUSE=<house_root>` set — defaults to `$PWD` otherwise, an easy
  footgun if you `cd` first).
- `#.desktop/strip_history.txt` — manager-layer, already-resolved
  decimal action codes (`KSC_HQ_HEADER_BASE`+n for a header cell,
  `KSC_HQ_ITEM_BASE`+n for a submenu row).
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
