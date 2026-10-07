# Entity word / synonym bank, hand-scored, chain-scored: design

Status: DESIGN ONLY, written 2026-10-07 by claude. Nothing here is built.
Owner brief (2026-10-07): "the entities should retroactively get some word / synonym bank; these should be created and hand-scored at creation, we can do the current existing ones; these banks will be scored, and scored even by the blockchain, which itself will be able to learn and query itself."

Read first: `HARNESS-BEHAVIOR-BANK-DESIGN.md` (4b-4e), NIGHT 15 (Synonym Bank row format), NIGHT 22 (hub-and-spoke, one source of truth), NIGHT 30 (attrition model, no auto-promotion), `HAI-ROBOTS-PHONES-SERVER-DESIGN.md` 3b/3c (the phone precedent), `AUCTION-SCREEN-DESIGN.md` (chain gate).

## 1. Precedent to copy: the phone

The owner already decided the same shape for phones: "a phone in every entity's inventory, from now on and retroactively" (design 3b). It was built as:
- one **text-included shared core** (`&.widgits/_shared-lib/khtpm_phone.c`, `ph_ensure()`), called from the **spawn hook** (so every new entity gets it) and from a **one-shot op** (`ops/phone_ensure_op.c`) for the existing ones;
- the op is a **dry run by default** (`--apply` to write, `--report FILE`, `--pals-root DIR` to prove it on a **copy**), **idempotent** (one `access()` per entity when nothing is missing, because of the startup-latency bug), and writes an index;
- identity is `entity_uid` (the entity's PAL hash), `entity_hash = sha256(entity_uid)`, "a pure function ... can be recomputed and verified by anyone".
- ran over **55 entities** (32 top-level + 23 inventory items). Entities live under `xyzfs/users/<uuid>/home/livedesk/pals/<entity>`.

**The word bank copies all of that**: `wordbank_ensure()` in a text-included core, called by the spawn hook and by `wordbank_ensure_op` (dry run by default, copy-first, idempotent, report file). One caveat copied from phones: `livedesk_hash_dir()` hashes the whole folder, so a new folder changes entity hashes unless the hash ignores it (phones got "hash_dir ignores them"; the bank folder must be added to the same ignore list, a named follow-up, not a surprise).

## 2. What an entity's bank is

Folder `<entity>/inventory/zz.wordbank/` (next to `zz.phone`; hash-ignored):
- `words.txt`: **house Synonym Bank rows** (NIGHT 15, same convention as every other state file): `CANON=<what it means for this entity>|ALIAS=<phrase>|WEIGHT=<0..1>|SOURCE=<user|learned|chain|seed>`. Hub-and-spoke: an ALIAS points at exactly one CANON; the CANON is a real thing the entity has (a method/menu row/command name, its kind, its name, its owner-visible verbs), never another alias.
- `scores.txt`: **append-only** score events, one row each, never rewritten: `SCORE|canon|alias|valence=+1|-1|0|source=hand|use|chain|ts|id`. Marker/cursor reading (size growth), never mtime.
- Weights are **derived**, not stored as a second copy: `weight(canon, alias)` = Laplace `(reward+1)/(reward+punish+2)` over its SCORE rows, plus the hand prior. `words.txt` holds the *current derived value as a mirror* that a rebuild op regenerates (NIGHT 22: the mirror is derived, never an independently editable copy). One authoritative source = `scores.txt`.
- Optional later: a `corpus=` tag and concept slots (links to Concept Bank masters) once software/domain masters exist (HARNESS-BEHAVIOR-BANK-DESIGN 4d item 4). Not needed for v1.

## 3. Seeding and hand-scoring at creation

**Where the first words come from** (deterministic, no model deciding): the entity's own name, its kind/template, its menu/method names (what a user can make it do), its owner-facing labels, and a small per-kind template (e.g. a "robot" template carries `follow`, `stay`, `chat`). For existing entities the same sources are read from what is already on disk.

**Hand-scoring** (the owner's rule): each seeded row starts as `SOURCE=seed` at the neutral 0.5 and is **scored by a person** into `SOURCE=user`, via a bounded review screen (rows with `+`, `-`, `0`, skip; same shape as the Part 4 Review Queue / NIGHT 14 bounded pickers; no free text required). Hand scores are the strongest prior but still just SCORE rows (`source=hand`), so they can be outvoted by use and by the chain over time, and are never silently overwritten.
- At **creation**: the spawn hook seeds the rows; the entity shows up in a "to score" queue (it does not block the spawn).
- **Gemma may propose** extra aliases only in the NIGHT 26 fixed line over the entity's real CANON list (`TARGET: <canon from the list> | STRENGTH: ... | REASON: <phrase>`), written as candidate rows to a review file; a human accepts. No model scores anything.

## 4. Retroactive pass over the existing entities

`wordbank_ensure_op <house_root> [--apply] [--report FILE] [--pals-root DIR]`, same contract as `phone_ensure_op`:
1. **Dry run** prints, per entity, the rows it would seed. Prove `--apply` on a **copy** of the pals tree first (as the phone did).
2. Never touches an existing `scores.txt` or any `SOURCE=user` row; adding a new seed alias to an entity that already has a bank is fine, removing/overwriting is not.
3. **Safety (AGENTS.md rule, binding):** `xyzfs/users/` is live user data, never in code branches. Before `--apply` on the real tree: tarball + `sha256sum` list **outside the repo** and a file count check. Never `git add` anything under `xyzfs/users`. The op ships in the code branch; the data it writes lands in the user data branches via `save-data`.
4. Existing-entity coverage list (the report) is the to-score queue's initial content, so the owner scores them in one sitting or over time.

## 5. Scoring by use (the loop)

A harness-style Watch record per use: when the parser maps a typed phrase to an entity CANON via an alias and the action then succeeds/fails (or the user corrects it), a `SCORE|...|source=use` row is appended. This is NIGHT 31's two-number rule applied to words: **reliability** of an alias is counted from rows; changing which CANON an alias *means* is a human edit (a candidate review), never an automatic effect of a failure. The existing `ai_describe`/`halo_chat_describe`/`irl_bootstrap_fsm` paths stay human-reviewed; nothing here auto-promotes.

## 6. Scoring by the blockchain, and the chain learning and querying itself

Honest base (read from the code): the house chain has one transaction type (`TX|from|to|amount|ts|tx_id`), **no signing** (a local process can forge a `from`), same-machine peers only, balances derived by replaying `blockchain.txt` (`chain_balance` = "the single authoritative derivation").

Design, in three pieces, matching how the chain already works:
1. **A second record type on the chain**: `SCORE|<entity_hash>|<canon>|<alias>|<valence>|<ts>|<id>` appended to pending, mined into blocks like a TX. It carries the **entity_hash** (phone design 3c), so a score is about a verifiable entity, and carries no balance effect (balance replay ignores it, to be proven by a harness case).
2. **Chain-derived aggregates, by replay**: `chain_bank_query` (a new op next to `chain_balance`, same pattern: replay the ledger, never keep a second copy) answers "what is the global score of (canon, alias) across all entities", "which aliases do other entities use for this CANON", "top aliases for canon X". That is the chain **querying itself**.
3. **Chain learning = feeding back as a prior**: a periodic derive step writes `SOURCE=chain` SCORE rows into each entity's `scores.txt` (read-only mirror of the chain aggregate, tagged with the chain block height it came from). Entities thereby learn from each other's words without any model. Learning is *aggregation of rows*, not a trained matrix (hand-tunable, auditable: the attrition model's point).

**The gate (same one as the auction): signing.** Until transactions are signed and an actor can prove its identity, anyone can forge SCORE rows and poison the shared prior. So: chain scoring ships **read-only and advisory** first (derive from locally written rows, weight capped well below hand scores, e.g. chain prior counts as at most a fixed number of pseudo-observations), and open mining of other people's SCORE rows waits on signing + peer identity. Same gate list as `AUCTION-SCREEN-DESIGN.md` section 4.

## 7. How it ties into the rest

- **Parser/AI**: the parser's alias lookup for entity commands reads `words.txt` (the NIGHT 15 first step, one wiring point), so typing "fetch" can resolve to an entity's real action.
- **Concept Bank**: CANON rows can later point at masters (software domain) so entity words and harness words share one concept layer; "3D" remains undefined (no document defines it); not decided here.
- **Auction/search**: the auction screen's search can use the same alias lookup, so "robot" finds entities listed as `kind=robot`.
- **Harnesses**: every piece below gets a pal harness first (`_shared-lib/harness/`).

## 8. Build order

1. `wordbank` core (text-include): seed from on-disk sources, write `words.txt`/`scores.txt`, rebuild mirror; harness cases (seed, idempotent, never overwrites `source=user`, mirror equals replay).
2. `wordbank_ensure_op` dry run + report on a **copy** of the pals tree; owner reads the report.
3. Spawn hook call + hash-ignore entry (follow the phone's hash follow-up).
4. Hand-scoring screen (bounded rows) + to-score queue.
5. Use-scoring rows from the parser path.
6. Real-tree `--apply` after backup + sha256 + count (AGENTS rule).
7. Chain: SCORE record type, balance-ignores-SCORE harness case, `chain_bank_query`, advisory derive step. Open mining of foreign SCORE rows only after signing.

## 9. Open questions for the owner

1. Is the CANON for an entity its **actions/menu rows** (recommended), its **kind and name**, or both?
2. Per-kind seed templates: who authors them (me from existing entity kinds, then you score)?
3. Scoring screen: its own window, or a tab on the entity's menu / db-hq?
4. Chain cap for the shared prior (how many pseudo-observations at most), and should chain rows ever override a hand score? (recommended: never)
5. One bank per entity, or also one per user/world shared bank (the chain aggregate is the world level)?
6. Are inventory items (the 23 phone-migration items) entities for this purpose, or only top-level pals?
