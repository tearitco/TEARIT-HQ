# _shared-lib

Single canonical source for small files that were hand-duplicated,
byte-identical, across multiple widget `ops/` dirs (found + confirmed
via md5sum during the 2026-08-12 duplication-inventory pass):

- `khtpm_css_parser.c` / `.h` — the `.chtpm` stylesheet parser, was a
  hand copy in both `*.monads/*.livedesk-taskbar/ops/` (used by
  khtpm_strip_parser AND khtpm_hq_render/db-hq) and
  `&.widgits/events-hq/ops/`.
- `stb_image_write.h` — same two consumers, PNG dump support.
- `khtpm_render_core.c` — added 2026-08-16 (au11-hq/khtpm-merge-how2.md
  Stage 2a): the `Elem` struct + `hit_test()`/`find_by_tag()`/
  `find_by_id()`, verified byte-/functionally-identical across
  `khtpm_hq_render.c` (db-hq), `khtpm_events_hq_render.c` (events-hq),
  and `chat_hai_hq_render.c` (chat-hai) before being moved here.
  **Different mechanism from the other files in this dir on purpose**:
  this one is meant to be pulled in via `#include "khtpm_render_core.c"`
  (a direct text-include of a `.c` file, NOT compiled+linked as its own
  translation unit like `khtpm_css_parser.c` is) — the real house
  convention checked against `1.TPMOS_c_+rmmp.0103.0001/projects/
  wraith-alpha/ops/*.c` has ZERO in-house header files anywhere (only
  system headers, with `#ifdef _WIN32`-shaped blocks as the sole
  cross-OS-shim exception) — so sharing a real struct definition across
  multiple standalone binaries without a header means text-including a
  `.c` file, not writing a new `.h`. See that file's own header comment
  for the full reasoning.
  `khtpm_open_hai_render.c`/`khtpm_taskbar_settings_render.c`/
  `khtpm_strip_parser.c` do NOT use this — they don't share the Elem/
  CSS-parser architecture at all (confirmed, not assumed — see
  khtpm-merge-how2.md's own STATUS section).

**Convention — UPDATED 2026-09-09
(`SHARED-SOURCE-COMPILE-IN-PLACE.md`):** this is a source-of-truth
directory, and dev builds **compile the canonical file in place** —
`build_*.sh` passes `-I "<this dir>"` and names
`"$SHARED/khtpm_css_parser.c"` on the compile line; text-includes
(`#include "khtpm_render_core.c"` / `"khtpm_draw_core.c"`) resolve
through the same `-I`. Only the built binary lands in the consumer's
`ops/`. **No `.c`/`.h` from this dir is copied into a consumer `ops/`
at build time any more** — every stray copy is `.gitignore`d
(`**/ops/khtpm_*`).

The *earlier* convention copied these files into each `ops/` dir before
compiling. That was struck because it caused real drift (six copies had
already diverged from canonical) and silently discarded edits made to
the wrong copy on the next build. `-I` is a compile-time search path,
not a runtime cross-reference — the output binary is exactly as
self-contained after the link as it was before.

**Shipping a self-contained subtree** (the
`04.harnecient-fresh-install-design.md` §5.1 concern — a packaged
`ops/` dir that builds with no `-I`): the install / tree-assembly step
runs **`vendor-into.sh <target_ops_dir>`** (in this directory), the one
blessed place that copies these files. Never call it from a
`build_*.sh`. A tree it has run over builds with no `-I`, byte-identical
to the `-I` build (verified).

**If you add a new consumer**: add `-I "$SHARED"` + the canonical
`.c` path to its compile line (see `build_core_render.sh` /
`build_lc_clock.sh` / `tile-picker/scripts/build.sh` for the pattern).
Do **not** add a `cp` line.

**If `_shared-lib` itself needs to be installable** (e.g. a future
widget that ships without ever being dev-built first): flag it as a
Phase-1 installer follow-up in `04.harnecient-fresh-install-design.md`
§10 — not needed today since every current consumer builds from dev
source before packaging.
