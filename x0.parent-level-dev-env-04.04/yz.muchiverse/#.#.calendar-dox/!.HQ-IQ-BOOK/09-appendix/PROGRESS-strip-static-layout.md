# PROGRESS — taskbar off layout-updates (static layout + vars + frame)

**Branch:** `chtpm-var-substitution`
**Started:** 2026-09-03
**Status:** code landed, not live until strip relaunch (`run_khtpm_strip.sh run`).

## Goal

The livedesk taskbar (biggest live surface) must **not** rewrite its
`.chtpm` layout on every manager tick. That is the layout-update
abomination. Contract:

- **Layout** = static template (checked in, mtime almost never moves)
- **Data** = `#.desktop/strip_ui.txt` key=value (manager)
- **Paint** = existing frame dump (`entity_menu_frame_<pid>.txt`)

Same recipe as `db-hq-pal/dashboard.xhtpm` + `state/ui.txt`.

## Why this exists

`khtpm_taskbar_manager_main.c` `publish_live_chtpm()` currently
`write_small_file`s the **entire** `<window>…<item>…` tree to:

- `#.desktop/strip_header.chtpm`
- `#.desktop/strip_bottom.chtpm`

`run_khtpm_strip.sh` launches the renderer on those generated files.
Datetime is baked into a cell **label** in the layout. Every
`publish_state()` is a layout change → `reparse_chtpm_if_changed()`
because the **template** mtime moved.

A unused-correct template already exists:
`*.monads/*.livedesk-taskbar/khtpm_strip_header.chtpm` with `${username}`
etc. — but it uses the old strip-parser `<panel>/<button>` tags, not the
live dock vocabulary (`<window class="dock-header">` / `<item class="dock-cell">`).

Var **fragments** (`strip_var_tabs.txt` as raw `<button>` markup) are
also the wrong shape for generic `${var}` (KH_VAR_VALUE is 2048; lists
belong in `<repeat>`).

## Plan

1. Manager writes **one** `#.desktop/strip_ui.txt` (key=value rows,
   `<repeat>` fields: `n_tabs`, `tab_0_label`, …, `n_hqitems`,
   `n_hqwins`, `n_shortcuts`, labels, sprites, onclicks).
2. Static `khtpm_strip_header.xhtpm` + `khtpm_strip_bottom.xhtpm` next
   to the old `.chtpm` files, `vars="#.desktop/strip_ui.txt"`.
3. Stop calling `publish_live_chtpm()`. Keep `publish_var_fragments()`
   for the ASCII strip helper until that is retired.
4. `run_khtpm_strip.sh` + `g_dock_peer_path` point at the static
   templates. Renderer resolves `#.desktop/…` vars paths against
   `g_house_root`.
5. Clock: republish ui.txt when the formatted datetime **string**
   changes (minute), not by rewriting layout.

## Log

### 2026-09-03 — kickoff

- Diagnosed live `#.desktop/strip_header.chtpm` as full codegen
  (`publish_live_chtpm` ~line 500 of `khtpm_taskbar_manager_main.c`).
- Confirmed generic parser already has `${var}` + `<repeat>` +
  content-hash reparse on the vars file (`reparse_chtpm_if_changed`).
- This file created so a cutoff does not lose the plan.

### 2026-09-03 — code (not yet relaunched)

Built `khtpm_taskbar_manager_main.+x` + `khtpm_core_render.+x`.

- `publish_strip_ui()` writes `#.desktop/strip_ui.txt` (key=value +
  `<repeat>` fields). `publish_live_chtpm()` `#if 0`.
- Static templates:
  `*.monads/*.livedesk-taskbar/khtpm_strip_header.xhtpm`
  `khtpm_strip_bottom.xhtpm`
  `vars="#.desktop/strip_ui.txt"` (renderer resolves `#.` against house_root).
- `g_dock_peer_path` = sibling `khtpm_strip_bottom.xhtpm` (not generated).
- `run_khtpm_strip.sh` waits on `strip_ui.txt`, launches header.xhtpm.
- Clock: republish when formatted datetime string changes.

**Live 2026-09-03 11:19:** `sh …/ops/run_khtpm_strip.sh run`

Killed old strip 231153/231151. New: manager **262353**, renderer **262368**
on `khtpm_strip_header.xhtpm` (not `#.desktop/strip_header.chtpm`).
`strip_ui.txt` has username/datetime/n_tabs=7. Frame dump shows all 15
header cells with substituted labels; bottom dump shows 7 tab items from
`<repeat>`. Generated `strip_header.chtpm` mtime stayed **11:04** (stale).

Recipe (from house `44.xyz.01.00`):
```
sh "*.monads/*.livedesk-taskbar/ops/run_khtpm_strip.sh" run
```
That script rebuilds, kills the previous strip pair, waits for
`strip_ui.txt`, launches renderer on the static header template.

**If cut off:** next agent verifies those files, then dropdowns (HQ
ACTIVATE + `${n_hqitems}`), bottom tabs/shortcuts, FOCUSWIN cells,
cli_io show=. Pitfall: `onclick` and `action` are the SAME Elem field —
dropdown rows use one `hi_N_cmd`. Do not emit both attributes.
