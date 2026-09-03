# PROGRESS — taskbar off layout-updates (static layout + vars + frame)

**Branch:** `chtpm-var-substitution`
**Started:** 2026-09-03
**Status:** in progress — pass this doc to the next agent if cut off.

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

### next

Implement ui.txt publisher, templates, launcher, peer path, clock tick.
Do **not** `git add -A`. Do not kill the user's other apps except as
needed to relaunch the strip after a verified build.
