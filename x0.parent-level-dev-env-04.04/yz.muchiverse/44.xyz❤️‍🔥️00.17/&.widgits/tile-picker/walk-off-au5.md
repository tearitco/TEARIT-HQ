# 🪟 walk-off-au5.md — tile-picker (KHTPM home), session pause 2026-08-05

> **Superseded for current status:** house-root **`walk-off-au6.md`** (2026-08-06) — content-aware popup width, idle GL (no swap when static), frame pacing. Keep this file for 08-05 history.

Read `TILE_PICKER_DESIGN.md` §9-§13 first — this file is a short pointer/status summary, that doc has the real detail.

## What's real in `tp_desktop_window.c` right now

The raw-X11 desktop-entity window (every pet, asa/ava, and MUCHI_RANCHER monster runs this SAME binary) got substantial real work this session, now informally named **KHTPM** — confirmed a genuinely separate, house-own parser/convention, NOT the real `chtpm_parser_pal.c` pipeline, but deliberately mirroring its real visual/nav conventions on purpose:

1. **Real history + injection** (§10) — `history.txt` + `interact_relay.txt` per package, `RUN_METHOD:<Label>`/`ACTIVATE_NAV:<N>`/`CLOSE` relay commands.
2. **Real `objects.pdl` multi-page menus** (§11) — `PAGE`/`OBJECT` rows, `GOTO:<page>`/`BACK` navigation (real page-stack, reopens the popup so the switch is actually visible — a real gap caught and fixed mid-build), auto-appended `Cancel` row if a page's author forgets one.
3. **Real shared live nav-claim pool** (§13) — `#.desktop/livedesk_nav_claims.txt`. A `KIND=row` claim per open menu's own rows, a `KIND=tab` claim per open taskbar tab, same pool, numbers never collide, freed on close/reuse.
4. **Real `[ ]`/`[>]` focus-cursor** (§13) — Up/Down/Enter, matches actual captured CHTPM frame format exactly (corrected TWICE this session — first attempt put the number inside the bracket, wrong; real format is `[ ] N. Label`/`[>] N. Label`, empty bracket is its own real cursor marker).
5. **Real text-input rows** (`STATE:<key>` action) — click-to-activate/Escape-to-commit, same shape as `cli_io`'s own convention.

## `&.widgits/livedesk-taskbar/` — real, separate widget

Auto-launches itself the first time ANY livedesk entity opens (real PID-file singleton check via `tp_desktop_window.c`'s own `ensure_taskbar_running()`), every entity after that just adds a tab. Real `Nav > ` terminal input in the middle of the bar — type a number, Enter jumps to a tab (raise+focus) or remotely activates a row in another window's open menu (writes `ACTIVATE_NAV:<N>` into that entity's own `interact_relay.txt`).

**Placement note**: this was originally built as a file inside `tile-picker/ops/` by mistake — corrected to its own real top-level widget dir (`&.widgits/livedesk-taskbar/`) after direct correction. If you ever see taskbar-related code show up inside `tile-picker/` again, that's a regression of this same mistake, move it back out.

## `#.desktop/livedesk_*.txt` — house-wide, not project-scoped

- `livedesk_master_ledger.txt` / `livedesk_next_index.txt` — PERMANENT per-entity index, assigned once, reused forever across relaunches. Different from...
- `livedesk_open.txt` — LIVE registry of currently-open entities only (taskbar's own poll source).
- `livedesk_nav_claims.txt` — LIVE, ephemeral shared number pool (§13 above).
- `livedesk_taskbar.pid` — singleton lock for the taskbar process.

Don't conflate the PERMANENT ledger index with the LIVE nav-claim number — they're deliberately different numbering systems for different purposes (a real, confirmed correction mid-session after the first attempt conflated them).

## Real, confirmed-not-broken: the Gallery↔Page href "bug"

If you're coming here from an event-ez investigation: the href-nav bug reported in `EVENT_SCRIPTING_PROGRESS_AND_GOALS.md` was root-caused this session and found to be a test-hygiene issue (stale sessions sharing the same GL window title), not a real parser bug. Full writeup: `&.widgits/event-editor/walk-off-au5.md`, `!.HOUSE_STDS.md` §H.5.4.

## Known gaps, not built

- `tp_range_grid.c` has NONE of KHTPM's real properties (history/injection/objects.pdl) — completely untouched.
- `OPEN_USER` (legacy submenu) can't be remote-activated via `ACTIVATE_NAV` — needs live popup-position context the relay path doesn't have.
- No real up/down focus-cursor persists ACROSS a page switch yet (resets to row 0 on every `GOTO`/`BACK`, which is probably fine/expected, just noting it's not "remembers where you were").
- Pets/asa/ava are running the pre-KHTPM binary right now — relaunching them picks up everything above for free, no new code needed, but hasn't been done (their currently-running processes predate this session's work).

## CPU/process safety

See `!.HOUSE_STDS.md` §H.5.4. Every `gl_mirror`-based session (event-ez in particular) shares one window title — verify zero stray processes before AND after every test cycle.
