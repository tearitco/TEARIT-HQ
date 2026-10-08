# Making the livedesk taskbar look like the pc-hq taskbar, once and for all

Written 2026-10-07. Status: ANALYSIS + PLAN. No code changed. No screenshot
was taken of either bar for this document; the diagnosis comes from reading
the code, CSS and design docs, so the "why it looks different" part is
strongly supported but not yet confirmed pixel-by-pixel (see section 6).

## 1. The short answer

The two bars look different because they are drawn by **two different layout
engines with two different stylesheets**, not because of a missing setting.

| | livedesk taskbar (strip) | pc-hq entities bar and every HQ window |
|---|---|---|
| Engine | **LayDoc** (`khtpm_strip_layout.c`, flat array, `parent_index`) | **Elem/CSS** (`khtpm_render_core.c` + `khtpm_draw_core.c`, pointer tree, CSS box model) |
| Markup | `khtpm_strip_header.xhtpm`, `khtpm_strip_bottom.xhtpm` | `pchq-board.xhtpm`, `<footer class="pchq-footer">` with `<repeat>` cells |
| Style source | `khtpm_strip_*.css` (about 4 KB total, only the `.dock-flexrow` flex rule) plus `livedesk_theme.pdl` (3 colors: bg `#1a1a1a`, fg `#f97316`, opacity 0.85) and sizes set from C (`dock_item_cw()`, `DOCK_BAR_H`) | real CSS: `.pchq-footer { height:44px; background:#181818; border:1px solid #2a2a2a }`, `.tab.pchq-tb`, `.dock-cell` rules, shared chrome (X / ! / _ buttons, nav badges) from the generic renderer |
| Look is decided by | C code plus 3 theme colors | CSS, same as every other window |

So the strip is the one surface that does NOT go through the shared
CSS box model. Everything the owner says "looks fine" shares one engine; the
strip is the only holdout. This is already known and recorded:
`INPUT-RELAY-PIPELINE.md` "Two tree/render systems, not one" says "The taskbar
itself has not been retargeted onto Elem yet - a separate, later step."

## 2. Is it a format by now?

Yes for the window, no for the strip. The format is `.xhtpm` + `.css` parsed
into the Elem tree (CENTROID_GOLD_STD.md: one real parsed, laid-out, styled
tree is the single source of truth). pc-hq's footer already uses it, and its
own design doc (`PCHQ-ENTITY-MENU-AND-TASKBAR-DESIGN.md`) says the entity
cell rendering should REUSE the livedesk strip's `dock-cell` markup and CSS,
"extracting it from `khtpm_taskbar_manager` if needed". So pc-hq copied the
strip's cell idea into the Elem world; the strip itself never moved.

Partial progress already exists: since 2026-09-14
(`DOCK-BAR-GENERIC-LAYOUT-MIGRATION.md`, phase 1) the strip hands row-wrap
and column math to the shared flex engine (`css_layout_pass`) through
`.dock-flexrow`. Cell width and height, colors and chrome still come from C.

## 3. What actually differs on screen (hypotheses to confirm)

1. Cell chrome: pc-hq cells get border, radius-free dark fill (`#232323`,
   `#181818`, border `#2a2a2a`) and bold light-blue text (`#cfe8ff`) from CSS;
   the strip paints orange-on-dark (`#f97316` on `#1a1a1a`) from the 3-color
   theme with sizes fixed in C.
2. Header (menu row) and bottom dock are two LayDoc documents sharing one
   `g_sheet`; the peer's own CSS is never loaded (documented in
   `khtpm_strip_header.css`), so per-bar styling has nowhere to live.
3. Nav badges, hover, and active-state styling exist as CSS classes in Elem
   windows; the strip's equivalents are hand code.

## 4. What it would take to fix it for good

The permanent fix is the one the architecture already names: **retarget the
taskbar onto Elem/CSS**, so it is just another window and its look is CSS that
lives next to the pc-hq CSS. Steps, smallest and safest first:

- **Step A: share one stylesheet (cheap, visible win, low risk).** Create a
  single `dock.css` holding the `.dock-cell` / `.pchq-footer` look (colors,
  border, font weight, sizes as CSS values). Make pc-hq's footer and the strip
  both load it. Move the colors the strip takes from C into CSS variables fed
  by `livedesk_theme.pdl` (the theme file stays the data knob; CSS reads it).
  Fix the "peer CSS never loaded" limitation by giving the bottom bar its own
  load site, or merge the two sheets deliberately.
- **Step B: make cell size a CSS concern.** Replace `dock_item_cw()` /
  `DOCK_BAR_H` C constants with CSS width/height (min-width, padding) where the
  engine supports it; keep C only for content-driven widths the engine cannot
  express, and document that list.
- **Step C: render the strip's cells through Elem.** Extract the
  `khtpm_taskbar_manager` dock-cell rendering into the shared path pc-hq's
  `<footer><repeat>` already uses. After this, one cell implementation serves
  both bars (the reuse the pc-hq design doc already requires).
- **Step D: retire LayDoc for the strip.** Only after A to C are live and
  verified. LayDoc-only features (var substitution, ACTIVATE scope, cli_io)
  were mostly ported into Elem on 2026-08-28 (6 of 8 gaps); the plan's Gap 7
  correction says the strip keeps its own `g_nav_focus` / `unified_step` nav,
  so nav must be handled explicitly in this step.

Guardrails from the house standards that apply (khtpm-house-standards):
no new per-project branch in `khtpm_core_render.c`; layout mutations stay
idempotent (`assign_nav_and_layout` runs many times per frame); the shared
core files in `&.widgits/_shared-lib/` are the ones to edit (the ops copy is
overwritten on build); running windows only pick up changes on relaunch. The
strip is the whole desktop's taskbar, so every step needs a dock-first,
rollback-ready rollout (same rule as the incremental-reparse design).

## 5. Recommendation

Do Step A first. It makes the two bars read as one family within one session
of work, it touches CSS and theme plumbing rather than layout code, and it
produces the shared stylesheet that Steps B to D then build on. Steps C and D
are a real migration and should be scheduled, not slipped in.

## 6. What is NOT verified

- No screenshot comparison was made; section 3 is from reading code and CSS.
  First task of Step A: dump both bars with `dump_frame_png_op.+x` and diff.
- The exact current visual defect the owner calls "outdated weird look" is not
  yet identified. Please name it (colors, spacing, font, cell shape?) so the
  shared stylesheet targets it.
- `livedesk_theme.pdl` is modified in the live working tree right now (owner's
  settings); this document does not touch it.

## Sources read

`CENTROID_GOLD_STD.md`, `TWO-PARSER-FAMILIES.md`, `INPUT-RELAY-PIPELINE.md`,
`LAYDOC-ELEM-PORT-IMPLEMENTATION-PLAN.md`, `PCHQ-ENTITY-MENU-AND-TASKBAR-DESIGN.md`,
`_.monads/_.livedesk-taskbar/khtpm_strip_{header,bottom}.css`,
`@.apps/piececraft-hq/pchq-board.{xhtpm,css}`, `apply_theme_op.c`,
`#.desktop/livedesk_theme.pdl`.
