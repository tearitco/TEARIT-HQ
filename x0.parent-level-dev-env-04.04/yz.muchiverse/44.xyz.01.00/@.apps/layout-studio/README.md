# layout-studio (claude's lane) - layers 1-2 built 2026-10-09

Strategy: `!.HQ-IQ-BOOK/08-roadmap/design-docs/LAYOUT-STUDIO-FOR-SIMPLE-AGENTS-STRATEGY-2026-10-09.md`. Goal: any agent, including weak ones, can make and edit
a layout (`.xhtpm`/`.chtpm`) safely.

- `CATALOG.md` - every allowed tag/attribute with an example. **Generated**: `sh ops/build_layout_check.sh && ops/+x/layout_check.+x --catalog > CATALOG.md`.
- `ops/layout_check.c` - the validator. `layout_check <file> [--ui state/ui.txt]` prints `ERROR/WARN line N: what | fix: how`, exit 1 on any ERROR.
  `--drift <khtpm_core_render.c>` fails when the renderer reads an attribute/tag the tables do not list (run it after touching the renderer).
- `samples/shop-menu.broken.xhtpm` (8 errors + 1 warning, each with a fix line) and `shop-menu.fixed.xhtpm` (0) with `samples/shop-ui.txt`.
- Checked against all 238 real layouts in the house: 0 errors (the tables were corrected twice against them: `<module>` is also a plain container in old
  .chtpm, `cli_io` without `target_id` is only a warning).

Next (strategy table): templates + `layout_op preview`, then `layout_op` CLI, manager scaffolds, then the studio window that loads a `.xhtpm` and edits it.
