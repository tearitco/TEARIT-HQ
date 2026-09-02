# piececraft-hq — real clone note (2026-08-30)

**What this is:** a real, complete filesystem clone of `@.apps/piececraft-xyz`
(as it stood right after this session's real setup-screen removal + auto-
board-widget fixes, `8f11e4c8`/`c2dc9631`/`19b0a2db`), made specifically to
experiment with a khtpm-style layout — a real menu window positioned below
the game screen, using the same generic Elem/CSS renderer db-hq/events-hq/
Settings already use, instead of piececraft-xyz's own real `chtpm_parser_pal`/
`prisc+x` engine's text-grid rendering.

**Why a clone, not a direct edit of piececraft-xyz:** confirmed via
`!.HOUSE_STDS.md` §J - piececraft-xyz/board-viewer run on a completely
separate, unrelated engine (`chtpm_parser_pal.c`/`prisc+x`) from the
`khtpm_*` family (db-hq/events-hq/chat-hai/Settings, merged into
`khtpm_core_render.c`). They only share the `.chtpm` file
extension, nothing else. A real merge between the two families is
explicitly documented as "a real architectural decision for a future
session, not something to attempt inside a feature-shipping task" -
never attempted anywhere in this house. Cloning here keeps piececraft-xyz
itself stable/working (just verified end-to-end this session) while this
copy takes the real architectural risk.

**What was renamed** (a straightforward find+sed pass, not hand-verified
line by line): every real `piececraft-xyz`/`piececraft_xyz` string
occurrence across `.c`/`.h`/`.sh`/`.ps1`/`.pdl`/`.chtpm`/`.txt`/`.md`
files → `piececraft-hq`/`piececraft_hq`, and `projects/piececraft-xyz/` →
`projects/piececraft-hq/`. **Known, deliberate exception**: prose inside
`PIECECRAFT_XYZ_DESIGN.md` (filename kept as-is) also got string-
replaced by the same blanket pass, so some of its own historical
narrative ("cloned FROM civ-txt, named piececraft-xyz...") now reads with
"piececraft-hq" in places where it's describing PAST history, not this
clone's own identity - a real, minor doc-accuracy artifact of the quick
rename, not a functional bug. Worth a manual pass later if that doc gets
used as a real reference again; not worth blocking the clone on now.

**Verified live**: `bash button.sh build` - zero errors. `bash button.sh
run` - boots straight into the game (same real setup-screen-removal fix
as piececraft-xyz, carried over by the clone), auto-opens the real
board-viewer widget, confirmed via `/tmp/pchq_run.log` showing
`[main]`/`Tick`/`Hero HP`/`Pos`/`Chunk` immediately, zero setup steps.

**Not started yet**: the actual khtpm-style menu-below-screen work itself.
This clone is the real starting point only.
