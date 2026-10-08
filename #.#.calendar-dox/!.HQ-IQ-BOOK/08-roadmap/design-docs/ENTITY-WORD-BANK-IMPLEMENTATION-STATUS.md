# Entity Word Bank — Implementation Status

*Companion to ENTITY-WORD-BANK-DESIGN.md. Tracks build-order progress (design sec 8).*

## Status

| # | Build-order item | Status |
|---|------------------|--------|
| 1 | `wordbank` core (text-include): seed from on-disk, write `words.txt`/`scores.txt`/`vars.txt`, rebuild mirror | DONE |
| 2 | `wordbank_ensure_op` dry run + report on a copy of the pals tree | DONE |
| 3 | Hash-ignore entry (exclude `zz.wordbank/` from `livedesk_hash_dir`) | DONE |
| 4 | Spawn-hook call (auto-seed on new entity creation) | DONE |
| 5 | Hand-scoring screen (bounded rows) + to-score queue | DONE |
| 6 | Use-scoring rows from the parser path | DONE |
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

## Spawn hook (commit `6a56da126`)

`khtpm_taskbar_manager.c` now `#include "khtpm_wordbank.c"` and calls `livedesk_wordbank_ensure(child_dir, &ctx)` at both entity-creation sites:
- line 2687 (cursword / desk-pal spawn)
- line 2943 (inventory entity spawn)

`build_phone_ensure_op.sh` `MGR_SRCS` updated to include `$SHARED/khtpm_wordbank.c`.

## Hand-scoring op (commit `f32b874c5`)

### `wordbank_score_op.c` — bounded console review + to-score queue

A console-based review tool (mirrors `concept_edit_validate.c` / `pending_review.txt` conventions from HALO):

- `--queue` — scans all entities, lists every `SOURCE=seed` row across all wordbanks. Output: `<entity_relpath>|<canon>|<alias>|<weight>`. Sorted by entity then canon.
- `--apply <file>` — reads a batch score file (`entity_relpath|canon|alias|valence`), changes `SOURCE=seed`→`SOURCE=user` in `words.txt`, sets `WEIGHT=1.0` (+1) / `0.0` (-1) / `0.5` (0), and appends a `SCORE|...` row to `scores.txt` with `source=hand`.
- `--interactive` — bounded console review: one row at a time, keys `+`/`-`/`0`/`s` (skip), writes to words.txt + scores.txt.
- `--selftest` — verifies parse + apply + append on a temp tree.

Batch score file format:
```
# entity_relpath | canon | alias | valence(-1/0/1)
asa|name:asa|asa|1
asa|action:Chat|Chat|0
terumon_003_murmur|name:terumon_003_murmur|-1
```

`build_phone_ensure_op.sh` updated to compile `wordbank_score_op` alongside `wordbank_ensure_op`.

## Verified

On a copy of the pals tree (85 entities: 32 top-level + 53 inventory items):
- `wordbank_ensure_op --selftest`: passes (parsing + dedup)
- Dry run → `--apply` on copy → second `--apply`:
  - 85 banks created, 0 on re-run (idempotent, 0 errors)
- `wordbank_score_op --selftest`: passes (update, dedup, append)
- `--queue`: lists 297 un-scored seed rows across all entities
- `--apply` on a batch of 4 scores: all applied, `words.txt` updated (SOURCE=seed→user, WEIGHT set), `SCORE` rows appended to `scores.txt`
- Idempotency: after scoring 4 rows, `--queue` reports 293 (4 scored rows gone)
- `--interactive`: 3/5 scored + 1 skipped on piped input; results verified in words.txt and scores.txt
- SCORE record format: `SCORE|name:asa|asa|valence=+1|source=hand|ts=<unix>|id=asa`

Sample `words.txt` row format: `CANON=name:dsr_castle_b|ALIAS=dsr_castle_b|WEIGHT=0.5000|SOURCE=seed`

## Parser-path wiring (commit `46d54e25a`)

### `wordbank_alias_op.c` — alias resolution + use-scoring CLI

Added to the shared core (`khtpm_wordbank.c`):
- `wb_word_present()` — case-insensitive whole-word match (same semantics as `send_message.c`'s `message_has_word`)
- `wb_alias_lookup()` — parses an entity's `words.txt`, matches input phrase against ALIAS rows, returns best CANON+ALIAS+WEIGHT (highest weight wins ties)
- `wb_use_score()` — appends `SCORE|canon|alias|valence=+1|source=use|pal_hash=...|ts|id=alias` to `scores.txt`

CLI modes:
- `--lookup <entity_dir> "<input>"` — prints `CANON=...|ALIAS=...|WEIGHT=...` (exit 0 match, exit 1 no match)
- `--use <entity_dir> <canon> <alias> <valence> [pal_hash]` — appends a `source=use` SCORE row
- `--selftest` — verifies lookup, whole-word matching, use-scoring

### `gemma_strategy.c` wiring

After `detect_tool()` returns `"none"` (no built-in tool keyword matched), `gemma_strategy.c` calls `wordbank_alias_op.+x --lookup` against the pal's own word bank. On match:
- Writes `detected_tool=entity_action` + `detected_canon` + `detected_alias` to state
- Appends a `source=use` SCORE row via `wordbank_alias_op --use`
- Falls through to ordinary chat if no bank exists or no match (graceful degradation)

`build.sh` updated to compile `wordbank_alias_op` from the shared-lib source into `ops/+x/`.

### Seeding muchi-pal-agent

The muchi-pal-agent project doesn't have a `menu.chtpm`/`meta.pdl` (it's a dev project, not a live pal entity), so `wordbank_ensure_op` won't seed it. Its word bank was seeded manually with 39 CANON rows: `name:muchi-pal-agent` + `action:*` CANONs from `detect_tool()`'s keyword set (read, write, search, list, speak, web_search, plan_cells, etc.) with their aliases.

### Verified

- `wordbank_alias_op --selftest`: passes (lookup, whole-word, use-score)
- `--lookup` against muchi-pal-agent's bank: "I want to read a file" → `CANON=action:read_file|ALIAS=read`; "show me the files" → `CANON=action:list_dir|ALIAS=show`; "hello" → no match (exit 1)
- `--use`: appends `SCORE|action:read_file|read|valence=+1|source=use|...` to scores.txt
- `gemma_strategy.c` compiles cleanly with `-Wall -Wextra` (only `-Wformat-truncation` on path buffers)

