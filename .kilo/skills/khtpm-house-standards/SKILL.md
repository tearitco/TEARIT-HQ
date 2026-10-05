---
name: khtpm-house-standards
description: Mandatory reading before touching any khtpm_core_render.c-family file, any *_manager.c/*_render.c pair, or any taskbar-launched window/menu. Use whenever a task involves the khtpm renderer, a .chtpm file, a manager process, or CENTROID_GOLD_STD.md.
---

# khtpm/CENTROID House Standards

**Written after a real, concrete incident**: an agent spent a full session rewriting a deprecated standalone renderer while the real, current, compliant version of that exact app already existed unused in the same directory. Root cause: it read `CENTROID_GOLD_STD.md` once and then improvised, instead of working through the house's own living index.

**Do not repeat this.** Before writing or editing any code that touches:
- `khtpm_core_render.c` or anything in `_.monads/_.livedesk-taskbar/ops/`
- any `<app>_manager.c` / `<app>_render.c` pair
- any `.chtpm`/`.css` file, or the generic `<cli_io>`/nav-index/layout-pass machinery
- any taskbar-launched window, menu, or HQ app

**you must read, in full, not skimmed:**

1. `x0.parent-level-dev-env-04.04/yz.muchiverse/#.#.calendar-dox/!.HQ-IQ-BOOK/02-architecture/CENTROID_GOLD_STD.md` — the real rendering architecture rule.
2. `02-architecture/xperiments/khtpm-generic-dispatch-design.md` — **READ THE TOP OF THE FILE**, not a description in another doc. It is a living document with dated status updates; an older doc's summary can be stale the same day it was written.
3. `08-roadmap/design-docs/TPMOS-COMPLIANCE-DEBT.md` — real, confirmed architecture violations, several involving this exact app family.
4. `x0.parent-level-dev-env-04.04/yz.muchiverse/#.#.calendar-dox/!.HQ-IQ-BOOK/01-orientation/SKILLS.md` — general house operating judgment.

**Before touching a specific app**, grep the house for its own real launcher script (`button.sh`, `open_*.sh`) and read it fully — it may already point at a newer, compliant architecture than whatever `.c` file has the most obvious name.

**A doc that cross-references another doc's status is not a substitute for opening that doc.** Open the referenced file and check its own latest dated entry before acting on what it's assumed to say.

## The concrete, adopted answer for khtpm apps

No per-app dispatch table, no `.so`/plugin loading, no linking to share behavior across binaries. Instead:
- A real, separate, compiled **manager** process owns business logic and publishes a plain-text or `.chtpm` projection.
- The **shared, generic** `khtpm_core_render.c` renders any app using its already-generic tag vocabulary (`window`/`panel`/`button`/`text`/`cli_io`/...) — zero new per-project C, ever again (no new `g_is_<project>` global, no new per-project dispatch branch).
- Two real, generic capabilities make this sufficient: **live `.chtpm` re-parse on file change** (`reparse_chtpm_if_changed()`) and a **generic `<cli_io>` text-input element**.
- A genuinely new, standalone app must still *actually parse* a real `.chtpm`+CSS through the real pipeline, never hand-build the `Elem` tree in C.

If you're about to write a bracket/nav-badge string, a `parse_chtpm()` loop, or armed-input-state logic by hand: stop and check whether `khtpm_render_core.c`/`khtpm_draw_core.c`/`khtpm_reparse_diff.c` already provide it. They usually do.

## Element identity across reparse

`reparse_chtpm_if_changed()` diffs the new parse against the live tree by key (`target_id` else `id`) and patches matched elements IN PLACE. **`content=` seeds a field's editable `input_buffer`; `label=` is only ever display text or a short prefix, never the same value as content=.** Putting real content in `label=` breaks Backspace and visibly DOUBLES the text on-screen. Do not add a new `kh_*_reload()` bolt-on — a new runtime-state field needs one line added to `kh_diff_apply_template()`'s preserve-list in `khtpm_reparse_diff.c`.

## Adding a layout branch to `khtpm_core_render.c`

Before adding any `layout_*` branch:
1. Grep for a sibling that already renders the same shape and route through it.
2. If you genuinely must add a branch, it MUST clip (never translate off-screen children), not add key handling, and not write a `g_*_scroll` directly.
3. `assign_nav_and_layout()` runs many times per frame — every mutation must be idempotent.

## Driving/testing a khtpm window: use the relay, not xdotool

Direct instruction: drive windows via relay file injection, not `xdotool`. The mechanism already live in every `khtpm_core_render.c` window (`poll_agent_history()`/`history_path()`):

```
#.desktop/entity_menu_history/<pid>.txt
```

One line per event, appended:
- `KEY_PRESSED: <decimal>` — printable ASCII 32-126; `13`=Enter, `27`=Escape, `8`=Backspace, `9`=Tab; `200`/`201`/`202`/`203`=Up/Down/Left/Right
- `MOUSE_EVENT: <button> <x> <y> <is_press>` — real clicks/wheel

Find the PID from `ps aux | grep khtpm_core_render` or the window's own `module_parent.pid`.

**Before sending ANY digit for nav-jump, dump the frame first and read the ACTUAL rendered nav numbers.** Nav numbering is global/unified across every concurrently-open khtpm window, not reset to 1 per window.

Order of preference, strict:
1. The relay file above
2. A cheap text state read
3. `dump_frame_png_op.+x <window-id> <out.png>`
4. `xdotool`/XTest — last resort only
