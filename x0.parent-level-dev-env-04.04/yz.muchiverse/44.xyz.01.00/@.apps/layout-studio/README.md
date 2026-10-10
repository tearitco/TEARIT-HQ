# layout-studio (claude's lane) - layers 1-2 built 2026-10-09

Strategy: `!.HQ-IQ-BOOK/08-roadmap/design-docs/LAYOUT-STUDIO-FOR-SIMPLE-AGENTS-STRATEGY-2026-10-09.md`. Goal: any agent, including weak ones, can make and edit
a layout (`.xhtpm`/`.chtpm`) safely.

- `CATALOG.md` - every allowed tag/attribute with an example. **Generated**: `sh ops/build_layout_check.sh && ops/+x/layout_check.+x --catalog > CATALOG.md`.
- `ops/layout_check.c` - the validator. `layout_check <file> [--ui state/ui.txt]` prints `ERROR/WARN line N: what | fix: how`, exit 1 on any ERROR.
  `--drift <khtpm_core_render.c>` fails when the renderer reads an attribute/tag the tables do not list (run it after touching the renderer).
- `samples/shop-menu.broken.xhtpm` (7 errors + 2 warnings, each with a fix line) and `shop-menu.fixed.xhtpm` (0) with `samples/shop-ui.txt`.
- Checked against all 238 real layouts in the house: 0 errors (the tables were corrected twice against them: `<module>` is also a plain container in old
  .chtpm, `cli_io` without `target_id` is only a warning).

Next (strategy table): templates + `layout_op preview`, then `layout_op` CLI, manager scaffolds, then the studio window that loads a `.xhtpm` and edits it.

## Templates and the pet generator (2026-10-09)

- `templates/` has five starter layouts that pass `layout_check`: `menu`, `menu-search`, `hud-strip`, `confirm`, `status-panel`. Each has a sample
  `<name>.ui.txt` (the manager keys it reads) and all share `template.css`. **A window body is one `<sidebar>` (the clickable list) next to one
  `<panel>`; with only one of them nothing draws** (found by rendering). Use `class="... database-window"` on the `<window>` or it opens 264x42. No angle
  brackets inside comments.
- `ops/pet_gen.c` (`sh ops/build_pet_gen.sh`) generates an animated tamagotchi-style pet from a seed, no downloads: `pet_gen <out_dir> [seed] [pet.pdl]`
  writes `obj/<anim>_<NN>.obj` + `pet.mtl` (idle, eat, hungry: 8 poses each, OBJ has no animation so play the numbered poses in order), 16x24 `sprites/*.rgba`,
  `sprites_csv/<anim>_<NN>/sprite.csv` (what `<item sprite="DIR">` loads, verified in a window) and `sheet.png`. Pin colour/ears/size with `KEY | body_r | 230` rows.
  No FBX yet (binary container); an ASCII-FBX exporter can reuse the same mesh list.

## layout_flow: a game's flow as data (2026-10-09)

`ops/layout_flow.c` (`sh ops/build_layout_flow.sh`). A `flow.pdl` (first user: `@.apps/pet-trainer/flow.pdl`) names the layout, the nav table, the verb script, the keybinds and the views. Then:
- `layout_flow check flow.pdl` - `layout_check` + every button verb has a case in the verb script + verbs dropped by the stopped-gate (WARN) + duplicate key codes.
- `layout_flow navmap flow.pdl <view>` - the buttons with their real numbers: tabs, bottom footer, sidebar rows (nav.pdl rows expanded, `{party}` = one per pet), chat field. Dropdown rows are listed with `0` and their tab.
- `layout_flow press flow.pdl <view> "<label>" --pid <window pid> [--esc]` - sends the relay keys. A dropdown row = tab digit, then Down x (row-1), then Enter (digits do not jump inside an open dropdown). `--esc` first leaves Interact mode.
Not an editor: there is still no palette, drag and drop or play preview (phases 5-6 of `IN-GAME-LAYOUTS-PLAN.md`); this is the data + audit + drive layer they will sit on. The meta quest: agents and users craft layouts and game scenes with events through the studio.
