# Entity Word Bank — Implementation Status

*Companion to ENTITY-WORD-BANK-DESIGN.md. Tracks build-order progress (design sec 8).*

## Status

| # | Build-order item | Status |
|---|------------------|--------|
| 1 | `wordbank` core (text-include): seed from on-disk, write `words.txt`/`scores.txt`/`vars.txt`, rebuild mirror | DONE |
| 2 | `wordbank_ensure_op` dry run + report on a copy of the pals tree | DONE |
| 3 | Hash-ignore entry (exclude `zz.wordbank/` from `livedesk_hash_dir`) | DONE |
| 4 | Spawn-hook call (auto-seed on new entity creation) | TODO |
| 5 | Hand-scoring screen (bounded rows) + to-score queue | TODO |
| 6 | Use-scoring rows from the parser path | TODO |
| 7 | Real-tree `--apply` after backup + sha256 + count | TODO |
| 8 | Chain: SCORE record type, balance-ignores-SCORE case, `chain_bank_query`, advisory derive | TODO |

## What shipped (commit `f2fd9a8e5`)

### `khtpm_wordbank.c` — text-included core

`WbCtx` + `wb_ensure(dir, ctx, depth)`. Mirrors `khtpm_phone.c`:

- Creates `<entity>/inventory/zz.wordbank/` (and parent `inventory/`) if missing.
- Creates `words.txt`, `scores.txt`, `vars.txt` (empty).
- Seeds `SOURCE=seed` rows at `WEIGHT=0.5` from three CANON classes:
  - `CANON=name:<dir>` — entity directory name
  - `CANON=kind:<kind>` — from `meta.pdl` `STATE | kind | ...`
  - `CANON=action:<label>` — from `menu.chtpm` `<item label="...">` + `meta.pdl` `METHOD | <name>`
- Appends to `words.txt` with dedup (`wb_row_exists` per CANON+ALIAS).
- Rebuild mirror (`wb_rebuild_mirror`): recomputes `WEIGHT` via Laplace `(reward+1)/(reward+punish+2)` over `scores.txt`, rewrites `words.txt` weights.
- Recurses into inventory items that are entities (have `pal.pdl`).
- `--apply=0` (dry run): nothing is written, only counts are reported.
- Never touches existing `SOURCE=user` rows (seeds are only appended if the CANON+ALIAS pair is absent).

### `wordbank_ensure_op.c` — CLI op

Flags (same contract as `phone_ensure_op`):
- `--selftest` — verifies `wb_row_exists`, `wb_meta_kind`, `wb_menu_labels`, `wb_method_names`, `wb_add_seed` dedup
- `--apply` — dry run by default; with this flag, writes files
- `--report FILE` — write report to FILE (default stdout)
- `--pals-root DIR` — scan one pals folder (for copy-first testing)

### Hash-ignore follow-up

`khtpm_taskbar_manager.c:livedesk_hash_dir()` — added `! -path "*/inventory/zz.wordbank/*"` so the bank folder does not drift the entity's `PAL | hash`.

## Verified

On a copy of the pals tree (85 entities: 32 top-level + 53 inventory items):
- `--selftest`: passes (parsing + dedup)
- Dry run → `--apply` on copy → second `--apply`:
  - 85 banks created, 0 on re-run (idempotent, 0 errors)
- Sample `words.txt` row format: `CANON=name:dsr_castle_b|ALIAS=dsr_castle_b|WEIGHT=0.5000|SOURCE=seed`
- `kind:deskpal`, `action:Events (hq)` etc. all present

## Next

Spawn hook wiring (sec 8.4) — call `wb_ensure()` from the entity spawn script.
