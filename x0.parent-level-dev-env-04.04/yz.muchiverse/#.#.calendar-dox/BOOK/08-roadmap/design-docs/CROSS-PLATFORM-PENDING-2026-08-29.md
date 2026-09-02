# Cross-Platform Pending Work (since last verified pass)

**Last verified passes:** Windows 2026-08-21 (`8.21.GROK-win.md`), macOS 2026-08-23
(`LINUX_ROUNDTRIP.md`/`ROUNDTRIP_FIX.md`). Everything below landed on Linux
(canonical) between then and 2026-08-29 and has **not** been checked against
either other leg. This is a delta/backlog doc, not a fix — read it before the
next Windows or macOS pass so nothing here gets missed.

## 1. `khtpm_entity_menu_render.c` has NO Windows twin at all

Confirmed via file search: `khtpm_strip_parser.c` (taskbar strip) has a real
Windows twin (`khtpm_strip_parser_win.c` + `khtpm_strip_x11_win.c`, a Win32
GDI reimplementation of the Xlib/Xft subset it needs), and
`tp_desktop_window_rgb.c`'s predecessor has `tp_desktop_window_win.c`. But
`khtpm_entity_menu_render.c` — now the **canonical merged renderer for 8
window modes** (db-hq, events-hq, chat-hai, Settings/swatch-picker,
palettes, generic popup, entity-menu, and this session's new "Debug"
category) — has no `_win.c` counterpart, no `_WIN32` guards anywhere in the
file, and `build_entity_menu.sh` has no Windows variant. This is the file
that received the bulk of this session's real work (opacity fixes, drag
support, RMMV click-capture, Debug HQ category). **Practical effect: none of
db-hq/events-hq/chat-hai/Settings/palettes/popup windows are known to build
or run on Windows right now** — this predates this session (the file itself
is older), but the gap has only grown since the last Windows pass, since
every new feature this session landed exclusively here.

Action needed: either port a `khtpm_entity_menu_render_win.c` (same shape as
the strip's own Win32 twin), or confirm/document that Windows intentionally
doesn't get these windows yet and scope that explicitly in `INDEX.md`'s
Cross-Platform table instead of leaving it implicit.

## 2. New tile-picker ops are Linux/X11-only, no `_WIN32` branches

Built/rewritten this session, all real X11 (`Xlib.h`) with zero Windows
handling:
- `&.widgits/tile-picker/ops/tp_arm_placer_rmmv.c` — real `XCreateWindow`
  (InputOutput, `_NET_WM_WINDOW_OPACITY`, `XGrabKeyboard`) tiled
  click-capture overlay windows.
- `&.widgits/tile-picker/ops/tp_place_desktop.c` / `tp_place_desktop_rmmv.c`
  — rewritten today to resolve the active session/desk/pals tree via
  `opendir`/`readdir` (POSIX `dirent.h`) and shell out via `system("mkdir
  -p ...")`, `pgrep -f`, `setsid ... &`. None of these exist as Windows
  APIs; `khtpm_taskbar_manager.c`'s own equivalent resolvers
  (`livedesk_login_root`, `livedesk_sessions_root`, etc.) already carry
  real `#ifdef _WIN32` branches (`FindFirstFileW`, `win_spawn_cwd`,
  `MultiByteToWideChar`) for the exact same job - these two new ops did
  not get that treatment and would need it before a Windows pass.
- `*.monads/*.livedesk-taskbar/ops/tp_desktop_window_rgb.c` — real
  `WIN_PX`/opacity fixes landed this session; unclear whether
  `tp_desktop_window_win.c` (its apparent Windows twin) received the
  equivalent fixes — needs a diff pass, not assumed in sync.

## 3. Everything else that landed 2026-08-23 → 2026-08-29 (Linux-only, unverified elsewhere)

Large, unaudited body of work - listed by theme, not exhaustively, since a
full per-commit cross-platform audit is its own task:

- The full render/input refactor + entity-menu migration (frame-file paint,
  file-boundary input, marker-gated redraw, LayDoc→Elem port) -
  `GROM-RENDER-INPUT-REFACTOR-HANDOFF.md`, `RENDER-REFACTOR-2DO-PROGRESS.md`.
- events-hq Scratch/Blueprints/Common-Events work (multiple real bugfixes:
  missing `css_compute_style()` calls, nav-index collisions, drag guards).
- RMMV tile-picker + armed-brush placement end-to-end (this session's own
  earlier arc - TILE-SYSTEM-DESIGN.md §6 item 6).
- House-wide opacity fix (`_NET_WM_WINDOW_OPACITY`, `CWOverrideRedirect`
  mask fix, delayed re-apply pattern) across
  db-hq/events-hq/chat-hai/popup/entities/taskbar-dropdown.
- Settings/popup window drag support + close-button fix (event-mask gap).
- Tile-placement desk-persistence rewrite (today, see
  `TILE-PLACEMENT-DESK-PERSISTENCE-GAP-2026-08-29.txt`).
- Doc-audit passes 1/2 + archive-folder deletion (docs only, no code -
  lowest priority to re-check, but confirm no now-deleted doc is still
  referenced from a Windows/macOS-specific doc).

## Suggested next step

Before the next Windows or macOS leg: read this doc top to bottom, start
with item 1 (the missing renderer twin - highest impact, blocks the most
windows), then item 2 (the two new tile-picker ops), then spot-check item 3
against whatever's actually exercised on that platform pass.

Once ported/verified, fold the relevant findings back into
`8.21.GROK-win.md` / `LINUX_ROUNDTRIP.md` (whichever leg) and update this
doc's own status or retire it - this is meant to be a working backlog, not a
permanent doc.
