# Pal-Chain: Meta-Chain, Sub-Chains, the Exchange, and Event-System-as-EVM

**Status: Part I is a RECONSTRUCTION of the real v1 system from its own
source code (the spec file that used to document this,
`PAL-CHAIN-STANDARD.txt`, is missing from the current tree — every
`chain_*.c` file cites it by name but it could not be located anywhere
in git history or on disk). Part II is NEW design, 🔴 not built,
written top-down per the owner's direct request: meta-chain first,
then sub-chains, then sync, then the exchange.**

**Also relevant:** `RUSSIAN_DOLL_HOUSE_DESIGN/ATTRITION_DIAGRAM.md`'s
BOOK:SYSTEM proposal — this doc's exchange-rate work is what would
actually give a BOOK:PAGE a value, per the owner's stated goal.

---

# Part I — What Actually Exists Today (reconstructed from code, 2026-09-29)

`44.xyz.01.00/041.pal-chain⛓️/` is a real, working, single-instance
proof-of-work blockchain. **14,000 real blocks mined** as of this
writing (`data/blockchain.txt`, 2.39 MB). This is not a prototype that
never ran — it has been actively mined. A duplicate copy also exists at
`@.apps/myne-qrypto/qtc/projects/pal-chain` (same ops, presumably a
fork/reference copy — not reconciled with the main copy in this pass).

## 1. Block format (from `chain_miner.c`)

```
BLOCK|<index>|<prev_hash>|<nonce>|<hash>|<timestamp>|<miner_wallet_id>|<tx_list>
```

- **Proof of work:** real SHA-256 over `index|prev_hash|nonce|tx_list`,
  accepted once the hex digest has N leading `'0'` characters
  (`DIFFICULTY_HEX_ZEROS`, default 5 = ~1M average tries, override via
  `CHAIN_DIFFICULTY_HEX_ZEROS` env var). This is a real, if coarse
  (4-bit-granular, not exact-bit) difficulty check — the code's own
  comment calls this "simpler to implement correctly... calibrate
  empirically" rather than a flaw.
- **Reward schedule:** halving every 1000 blocks, starting at 10,500
  millicones, hard-capped at 21,000,000 total millicones supply
  (`TOTAL_SUPPLY_MILLICONES`) — a direct, acknowledged homage to
  Bitcoin's own 21M/halving shape, scaled to this house's own unit
  ("millicones," not further defined in the code — worth nailing down
  what one millicone is worth in-house before the exchange work below
  depends on it).
- **Genesis / prev_hash of block 0:** a string of `'0'` characters
  (no real genesis block special-cased beyond that).
- **Miner stops entirely once the supply cap is reached** — idles
  rather than exiting, so a status UI can keep showing "cap reached" as
  a real terminal state.

## 2. Transaction format (from `chain_send.c`)

```
TX|<from_wallet_id>|<to_wallet_id>|<amount_millicones>|<timestamp>|<tx_id>
```

- **`tx_id` is a local dedup key only — NOT a cryptographic
  signature.** The code says this explicitly: v1 has no transaction
  signing. A malicious local process could forge a TX claiming to be
  from any wallet. **This is the single most important gap to close
  before any real value (per the owner's "buy/store things of value"
  goal) should move on this chain.**
- Balance is checked before a send by shelling out to `chain_balance.+x`
  (single authoritative derivation, not re-implemented per-op — a real,
  good convention already in place: never trust a second
  balance-computation path to agree with the first).
- A validated send appends the TX to **both** `data/pending_tx.txt`
  (this node's own mempool) and `net/outbox.txt` (for propagation to
  peers).

## 3. Wallets (from `chain_create_wallet.c`, `chain_login.c`)

- One `wallet.txt` per wallet, at `wallets/<wallet_id>/wallet.txt`:
  `wallet_id`, `password_hash`, `created_at`, `cached_balance`,
  `last_processed_block`.
- **37 wallets exist today, almost all synthetic test data**
  (`walletA_<random>`, `walletB_<random>` pairs — clearly automated
  test-harness fixtures) plus two real-looking ones (`jb`,
  `manualtest1`). **None are tied to a real user account or a pal
  entity yet** — that link does not exist anywhere in the current
  system.

## 4. Sync / mesh (from `palnet_peer.c`, `chain_inbox_watcher.c`)

