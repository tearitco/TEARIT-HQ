# db-hq / events-hq / the rest: full port to template + projector

**Status:** design · **Date:** 2026-09-03
**Supersedes:** the `db-hq-pal` experiment (parked)
**Blocks nothing — this is the plan to remove the debt**

---

## 1. The debt

`khtpm_core_render.c` is **18,506 lines**. A huge slice is per-app C that
builds Elem trees and handles input, switched on `g_is_*` flags:

| prefix | refs | what it is |
|---|---|---|
| `dbhq_*` | 435 | db-hq: tabbar, 15-tab list/detail, actor stat panels, the embedded Common Events command editor, scoped nav |
| `evhq_*` | 235 | events-hq: view sub-tabs, command list, the Add-Command picker, Play |
| `pchq_*` | 138 | piececraft-hq board mode (out of scope here) |
| `g_is_db_hq` (48) / `g_is_palettes` (35) / `g_is_events_hq` (30) / `g_is_stats_hq` (23) / `g_is_bookmarks` (20) / `g_is_swatch_picker` (18) | | mode gates |

Every one of these is **layout expressed as C** — the exact thing
`CHTPM-ARCHITECTURE-FIX.md` set out to kill. `db-hq-pal` tried to sidestep
it with a parallel `class="db-hq-pal"` window and ended up as a
half-feature that punts Common Events back to the old window. Dead end.

## 2. The reframe (why the renderer barely changes)

Think of it exactly like browser tabs in HTML/CSS/JS:

| browser | here |
|---|---|
| `<div class="tabs">` headers + `<div class="pane">` bodies | `<tabbar><tab></tabbar>` + content regions in the `.xhtpm` |
| CSS hides every pane but `.active`; underlines the active tab | `show="${is_<tab>}"` drops the inactive regions from the tree (already implemented); `<tab class="active">` when its id matches `g_default_active_tab_id` (already implemented) |
| JS onclick: clear old active, hide old pane, set new active, show new pane | a `<tab>` click sets `g_default_active_tab_id` (one string, the "checked radio") and runs its `action=` which tells the projector which tab is live; the projector emits `is_actors=1 / is_items=0 / …` (already implemented) |
| a modal / popup (colour picker, file dialog) | `<panel class="overlay" show="${picker_open}">` with a `<repeat>` of choices — a normal gated region, driven by the projector |

**Net renderer work:** essentially what's already on branch
`chtpm-var-substitution` (`${var}`, `<repeat>`, `show=`, `<tabbar>/<tab>`,
`ACTIVATE`/`[^]`/`Esc` scoped nav, content-hashed reparse). Plus a
handful of small, generic additions listed in §4. **No per-app C.**

## 3. Target architecture — one shell module + one module per tab

chtpm is meant to carry **multiple `<module>` tags**, the way an HTML
page carries multiple `<script src>`. One is the view/shell; the rest
are per-tab logic, isolated like JS modules — add or drop a tab by
adding or dropping a `<module>` + its region, touching nothing else.

```
khtpm_core_render.+x        ONE generic renderer. Modes: strip/dock,
                            tile, entity-menu, window(sidebar/panel/
                            tabbar). NO g_is_<app> flags, NO dbhq_*/
                            evhq_* functions. Forks EVERY <module>,
                            tracks all their pids, cleans them all up.

<app>/<name>.xhtpm          static template:
  <module src="…/shell.pal"/>          the view/shell
  <module src="…/tab_actors.pal"/>     per-tab logic
  <module src="…/tab_common_events.pal"/>
  ...
  <window vars="state/shell.txt state/tab_actors.txt state/tab_common_events.txt …">
  <tabbar>… <sidebar>… <panel>…    all regions, gated by show="${is_<tab>}"

<app>/<name>.css
<app>/ops/<name>_action.sh  the one write path (tab / select / edit /
                            add-command / play / picker-pick …)
```

- **shell module** owns tab state: reads `state/active.pdl` (written by
  `<name>_action.sh` on a tab click), writes `state/shell.txt` —
  `active_tab`, `is_actors=1 / is_items=0 / …`, window title.
- **each tab module** owns its pane: writes `state/tab_<x>.txt` with
  that tab's `${var}` / `<repeat>` data. Cheap idle no-op while its tab
  isn't active (checks `active_tab` in `state/active.pdl`, returns).
  Common Events' module is the command editor: reads the selected
  event's `event.ir.pdl`, emits the command-list rows + picker choices,
  and its `action.sh` shells to `khtpm_events_hq_manager` compile/run.
- **multi-`vars=`**: `vars="a.txt b.txt c.txt"` — the renderer loads
  each into the var table (later files add/override). One file per
  module, zero write contention.

Projector language is free per module — `.pal` via the shared `prisc+x`
(string opcodes already added) **or** a compiled `.+x` in C.

