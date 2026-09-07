# Cross-Platform Seam + Shared Infra — design + status

**Started 2026-09-06.** Scope: make the eventual Windows / native-macOS
handoff mechanical instead of a rewrite, and stop every manager
re-implementing the same OS glue. Linux stays canonical throughout.

> **If you are the Windows or macOS porter reading this for the first
> time: skip to Part 2.** It is written for you. Parts 1 and 3 are
> Linux-side refactors that reduce what you have to touch.

---

## Ground rules (unchanged, from `07-install-and-ship/windows-mac/WIN-COMPAT-RULE.md`)

- Linux is the product. Windows/macOS stay **compatible via thin
  shims**, never a parallel feature tree.
- OS differences isolated behind: window create / event pump / draw
  primitives / process spawn+kill / path sep + binary suffix. Nothing
  else.
- No new absolute `/home/no/...` load-bearing paths — house-relative
  only.
- Reference pattern of truth: `egg_window.c` (`#ifdef _WIN32` vs POSIX
  behind one API), and — proven in this house — the livedesk strip's
  `khtpm_strip_x11_win.c`: *"Win32 implementation of the Xlib/Xft
  subset used by `khtpm_strip_parser.c`"*. Same shared parser/layout
  logic, one small Win32 file supplying the primitives.

## The cautionary tale — do NOT build a seam ahead of a real consumer

The house **already built** a clean renderer platform seam once:
`khtpm_core.c` + `khtpm_plat_x11.c` + `khtpm_plat_win.c` +
`khtpm_plat.h` (the header still sits, orphaned, in
`&.widgits/tile-picker/ops/khtpm_plat.h`). The X11 backend was written
but **never wired into the live Linux build** — it sat unreferenced,
bit-rotted, and was confirmed dead + archived 2026-08-11
(`WIN-COMPAT-RULE.md` "KHTPM entity layout" section).

**Lesson baked into this plan:** a seam only stays honest when a second
backend actually compiles against it. Therefore:
- Part 1 (`kh_plat` for managers) ships now, because a Linux consumer
  uses it immediately — de-duping is real Linux value even if Windows
  never happens.
- Part 2 (renderer seam) is **guidance only** until a Windows/macOS
  pass is actually scheduled. Writing the abstraction now, Linux-only,
  would repeat the 2026-08-11 rot exactly.

---

## Part 1 — `kh_plat`: shared platform module for manager processes  ·  STATUS: v1 shipped (Linux) 2026-09-06

### Problem
`khtpm_taskbar_manager.c` alone carries **49** `#ifdef _WIN32` blocks —
`win_spawn_cwd()`, a UTF-8 `fopen` wrapper, `FindFirstFileW` directory
iteration, `Sleep` vs `usleep`, etc. Every *other* manager
(`elements_palette_manager.c`, `palettes_manager.c`, the HQ managers,
future ones) either re-invents these or is simply Linux-only. A new
contributor writing a manager should never type `#ifdef _WIN32`.

### Solution
One header-only module, `&.widgits/_shared-lib/kh_plat.h`, with inline
POSIX and Win32 implementations behind `KH_PLAT_IMPL` (same
text-include idiom as `khtpm_render_core.c`). A manager does:

```c
#define KH_PLAT_IMPL
#include "../../_shared-lib/kh_plat.h"
```

and calls portable functions instead of raw syscalls.

### v1 API (shipped)
| function | POSIX | Windows |
|---|---|---|
| `kh_plat_sleep_ms(int ms)` | `nanosleep` | `Sleep` |
| `kh_plat_mono_ms(void)` → `long long` | `clock_gettime(CLOCK_MONOTONIC)` | `GetTickCount64` |
| `kh_plat_mkdir_p(const char *path)` | recursive `mkdir(,0777)` | recursive `_mkdir` (TODO: `_wmkdir` + UTF-16 for emoji paths) |
| `kh_plat_run_house_script(house_root, rel_script, argv[], argc)` | `sh 'HOUSE/rel' arg…` via `system` | **stub** — see porter note below |
| `kh_plat_on_terminate(void (*fn)(int))` | `SIGTERM`/`SIGINT`/`SIGHUP` | `SetConsoleCtrlHandler` |

### v1 consumer (proof)
`&.widgits/palettes/ops/elements_palette_manager.c` — migrated off raw
`usleep` / `clock_gettime` / `mkdir` / `signal` / `system("sh …")`.
Built + headless-verified: the periodic picker still renders and a
`CLICK`→place action still fires. **Taskbar manager deliberately not
touched in v1** (large, live, own migration later).

### TODO (not v1)
- `kh_plat_listdir(path, cb)` — port `khtpm_taskbar_manager.c`'s
  `FindFirstFileW` / `opendir` pair into here, then migrate that file.
- `kh_plat_fopen_utf8(path, mode)` — fold the taskbar manager's
  `host_fopen` (`MultiByteToWideChar` CP_UTF8 + `_wfopen`) so emoji
  house paths work on MinGW.
- Decide the **script policy** for `kh_plat_run_house_script` on
  Windows (below).

### PORTER NOTE — `kh_plat_run_house_script` on Windows
Managers spawn house `.sh` scripts (`palettes_menu.sh 'place' …`). On
Windows you have two options, pick one house-wide:
1. **Bundle a `sh.exe`** (busybox-w32 or the git-for-windows one) and
   keep running the `.sh` verbatim. Least code, one binary to ship.
2. **`.ps1` siblings** — every house `.sh` gets a `.ps1` mirror
   (pattern already exists: `button.ps1`, `EMERGENCY_CLOSE.ps1`).
   `kh_plat_run_house_script` swaps the extension and runs
   `powershell -File`. More files, no bundled binary. Keep every `.sh`
   tiny and logic-free so its `.ps1` is a trivial 1:1 mirror.

