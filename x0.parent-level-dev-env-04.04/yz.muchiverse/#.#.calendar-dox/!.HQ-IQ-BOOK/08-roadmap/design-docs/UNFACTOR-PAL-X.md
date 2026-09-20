# Unfactor pal `+x` — one piece at a time (2026-09-18)

Restore the **already-IPC** desktop pal process. Placement/cli-io live
in `PLACEMENT-FROM-EXPLORER.md`, not here.

## What you should check after each piece

### After 1 (entity binary exists; HQ still `argc==2`)

Already landed. Check: `ops/+x/khtpm_entity.+x` exists. Desk pals
**still** start via `khtpm_core_render.+x <pal_dir>` — no behavior
change. HQ windows (File Explorer, strip, events-hq) still open.

### After 2 (spawners → `khtpm_entity.+x`)

**Pals:** open Cursword / a pet / book-stack from livedesk or
`button.sh`. `ps` should show `khtpm_entity.+x /path/to/pal` **not**
`khtpm_core_render.+x /path/to/pal`. Drag, right-click menu, z-layer,
Inventory drop still work.

**HQ (must not change):** File Explorer, taskbar strip, events-hq,
palettes — still `khtpm_core_render.+x <house> <file.xhtpm>`.

**Idempotent open:** second click on the same pal should not double
spawn (taskbar `pgrep` must see `khtpm_entity`).

### After 3 (HQ `argc==2` no longer `tp_main`)

**Pals still open** via entity binary.

**`khtpm_core_render.+x <something>` with only one path** should print
usage and exit — must **not** open a pal. Strip must still start
(`house` + `khtpm_strip_header.xhtpm`), including if a pal
`ensure_taskbar` runs.

### After 4 (`khtpm_entity.c` is the pal compile unit)

Rebuild `build_core_render.sh`. Both `+x` exist. Repeat **After 2**
checks. Source for pal `main` is `khtpm_entity.c` (may still
`#include` the engine `.c` until a later surgical split).

## Pieces

| # | What | Status |
|---|---|---|
| 1 | `-DKHTPM_ENTITY_BIN` → `khtpm_entity.+x` | done |
| 2 | Pal spawners use `khtpm_entity.+x` | this burst |
| 3 | HQ `argc==2` errors; strip spawn uses house+xhtpm | this burst |
| 4 | `khtpm_entity.c` compile unit | done (was an include wrapper; superseded by 5) |
| 5 | Real extraction: `tp_main` + pal-only code moved into `khtpm_entity.c`; shared bits in `_shared-lib/khtpm_ui_common.c` | done 2026-09-20 (5a `9398e034` common file, 5b entity cut) |

### After 5 (real cut, 2026-09-20)

Measured by reachability (`-ffunction-sections` + relocation graph from
`tp_main` vs the HQ `main`): the pal-only code was ~2.4k lines of functions
in the tile block plus five drag/drop/hex helpers and ~57 globals, not
6-8k. `khtpm_core_render.c` 19134 -> 12492 lines, `khtpm_entity.c`
6 -> 6322, new `khtpm_ui_common.c` 447. `khtpm_entity.+x` is 109KB (was
191KB) because it no longer carries the HQ engine.

- `khtpm_entity.c` includes only system headers and `khtpm_ui_common.c`; no
  Elem tree, CSS parser, or `khtpm_render_core.c`. Build line no longer
  passes the CSS parser.
- `khtpm_ui_common.c` (text-included by both, same convention as
  `khtpm_render_core.c`) = hq_ui.pdl loader with an `extra` hook for HQ-only
  keys, theme loader, UI-scale math, METHOD reader, menu launcher, globals.
- The entity keeps a local `dpy = NULL` (tile mode uses a local Display,
  see the tp_main globals footgun) and its own `entity_ui_pdl_reload_if_changed`
  (config reload + repaint flag; the HQ relayout half stays in core).
- The HQ `main()` no longer has the `KHTPM_ENTITY_BIN` branch.

Verified on a private Xephyr + private house root, pixel-diffed against the
pre-cut binaries (only the clock and a PID digit differ): pal opens via
`khtpm_entity.+x`, right-click menu (Chat/Events/Play/Inventory...), drag
writes `desktop_pos.txt`, drag over an open Inventory registers the drop zone
+ `drag_hover_pid.txt` and the drop moves the pal, File Explorer (Search,
sprite icons) unchanged, `khtpm_core_render.+x <one path>` prints usage.
Not re-verified: cursword 3D/phymoji camera keys, z-layer changes, XDND.

## Dock (2026-09-20)

The dock/strip is a separate unfactor track: `DOCK-UNFACTOR-AUDIT.md` (audit, stage table, results). Stages 1-2 done (dead code; `ktb_zorder_op.+x`); stages 3-5 blocked on generic engine features.
