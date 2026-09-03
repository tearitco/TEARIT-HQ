---
name: khtpm-house-standards
description: Mandatory reading before touching any khtpm_core_render.c-family file, any *_manager.c/*_render.c pair, or any taskbar-launched window/menu in this house. Use whenever a task involves the khtpm renderer, a .chtpm file, a manager process, or CENTROID_GOLD_STD.md.
---

# khtpm/CENTROID house standards — read this before writing code

**Written 2026-09-01 after a real, concrete incident**: an agent (a
past instance of Claude, this same house) spent a full session
rewriting `network_browser_render.c` — hand-porting a `parse_chtpm()`
loader, hand-drawing `cli_io` state, reinventing nav-bracket logic that
already existed as a shared function (`elem_cursor_prefix()`) — while
the REAL, current, compliant version of that exact app had already
been built, live-tested, and was sitting unused in the same directory
(`button.sh`, launching the shared `khtpm_core_render.+x` against a
real manager). The agent had even read `button.sh` earlier in the same
session, wrote down what it did, and then kept fixing the wrong file
anyway. Root cause: it read `CENTROID_GOLD_STD.md` once and then
improvised, instead of working through the house's own living index.

**Do not repeat this.** Before writing or editing any code that
touches:
- `khtpm_core_render.c` or anything in `*.monads/*.livedesk-taskbar/ops/`
- any `<app>_manager.c` / `<app>_render.c` pair
- any `.chtpm`/`.css` file, or the generic `<cli_io>`/nav-index/
  layout-pass machinery
- any taskbar-launched window, menu, or HQ app

**you must read, in full, not skimmed**:

1. `#.#.calendar-dox/1.^V-hq/INDEX.md` — **the Tier 1 list**,
   specifically:
   - `CENTROID_GOLD_STD.md` (house root, `44.xyz❤️‍🔥️.../`) — the real
     rendering architecture rule.
   - `xperiments/khtpm-generic-dispatch-design.md` — **READ THE TOP OF
     THE FILE, not a description of it in another doc.** This is a
     living document with dated status updates; an older doc's summary
     of "what it says" can be stale the same day it was written (this
     happened — the `g_khtpm_modes[]` idea it originally proposed was
     rejected hours later, in the same doc, by the same date-stamp).
   - `TPMOS-COMPLIANCE-DEBT.md` — real, confirmed architecture
     violations, several involving this exact app family, including a
     near-identical mistake by an earlier agent instance. Its own
     **standing rule**: if a compliant pattern already exists for a
     sibling app, *stop and ask* whether to build the real thing now
     instead of patching around the gap — don't silently do the
     narrower literal thing just because that's what was asked.
   - `SKILLS.md` — general house operating judgment.

2. **Before touching a specific app**, grep the house for its own real
   launcher script (`button.sh`, `open_*.sh`) and read it fully — it
   may already point at a newer, compliant architecture than whatever
   `.c` file has the most obvious name. A launcher's own header comment
   documenting "this replaces the old X" is a real signal, not
   incidental color.

3. **A doc that cross-references another doc's status is not a
   substitute for opening that doc.** Treat any such summary as
   potentially stale the moment you read it. Open the referenced file
   and check its own latest dated entry at the top before acting on
   what it's assumed to say.

## The concrete, adopted answer for khtpm apps (as of 2026-09-01)

No per-app dispatch table, no `.so`/plugin loading, no linking to share
behavior across binaries — this house's standing rule everywhere, not
just here. Instead:
- A real, separate, compiled **manager** process owns business logic
  and publishes a plain-text or `.chtpm` projection.
- The **shared, generic** `khtpm_core_render.c` renders any app using
  its already-generic tag vocabulary (`window`/`panel`/`button`/
  `text`/`cli_io`/...) — zero new per-project C, ever again (this is a
  standing, explicit rule: no new `g_is_<project>` global, no new
  per-project dispatch branch in that file).
- Two real, generic capabilities make this sufficient for interactive
  apps: **live `.chtpm` re-parse on file change**
  (`reparse_chtpm_if_changed()`) and a **generic `<cli_io>` text-input
  element** (armed/typed/committed, with a real `XGrabKeyboard` while
  armed — required, or typing silently breaks once the pointer leaves
  the window under this desktop's focus-follows-mouse policy).
- A genuinely new, standalone app (not a mode of the shared renderer)
  is still real house convention when it's its own complete, small,
  self-contained binary (see `khtpm_choice_picker.c`-style precedent) —
  but it must still *actually parse* a real `.chtpm`+CSS through the
  real pipeline, never hand-build the `Elem` tree in C. There is no
  size/complexity exception to this — it was tried, and struck.

If you're about to write a bracket/nav-badge string, a `parse_chtpm()`
loop, or armed-input-state logic by hand: stop and check whether
`khtpm_render_core.c`/`khtpm_draw_core.c` (the shared, text-included
core files) already provide it. They usually do.

**Scoped nav / `[^]` `[>]` (2026-09-03):** do not compact `g_nav[]`.
Read
`yz.muchiverse/#.#.calendar-dox/!.HQ-IQ-BOOK/09-appendix/HANDOFF-scope-nav-and-chtpm-port.md`
**§9** before touching `kh_apply_scope_confine`, `kh_elem_in_scope`,
or nav badges. Edit `&.widgits/_shared-lib/khtpm_draw_core.c` (the
ops copy is overwritten on build). Running windows do not pick up a
rebuild until relaunch.
