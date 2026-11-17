# Trunk and branch: the house standard for shared, versioned data (2026-11-16)

Status: **STANDARD + the shared op BUILT 2026-11-16: `&.widgits/_shared-lib/ops/trunk_op.c`, harness `trunk_op` 59 PASS (sabotage proven for 6 rules). No existing data has been migrated onto it.** Original status: standard (design) + one shared op to build. Owner 2026-11-16, while designing the Texting address book: "that git design should be reusable for other ops/apps wanting to share trunked data across entities etc, as a std habit for data versioning and footprint." Label key: OWNER = said, FOUND = read in the repo, DOC = my proposal. Used first by `16.game/PLUS-PHONE-TEXTING-APP-DESIGN-2026-11-16.md` section 3.10.

## 1. The habit in one paragraph
When two or more owners (entities, users, phones, books, apps) need the SAME base data, keep **one shared trunk** and give each owner a **branch that stores only its own changes**. A branch is read as *trunk + my rows*, where my row for a key wins over the trunk's row for that key, for me only. Nothing is copied wholesale. Changes are **append-only rows with ids**, so versions, sharing and merging are all the same operation: **replay rows**. It is the ledger rule (NO-GHOST-DATA) applied to shared data, with a git-like vocabulary for the mental model only: **no git is run**.

