# The two parser families, why both exist

*Condensed from `HOUSE_FAQ.md` "ARCHITECTURE" section, 2026-09-02.*

**Why 7+ different khtpm "modes" instead of one generic engine with
managers doing the heavy lifting?** Historically `khtpm_core_render.c`
(formerly `khtpm_entity_menu_render.c`) is a MERGED binary — several
separate standalone renderer files squashed into one during an earlier
merge stage; the per-mode `if`/`else` branches are leftover seams from
that merge, not deliberate design. The real end-goal (in progress, not
100% there) is "one generic engine, all app-specific behavior in
manager processes" — see `CENTROID_GOLD_STD.md` §3 rule 7 and the
generic-dispatch-table plan in 08-roadmap. Good news: each mode still
runs as its own separate process of the same binary (mode flag set
once at startup, never changes), so a generic mechanism can safely be
shared/reused across modes one process at a time.

**Do all modes share one layout parser?** Yes — `khtpm_css_parser.c`/
`khtpm_render_core.c` (parse `.chtpm` into the Elem tree) +
`khtpm_draw_core.c` (paint it), never duplicated per-mode. What looks
like "its own parser" per feature is really reading a manager's live
DATA (state-file lines shaped differently per feature) — same way one
web app has one shared HTML/CSS renderer but a different parser per
JSON API it calls. Not a gap needing closing.

**Two real tree/render systems exist, though** — LayDoc (taskbar) and
Elem/CSS (every HQ window). See
`02-architecture/INPUT-RELAY-PIPELINE.md` for the split and current
convergence status.

**chtpm_parser_pal vs khtpm, at a glance:**

| | chtpm_parser_pal | khtpm |
|---|---|---|
| Output | plain character grid, no box model | positioned/styled Elem tree (x/y/w/h + CSS) |
| Native GUI-capable | no, structurally | yes |
| Status | still running (IRC chat, forum, chain, mutaclysm, piececraft, most `@.apps/`) | gold-standard target for all new UI |
| Retrofit posture | not deprecated, no forced migration | opportunistic migration when a real touch already exists |

Full technical history and the 4-stage arc that led to the current
standard: `CENTROID_GOLD_STD.md` §2.
