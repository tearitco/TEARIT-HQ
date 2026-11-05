# ledger-lab: quarantined book (2026-11-04)

Holds the item ledger lab. NOT listed in the editor's book dropdown and NOT read by any game book yet.
- Op: `@.apps/rpg-edit-hq/ops/item_ledger_op.c` (built to `ops/+x/`). Refuses to run unless the folder it is given contains `QUARANTINE` (exit 4).
- Items: `items.pdl`, THREE ids that never convert: `cdda.stone`, `df.stone`, `mcl.cobble` (owner 2026-11-04). A shop trades across ids; the ledger never maps one to another.
- Ledger: `state/ledger.txt`, append-only, hash chained (edit any row and `check` fails). Size cap 256 KB, then compacted to SNAP rows.
- Harness: `&.widgits/_shared-lib/harness/item_ledger.pal` (scratch book only, never this folder).
- Rule: do not call the op on this folder from a game or a pet until the owner lifts quarantine. The harness uses scratch copies.
- Release criteria (proposal): harness green, one real run in this book with a hand-read ledger, owner reads `check` output, then ONE named caller.
