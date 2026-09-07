# Making the File-Lineage Blockchain offer real, ASAP — house-tech integration

**Author:** Claude (technical assessment), 2026-09-07. Grounded in the
actual code under `44.xyz.01.00/041.pal-chain⛓️/` and the house's
file-based-state architecture — not aspirational. Pairs with the
founder's `^.Executive Blueprint - Non-Invasive File Lineage Blockchain
Solution V2.md`.

**Bottom line:** ~70% of the blueprint already exists and runs. A
deployable v1 (watch a client's files → anchor hashes on a permissioned
ledger → verify + produce a court-style receipt) is **~2–3 focused
weeks**, which independently confirms the blueprint's own "2–4 weeks /
zero disruption" claims.

---

## 1. What already exists in `041.pal-chain⛓️` (verified in code)

| Capability | Where | State |
|---|---|---|
| Plain-text blockchain, one line per block: `BLOCK\|height\|prev_hash\|value\|hash\|ts\|wallet\|` | `data/blockchain.txt` (14,000 blocks live) | working |
| SHA-256 hashing + hex-zero-prefix Proof-of-Work difficulty | `ops/chain_miner.c`, `ops/chain_inbox_watcher.c` | working |
| Prev-hash chain linking + **on-receipt block validation** ("never trusted verbatim") | `ops/chain_inbox_watcher.c` | working |
| Pending-tx pool + dedup by tx_id against pool *and* chain | `data/pending_tx.txt`, `chain_inbox_watcher.c` | working |
| Wallets / identity (SHA-256-derived IDs), login | `ops/chain_create_wallet.c`, `chain_login.c` | working |
| **Peer-to-peer block/tx gossip** — HELLO handshake, node-id tracking, dedup, non-blocking sockets, append-only outbox/inbox | `ops/palnet_peer.c` (~540 LOC) | working (v1, static peer set) |
| **Persistent file-watcher daemon** — tails an append-only file by line-cursor, validates + applies new lines, tracks its own read position | `ops/chain_inbox_watcher.c` (~330 LOC) | working — *this is the skeleton of the file-anchor watcher* |
| Snapshot-before-change lineage pattern (`blockchain.txt.pre-harness-run-<ts>` + `proof/harness-<ts>/`) | repo history | already a working demo of file-lineage proof |

Plus the platform itself: **every artifact is a human-readable flat
file, append-only, cursor-tracked.** "Non-invasive, works alongside your
existing file system, raw data never moves" is not a feature to build —
it is the house's architecture.

## 2. Gap analysis — blueprint deliverable → what's missing

| Blueprint asks for | Gap | Effort |
|---|---|---|
| Anchor a **file's** hash + metadata (not a currency amount) | Add an `ANCHOR` tx type: `ANCHOR\|tx_id\|file_sha256\|size\|mtime\|rel_path\|prev_file_hash\|actor\|sig\|ts\|`, mined into a block like any tx. Block hash already covers contents → immutable + chain-linked once mined. | ~1 day |
| **"Custom local hash watcher"** (Tier 2 deliverable) | Fork `chain_inbox_watcher.c` → `file_anchor_watcher.c`: poll/inotify a client directory tree, SHA-256 each new/changed file, emit an `ANCHOR` tx to `outbox.txt` (peer gossips it to the permissioned nodes). Reuses the cursor / dedup / sha256 / signal-handling code verbatim. | ~2–3 days |
| **Verify lineage / audit trail** | `verify_file.+x <path> [--receipt]`: hash the file, scan `blockchain.txt` for matching `ANCHOR`(s), re-validate the chain from genesis, print `{first_seen_height, timestamp_utc, all_versions[], chain_valid}`. `--receipt` → signed JSON + a rendered one-page PDF (reuse the house's own frame→PNG/PDF render path). | ~3–4 days |
| **Permissioned** (not open mining) | `validators.txt` allowlist of wallet IDs; `palnet_peer` HELLO already carries node_id — gate block acceptance in `chain_inbox_watcher.c` on "mined/signed by an allowed validator". Client runs 1 node; we run 1–2 witness nodes. | ~1–2 days |
| **Non-invasive install** (Tier 2) | One script that drops `file_anchor_watcher` + `palnet_peer` + a read-only CLI/tiny local web audit view next to the client's files and touches nothing else. `systemd --user` unit or a `nohup` supervisor. | ~2–3 days |
| **Architectural Audit & Blueprint** (Tier 1, $5–10k) | No build needed — it's discovery + a written report. The `verify_file` + watcher become the pilot artifact that closes Tier 1 → Tier 2. | 0 (services) |

**Total to deployable v1: ~9–13 working days → 2–3 calendar weeks** with
testing and a client pilot. Matches the blueprint's "2–4 weeks."

## 3. Why the "non-invasive / zero-disruption" claims hold up

- Raw files never move or open — only `sha256(file_bytes)` + `stat()`
  metadata are read. The client's storage, permissions, and apps are
  untouched.
- The ledger is append-only plain text the client can read, `grep`,
  back up, and audit with no special tooling — and can walk away from
  (the receipts stay verifiable with nothing but `sha256sum` + the
  exported chain file).
- No database migration, no schema, no agent in the data path. The
  watcher is a sidecar; if it stops, nothing breaks — it just resumes
  from its line-cursor.
- Court-admissibility story: each version's hash is fixed in a block
  whose own hash chains to every later block; tampering with block N
  invalidates N+1…tip, which the witness node(s) reject on gossip.

## 4. Ready-made demo / pilot assets

- **The house's own history**: `blockchain.txt.pre-harness-run-<ts>`
  snapshots + `proof/harness-<ts>/` dirs are a real, dated,
  before/after file-lineage record — screen-record `verify_file`
  against them.
- A 5-minute live demo: drop 3 PDFs in a watched folder → show the 3
  `ANCHOR` blocks appear → edit one PDF → show a new version block →
  hand-edit `blockchain.txt` → show the witness node rejecting it and
  `verify_file` reporting `chain_valid: false`.

## 5. Honest caveats (say these on a sales call)

- `palnet_peer` v1 has a **static peer set** and no dynamic discovery —
  fine for a 2–3 node permissioned witness setup, not a public network.
- PoW difficulty is currently a **documented target, loosely enforced**
  — for a permissioned ledger, switch to "valid iff signed by an
  allowed validator" and drop PoW entirely (simpler, faster, correct
  for this trust model).
- No formal third-party security audit of the crypto code yet — budget
  one before a regulated-industry production deployment (and price it
  into Tier 2 / a Tier-1 finding).
- Signature scheme: wallet IDs are SHA-256-derived, not full public-key
  signatures yet — add ed25519 signing of `ANCHOR` txs before "court-
  admissible" is a claim rather than a goal (~2–3 extra days; do it).

## 6. Suggested sequencing

1. **Week 0 (services, no code):** land one Tier-1 Architectural Audit.
   Use it to spec the exact watched paths + validator topology.
2. **Weeks 1–2:** `ANCHOR` tx type + `file_anchor_watcher` +
   `verify_file --receipt` + `validators.txt` gating + install script.
   Ed25519 signing folded in here.
3. **Week 3:** pilot on the audit client's real (non-sensitive)
   directory; produce the first signed receipts; convert to a Tier-3
   retainer.
4. Only then generalize (multi-tenant, dashboard polish, dynamic peer
   discovery) as retainer-funded work.

## Related
- `../../STRATEGY/pricing.md` (map the effort above to the tiers)
- `../../STRATEGY/legal.md` ("court-admissible" needs the ed25519 +
  audit caveats resolved first)
- `44.xyz.01.00/041.pal-chain⛓️/walkthru-j30.txt`,
  `phase2-module-split-report.txt` — the chain's own current state
