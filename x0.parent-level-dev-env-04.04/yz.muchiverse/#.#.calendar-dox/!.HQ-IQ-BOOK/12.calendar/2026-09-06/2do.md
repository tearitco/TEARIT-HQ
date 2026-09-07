# 2026-09-06

## Done today

- **Periodic-table element picker — Down-arrow bug fixed** (live report:
  pressing Down scrolled the window chrome off the top + moved the
  scrollbar thumb the wrong way). Root cause: a brand-new
  `<sidebar style="display:flex">` scroll branch in
  `khtpm_core_render.c` that "scrolled" by *translating* the whole
  laid-out grid subtree (`kh_shift_subtree`) every frame instead of
  *clipping* it — off-fold tiles then painted up over the pinned
  chrome — plus a bespoke `XK_Up/XK_Down` handler that wrote
  `g_default_sidebar_scroll` directly, one direction only. All of it
  re-implemented scroll/nav machinery the shared file already has
  (`layout_scroll_region` / the swatch-grid path, which
  `palettes-rmmv.xhtpm` already uses for the same shape).
  - Fix (`983c4649`): rewrote the flex branch to the existing pattern —
    clip by parking off-fold tiles at `y=-100000`, nav-number only
    visible tiles; **deleted** the bespoke key handler + the
    `g_default_poe_cols` global. Scroll is now `Page_Up/Down` + the
    generic_sbar `^/v` arrows + wheel, same as every other region.
    Kept the generic `flex-wrap` support in `css_layout_pass` (real,
    reusable, OS-free). Verified headless: header stays pinned,
    PageDown scrolls the grid *down*.
  - Docs: `03-pitfalls/HOUSE_CODE_PITFALLS.md` #14 (translate-vs-clip;
    reuse the existing scroll path), `02-architecture/RENDERER-
    MODULARITY-AND-PERF-AUDIT.md` addendum 2026-09-06 (new standing
    audit item: `layout_*` reuse-check reflex), skill
    `khtpm-house-standards` "adding a layout branch" section
    (`a71086e8`), auto-memory `khtpm-shared-layout-caution` 4th
    incident.
- **House `.7z`** re-made: `x0.parent-level-dev-env-04.04_20260906-125054.7z`
  (79 MiB, OK). Previous timestamped archive deleted.

## Git / branches — newbie confusion cleared up + `main` unified

Context: user is a git beginner, nervous about losing work, unclear
why two agents (`claude` = this one, `opencode`) are on separate
branches + separate checkout folders (`NNEST-12.00` /
`NNEST-12.00-opencode`, git *worktrees*). Wanted both agents' work on
`main` and confirmation that we collaborate, not compete.

- **The branches are not competition** — they're save-lanes so two
  automated tools don't clobber each other's *uncommitted* files. The
  worktrees make a cross-branch `checkout` physically impossible (git
  refuses). `opencode` has in fact been merging `claude`'s work into
  its own branch all along ("bidirectional sync").
- **State check:** every `claude` commit was already on `opencode`,
  which had 3 more on top (`f0559614` nb-js rung-4 network,
  `3b1fadcb` + `4290b4a0` sync-protocol docs). So `opencode` already =
  both agents' combined work.
- **Action (user ran it):** `git push origin opencode:main` —
  clean fast-forward, `584cbc0b..4290b4a0`. `origin/main` and local
  `main` now at **`4290b4a0`** = everything. (Undo point if ever
  needed: old `main` was `584cbc0b`.)