## 2. Why (the four design-choice questions, answered)
- **One authority per fact:** the trunk row (or the branch row that overrides it) is the only place a value lives; every view is a projection that can be deleted and rebuilt.
- **Modular:** one small shared op and one row format, not a copy of the logic in every app (the house rule: pure shared piece = op + IPC, never header+link).
- **Scalable (the 10th consumer):** a new consumer adds a trunk path and a key rule, no new code. Branch size grows with the number of CHANGES, not the number of owners.
- **Footprint (OWNER's word):** a branch with no changes costs one pointer row. FOUND precedent: the 11 petslike books' `db/` copies were byte-identical to one shared trunk; commit `630a29268` removed 154 files / 1.4 MB, each verified byte-identical first (`&.widgits/_shared-lib/ops/trim_book_dbs.sh`). The standard turns that one-off trim into the default shape.
- **Long term:** it is the same replay model the chain, the router ledger and the game ledgers already use, so sharing across machines later is "ship the rows".

## 3. Three levels (use the smallest that fits)
| Level | Unit | Status | Example |
|---|---|---|---|
| **L1 file layer** | a whole file: the branch file, if present, replaces the trunk file | FOUND, built: per-book `db/` layers over the shared trunk (`trim_book_dbs.sh`) | a book's `db/items.pdl` |
| **L2 record layer** | one record by key: a branch row overrides the trunk record with the same key | FOUND, planned (HANDOFF-2026-10-28-PIECES-DB-CHAIN.md item 4: `db/cast.pdl` overrides by record, "today a layer file is a full copy of a 777-line actors.pdl") | an entity's stat tweak over the shared `actors.pdl` |
| **L3 row ledger** | an append-only file of change rows with ids; the current state is derived by replay | DOC, new (Texting contacts) | contacts, notes, tags, settings shared by many entities |
L2 and L3 are the same idea with a different storage; the op below serves both (an L2 layer file is just an L3 ledger whose rows are `PUT` records).

## 4. The rules (every consumer follows all of them)
1. **Row format:** `<id>|<op>|<key>|<value...>|<flags>`; `id = <ms>-<owner id>-<n>` (unique, sortable); ops `PUT`, `DEL`, `TRUNK` (pointer, below). Plain text, one row per line, no binary; `|` and newlines in values are escaped (house rule). Append-only: never edit or delete a row; a change is a new row.
2. **Pointer, not copy:** a branch starts with `TRUNK | <trunk path or id> | <head id the branch last synced to>`. A branch with no other rows is "identical to the trunk".
3. **Read** = replay trunk rows, then branch rows, **latest row per key wins**; a `DEL` hides the key (in a branch: for that branch only; in the trunk: for everyone without an override).
4. **Sync (fast-forward)** = apply trunk rows newer than the recorded head; it is idempotent (a second run changes nothing) and never rewrites branch rows, so a branch override is never clobbered.
5. **Publish** = copy a branch row to the trunk (with a new trunk id); only the **trunk owner** may write the trunk. A row from another owner lands on THAT owner's branch (a pull request in spirit), never silently on someone else's trunk.
6. **Private by flag:** a row flagged `private` is never published and never read by another owner's code. Notes, secrets and per-user preferences use it.
7. **Versioning for free:** a "version" is a head id; to see the data as of a version, replay up to it. Nothing keeps an old copy.
8. **Projections are disposable:** any cache built from the rows (a merged view, a search index) is derived, safe to delete, rebuilt byte for byte (NO-GHOST-DATA), and carries its own byte cursor (never mtime).
9. **Footprint rules:** a branch stores only rows that differ; **compaction** (`verify`/`compact`) may drop a branch row that equals the trunk row **only after verifying it is identical** (the `trim_book_dbs` rule); trunk and branch files are capped and rotate by generation like every runtime file (RUNTIME-FILE-GROWTH-STANDARD) and need their own row there.
10. **Permissions are data, not code:** who may write a trunk, who may read a branch, is a row in the trunk's own `TRUNK` header (owner id, readers). A reader never needs to open another owner's private files; it reads the published trunk.
11. **No ghost data:** a value that exists only in memory or only in a projection is a bug; the rows are the authority.

## 5. The shared op (BUILT: `trunk_op`, C only, no `.sh`; section 5 describes the intended verbs, the header of `trunk_op.c` is the exact usage)
`&.widgits/_shared-lib/ops/trunk_op.c` (op + file IPC, no linking):
`trunk_op <branch file> get <key>` | `list` | `put <key> <value> [--private]` | `del <key>` | `sync` | `publish <key>` | `status` (rows ahead/behind, bytes saved vs a full copy) | `verify` (replay twice, compare; refuse a trunk row edited in place: the ledger-replay refusal the stock+x engine already has) | `compact` (drop branch rows identical to the trunk, after verifying).
Exit 0 ok, 1 refused (message on stderr, nothing written), 2 usage. Pure file I/O, no network, no shell. Harness `trunk_op` (scratch house, real op) must prove: trunk row visible in two branches; a branch override changes only that branch; a later trunk edit does not clobber the override; publish copies a public row and refuses a private one; sync is idempotent; a trunk row edited in place is refused by `verify`; compaction keeps meaning and shrinks bytes (measured, reported); deleting every projection rebuilds identical output; the owner check blocks a non-owner trunk write. Each assertion gets a sabotage run against the real source (rebuilt binary verified), per the harness standard.

## 6. Candidates to move onto it (not done; each is its own slice, ask first)
| Data | Today | Gain |
|---|---|---|
| Texting contacts, notes, tags (**first user**) | designed (Texting 3.9, 3.10) | shared across the owner's entities, private notes |
| petslike book `db/` (L1, built) and the planned per-record layer (L2) | files / planned | the 777-line `actors.pdl` copy per book becomes a few rows |
| An entity's per-entity tweaks to shared `cast.pdl` / stats | full copies in places | footprint |
| Store catalog overrides (a user's local catalog vs the pinned repo) | `STORE_SOURCE` prefs row | a branch of the approved list |
| Reply policies (`reply.pdl`, Texting 3.5): a default trunk template, per-phone overrides | designed | "default editable reply" is literally a branch over a trunk |
| Phone prefs / themes per user over house defaults | separate files | one habit |
Rule for adopters: use L1 if whole files are shared and rarely differ, L2 if records differ, L3 if the data is a list that people add to. Do NOT use it for hard game state (that is a single ledger, no sharing) or for secrets (use private rows, or keep them out).

## 7. Anti-patterns (each a way this goes wrong)
Copying the trunk into the branch "for speed" (footprint lost, drift begins); editing a trunk row in place (history lost: add a row); letting a branch write the trunk; reading another owner's private rows; a merged-view file that something reads back as INPUT (ghost data); a branch with no pointer row (nobody can tell what it inherits).

## 8. What I read for this standard
FOUND: commit `630a29268` (message and file list), `HANDOFF-2026-10-28-PIECES-DB-CHAIN.md` lines 8 and 30, `trim_book_dbs.sh` exists, `DESIGN-CHOICE-TEST-AND-DECISION-LOG.md` headings (last entry D86). NOT read: the body of `trim_book_dbs.sh` and `petslike_core.c` (the L1 and L2 consumers), PAL-CHAIN-STANDARD. A search for `trunk_op` / `branch_op` files found none, so the op is new.