The event-compile chain (`event.ir.pdl → event.pal → cmd_N.sh`,
`compile_page()` in `&.widgits/events-hq/ops/khtpm_events_hq_manager.c`)
**does not move** — it is already ops-side data + code. The Common
Events / events-hq projector shells out to it exactly like the old C
did, via an action script.

## 4. Generic renderer additions still needed

Small, each reusable by every window — NOT per-app:

| # | capability | for | size |
|---|---|---|---|
| 0 | **fork every `<module>`, not just the first** — loop over the `<module>` children, `launch_module()` each, keep `pid_t g_module_pids[N]`, kill/wait all in `cleanup`. Plus **multi-`vars=`**: split the attr on spaces, `kh_load_vars` each in order. | the shell + per-tab module architecture in §3 | ~30 + ~10 lines |
| 1 | **`<repeat>` v2** — per-row element choice. In one `<repeat>` body, several candidate elements each with `show="${row.is_X}"`; the tab module sets exactly one per row. Covers heterogeneous lists. | events-hq command list (each row a different command shape), network_browser page content, db-hq detail rows that mix `<text>` + `<cli_io>` | ~40 lines in `kh_expand_repeats` |
| 2 | **editable detail rows** — `<cli_io>` inside a `<repeat>`, `target_id="${row.key}"`, value `${row.value}`; on Enter the renderer runs the row's `action=` with the typed text. (Mechanism already exists for standalone `<cli_io>` — just confirm it works inside `<repeat>`.) | RPG-Maker field editing (actor name/stats, item price, …) | verify only |
| 3 | **overlay region** — `<panel class="overlay">` drawn on top, centred, dims the rest; nav confined to it (reuse the `ACTIVATE` scope machinery); `Esc` closes. Purely a CSS class + the existing scope confinement. | the Add-Command picker, any future modal | ~30 lines (mostly the dim + centre in layout) |
| 4 | **nested `<repeat>`** (one level) | events-hq: pages → commands | ~30 lines |
| 5 | delete `dbhq_*` / `evhq_*` / their `g_is_*` gates | — | removes ~1,300 lines |

That's the whole renderer budget. Everything else is per-tab modules.

## 5. Per-app projector work

Ordered easiest → hardest. Each: author `<name>.xhtpm` + `<name>.css`,
write the projector, wire `ops/<name>_action.sh`, verify with
`khtpm_png_dump.sh`, launch in parallel as `class="<app>"` (a fresh
class, not the old one), swap the launcher only after sign-off, then
delete the old C.