- A real inbox/outbox file-pair mesh: `net/outbox.txt` (what this node
  has sent), `net/inbox.txt` (what a peer's `palnet_peer.c` wrote here).
- `chain_inbox_watcher.c` tails `inbox.txt`, and for each new line:
  - a `TX` not already known (deduped against both `pending_tx.txt` and
    `blockchain.txt`) gets added to this node's own mempool.
  - a `BLOCK` is only accepted if it's exactly this node's next expected
    index, its `prev_hash` matches this node's current tip, AND its
    claimed hash is **independently recomputed and verified** — never
    trusted verbatim from the peer. A block that doesn't match is
    silently dropped, not queued or retried.
- **v1 has no fork resolution at all** (the code's own words) — if two
  peers mine competing blocks at the same index, nothing reconciles
  them beyond "whichever one a given node saw first."
- **Real, known, already-documented constraint** (per this house's own
  `cross-machine-networking-plan` memory): `palnet_peer.c` hardcodes
  `127.0.0.1`. **The current mesh is same-machine only.** Any real
  multi-user or multi-machine chain work depends on that cross-machine
  networking plan landing first — it is designed but not started,
  separately from this doc.

## 5. Honest v1 gap list (source-cited, not guessed)

1. No transaction signing (§2) — the biggest one to close before real
   value moves.
2. No fork resolution (§4).
3. Same-machine-only networking (§4) — blocks any real "sub-chain on a
   different user's machine" scenario until fixed.
4. No wallet-to-account/entity binding (§3) — wallets today are
   free-floating, not owned by anything.
5. `PAL-CHAIN-STANDARD.txt` itself is missing — this document is now
   the closest thing to a current spec until/unless the original is
   recovered from an older backup or someone's local copy.

---

# Part II — Meta-Chain, Sub-Chains, and the Exchange (NEW, 🔴 not built)

Written top-down per direct instruction: meta-chain first, then
sub-chains, then how they sync, then the exchange rate mechanism that
depends on both existing.

## 6. The meta-chain (global, TEARIT-corporate owned)

**Proposal:** the existing `041.pal-chain⛓️` instance — the one with
14,000 real blocks already mined — **becomes the meta-chain**, rather
than starting a new chain from scratch. It already has the right
shape: one canonical, shared ledger, owned and operated by the house
itself (not any individual user), with a real fixed supply schedule.
Renaming/re-scoping it as "the meta-chain" is cheap; re-mining 14,000
blocks of history is not, and there's no reason to.

**What "TEARIT-corporate owned" should mean concretely:** the mining
and canonical-chain-tip authority for the meta-chain stays with
house-operated infrastructure — not something any individual user's
sub-chain can outvote or fork away from. This doesn't need a new
mechanism invented; it's the natural role of whichever node(s) the
house itself runs `chain_miner.+x` on, formalized as policy rather than
left implicit.

**Open, load-bearing question, not answered by existing code:** does
the meta-chain hold real user-to-user transactions directly, or does it
only hold sub-chain checkpoints/settlements (see §8)? Recommend the
latter — the meta-chain should be the slow, authoritative,
infrequently-written settlement layer, not where every small in-house
transaction lands. That's what sub-chains are for.

## 7. Sub-chains (per-user local blockchains)

**Proposal:** each user account gets its own local chain instance —
same `chain_miner.c`/`chain_send.c`/block-format shape as the
meta-chain, just running against that user's own `data/blockchain.txt`
under their own account directory, with the same PoW mechanism (tuned
to a much lower difficulty, since a personal sub-chain doesn't need
1M-hash-try security against an attacker who already controls the
machine it runs on — this is a real, sensible place to diverge from
the meta-chain's parameters, not a place to copy them blindly).

**Why sub-chains, not just accounts on the one meta-chain directly:**
per the owner's stated goal, entities (pals, not just user accounts)
should also be able to hold and spend value. A sub-chain per user gives
every pal under that user's account a natural, already-scoped place to
have its own wallet (§9) without every pal's every micro-transaction
needing to touch the shared, house-owned meta-chain. The meta-chain
only needs to know about a user's *aggregate* position, not every
internal transfer between that user's own pals.

**What a sub-chain genuinely needs that the meta-chain doesn't:**
- A binding to exactly one user account (see §9's retrofit section —
  this binding doesn't exist in the current wallet format at all).
- A much cheaper difficulty setting (already overridable via
  `CHAIN_DIFFICULTY_HEX_ZEROS`, so this is a config choice, not new
  code).
- The sync mechanism below, since a sub-chain's whole reason for
  existing depends on periodically reconciling with the meta-chain.

## 8. Sync between sub-chains and the meta-chain

**This is the part with no existing precedent to reuse** — the current
`palnet_peer.c`/`chain_inbox_watcher.c` mesh assumes every peer is a
symmetric copy of the *same* chain (flat gossip, not a two-tier
hierarchy). Sub-chain ↔ meta-chain sync needs a different shape:

**Proposed mechanism (checkpoint/anchor, not full gossip):**
1. Periodically (a fixed block count, e.g. every N sub-chain blocks —
   mirroring the existing `HALVING_PERIOD_BLOCKS` convention of a fixed
   cadence rather than a wall-clock timer), a sub-chain computes a
   single hash summarizing its own state since the last checkpoint —
   the existing block-hash-chaining mechanism already gives this for
   free; the sub-chain's own current tip hash already IS a summary of
   everything before it.
2. That tip hash gets submitted as a special transaction type on the
   meta-chain — an **anchor tx** — `ANCHOR|<sub_chain_owner>|<sub_chain_tip_hash>|<sub_chain_block_index>|<timestamp>`.
   This is a genuinely new tx type; today's format only has `TX` and
   implicit block records. Anchoring costs the user a small meta-chain
   fee (paid in millicones), which is also the natural place to define
   what a millicone is actually worth (§10) — the fee itself becomes a
   real, observable price point.
3. **This does NOT need cross-machine networking to work for the
   common case** (a user's own sub-chain runs on the same machine as
   their session) — the same-machine constraint noted in §4.5 only
   blocks *sub-chain-to-sub-chain* direct sync between two different
   users' machines, not a sub-chain anchoring into a house-run
   meta-chain the user's own machine can already reach. This is worth
   calling out because it means real anchor-sync work does NOT have to
   wait on the cross-machine networking plan — only true peer-to-peer
   sub-chain sync between two users does.

**What anchoring buys, concretely:** an auditable, tamper-evident
record that "this user's sub-chain was in exactly this state at this
point in house-time," without the meta-chain needing to store or
re-verify every individual sub-chain transaction. This is the same
checkpoint idea real Layer-2 systems use, scaled down to this house's
own file-based conventions instead of a general-purpose smart-contract
layer — deliberately simpler, matching this house's own "no framework
where a file will do" convention.

## 9. Wallets: auto-creation and the retrofit plan

**Confirmed requirement, direct from the owner:** every new account
gets a wallet address automatically at creation. Entities (pals) are
also allowed wallets. This is how both buy/store value on-chain.

**What needs to change in the current wallet format (§3) to support
this, concretely:**
- Add an `owner_kind` field (`user` or `entity`) and an `owner_id`
  field (the account UUID or the pal's own entity-id) to
  `wallet.txt`. Today's format has no ownership binding at all — this
  is the single most important structural change needed.
- Wallet creation should be triggered automatically by whatever process
  already creates a new user account or a new pal entity — not a
  manually-run `chain_create_wallet.+x` invocation, which is how every
  wallet today got made.

**The retrofit problem, named honestly, since the owner flagged this
as important:** 37 wallets and 14,000 blocks of transaction history
already exist with NO owner binding. Retrofitting this means:
1. Deciding what happens to the two real-looking existing wallets
   (`jb`, `manualtest1`) — do they get manually bound to whichever real
   account they actually belong to, or archived as pre-retrofit
   artifacts? This needs a human decision, not a script guessing.
2. The ~35 synthetic test wallets (`walletA_*`/`walletB_*`) are almost
   certainly safe to leave unbound or archive outright — they look like
   automated harness fixtures, not real value-holding accounts. Confirm
   this by checking whether any of them show nonzero `cached_balance`
   or real transaction history before archiving, rather than assuming.
3. **Going forward, never let a new wallet exist without an owner
   binding** — the moment auto-creation ships, the retrofit boundary is
   final: every wallet created before that moment is "legacy,"
   everything after has a real owner from birth.

## 10. The crypto exchange and BOOK:PAGE exchange rates

**This is the part that depends on everything above existing first —
building it before §6-9 land would mean pricing something that doesn't
have a stable supply/ownership model yet.**

**What an "exchange rate" actually needs to be computed from, once the
above exists:**
- A **meta-chain millicone** has a value the house itself can set or
  let float (policy decision, not a technical one — flag for the
  owner, not assumed here).
- A **sub-chain's own balance**, converted to meta-chain millicones via
  whatever rate is set, gives that user's BOOK a real, computed value —
  this is literally what "BOOK:PAGE exchange rate" means once wallets
  are bound to accounts (§9) and sub-chains anchor into the meta-chain
  (§8): the anchor record plus the sub-chain's own balance ledger is
  enough to derive a real number.
- **Cross-sub-chain exchange** (user A's sub-chain value against user
  B's) should route through the meta-chain as the common settlement
  layer, not direct sub-chain-to-sub-chain trading — this avoids
  needing N-squared exchange rates between every pair of users, and
  matches the meta-chain's proposed role in §6 as the settlement layer.

**Recommendation on the actual question asked — build in network-tb now,
or design doc first:** **design doc first, further than this one.**
This document establishes the shape (meta-chain, sub-chains, anchor
sync, exchange-through-settlement) but deliberately leaves open the
millicone's real-world value policy, the anchor-fee economics, and the
exact UI a network-tb "crypto exchange" widget would need — those need
their own pass, informed by whichever of §6-9 gets built first. Wrapping
the *existing* single-instance chain into a network-tb window (the
already-planned `IRC-FORUM-CHAIN-HQ-WINDOWS.md` Phase 2 work) can
proceed independently and in parallel — it doesn't depend on any of
Part II, since it's just giving the current chain a real window, not
building the meta/sub-chain split.

## 11. Should the whole event system work like an EVM — contracts, gas?

**Direct answer: yes, this makes real sense, and it maps onto things
that already exist better than a from-scratch EVM clone would.** This
isn't a stretch analogy — walk the mapping piece by piece, honestly,
including where it doesn't fit.

**Contracts → compiled event pages, already real.** An Ethereum
contract is deployed bytecode, deterministic, tied to an address. This
house's own event pages are already exactly that shape:
`event.ir.pdl` → `event.pal` → `cmd_N.sh`, compiled once, deterministic,
tied to the entity that owns them (per `ROBOT-CHAT-BLUEPRINT.md` §2.2's
own real, working example). **The house didn't need to invent
contracts — event pages already are them.** What's missing is only the
economic metering layer, not the execution model.

**Contract address → entity ID, already real.** The host-context bridge
(`MUCHI_TARGET_ENT`, `kh_inventory_host_dir()`) already resolves which
entity an event actually runs against. That's already address
resolution in EVM terms — `$ENT` is the contract address a call
targets.

**The virtual machine → prisc+x, already real, not hypothetical.** This
is the most important piece of the mapping, and it's not a metaphor:
this house already runs a real RISC-V virtual machine, `prisc+x`, for
`.pal` script orchestration (per `PRISC-OPS-ARCHITECTURE.md` and this
house's own standing convention: `.pal` files ARE real assembly, not a
scripting language). **An event-system EVM doesn't need a new VM built
— it needs gas metering added to the VM that already exists and already
executes real instructions for exactly this kind of work.**

**Gas → a real, useful fix for an already-documented bug class, not
just an EVM nicety.** This house's own memory record
(`prisc-x-popen-custom-op-freeze`) already documents a real incident: a
custom op's `popen()` call with no timeout froze an entire pal VM,
fixed by a fork+exec+waitpid+watchdog pattern. **Gas metering is the
general-purpose version of that exact fix** — cap the instruction count
or wall-time an event/contract call is allowed before it's killed and
charged for what it used, rather than hand-fixing one popen call at a
time as new hangs get discovered. This isn't just borrowing Ethereum's
idea for flavor — it directly prevents the same class of freeze this
house has already been bitten by once, generalized.

**Transactions → the missing piece, and this is where it closes the
loop from earlier in this thread.** The owner's original ask — "every
event should post to a transaction ledger" — is EXACTLY what an
EVM-style system does by construction: every contract call IS a
transaction, gas-metered, recorded, on a real ledger. Under this
framing, that earlier ask isn't a separate feature bolted onto the
event system — **it's what "making the event system work like an EVM"
already means.** One design, not two:
- An event COMMAND invocation = a contract call.
- Its cost (real instruction count or wall-time from `prisc+x`,
  converted to a millicone cost) = gas, paid from the invoking
  entity's or user's sub-chain wallet (§7, §9).
- The invocation, its cost, and its effect = the TX record, written to
  that user's sub-chain (§7) — not the meta-chain directly, matching
  §6's proposed division of labor (meta-chain = settlement, sub-chain =
  everyday activity).
- State changes → this house's existing file-backed state IS the state
  trie, already more directly auditable than EVM's own abstract
  Merkle-Patricia state (a real, human-readable file diff beats an
  opaque state root for a house whose whole design philosophy is
  "everything is a readable file").

**Where the analogy genuinely doesn't fit, stated honestly:**
- EVM contracts are adversarially trusted — any address can deploy
  hostile bytecode, so gas exists partly to bound an attacker's cost.
  This house's event pages are authored by known agents under
  human-approval gates (per the whole colab-hai/open-hai approval
  philosophy already documented elsewhere in this thread) — the
  adversarial-execution threat model is much weaker here. Gas is still
  worth having (for the freeze-prevention reason above), but don't
  import EVM's full adversarial-security posture wholesale; this
  house's real threat model is closer to "prevent an honest mistake
  from hanging a VM" than "prevent a hostile contract from draining
  funds."
- EVM has no house-law equivalent to "DESCRIBE, never CLASSIFY" — that
  constraint is specific to this house's AI architecture (§0 of
  `AGENT_ROADMAP_ANSWERS.md`) and has nothing to do with the
  event-as-contract mapping. Keep the two designs conceptually
  separate even though both eventually touch the same ledger.
- Real Ethereum gas pricing is market-driven (gas price bids, block
  space auctions). Nothing here needs that complexity yet — a fixed or
  simply-tunable gas-cost-per-instruction (mirroring how
  `DIFFICULTY_HEX_ZEROS` is already a simple tunable env var, not a
  market) is the right starting scope.

**What this means for the build order below:** step 5 (anchor sync)
and the broader "every event posts a transaction" ask are now the same
piece of work as adding gas metering to `prisc+x` and wiring event
COMMAND invocations to charge it — not two separate features. Sequence
accordingly.

## 12. Suggested build order

1. **Recover or formally rewrite `PAL-CHAIN-STANDARD.txt`** — every
   `chain_*.c` file cites a spec that no longer exists; this document
   (Part I) is a stopgap reconstruction, not a replacement for a real,
   deliberately-written standard doc.
2. **Add transaction signing** (§2's named gap) — before any real value
   moves on this chain, this is the actual security prerequisite, not
   optional hardening.
3. **Wallet ownership binding + auto-creation** (§9) — needed before
   sub-chains mean anything, since a sub-chain is defined by "the chain
   belonging to this account."
4. **Sub-chain instantiation** (§7) — spin up the per-account chain
   using the existing, proven single-instance code at a lower
   difficulty, bound to an owner.
5. **Gas metering in `prisc+x`** (§11) — instruction-count or
   wall-time cap per event/COMMAND invocation, generalizing the
   already-fixed popen-freeze bug class into a standing VM feature,
   not a one-off patch the next time something hangs.
6. **Event-invocation-as-transaction** (§11, formerly listed separately
   as "anchor sync" in an earlier pass of this doc) — every COMMAND
   call charges gas from the invoking entity's/user's sub-chain wallet
   and writes a real TX record. This is the actual mechanism that
   satisfies "every event posts to a transaction ledger" — not a
   separate feature from gas metering, the same one.
7. **Exchange rate / BOOK:PAGE value derivation** (§10) — only once 1-6
   are real; this is a read/derive step on top of an existing,
   trustworthy ledger, not something to build against placeholder data.
8. **Retrofit pass** (§9) — apply once, deliberately, with a human
   decision on the two real legacy wallets, after 1-6 are stable enough
   that "legacy" has a fixed, final boundary.

---

*Written 2026-09-29, reconstructing Part I directly from
`041.pal-chain⛓️`'s own source (`chain_miner.c`, `chain_send.c`,
`chain_create_wallet.c`, `chain_inbox_watcher.c`, `palnet_peer.c`) since
`PAL-CHAIN-STANDARD.txt` could not be located, and designing Part II
top-down per direct instruction: meta-chain, then sub-chains, then
sync, then the exchange. Nothing in Part II is built — treat it as a
starting shape for real design work, not a finished spec.*