Until decided, the Windows branch of this function is a logged no-op
that returns non-zero — a Windows build links, managers run, only the
"place on desktop" side-effect is inert.

---

## Part 2 — renderer platform seam (`khtpm_core_render.c`)  ·  STATUS: guidance only, NOT started

**Read this before a Windows or macOS pass. Do not pre-build it.**

### What this file is
`*.monads/*.livedesk-taskbar/ops/khtpm_core_render.c` (~12k lines) is
the single shared renderer for ~8 window modes (db-hq, events-hq,
chat-hai, Settings/swatch-picker, palettes, generic popup, entity
menu, Debug). It has **no `_WIN32` guard anywhere** and no Windows
twin. (Note: `CROSS-PLATFORM-PENDING-2026-08-29.md` #1 still calls it
by its old name `khtpm_entity_menu_render.c` — renamed since.)

### macOS: already works via XQuartz
`build_core_render.sh` has a `Darwin` leg (`/opt/X11` paths). The
2026-08-23 macOS pass ran the house under XQuartz. **No seam is needed
for macOS to function** — only for a *native* Cocoa/Metal port, which
is not planned. If someone wants native macOS later, it reuses this
same seam with a third backend.

### The seam, when it's time
Do **not** fork the renderer. Do what the strip did:

1. Identify the Xlib/Xft surface this file actually calls. It is
   small and stable — window create/map/destroy, the event loop
   (`XNextEvent` / key / button / motion / expose / configure),
   `XGrabKeyboard`, pixmap/`XImage` present, draw rect/line/text,
   `XftTextExtents` font metrics, the `_NET_WM_WINDOW_OPACITY` atom,
   `CWOverrideRedirect`. Grep `X[A-Z]` / `Xft` in the file for the
   real list.
2. Put that surface behind one header (`khtpm_core_render_plat.h`) —
   mirror `khtpm_strip_x11_win.h`'s shape so the two seams look alike.
3. Linux backend = the existing calls, moved, unchanged. Verify every
   window mode still renders identically (headless frame dumps +
   `dump_frame_png_op.+x` before/after).
4. Windows backend = `khtpm_core_render_x11_win.c`, a Win32 GDI impl
   of that subset — same approach and, where possible, same code as
   `khtpm_strip_x11_win.c` (it already implements `Display`, `XEvent`,
   window + pixmap + text for the strip; extend, don't re-derive).
5. The parser, CSS engine, `css_layout_pass`, all `layout_*`, nav
   math, the frame-serialization paint model — **untouched, one
   copy.** Every window mode and every future feature rides that
   shared vocabulary for free.

### What makes this cheap or expensive
Cheap: the Xlib surface barely grows — new features are built from
`window`/`panel`/`text`/`cli_io`/layout, not new X calls. Every line
of drawing/layout/nav logic kept in the OS-free shared functions is a
line the port never sees. **House habit (reinforced by the 2026-09-06
periodic-picker fix): new rendering logic goes in `css_layout_pass` /
`layout_*` / nav math, never in the X11 draw/event code.**
Expensive: if between now and the port, per-mode logic leaks into the
event loop or the draw path. Audit for that (`RENDERER-MODULARITY-AND-
PERF-AUDIT.md`) before the seam.

---

## Part 3 — generic manager runtime  ·  STATUS: idea, not scheduled

Every manager repeats the same skeleton: parse `argv` (house root,
package dir, id), loop on a sleep, poll `<name>_action.txt` for
`seq=N` / `cmd=…`, publish `<name>_ui.txt` atomically (tmp + rename).
~300–400 lines of identical scaffolding per manager.

Sketch: `&.widgits/_shared-lib/kh_mgr.h` providing
`kh_mgr_main(argc, argv, &callbacks)` where callbacks are
`on_start(ctx)`, `on_cmd(ctx, cmd_str)`, `render_ui(ctx, FILE*)`. The
runtime owns the loop, the atomic publish, the action-file seq
tracking, `kh_plat_on_terminate`, and change-detection via marker-file
growth (house rule: never `st_mtime`). A new manager becomes ~100
lines of business logic.

Prerequisite: `kh_plat` v1 (done). Wants its own brainstorm doc before
it graduates — real questions: does every existing manager's
action/UI contract actually fit one shape, or are there 2–3 shapes;
how do multi-file publishers (rmmv: state + options + active) fit;
migration order.

---

## Sequencing

1. ✅ `kh_plat` v1 + one consumer (2026-09-06).
2. Migrate `palettes_manager.c` + the HQ managers to `kh_plat` as they
   are next touched (opportunistic, not a big-bang).
3. `kh_plat` v2: `listdir` + `fopen_utf8`, then migrate
   `khtpm_taskbar_manager.c` (retire its 49 inline `#ifdef`s).
4. Decide the Windows script policy (bundled `sh.exe` vs `.ps1`
   mirrors).
5. Only when a Windows/macOS pass is actually scheduled: Part 2 seam,
   Linux-refactor-then-verify first, Win32 backend second.
6. Part 3 (`kh_mgr`) after its own brainstorm.

## Related
- `07-install-and-ship/windows-mac/WIN-COMPAT-RULE.md` — the rule
- `07-install-and-ship/windows-mac/8.21.GROK-win.md` — current Win strip pass
- `08-roadmap/design-docs/CROSS-PLATFORM-PENDING-2026-08-29.md` — the delta backlog (Part 2 is its item #1 + #2)
- `02-architecture/RENDERER-MODULARITY-AND-PERF-AUDIT.md` — keep logic out of the draw/event path
- `&.widgits/tile-picker/ops/khtpm_plat.h` — the orphaned earlier seam attempt (do not revive as-is)