| app | state it reads | notes |
|---|---|---|
| **swatch-picker / taskbar-settings** | `taskbar_settings_state.txt` | flat list + `OPACITY_MINUS/PLUS` items. Smallest. |
| **bookmarks** | `bookmarks_state.txt` | one `<repeat>` list + New / Open Folder items; `backspace_action` for delete (generic already). |
| **stats-hq** | `stats_summary.txt` | read-only `<repeat>` of stat rows. |
| **palettes** | `palettes-*_state.txt` | `<repeat>` grid of `sprite=` tiles (generic `draw_elem` sprite support already exists); category tabbar; scroll arrows are generic. |
| **db-hq (14 list tabs)** | `#.desktop/db_hq_<x>.state.txt` | exactly `db-hq-pal` today: tabbar + `<repeat>` record list + detail. Make the detail rows **editable** (`<cli_io>` per field, capability #2) → the projector writes edits back to the `.pdl`. |
| **db-hq Common Events tab** | `db_hq_common_events.state.txt` (name list) + per-event `event.ir.pdl` | the editor pane: `<repeat>` of command rows (v2, capability #1), `+ Add Command` `<item>` → opens the picker overlay (capability #3) whose choices are a `<repeat>` over the command registry, `Play` `<item>` → `ce_action.sh play` → shells to `khtpm_events_hq_manager` compile+run. Nested `<repeat>` if pages are shown (capability #4). |
| **events-hq** | `events_hq_frame`/page state + registry | same editor as CE (it *is* the standalone version). Convert once, db-hq's CE pane reuses the same template fragment + projector logic. **Implementation spec:** `EVENTS-HQ-XHTPM-PORT.md` (2026-09-03). |
| **network_browser** | `page_state.txt` | heterogeneous content list → capability #1. Sprite-grid wrapping via a `<repeat wrap-class=…>` extension, or accept lone tiles first cut. |
| **chat-hai** | `state/sessions/*.ledger` etc. | drop the bash `chat_hai_projector.sh`; `.pal` or `.c`. Session list + transcript = two `<repeat>`s. |

## 6. Migration procedure (per app, safe)

1. Branch off `chtpm-var-substitution` (or its successor).
2. Build the generic capability the app needs (§4) if not already there;
   verify **every existing window** still dumps identically headless.
3. Author `<name>.xhtpm` + `.css` + projector + `action.sh`.
4. Launch it in parallel (`class="<app>-v2"`), dump, diff against a dump
   of the old window, iterate until pixel-and-nav identical.
5. Owner sign-off on the parallel window.
6. Point the real launcher (`open_db_hq.sh` / `button.sh` / the
   `livedesk:*` verb) at `<name>.xhtpm`; drop the old `class`.
7. **Delete** the app's `*_*` C from `khtpm_core_render.c` + its
   `g_is_*` gate + its `*_manager.c` if it only generated markup.
8. Rebuild, full headless regression, commit.

Common Events / events-hq is the one that must land together (shared
editor). db-hq's 14 list tabs can land before CE.

## 7. What "done" is

- `khtpm_core_render.c` has zero `dbhq_`, `evhq_`, `g_is_db_hq`,
  `g_is_events_hq`, `g_is_palettes`, `g_is_bookmarks`, `g_is_stats_hq`,
  `g_is_swatch_picker` symbols. (Target ~5k lines lighter.)
- Every window is `<name>.xhtpm` + projector. `grep -rl 'fprintf.*"<window' --include=*.c` returns nothing but the legacy `chtpm_parser_pal.c` family.
- `CENTROID_GOLD_STD.md` documents: static template + projector
  (`.pal` or `.c`) + `action.sh`, one generic renderer.

## 8. Rough effort

- Renderer capabilities §4 (1,3,4 + verify 2): ~1 focused pass.
- Projectors: swatch/bookmarks/stats/palettes ~½ day each; db-hq 14
  tabs ~1–2 days (editable fields); CE + events-hq shared editor ~3–5
  days (the picker + compile wiring is the real work); network_browser
  ~1–2 days; chat-hai ~½ day.
- Deleting the C + regression: ~1 day.

The parser is **not** the bottleneck. The work is writing ~9 projectors
and then deleting code.

---

## 9. Renderer capabilities — actual status (2026-09-03, branch `chtpm-var-substitution`)

Turned out much smaller than §4 estimated:

| # | status | commit |
|---|---|---|
| 0 | **DONE** — fork every `<module>`, track all pids, cleanup all; `vars="a b c"` multi-file, content-hashed reparse | `2394fd0d` chain (`kh_launch_window_modules`, `kh_load_vars_multi`, `kh_files_hash`) |
| 1 | **FREE** — `<repeat>` v2 (heterogeneous rows) already works: put several `show="${row.is_X}"` candidates in one body, the projector sets one `${row_i_is_X}=1`, `show=` drops the rest | no change |
| 2 | **DONE** — `<cli_io>` rows lay out + nav-number inside a `<scrolllist>` (editable field lists) | `38a3da4d` |
| 3 | **DEFERRED to the CE editor** — `class="overlay"` (centre + dim + auto-scope + Esc). Not needed by any simple app; build it while doing the Add-Command picker. |
| 4 | **PARTIAL** — nested `<repeat>` expands structurally (depth-matched close, up to 5 passes); `${outer.#}`/`${inner.#}` resolve. **Page-scoped data across a nested list wants the flat pattern instead**: projector emits one flat `<repeat>`, marks page boundaries with a `show=`-gated header row. | `<repeat>` depth-match commit |

**So the generic renderer is ready for every simple app and db-hq's 14
list tabs right now.** Only the CE/events command editor needs more
(capability #3 + the compile-chain action wiring), and that is its own
task.

## 10. Haiku-ready task shape (simple apps)

For **swatch-picker**, **bookmarks**, **stats-hq**, **palettes** — each:

1. Read this doc + `CHTPM-ARCHITECTURE-FIX.md` §8 + `string-ops.md`.
2. Reference: `&.widgits/db-hq-actors-pal/` — a complete, working
   PAL-projector app (`.xhtpm` + `pal/*.pal` + `ops/*_action.sh` +
   `button.sh` + `toy.pdl`).
3. Build `&.<area>/<app>-v2/` (fresh `class="<app>-v2"`, NOT the old
   class — the old window must keep working for A/B):
   - `<app>.xhtpm` — static: sidebar+panel (+ `<tabbar>` for palettes'
     categories). Every dynamic bit `${var}` / `<repeat>` / `show=`.
   - `pal/<app>_projector.pal` OR `ops/+x/<app>_projector.+x` — read the
     app's existing state file(s) (see the table in §5), write
     `state/ui.txt` key=value, ONLY on change (content differs).
   - `ops/<app>_action.sh` — the one write path (select, toggle, edit…).
   - `button.sh` (copy `db-hq-actors-pal/button.sh`, adjust names),
     `toy.pdl`.
4. Verify headless every iteration:
   `sh &.widgits/_shared-lib/ops/khtpm_png_dump.sh <app>.xhtpm <house> /tmp/o`
   then diff `/tmp/o/<app>.frame.txt` against a dump of the old window.
5. Do NOT touch `khtpm_core_render.c` or `khtpm_taskbar_manager.c`.
   Do NOT wire it into any launcher. Stage only files under your
   `<app>-v2/` dir. Report the frame.txt diff.
