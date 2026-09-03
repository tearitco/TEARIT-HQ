# Rendering orientation — the khtpm family, concretely

*Condensed from `SKILLS.md` §2, 2026-09-02. Read `CENTROID_GOLD_STD.md`
first for the governing standard — this is the "how it actually works
today" companion.*

- Most visible windows (db-hq, events-hq, chat-hai, palettes,
  bookmarks, stats-hq, taskbar-settings, entity-menu popups, and as of
  2026-09-01 the taskbar strip itself) are all **one binary**,
  `khtpm_core_render.+x` (built from
  `*.monads/*.livedesk-taskbar/ops/khtpm_core_render.c`). Which mode a
  process runs is a `g_is_<mode>` flag set from argv/the `.chtpm` it
  launched against — genuinely one merged binary serving many roles.
  **This flag-per-mode shape is exactly what `CENTROID_GOLD_STD.md` §3
  rule 7 forbids adding to again — it is legacy debt, not a template.**
- The paint layer is shared: `khtpm_render_core.c` (Elem tree,
  `hit_test`, `find_by_id`/`find_by_tag`, `css_layout_pass`),
  `khtpm_draw_core.c` (`draw_elem`, `render_tree`, sprite/font
  caches), `khtpm_css_parser.c` (CSS-like stylesheets, compound class
  selectors). **Before writing per-mode drawing code, check whether
  the shared layer already does it** — a large share of past cleanup
  was killing duplicated per-mode paint code that should have called
  shared functions from the start.
- Dynamic UI content (a command list, a session sidebar, a block
  palette) is injected into the Elem tree via
  `reusable_slot(pool[], max, index, tag)` — a fixed static array
  reused every rebuild, never a fresh `elem_new()` per frame.
- Keyboard/mouse nav: every Elem carrying a real action gets a
  `nav_index` (visible as a `[ ]N.` badge); digits jump focus, Enter
  activates. Standard is **onClick-driven auto-numbering** — any Elem
  with a real `onclick` should be nav-reachable; adding a new
  interactive row means checking that mode's `assign_nav_indices()`
  actually walks to it. Forgetting this extension is historically the
  single most common bug class here.

## Naming note

If you see `khtpm_entity_menu_render` in an older doc, it's this same
file (`khtpm_core_render.c`) under its pre-2026-09-01 name — the file
was renamed, and as of 2026-09-01 also absorbed the taskbar strip
parser's own duty (`khtpm_strip_parser.c` folded in verbatim). Only
correct this name in living/reference docs — leave it alone inside
genuinely historical, dated narrative where the old name was factually
accurate at the time.
