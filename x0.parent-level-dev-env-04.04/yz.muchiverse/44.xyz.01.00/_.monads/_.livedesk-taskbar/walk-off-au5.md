# 🗂️ walk-off-au5.md — livedesk-taskbar, session pause 2026-08-05

> **Current stack handoff:** house-root **`walk-off-au6.md`** (2026-08-06). Taskbar itself was already low-CPU (~300ms poll); au6 CPU work was mostly event-ez + entity GL.

Built this session, real and working. Full design/build history: `&.widgits/tile-picker/TILE_PICKER_DESIGN.md` §12-§13, `&.widgits/tile-picker/walk-off-au5.md`.

## What this is

A single, persistent, override_redirect bar at the bottom of the screen, auto-launched by `tp_desktop_window.c`'s own `ensure_taskbar_running()` the first time ANY livedesk entity (pet, asa/ava, MUCHI_RANCHER monster) opens. Real singleton check (PID file + `kill(pid,0)` liveness probe) — every entity after the first just adds a tab, never a second bar.

- Polls `#.desktop/livedesk_open.txt` (~1s) for currently-open entities, shows one real tab per entity: `[ ] N. entity_name`.
- Real `Nav > ` terminal input in the middle of the bar — type a number, Enter jumps: raises+focuses a tab's window, or writes a real `ACTIVATE_NAV:<N>` command into another window's own `interact_relay.txt` if the number belongs to a currently-open menu row elsewhere.
- Tab numbers come from the SAME shared, live nav-claim pool (`#.desktop/livedesk_nav_claims.txt`) context-menu rows claim from — never collides with a menu row's own number.

## Placement history (read this if you're about to move files around)

This widget was originally built as a file inside `&.widgits/tile-picker/ops/` by mistake, before being corrected to its own real top-level widget dir here, matching every other real widget's own layout (`event-editor/`, `event-ez/`, `tile-picker/` itself). Direct correction: *"why dont i see task bar in &.widgits dir? thats where its ment to be... its not a member of tile-picker."* If you ever see taskbar code inside `tile-picker/` again, that's this same mistake recurring — move it back here.

## Known gaps

- No stale-PID cleanup in `livedesk_open.txt` if an entity is SIGKILLed rather than closed cleanly (its tab would linger until the taskbar's own PID-liveness check on the OWNER PID eventually... actually check: `sync_tab_claims()` in `ops/tp_taskbar.c` only drops a claim when the entity's own PID is no longer in `livedesk_open.txt` at all — if THAT removal itself never happened because the entity was SIGKILLed, the stale tab persists. Not fixed this session.
- ~~No real up/down keyboard navigation of the tab bar itself — mouse click or the `Nav > ` number input only.~~ FIXED 2026-08-09: real arrow-key + digit-jump navigation now exists across a single unified cursor spanning the top strip buttons AND the bottom tab bar (see `nav_focus_apply()`/`nav_focus_step()` in `ops/tp_taskbar.c`) — right-click either bar to arm, arrows/digits move one shared `[>]` cursor, Enter activates.

## 2026-08-08/09 additions — top-left command strip + popups (post-au5)

Since this doc was written, a persistent top-left "command strip" (HQ/user/file/desks/player/db/plugins) was added in the same process (`ops/tp_taskbar.c`), each button opening a real popup submenu. Full build history + today's bug fixes: `yz.muchiverse/a8-cc-++fix.md` (nav-number bugs, unified focus cursor, opacity) and `yz.muchiverse/opacity-bug-aug9.txt` (still-open opacity investigation).

**Real, hard-won find (2026-08-09) — popup keyboard focus under Wayland**: this environment (confirmed: GNOME Shell + Mutter, `$XDG_SESSION_TYPE=wayland`, Xwayland rootless) does not reliably deliver real keyboard input to a brand-new override-redirect popup window via bare `XSetInputFocus`, even though `XGetInputFocus` reports the request succeeded. Fixed by reusing this same file's already-proven `taskbar_soft_focus()` (`XRaiseWindow` → `XSetInputFocus` → `XFlush`) for popup windows too, instead of bare `XSetInputFocus`. Confirmed via a real XTest-based key/click injector (tails `#.desktop/tp_taskbar_debug/key_inject.txt`) + a new per-frame popup draw log (`#.desktop/tp_taskbar_debug/popup_frame_log.txt`) — not by guessing from code review. Full writeup + the general rule for any future override-redirect popup in this house: `!.HOUSE_STDS.md` §F-19.

**Submenu numbering**: popup rows now display and accept LOCAL 1..n_menu numbers (what a human actually types while the popup is open), not the huge GLOBAL shared-pool claim numbers (`menu[i].nav`) they used to show — those global numbers still exist unchanged for the separate cross-window `ACTIVATE_NAV`/`lookup_nav` addressing mechanism, just no longer surfaced as what you type locally. See `popup_digit()`'s own header comment in `ops/tp_taskbar.c`.

## CPU/process safety

This widget's own process (`tp_taskbar.+x`) is lightweight and long-running by design (that's the point - it's a persistent bar). The REAL CPU risk this session came from a different class of process (`gl_mirror`/`chtpm_parser_pal`/`prisc+x` from event-ez testing) accumulating across test cycles - see `!.HOUSE_STDS.md` §H.5.4. The taskbar itself was not implicated in the crash, but always verify `ps aux | grep tp_taskbar` shows exactly one process if something seems off.
