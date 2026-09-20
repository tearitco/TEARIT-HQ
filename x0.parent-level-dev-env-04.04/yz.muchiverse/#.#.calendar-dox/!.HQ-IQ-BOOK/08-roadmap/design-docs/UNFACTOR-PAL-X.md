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
| 4 | `khtpm_entity.c` compile unit | this burst (include wrapper, not 8k cut) |
