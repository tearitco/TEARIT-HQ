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
