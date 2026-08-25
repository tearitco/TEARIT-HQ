# canon/ — Soul Pen franchise canon store (W0)

Deterministic, regenerable, ZERO model calls. Built by `scripts/canon_ingest.sh`,
verified by `scripts/canon_verify.sh`.

## Layout
- `source/<book>/` — screenplay scene atoms (immutable source canon) + `_meta/` seeds
- `lt/<book>/` — gold Living Testament chapters `chNN.txt` (immutable scripture)
- `lexicon/` — curated entity/place/item rows (pdl: `id|type|display|aliases|note`)
- `ledger/entity_index.pdl` — derived index: every alias → canon-file hit list
- `refs/` — cross-reference tables (built in W1)
- `manifest.pdl` — provenance + counts, auto-generated

## Book: solpen ("Book of the Soul Pen")
- 40 screenplay scenes (scene_01..scene_40) + outline → `source/solpen/`
- 21 gold LT chapters ch01..ch21 (ch16 pulled from `ooo/` "best") → `lt/solpen/`
- Sources:
  - `!/!.SP.all-writeez31-c0.5/0.Sol Pen]bk0-K-ARK-2.1/0.scenes-K-ARK-3.0/`
  - `!/!.SP_AS_BIBLEϕ©=LT-01.00/0.LT-SP/`

## Verify status
Last run: `scripts/canon_verify.sh` → PASS=4 FAIL=0
- 40 scenes, 21 LT chapters, ch01..ch21 contiguous
- 38 index rows, regenerable
- Zero-hit aliases are documented coverage gaps (franchise-level / meta-only)

## Known gaps (documented, not blocking)
- Elara, watcher birds: franchise-level (compendium), not named in bk0 text
- Lunaria, Energy Blades, Nanobots, Universal Bypass Key, HALO (full name):
  meta seeds only, absent from bk0 source/LT text
