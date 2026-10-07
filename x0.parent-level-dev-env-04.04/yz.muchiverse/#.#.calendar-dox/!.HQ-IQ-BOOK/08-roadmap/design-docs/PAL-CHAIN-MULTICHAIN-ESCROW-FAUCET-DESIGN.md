# pal-chain: several chains, a faucet, and escrow (spec)

Status: SPEC for a build in the `claude-alpha` branch (written 2026-10-07 by claude); the build report is appended at the end when done.
Owner (2026-10-07): "make chain escrow and faucet. We will do fake ads for now. Yes real mining and yes daily cap. ... (faucets) let's make different chains: 3: cones; and then some test chains, user-created chains or whatever, and just lab around and figure stuff out. Faucets for different test chains."
Builds on: `PLAY-ECONOMY-POT-FAUCETS-DESIGN.md`, `AUCTION-SCREEN-DESIGN.md` (gate), `GAME-SESSIONS-SEATS-RATINGS-LOBBIES-PATTERN.md`.

## 1. What exists (read in `041.pal-chain/ops/`)
- Every op finds its files through **`PRISC_PROJECT_ROOT`** (default `.`): `<root>/data/blockchain.txt`, `<root>/data/pending_tx.txt`, `<root>/wallets/<id>/wallet.txt`, `<root>/net/*`. So **a chain is a project root**: running the same binaries with a different root is a different chain. Nothing needs refactoring to get many chains.
- Lines: `BLOCK|n|prev|nonce|hash|ts|miner|` followed by its transactions; a transaction is `TX|from|to|amount_millicones|ts|tx_id`. Balance = replay of blocks (`chain_balance`, with a per-wallet `cached_balance`/`last_processed_block` optimization); the miner pays a block reward (`INITIAL_REWARD_MILLICONES 10500`, halving every `HALVING_PERIOD_BLOCKS 1000`, `TOTAL_SUPPLY_MILLICONES 21000000`: these constants are **duplicated in `chain_balance.c`, `chain_miner.c`, `chain_inbox_watcher.c`** by the house no-shared-headers rule). Difficulty from env `CHAIN_DIFFICULTY_HEX_ZEROS`.
- **No signing** (`tx_id` is a dedup key), **one tx type**, no escrow, no faucet.

## 2. The three kinds of chain (owner)
1. **cones**: the real chain: the existing `041.pal-chain` root, unchanged. No faucet (cones only by mining). Stays behind the gate for real value (signing etc.).
2. **test chains**: lab chains created at will, each its own root, with a faucet and easy mining.
3. **user chains**: a user creates their own chain the same way (a test chain they own). Same code path as 2; difference is only who runs `chain_new`.
Test/user chains live under `041.pal-chain/chains/<chain_id>/` (runtime data gitignored; the skeleton and `chain.pdl` may be tracked).

## 3. `chain.pdl` (per chain root; absent = today's constants, so the cones chain is unchanged)
```
CHAIN   | id                         | <chain_id>
CHAIN   | kind                       | cones | test | user
REWARD  | initial_millicones         | 10500
REWARD  | halving_blocks             | 1000
REWARD  | total_supply_millicones    | 21000000
MINING  | difficulty_hex_zeros       | 1                # test chains: easy
MINING  | daily_cap_blocks_per_wallet| 20               # 0 = unlimited (the "daily cap")
FAUCET  | enabled                    | 1                # 0 on cones, always
FAUCET  | amount_millicones          | 1000
FAUCET  | cooldown_seconds           | 3600
FAUCET  | daily_cap_millicones_per_wallet | 5000
ESCROW  | enabled                    | 1
```
The three ops that replay blocks all read the same reward rows (duplicated parsing, no shared header). **The cones chain gets no `chain.pdl` until the owner picks its numbers** (so its behavior is unchanged and its daily mining cap stays off until chosen).

## 4. Faucet
`chain_faucet <wallet_id>`: refuses unless `FAUCET enabled=1` (a root without `chain.pdl`, or kind `cones`, always refuses). Cooldown and daily cap come from an append-only **`data/faucet_ledger.txt`** (`CLAIM|wallet|amount|ts`, cursor-read, never mtime). On success it appends a mint transaction `FAUCET|wallet|amount|ts|tx_id` to `pending_tx.txt` (mined into a block like any tx); `chain_balance` counts `FAUCET` as +amount to the wallet. Faucet mints are **inflation by design on test chains** (their supply is not capped by the reward schedule); cones is unaffected.