- `origin/claude` stays at `a71086e8` (all its work is in `main`; it
  just doesn't carry `opencode`'s 3 later commits — fine, picked up
  next time it's useful).
- **Going forward:** each agent commits only to its own branch and
  `push origin <own-branch>`; merges to `main` are the user's call
  (one-liner above). Each agent merges the other's latest before
  starting work (protocol written up in `01-orientation/GIT-WORKFLOW-
  FOR-BEGINNERS.md` + the `13.agent-coms/` notes).
- User plans to **delete the `NNEST-12.00-opencode` worktree** once the
  opencode leg is done, before the gaming work starts.

## Concern raised today — cross-platform (Win/Mac) handoff friendliness

User asked: the pre-doc-cleanup Windows/macOS docs — still around? And
does today's renderer work stay port-friendly for the eventual
handoff (no plan to build/test other platforms now).

- **Docs still exist**, untouched by the cleanup:
  - `07-install-and-ship/windows-mac/` — `WIN-COMPAT-RULE.md` (the
    house rule), `WIN-CONVERSION-STATUS.md`, `windows-house-guide.md`,
    `8.21.GROK-win.md` (current livedesk Win pass).
  - `08-roadmap/design-docs/CROSS-PLATFORM-PENDING-2026-08-29.md` —
    the working backlog of everything that landed on Linux (canonical)
    since the last Win (2026-08-21) / macOS (2026-08-23) pass.
- **The house rule (WIN-COMPAT-RULE.md), unchanged:** Linux is
  canonical; Windows stays *compatible* via thin `#ifdef _WIN32` shims
  (window create/events, process spawn/kill, path sep + binary
  suffix), **never** a parallel `*_win.c` that re-implements
  menus/nav/layout. No absolute `/home/no/...` load-bearing paths.
- **Where today's file sits:** `khtpm_core_render.c` is the file
  `CROSS-PLATFORM-PENDING` item #1 flags — the canonical merged
  renderer for ~8 window modes, with **no Windows twin and zero
  `_WIN32` guards** (the taskbar *strip* has one — `khtpm_strip_x11_win.c`,
  a Win32 GDI reimpl of the Xlib/Xft subset — this file never got the
  same). That gap predates today and is the biggest single porting
  item. (Note: that doc still calls the file by its old name
  `khtpm_entity_menu_render.c` — it was renamed since; worth a
  one-line fix in the doc on the next pass.)
- **Is today's change port-friendly? Yes — net positive.** It *removed*
  a bespoke layout branch, a custom key handler, and a global, and
  routed the remaining path through shared, OS-free helpers
  (`css_layout_pass`, `generic_sbar_register`, the generic
  `Page_Up/Down` handler). No new Xlib calls, no new syscalls, no new
  paths, no `#ifdef`. Pure geometry/nav math. The one earlier addition
  in this arc (`flex-wrap` in `css_layout_pass`, in the shared-lib
  `khtpm_render_core.c`) is also pure layout math, no OS surface.
- **To make the eventual handoff friendlier (not scheduled, just
  captured):**
  1. Keep new drawing/layout/nav logic in the OS-free shared functions
     (`css_layout_pass`, `layout_*`, nav math) rather than in the X11
     draw/event code — every line there is a line the port doesn't
     have to shim. Today followed this; keep it a habit.
  2. The real unlock is still `CROSS-PLATFORM-PENDING` #1: give
     `khtpm_core_render.c` the same treatment the strip got — either a
     `_win.c` Xlib-subset shim or `#ifdef _WIN32` around its ~dozen
     Xlib entry points (window create, event loop, `XGrabKeyboard`,
     Xft text, `_NET_WM_WINDOW_OPACITY`). Big item, own task.
  3. New manager processes (e.g. `elements_palette_manager.c`) use
     `system("sh …")`, `opendir`, POSIX paths, `usleep`. The taskbar
     manager already has the `#ifdef _WIN32` pattern for the same jobs
     (`FindFirstFileW`, `win_spawn_cwd`, `MultiByteToWideChar`) — new
     managers should copy that pattern, or at least keep the
     Linux-isms in one clearly-marked spot so the port is mechanical.
  4. Each new `.sh` action shim (e.g. `elem_click.sh`) needs a `.ps1`
     sibling eventually — the pattern exists (`button.ps1`,
     `EMERGENCY_CLOSE.ps1`); keep the `.sh` tiny and logic-free so the
     `.ps1` is a trivial mirror.
  5. When a Win/macOS pass does happen: `CROSS-PLATFORM-PENDING` is the
     read-first doc; add today's picker + this renderer change to its
     item #3 list so it isn't missed.

## Cross-platform infra — design doc written + Linux piece shipped

Decision (user): write the scoped design doc AND do the Linux-verifiable
piece now; put the Windows seam guidance somewhere the porter will
actually read.

- **`08-roadmap/design-docs/CROSS-PLATFORM-SEAM-AND-SHARED-INFRA.md`** —
  new. Three parts: (1) `kh_plat` shared manager module, (2) renderer
  platform seam = **guidance only** for the eventual Win/Mac porter
  (Part 2 is written *to* them, top of the doc points there), (3)
  generic manager runtime idea. Bakes in the cautionary tale: the
  house already built a renderer seam once (`khtpm_core.c` /
  `khtpm_plat_x11.c`, orphaned header still in `tile-picker/ops/`) and
  it **rotted because nothing consumed it** — so Part 2 is explicitly
  not-yet-built.
- **`&.widgits/_shared-lib/kh_plat.h`** — new, header-only
  (`#define KH_PLAT_IMPL` in one TU). v1 API: `kh_plat_sleep_ms`,
  `kh_plat_mono_ms`, `kh_plat_mkdir_p`, `kh_plat_run_house_script`,
  `kh_plat_on_terminate`. POSIX backend real; Windows backend real
  where trivial (`Sleep`/`GetTickCount64`/`_mkdir`/`SetConsoleCtrlHandler`)
  + one documented stub (`run_house_script`, pending the house's
  bundled-`sh.exe`-vs-`.ps1` call — spelled out in the doc's PORTER
  NOTE).
- **`elements_palette_manager.c`** migrated onto `kh_plat` as the proof
  consumer — removed its raw `usleep` / `clock_gettime` / `mkdir` /
  `signal` / `system("sh …")`. Rebuilt clean; headless-verified: picker
  still renders, inspect click still populates the detail pane, and a
  rapid double-click still fires the place action
  (`msg=Placed Silver (Ag) on the desktop.`) through
  `kh_plat_run_house_script`. **Taskbar manager left untouched** (its 49
  inline `#ifdef _WIN32`s are the v2 migration).
- Not done, by design: renderer seam (no consumer yet), `kh_plat` v2
  (`listdir` + `fopen_utf8`), taskbar-manager migration, `.sh`/`.ps1`
  policy decision, generic manager runtime. All sequenced in the doc.

## Not started / next

- Fun gaming work (user flagged as the next thing after the opencode
  leg wraps + its worktree is deleted).
- Everything still open from `2026-09-05/2do.md` (fullscreen relayout
  bug, digit-accum taskbar check, toys editors follow-ups, FLEXFOLDER
  brainstorm, etc.).
