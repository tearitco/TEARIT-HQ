# livedesk-taskbar

A two-process, real declarative-layout-driven window pair:
`ops/khtpm_strip_parser.c` (the long-lived outer process — Xlib rendering,
layout parsing, hit-testing) forks `ops/khtpm_taskbar_manager_main.c`
(the pure-logic manager — tabs, sessions/desks, save/load, cli-io, no
Xlib) as needed, communicating via a small file relay
(`#.desktop/strip_history.txt`/`strip_state.txt`/`strip_frame_changed.txt`).
Auto-launched by `tp_desktop_window.c`'s own `ensure_taskbar_running()`
the first time any livedesk entity opens (singleton check via
`#.desktop/livedesk_taskbar.pid` + a `/proc` scan for an already-running
instance).

**2026-08-11: this replaces legacy `tp_taskbar.c`, fully retired** —
archived to `ops/LEGACY-ARCHIVE-20260811.zip`, originals deleted. See
`#.#.✅️.cal-user-sum/AU11-khtpm-gap-fixes.txt` for the complete
retirement record and every real bug found/fixed along the way.

## What it is

- **Persistent header** (`hq_win`): 12 cells (HQ, user, file, desks, pals,
  palettes, edit, player, db, plugins, store, network), each a real
  `<button>` in the layout file, most with their own `ACTIVATE`-scope
  popup submenu.
- **Submenu popup** (`popup_win`): a separate window, positioned under
  whichever header cell opened it (or, for cli-io text-entry modals like
  save-as/rename, centered on screen — a deliberate legacy convention
  ported forward, not a bug — see `khtpm_strip_parser.c`'s own
  `draw_popup_win()` comment).
- **Bottom bar** (`win`): one tab per currently-open entity,
  `N. entity_name`, with real sprite rendering from each entity's own
  `sprite.csv`.
- **One unified keyboard-navigation cursor** spans header cells then
  tabs as one sequence (`g_nav_focus`/`unified_apply()` in
  `khtpm_strip_parser.c`) — right-click any of the 3 windows arms it at
  position 0, arrow keys/digits move it, Enter activates whatever it's
  currently on.
- The layout itself is declarative: `khtpm_strip_header.chtpm`/
  `khtpm_strip_bottom.chtpm` (this dir), a real tag-tree format
  (`<panel>`/`<text>`/`<button>`/`<row>`/`<cli_io>`, `${var}`
  substitution, `ACTIVATE` scope) — see `khtpm-strip-parser-design.md`
  and `khtpm-strip-parser-SCOPE.md` for the format and its scoping
  rationale.

## Configuration

`#.desktop/livedesk_taskbar.pdl` (`SECTION | key | value` rows, no
recompile needed):

| Key | Meaning |
|---|---|
| `strip_x_offset` / `strip_y_offset` | Header position (screen-absolute) — y defaults to 40, below GNOME Shell's own top panel |
| `strip_user_cmd` | Command run when the user/guest cell is clicked |
| `hq_label`, `hq_menu_N_label` / `hq_menu_N_cmd` | The HQ button's own menu — includes the real, live `$.restart` command (`sh run_khtpm_strip.sh new`) |

Note: the older `strip_btn_N_*` rows also present in that file are a
leftover from an earlier config format and are NOT read by the current
manager — header cell content is built dynamically
(`livedesk_build_*_menu()` functions in `khtpm_taskbar_manager.c`), not
from static per-button rows.

Theme (`#.desktop/livedesk_theme.pdl`): `bg`/`fg` (bar colors),
`opacity` (0.0–1.0, applied to all 3 windows via
`_NET_WM_WINDOW_OPACITY`, re-flushed a moment after first paint to work
around a real Mutter/Xwayland first-paint quirk — see
`AU11-khtpm-gap-fixes.txt`'s "opacity-on-reset" entry).

## Build

```
sh ops/build_khtpm_strip.sh      # build only
sh ops/run_khtpm_strip.sh new    # build + kill any running instance + relaunch
```

## Feature status (2026-08-11, all live-verified this session, not assumed)

Save/save-as/load, session switching, desk switching, the `reset` player
command (kill-all-entities-then-reload-current-desk, with a real
`/proc`-scan orphan sweep), digit-jump nav across the full header+tabs
range, submenu row digit-select, tab sprites + the header's own avatar
sprite, an agent-relay input channel
(`#.desktop/livedesk_agent_relay.txt` — see
`#.desktop/harnesses/khtpm-livedesk-taskbar/nav.sh`), and window opacity
on ALL 3 windows including a fresh launch (not just after a manual
restart) are all real, wired, and live-verified — not just build-clean.

## Known issues

- **Desk properties popup missing.** Legacy's real right-click-a-desk-row
  popup (name/rename/entity-count/delete/cancel) isn't ported — `edit-desk`
  currently goes straight to a bare rename modal. The underlying
  delete/count logic (`livedesk_delete_desk()`,
  `livedesk_desk_entity_count()`) is already in `khtpm_taskbar_manager.c`,
  just unwired.
- **No cross-process input-contention guard.** Legacy yields keyboard
  input when a different process (an entity) has its own context menu
  open (`remote_entity_menu_open()`); this port has no equivalent, in
  either the real KeyPress path or the new agent relay.

Full detail on both, plus everything already fixed, is in
`#.#.✅️.cal-user-sum/AU11-khtpm-gap-fixes.txt`.

## Debug tooling

- `#.desktop/khtpm_strip_frame_history.txt` — one line per redraw tick:
  header/bottom focus+active state, unified nav position, real X11 focus,
  cli-io/nav-armed state, `hq_focus`, element count. Truncated fresh on
  every launch (256KB cap otherwise). The primary tool for verifying
  state without a live human round-trip.
- `#.desktop/harnesses/khtpm-livedesk-taskbar/nav.sh` — agent-relay test
  driver (`nav <n>` / `row <n>` / `key <name>` / `type <text>` / `frame`)
  — see its own header comment for the full command list.

## History

- `ops/LEGACY-ARCHIVE-20260811.zip` — legacy `tp_taskbar.c` and every
  related file (its Windows port, its dedicated harness, an earlier
  abandoned khtpm architecture attempt), archived in full before deletion.
- `#.#.✅️.cal-user-sum/AU11-khtpm-gap-fixes.txt` — the complete,
  chronological record of the real parser build-out and every bug found
  fixing it, ending with the legacy retirement itself.
- `khtpm-strip-parser-design.md` / `khtpm-strip-parser-SCOPE.md` — the
  layout format and the locked tag vocabulary this parser implements.
- `walk-off-au5.md` — original 2026-08-05 legacy build session handoff
  (historical, predates the real parser — kept as a record, not current
  guidance).