## 5. Escrow (the missing transaction types)
Three new transaction lines (mined like `TX`):
```
LOCK   | escrow_id | from | amount | agent | ts | tx_id      from's balance -= amount (moved into escrow)
PAYOUT | escrow_id | by   | to   | amount | ts | tx_id      to's balance += amount (valid only if by == the lock's agent)
REFUND | escrow_id | by   | to   | amount | ts | tx_id      to's balance += amount (same rule)
```
- **Spendable balance** = received - sent - locked + paid out/refunded. Locked funds cannot be spent (`chain_send` already asks `chain_balance`, so it follows).
- **Invariant:** for each escrow id, `payouts + refunds <= locked`. A rake is a `PAYOUT` to the burn wallet `_burn` (never spendable; supply sink).
- **Authority until signing exists:** `agent` is an **honor field** (the session's server wallet, per the phone design where the server holds wallet credentials). Anyone can forge a `by`; so escrow is **trustworthy only on local/test chains** and for real cones it stays behind the gate. State this in every op's header.
- Ops: `chain_escrow lock <id> <from> <amount> <agent>`, `payout <id> <by> <to> <amount>`, `refund <id> <by> <to> <amount>`, `status <id>` (prints locked, paid out, refunded, remaining from a full replay). Issue-time checks (sufficient spendable balance, `by == agent`, `amount <= remaining`) and **inclusion-time checks in the miner** (it must not include an invalid LOCK/PAYOUT/REFUND), because a block cannot be un-mined.

## 6. Daily mining cap
`chain_miner` counts the blocks already mined by this wallet in the current UTC day (block `ts` + miner field in `blockchain.txt`) and stops mining when `MINING daily_cap_blocks_per_wallet` is reached (0 = no cap). Real proof of work, bounded CPU and bounded supply per wallet.

## 7. `chain_new`
`chain_new <chain_id> [--kind test|user] [--difficulty n] [--cap n] [--faucet-amount n] [--cooldown s]`: creates `chains/<id>/{data,wallets,net}` and a `chain.pdl`; refuses an existing id (never overwrites); chain ids are `[a-z0-9_-]+` only; `cones` is reserved.

## 8. Harness (pal, scratch roots only; never the real chain)
Cases for: `chain_new` (creates, refuses duplicate/reserved/bad id); faucet (claim ok, cooldown refused, daily cap refused, disabled on cones/no chain.pdl, append-only ledger); escrow (lock reduces spendable balance, a locked balance cannot be sent, payout to winner + rake to `_burn`, **conservation**: minted = sum of balances + locked + burn, refund path, over-payout refused, wrong `by` refused, status numbers); mining cap (stops at cap); legacy `TX` and an unconfigured root unchanged; replay equals cached balance.

## 9. Not done by this spec
Signing; cross-machine peers; cones faucet (never); converting test-chain coins to cones (never decided); escrow trust on cones. Open for the owner: the cones daily mining cap value; faucet defaults for test chains; whether user chains get a cap on how many one user may create.


<!-- appended from claude-alpha at merge 2026-10-07: sections present only there (build reports) -->

## 10. Build report (claude, branch claude-alpha, 2026-10-07)
Built: `chain.pdl` parsing in `chain_balance`, `chain_miner`, `chain_inbox_watcher` (absent file = old constants); new ops `chain_new`, `chain_faucet`, `chain_escrow` (lock/payout/refund/status + an extra `audit` verb); FAUCET/LOCK/PAYOUT/REFUND replay in `chain_balance`; inclusion-time validation, daily cap and `data/rejected_tx.txt` in `chain_miner`; `scripts/build.sh` lines for the three new ops. Harness `harness/chain_escrow_faucet.pal` + `cases/chain_escrow_faucet.pdl`, result `VERDICT|PASS|passed=132|failed=0`; a copy with one wrong expectation gave `VERDICT|FAIL|passed=131|failed=1` (deleted).

Deviations and findings (honest list):
- **Cached balance vs escrow works**: every new line is a +/- on one wallet, so `cached_balance`/`last_processed_block` stays valid; validity is enforced at issue and inclusion time, not at replay. Harness checks incremental == full replay.
- **Legacy bug found, left on cones**: `chain_balance` skips block 0 for every wallet (fresh wallets start at `last_processed_block=0` and the replay skips index <= that), so block 0's reward is never counted. On a root WITH `chain.pdl` a wallet at last=0 and balance 0 replays from -1 (idempotent), so conservation holds; a root without `chain.pdl` keeps the old behaviour (the harness asserts it). Owner decision needed before touching cones.
- `chain_miner` got `--blocks N` (mine N then exit; exit 3 if stopped by the daily cap or supply cap) because prisc cannot kill a daemon mid-case. No flag = daemon as before.
- `chain.pdl` MINING difficulty wins over env `CHAIN_DIFFICULTY_HEX_ZEROS` when the file sets it (a chain defines its own difficulty); env still applies with no `chain.pdl`.
- Escrow is disabled unless `chain.pdl` has `ESCROW enabled = 1` (a root without chain.pdl has no escrow); `FAUCET`/`TX from _burn` are only accepted at inclusion on a chain.pdl chain with faucet enabled and the exact configured amount.
- Issue-time checks run over the mined chain plus the lines already pending (a lock and its payout can be queued back to back). `chain_escrow status` counts the mined chain only.
- `_burn` needs no wallet file (full replay, never cached); `chain_send` now refuses `from=_burn`. `chain_new` also creates an `ops` symlink to the parent root's `ops/` (chain_send shells out to `./ops/+x/chain_balance.+x`) and takes an extra `--faucet-daily-cap`.
- The miner's pending-removal is now exact-line (was substring match) with a 4096-line buffer (was 64); invalid lines are dropped from pending and logged to `data/rejected_tx.txt`.
- The faucet requires the wallet to exist locally; `_`-prefixed ids are refused.
- Conservation is checked through `chain_escrow audit` (replay) and cross-checked per wallet against `chain_balance`; it is partly structural (the escrow invariant is what the miner enforces).
- Open: `chain_inbox_watcher` queues the new tx types and uses the chain.pdl difficulty, but does NOT validate escrow rules in blocks received from peers; no test for the watcher; the bank seed for the harness is not written (the pal has no bank exec line); the UTC day boundary is not faked in tests (an old-day block is hand-written instead).
