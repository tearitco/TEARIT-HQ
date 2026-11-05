# ledger-lab: quarantined book (2026-11-04)

Holds the item ledger lab. NOT listed in the editor's book dropdown and NOT read by any game book yet.
- Op: `@.apps/rpg-edit-hq/ops/item_ledger_op.c` (built to `ops/+x/`). Refuses to run unless the folder it is given contains `QUARANTINE` (exit 4).
- Items: `items.pdl`, THREE ids that never convert: `cdda.stone`, `df.stone`, `mcl.cobble` (owner 2026-11-04). A shop trades across ids; the ledger never maps one to another.
- Ledger: `state/ledger.txt`, append-only, hash chained (edit any row and `check` fails). Size cap 256 KB, then compacted to SNAP rows.
- Harness: `&.widgits/_shared-lib/harness/item_ledger.pal` (scratch book only, never this folder).
- Rule: do not call the op on this folder from a game or a pet until the owner lifts quarantine. The harness uses scratch copies.
- Release criteria (proposal): harness green, one real run in this book with a hand-read ledger, owner reads `check` output, then ONE named caller.

## Survivor slice step 1 (2026-11-04)
- `ops/survivor_op.c` (built to `ops/+x/survivor_op.+x`): place/smash/tick/eat/drink/wield/light/status. Also refuses any folder without `QUARANTINE`; writes items only through `item_ledger_op`.
- Data: `furniture.pdl`, `needs.pdl`, `itemdata.pdl`; items `cdda.plank cdda.rag cdda.pipe cdda.can cdda.bottle` added next to the three owner ids.
- `state/` here holds the one hand-read real run (7 ledger rows, `check` OK); the harness `survivor_slice.pal` never touches this folder.
- Not connected to cdda-hq or to the clock.

## Survivor slice step 2 (2026-11-04)
- `survivor_op` verbs `read`, `practice`, `recipes`, `craft`; data `books.pdl`, `skills.pdl`, `recipes.pdl`; items `cdda.book_fab cdda.spear cdda.bandage`. Real run as actor `scout` is in `state/` (18 ledger rows, `check` OK). Harness: `survivor_craft.pal` (scratch only).

## Needs on the game clock (2026-11-04)
- Core `clock_event` handles `common:needs_tick` and runs `survivor_op tick` for the actors in `@.apps/rpg-edit-hq/survivor_clock.pdl`, which ships `ENABLED | 0` (owner switch). Harness `needs_clock.pal`. Not installed on the live clock.

## Survivor slice step 3 (2026-11-04)
- `ops/gen_map_op.c`: `cdda_house <seed> <out>` and `df_column <seed> <out>`, plus `verify_house` / `verify_column`. Data here: `furniture.pdl` (bench, crate), `loot.pdl`, `minerals.pdl`. Same seed = byte-identical files. Harness `gen_map.pal`. Writes only into the out folder it is given; never an item.

## Survivor slice step 4 (2026-11-04)
- `ops/world_common_op.c`: `flow`, `rain [on|off]`, `fire`, `status` (one cell per call, no items). Files in `state/`: `fluids.txt`, `fire.txt`, `solid.txt`, `roofed.txt`, `containers.txt`, `dirt.txt`, `wet.txt`, `rain.txt`. Harness `world_common.pal`. Not run from the clock yet.

## Survivor slice step 5 (2026-11-04)
- `ops/mcl_op.c`: `craft <actor> <recipe> <x> <y>` (2x2 recipes need no bench, 3x3 need a placed bench cell at x,y), `smelt` (furnace cell), `place bench|bed|furnace`, `respawn` (own bed else fallback), `burst <x> <y> <r>` (no drops). Data: `mcl_recipes.pdl`; items `mcl.*` in `items.pdl`. Harness `mcl_bench.pal`. Not called by anything yet.

## Survivor slice step 6, the shop (2026-11-04)
- `ops/shop_op.c`: `list`, `buy <actor> <item> <n>`, `sell <actor> <item> <n>`. Coins are the ledger item `coin.play`; each sale is one atomic ledger `trade`; the shop never mints. Data: `shop.pdl` (COIN / SHOPKEEPER / PRICE with sell < buy). Spec: `16.game/SHOP-SLICE-SPEC-2026-11-04.md`. Harness `shop.pal`. No verb swaps one item id for another.

