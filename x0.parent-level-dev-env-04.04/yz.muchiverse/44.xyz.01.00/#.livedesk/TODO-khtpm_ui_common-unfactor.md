# RESOLVED: khtpm_ui_common.c Unfactor

**Status:** Resolved (2026-09-27)
**Original scope claim:** "Affects khtpm_entity.c AND khtpm_core_render.c" - **this was wrong**, see below.

## What Was Actually Wrong (vs. what this doc originally assumed)

This doc originally assumed `khtpm_ui_common.c` was a real, multi-binary
shared file (per its own then-header-comment: "text-included by BOTH
khtpm_core_render.c ... and khtpm_entity.c") and planned a header/config-
file split to de-duplicate it across binaries.

A grep audit before touching anything found that claim was **stale**:
`khtpm_core_render.c` never included `khtpm_ui_common.c` at all - only
`khtpm_entity.c` did. There was no real cross-binary duplication to fix,
just a single-consumer file living in a separate location for no live
reason.

## The Fix

Per direct instruction ("is there a reason the code isn't simply in the
same codefile? we like to keep uniform conventions for readability...
if its an exception lets make a note"):

- `khtpm_ui_common.c`'s entire content (globals, theme/scale loaders,
  `load_methods()`, `launch_khtpm_menu()`, etc.) is now inlined directly
  into `khtpm_entity.c`, at the same spot the old `#include
  "khtpm_ui_common.c"` line was.
- The file `&.widgits/_shared-lib/khtpm_ui_common.c` was deleted.
- Stale "shared with khtpm_core_render.c" comments in
  `build_core_render.sh` and `khtpm_grid_jump.c` (which referenced
  khtpm_ui_common.c as a shared-file example) were corrected.
- Rebuilt `khtpm_entity.+x` - identical binary size (117656 bytes)
  confirms nothing was lost or duplicated in the merge.

**`khtpm_ui_scale.c` was NOT touched** - it's a genuine exception: real
multiple consumers (khtpm_entity.c AND the placer ops, per its own header
comment), so it correctly stays a text-included shared file.

## House Rule Going Forward (the actual standing note requested)

- Code used by exactly **one** binary lives directly in that binary's own
  file. Do not create a separate file "for organization" if nothing else
  will ever include it - that's what functions/sections within the one
  file are for.
- Code used by **two or more** binaries has two legitimate house patterns,
  depending on what it is:
  - **Pure, no-I/O, no-X11 logic** (e.g. `khtpm_ui_scale.c`'s scale math,
    `khtpm_grid_jump.c`'s cell-jump parsing) - a real text-included
    canonical `.c` file, same copy compiled into each binary, no linking.
  - **Stateful behavior / orchestration** that could instead run as its
    own process - a separate compiled **op** + fork/exec + file-based IPC
    (state files/ledgers), same pattern as `move_entity_init.+x`/
    `move_entity_tick.+x` (see `PRISC-OPS-ARCHITECTURE.md`).
- **Never** a header+separately-linked-object split for in-house code
  (no `.h` declaring `extern` globals across translation units) - this
  was attempted mid-session for this exact file and reverted per direct
  instruction; it doesn't match either house convention above.

## Related

- **memory:** [[text-includes-not-the-standard]]
- **pattern:** `PRISC-OPS-ARCHITECTURE.md` - the real op+pal+IPC standard
- **precedent:** `move_entity_init.+x`/`move_entity_tick.+x`/`move_entity.pal`
  (2026-09-27) - the real multi-process pattern when something DOES need
  to be a separate consumer
