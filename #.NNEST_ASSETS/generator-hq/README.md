# generator-hq: Tear-it generator outputs (owner 2026-11-05: "final gens can go in its own dir in #.NNEST_ASSETS")

Written by `44.xyz.01.00/@.apps/generator-hq/ops/+x/generator_tile_op.+x`. Source of every picture is named in its folder.
- `interim/` : reference sheets while the generator learns to paint (decision D27: recolour the sliced RPG Maker cells, same 48x48 grid, alpha never changed). Looks like the original shapes with a different palette. **Interim and for reference. Not approved as final art. Internal only until the RPG Maker licence is read (parked by the owner).**
- `good/` : the kept files. Empty until the owner approves the Tear-it pattern in writing (TEAR-IT-TILES-100, answer 4).
Layout: `<style>/<sheet>_<cell>/sprite.csv` (the house cell format) + `tile.png` (4x view). Rebuild with the command in `BOOK/16.game/HANDOFF-GENERATOR-GROK-WORK-AND-INTERIM-ASSETS-2026-11-05.md`.

## Rebuild the whole-sheet set (cells are not committed: 576 cells is about 22 MB; the contact sheets in `interim/contact/` are)
`generator_tile_op.+x sheet @.apps/generator-hq/events/gen_restyle/blocks.pdl "#.NNEST_ASSETS/rmmv-www-img/tilesets/Outside_A2.png" <outdir>` writes `<outdir>/<style>/Outside_A2_bNN_sNN/{sprite.csv,tile.png}` for every block listed in `blocks.pdl`, restyled in memory (no original pixel is written). Contact sheets: `generator_tile_op.+x contact <out.png> 8 <48 sprite.csv files>`. Run through `cpu_gate`.

## good/ (since 2026-11-05)
Approved files only, see `good/APPROVED.md` for who approved what and the criteria. Check: `sha256sum -c good/APPROVED.sha256`; compatibility with the RPG Maker folder: `generator_tile_op.+x verify-root <rmmv-www-img> good`. Slots (people and face sheets) are built by `generator_cast_op.+x slots <cast.pdl> <slots.pdl> <outroot>`; themes by `@.apps/generator-hq/events/gen_theme/`.
