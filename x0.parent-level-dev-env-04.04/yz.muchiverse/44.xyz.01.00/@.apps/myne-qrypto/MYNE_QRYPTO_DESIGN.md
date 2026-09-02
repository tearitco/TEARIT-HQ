# ⛏️💎 MYNE-QRYPTO — A MY-CHARA-SHAPED GAME ABOUT MINING, ASICS, AND EXCHANGE

> **PURPOSE:** MYNE-QRYPTO IS ITS OWN PLAYABLE GAME — same character/turn/screen playstyle as `my-chara-txt` and `genesis-txt`, but the domain is cryptocurrency instead of farming: **Mine** (choose QTC or QTH, using owned ASICs), **Store** (buy more ASICs — more hash rate), **Exchange** (trade QTC/QTH with other players/NPCs). It is NOT a service that `my-chara-txt` calls into, and it is NOT primarily a headless audit tool — it's a sibling game in the same family, reusing the same proven CHTPM shell.
>
> **⚠️ MAJOR CORRECTION (2026-08-02):** This doc originally framed myne-qrypto as "not a game," a headless blockchain service `my-chara-txt` calls into via file-based request/response IPC. **That's wrong, per direct user correction.** The real vision: myne-qrypto has its own player, its own `main.chtpm` game shell, its own turns — structurally a sibling of `my-chara-txt`/`genesis-txt`, not a backend they depend on. §1, §5 (renamed), §6 (renamed), and §8 below are rewritten to reflect this. §3/§4 (the QTC/QTH chain rules themselves) are UNCHANGED — those were always correct, they're just now the **backend engine** the player-facing game shell calls into, not something exposed directly to the player.
>
> **What's already real and built (2026-08-01), and stays exactly as-is:** `myne-qrypto/qtc/` is a full, working, wholesale-unmodified copy of `041.pal-chain⛓️` (real SHA-256 PoW, real wallet crypto, its own complete CHTPM UI) — this is now understood as the **backend engine**, not the player-facing game. Its own login/wallet/mining-status screens are dev/debug tooling (useful for auditing the chain directly), not what a player of myne-qrypto-the-game ever sees. It has a real, passing test harness (`qtc/test-harn-same/`, 21 checks across two scenarios — see `#.haiku+/HANDOFF_NEXT_SESSION.md` §10.2) proving the engine itself works. **None of that testing/code needs to change** — what's missing is the game shell ON TOP of it.
>
> **Read `@.apps/my-chara-txt/MY_CHARA_TXT_DESIGN.md` §4 first** — the CHTPM pattern (named `.chtpm` screens + `href` nav + `${piece_methods}` from a `piece.pdl` + one PAL module per screen + a shared `*_menu_input`/`*_compose_frame` op pair) is now proven THREE times over (wsr-pal, muchi-pals, pal-chain) and is exactly what myne-qrypto's own game shell should be built as — a fourth application of the identical, working pattern, not a new design.

---

## 📖 1. THE VISION

**A single character plays myne-qrypto exactly the way they'd play `my-chara-txt`:** one player, one `main.chtpm` screen, turn-based, real CHTPM nav — but the actions are themed around crypto instead of farming.

**Player-facing actions (the game shell — NOT built yet, this is the real next step):**
- 🌾→⛏️ **Mine** (was Farm) — choose QTC (PoW) or QTH (PoS), mine using owned ASICs. Faster/better odds with more ASICs.
- 🛒 **Store** — buy more ASICs (increases hash rate / mining odds). Same shape as `my-chara-txt`'s Store screen, different inventory (ASICs instead of seeds).
- 💱 **Exchange** — buy/sell QTC/QTH at market price, or post buy/sell orders (same pattern as `genesis-txt`'s exchange design, or the simpler NPC-price model from `my-chara-txt`'s original single-player Store — open question, see §10).
- 📦 **Inventory** — view owned ASICs + QTC/QTH balances.
- ⏳ **EndTurn** — advance time.

**Two blockchains, both real, both the BACKEND the game shell's Mine action calls into (never shown to the player directly):**

- **QTC (Quantum Coin):** Scarcity-based PoW chain. **Already built and tested** — `myne-qrypto/qtc/` is a wholesale, unmodified copy of `041.pal-chain⛓️` (real SHA-256 PoW, halving reward schedule, hard supply cap). See §3.
- **QTH (Quantum Token):** Stake-based PoS chain + PAL-script smart-contract VM. **Not built yet** — genuinely new design, no existing precedent to reuse (unlike QTC). See §4.

**Why the backend engines stay separate processes from the game shell?**
- `qtc/`'s own CHTPM stack (login/wallet/mining-status screens) is real, working dev/debug tooling for auditing the chain directly — useful to keep, just not what a player of myne-qrypto-the-game ever navigates to.
- The blockchain ledgers (`data/blockchain.txt`, `wallets/`) stay genuinely independent audit trails — myne-qrypto's own game ledger (character state, ASIC inventory) and the blockchain's own ledger (blocks, wallet balances) can be replayed and cross-checked separately. This auditability is real and worth keeping, it's just not the PRIMARY framing anymore — the primary framing is "a fun, playable game," same as its siblings.
- Widgets (mining UI, wallet UI, exchange UI) built for myne-qrypto's own game shell are **reusable in `genesis-txt`** once that goes multiplayer with crypto too.

**How the game shell talks to the backend engine:** direct shell-out via `popen()`/`system()` to the backend's own compiled ops — the SAME real, proven pattern `chain_menu_input.c` itself already uses to call `chain_miner.+x`/`chain_balance.+x` (see `041.pal-chain⛓️/ops/chain_menu_input.c` lines ~352-405 for the exact "launch background daemon, guard against double-launch via `.pid` file, track it, stop with SIGTERM" shape). **Not** a request/response file-polling IPC — that was this doc's original (wrong) design, revised now that real precedent for the shell-out approach exists and is proven. See §6 (rewritten).

---

## 🏗️ 2. REFERENCE SOURCES & ARCHITECTURE

| Source | Pattern we reuse / learn from |
|--------|------|
| **my-chara-txt** (`MY_CHARA_TXT_DESIGN.md` §4, and its now-built+tested implementation) | THE game shell pattern to copy directly: `.chtpm` screens, `piece.pdl` METHOD dispatch, per-screen PAL module, shared `*_menu_input`/`*_compose_frame` op pair, `test-harn-same/` regression harness |
| **041.pal-chain⛓️** (now living, unmodified, at `myne-qrypto/qtc/`) | PoW chain structure, difficulty algorithm, block validation, `blockchain.txt` format, wallet ledger — the backend engine QTC's Mine action shells out to |
| **041.pal-chain⛓️'s own `chain_menu_input.c`** (CHAIN_START_MINING/CHAIN_STOP_MINING handlers) | THE proven shell-out pattern: background-launch a backend op via `popen()`/`system()`, guard double-launch with a `.pid` file, stop with SIGTERM — this is how myne-qrypto's own `myne_menu_input.c` should call into `qtc/ops/+x/chain_miner.+x` |
| **014.wsr-pal💸️📌️+2** | Exchange matching, price discovery (QTC/QTH prices emerge from trading volume) |
| **Ethereum / EVM** | Inspiration for QTH: stake model, gas metering, contract execution (but simpler: PAL instead of Solidity) |

**Key design principle:** QTC's BACKEND is proven existing tech (`041.pal-chain⛓️` works, tested, 21/21 harness checks passing). The GAME SHELL that wraps it is the same proven `.chtpm`/`piece.pdl`/menu_input pattern `my-chara-txt` already validated live. QTH is the one genuinely new piece — both its own backend chain rules (§4, unchanged from before) AND, once built, a game-shell screen that shells out to it the same way QTC's screen shells out to `qtc/`.

---

## ⛓️ 3. QTC (QUANTUM COIN) — SCARCITY-BASED PoW

### **Reuses 041.pal-chain directly**

QTC is **041.pal-chain unmodified** (or with minimal tuning for game difficulty):

- **PoW Algorithm:** Proof-of-Work, difficulty adjusts based on target block time (e.g., 1 block per 30 seconds).
- **Supply:** Fixed max supply (e.g., 21M coins, like Bitcoin).
- **Block reward:** Decreases over time (halving schedule). Early blocks worth more.
- **Block time:** ~30 seconds (or tuned for game pace).
- **Difficulty adjustment:** Every 2016 blocks (or game-configurable), recalculate based on actual block time vs. target.
- **Mining:** Brute-force hash search. Players buy "GPU" items (in my-chara-txt inventory) which add hash rate.

### **Data Structure**

**blockchain.txt (append-only):**
```
block_num|timestamp|miner_address|nonce|prev_hash|hash|reward|tx_count
0|2026-08-01T12:00:00|genesis|0|0x0000|0xabcd|50.0|0
1|2026-08-01T12:00:30|Alice|12345|0xabcd|0xdef0|50.0|2
2|2026-08-01T12:01:00|Bob|98765|0xdef0|0x1234|50.0|1
...
```

**wallets/alice.txt:**
```
address=Alice
balance=100.5
locked_stake=0.0
```

**wallet_ledger.txt (transaction history):**
```
timestamp|from|to|amount|tx_hash|block_num
2026-08-01T12:00:30|Alice|Bob|10.0|0x5678|1
2026-08-01T12:01:00|Bob|Charlie|5.0|0x9abc|2
```

### **Mining (from myne-qrypto's OWN Mine screen — see §6)**

When the player mines QTC from myne-qrypto's own `mine.chtpm` screen:
1. Player selects QTC, confirms (has enough ASICs to bother).
2. **`myne_menu_input.c`** shells out to `qtc/ops/+x/chain_miner.+x <wallet_id>` as a background process (same pattern as `chain_menu_input.c`'s own `CHAIN_START_MINING` handler — PID-tracked, guarded against double-launch).
3. `chain_miner.+x` runs its own real SHA-256 PoW loop against `qtc/data/blockchain.txt`, difficulty-gated (see §5's ASIC→difficulty mapping, still to be designed — an open question, §10).
4. When a block is found, `chain_miner.+x` (unmodified, exactly as it already works) appends to `qtc/data/blockchain.txt` and credits `qtc/wallets/<wallet_id>/wallet.txt`.
5. myne-qrypto's own `mine.chtpm` screen (via `myne_compose_frame.c`) polls `qtc/net/miner_status.txt` (a file `chain_miner.+x` already writes on every block — confirmed real, see `041.pal-chain⛓️/ops/chain_miner.c`'s own `write_status()`) to show live mining progress to the player, and reads the wallet balance via `qtc/ops/+x/chain_balance.+x <wallet_id>` to reflect it in the player's own inventory display.

### **Exchange (myne-qrypto's OWN Exchange screen)**

Players trade QTC/QTH — open question whether this is genesis-txt-style order matching or my-chara-txt-style NPC fixed-price trading (see §10). Either way, trades update the player's REAL wallet balance in `qtc/wallets/<wallet_id>/wallet.txt` via `qtc/ops/+x/chain_send.+x` (already real, working, tested code — a wallet-to-wallet transfer).

---

## 🪙️ 4. QTH (QUANTUM TOKEN) — STAKE-BASED PoS + VM

### **New Chain, New Rules**

QTH is **NOT** just a copy of 041.pal-chain. It's a different consensus model:

- **Consensus:** Proof-of-Stake (PoS). Validators **stake coins** to earn block rewards + transaction fees.
- **Staking:** Lock QTH in a validator account. Higher stake = higher chance of being selected to propose next block.
- **Rewards:** Block reward (fixed, e.g., 2 QTH per block) + transaction fees (QAS, similar to Ethereum gas).
- **Block time:** ~10 seconds (faster than QTC, for gameplay feedback).
- **Smart Contracts:** Programmable in `.pal` (PAL scripts). Users can deploy contracts, call them, pay gas (QAS).
- **QAS (Gas):** Virtual compute meter. Each contract operation costs QAS (e.g., 10 QAS to store a value, 5 QAS to read it). Users pay validators.

### **Data Structure**

**blockchain_qth.txt (append-only):**
```
block_num|timestamp|validator_address|stake_amount|block_reward|gas_collected|prev_hash|hash|tx_count
0|2026-08-01T12:00:00|genesis|0|0|0|0x0000|0xaaaa|0
1|2026-08-01T12:00:10|Alice|100.0|2.0|50.0|0xaaaa|0xbbbb|3
2|2026-08-01T12:00:20|Bob|50.0|2.0|30.0|0xbbbb|0xcccc|2
...
```

**validators.txt (stake ledger):**
```
address|stake|delegated_to|earned_rewards|slash_count
Alice|100.0|self|250.5|0
Bob|50.0|self|120.3|0
Charlie|0.0|Alice|5.0|0
```

**contract_state.txt (deployed contracts):**
```
contract_id|owner|code_hash|storage_root|creation_block
0x1234|Alice|0xabcd|0x5678|1
0x5678|Bob|0xef01|0x9abc|2
```

**call_log.txt (contract executions):**
```
timestamp|contract_id|method|gas_used|result|caller
2026-08-01T12:00:15|0x1234|transfer|15|ok|Charlie
2026-08-01T12:00:25|0x5678|query|8|ok|Alice
```

### **Staking (From my-chara-txt)**

Characters can lock QTH coins to become validators:
1. **my-chara** calls `qrypto_stake_qth [player_address] [amount]`.
2. **myne-qrypto daemon** validates balance, locks coins in validators.txt.
3. Player joins validator pool. Each block, a validator is **randomly selected weighted by stake** (higher stake = higher chance).
4. When selected, validator proposes next block → earns 2 QTH reward + gas collected.
5. **Slashing:** If validator is caught double-signing or offline too long, stake is reduced (e.g., 5% penalty).

### **Smart Contracts in PAL (The Fun Part)**

A player can **write and deploy a PAL script as a smart contract:**

```pal
# Escrow contract: hold QTH, release on condition
read_balance [address]        # Read account balance
write_state [key] [value]     # Store key-value (costs gas)
emit_event [type] [data]      # Log event
transfer_qth [to] [amount]    # Send QTH (costs gas)
halt                          # End execution
```

**Example contract (simplified):**
```pal
# "Dice game" contract
# User sends 10 QTH, contract rolls dice, pays out or keeps it

; Initialize (called once at deployment)
call_method init
  write_state "owner" [caller]
  write_state "pot" 0
  halt

; Player bets 10 QTH
call_method bet
  ; Transfer 10 QTH from caller to contract
  transfer_qth [self] 10        # Cost: 5 QAS
  
  ; Roll dice (pseudo-random from block hash)
  read_state "pot"              # Cost: 3 QAS
  add_to_state "pot" 10         # Cost: 5 QAS
  
  ; 50% chance to win double
  roll_d2 [luck]
  branch [luck]
    transfer_qth [caller] 20    # Win: send 20 back
  else
    ; Lose: QTH stays in pot
  halt
```

**Cost model:**
- Deploy contract: 1000 QAS (one-time, paid to validators).
- Call method: base cost + operations (read/write/transfer each cost QAS).
- Gas price: set by validators (or market-driven, e.g., "I'll pay 10 QAS per op").

---

## ⚙️ 5. TWO LAYERS: GAME SHELL (player-facing) + BACKEND ENGINES (per-chain)

### **The game shell (NOT built yet — the real next step)**

`myne-qrypto/` itself (top level) is a `my-chara-txt`-shaped game: own `button.sh`, own CHTPM session (own `system/` — reuse `my-chara-txt`'s copy directly, it's proven), own `main.chtpm`/`mine.chtpm`/`store.chtpm`/`exchange.chtpm` screens, own `myne_menu_input.c`/`myne_compose_frame.c` op pair, own `piece.pdl` per screen, own player state (`pieces/system/config.txt`: day, health, money, ASICs owned, QTC/QTH wallet IDs).

### **The backend engines (per-chain, headless from the player's perspective)**

**`qtc/`** (QTC's backend — already real, built, tested): the full, unmodified `041.pal-chain⛓️` copy. Its own `chain_miner.+x`, `chain_balance.+x`, `chain_send.+x`, `chain_create_wallet.+x` ops are what the game shell's `myne_menu_input.c` shells out to. Its own login/wallet/mining-status CHTPM screens remain real and usable (good for direct chain auditing/debugging) but are NOT part of the player's normal path through myne-qrypto-the-game.

**`qth/`** (QTH's backend — NOT built yet): would follow the exact same "headless backend engine, own data/ledger, own ops the game shell shells out to" shape once designed. No existing CHTPM UI needed for it either, unless it's useful for the same kind of direct-auditing purpose `qtc/`'s reused login/wallet screens serve.

### **Directory Layout — `myne-qrypto/`**

```
myne-qrypto/
├── MYNE_QRYPTO_DESIGN.md           ← you are here
├── location.txt
├── button.sh                       ← [NOT BUILT YET] game shell launcher (modeled on my-chara-txt's button.sh)
├── scripts/
│   └── build.sh                   ← [NOT BUILT YET] compile game shell ops (modeled on my-chara-txt's)
├── default_op.txt                 ← [NOT BUILT YET] op registry: myne_menu_input, myne_compose_frame
├── system/                         ← [NOT BUILT YET] reuse my-chara-txt's copy directly (prisc+x, chtpm_parser_pal, etc.)
├── ops/
│   ├── myne_menu_input.c          ← [NOT BUILT YET] THE dispatcher - shells out to qtc/ops/+x/*.+x
│   ├── myne_compose_frame.c       ← [NOT BUILT YET] THE renderer - polls qtc/net/miner_status.txt etc.
│   └── +x/
├── pal/
│   └── main_module.pal            ← [NOT BUILT YET] modeled directly on my-chara-txt's main_module.pal
├── pieces/
│   ├── chtpm/layouts/
│   │   ├── main.chtpm             ← [NOT BUILT YET] Mine / Store / Exchange / Inventory / EndTurn
│   │   ├── mine.chtpm             ← [NOT BUILT YET] choose QTC or QTH, shows live mining progress
│   │   ├── store.chtpm            ← [NOT BUILT YET] buy ASICs
│   │   └── exchange.chtpm         ← [NOT BUILT YET] trade QTC/QTH
│   ├── system/config.txt          ← [NOT BUILT YET] player state: day/health/money/ASICs/wallet IDs
│   └── apps/player_app/           ← [NOT BUILT YET] view.txt, interact_relay.txt, piece.pdl per screen
├── data/
│   └── master_ledger.txt          ← [NOT BUILT YET] myne-qrypto's OWN game ledger (mine attempts, ASIC purchases, trades) - separate from the blockchain's own ledger, same "two independent audit trails" pattern as §7
├── qtc/                            ← ✅ REAL, BUILT, TESTED (21/21 harness checks) - the QTC backend engine
│   └── (full unmodified 041.pal-chain⛓️ copy - system/ops/pal/pieces/chtpm/projects/data/wallets, own button.sh + test-harn-same/ - see #.haiku+/HANDOFF_NEXT_SESSION.md §10.2)
└── qth/                            ← ❌ NOT BUILT - the QTH backend engine, once §4's design is implemented
    └── (own data/blockchain_qth.txt, validators.txt, contracts/, own ops - no existing precedent to copy, unlike qtc/)
```

---

## 🔗 6. GAME SHELL ↔ BACKEND ENGINE (myne-qrypto's own internal integration)

**Not** file-based request/response IPC between two separate projects (this doc's original, wrong design). **Instead:** direct shell-out from the game shell's own `myne_menu_input.c` to the backend engine's own compiled ops via `popen()`/`system()` — exactly the pattern `041.pal-chain⛓️/ops/chain_menu_input.c` itself already uses for its `CHAIN_START_MINING`/`CHAIN_STOP_MINING`/`CHAIN_BALANCE` handlers (read that file's lines ~338-413 directly before implementing this — it's the real, live, working precedent, not a pattern to invent).

**Mining action (myne-qrypto's own `mine.chtpm` screen, dispatched by `myne_menu_input.c`):**
```
Player selects "Mine QTC" on mine.chtpm
→ myne_menu_input.c's MINE_QTC command handler runs
→ Guards against double-launch (check qtc/net/miner.pid for a still-alive PID,
  exact same guard chain_menu_input.c's own CHAIN_START_MINING uses)
→ popen("cd '<qtc_root>' && ./ops/+x/chain_miner.+x <wallet_id> &")
  (background, PID-tracked - real, not simulated)
→ myne_compose_frame.c (on next render) polls qtc/net/miner_status.txt
  (chain_miner.+x already writes this on every block, unmodified -
  see chain_miner.c's own write_status()) to show live progress
→ Player's own myne-qrypto ledger gets an entry once mining is stopped/
  a block is found: "2026-08-02|5|mine_qtc_start|wallet:<id>"
```

**Buy ASICs action (myne-qrypto's own `store.chtpm` screen):**
```
Player selects "Buy ASIC" on store.chtpm
→ myne_menu_input.c's BUY_ASIC command handler runs
→ Deducts cost from player's money (pieces/system/config.txt), increments
  asics_owned
→ ledger entry: "2026-08-02|5|buy_asic|cost:100|asics_owned:3"
→ (Open question, §10: does asics_owned actually change chain_miner.+x's
  own difficulty/odds? chain_miner.c doesn't currently take a hash-rate
  parameter - this needs either a real chain_miner.c modification, an
  ASIC-count-weighted RETRY-COUNT wrapper around it, or a decision that
  ASICs only affect display/flavor for now, not real odds. NOT decided.)
```

**Exchange action (myne-qrypto's own `exchange.chtpm` screen):**
```
Player posts a sell order or accepts an NPC price for QTC/QTH
→ myne_menu_input.c's EXCHANGE command handler runs
→ For a REAL wallet-to-wallet transfer, shells out to
  qtc/ops/+x/chain_send.+x <from_wallet> <to_wallet> <amount>
  (already real, working, tested code - a genuine on-chain transaction,
  not a simulated balance edit)
→ ledger entry: "2026-08-02|5|trade|sell:qtc:5:price:120"
```

**THREE independent ledgers, by design (same pattern as `my-chara-txt`'s own §7 audit framing) — not two, since QTC and QTH each have their own separate on-chain ledger:**

1. **`myne-qrypto/data/master_ledger.txt`** — the GAME's own ledger. Records events for BOTH chains alike: mine attempts started/stopped (QTC or QTH), ASICs bought, trades, staking actions. This is the single game-level audit trail regardless of which chain a given action targeted.
2. **`qtc/data/blockchain.txt` + `qtc/wallets/<id>/wallet.txt`** — QTC's own on-chain ledger. Real blocks, real wallet balances. Already built, already real.
3. **`qth/data/blockchain_qth.txt` + `qth/validators.txt`** — QTH's own on-chain ledger, once built (§4 already specifies this format). Real blocks/stakes, real validator balances, separate from QTC's chain entirely — a QTC transaction never appears in QTH's ledger or vice versa.

All three are independently replayable/auditable. The game ledger explains WHY a player's balance changed on either chain; each chain's own ledger is the actual source of truth for WHAT that chain's balances are right now. Cross-checking all three against each other (does the game ledger's claimed mining history match QTC's real blockchain? does it separately match QTH's?) is real, valuable audit work once both backends exist.

---

## 📊 7. AUDIT & MONITORING (a real, nice property — no longer the primary framing, see §1's correction)

### **Why this design enables auditing**

1. **Ledger transparency:** Both QTC and QTH blockchains are append-only ledgers (like my-chara-txt).
2. **Replay capability:** Read any blockchain from top to bottom, re-apply every block, re-compute state.
3. **Cross-validation:** Compare computed state (replaying blocks) vs. stored state (wallets.txt). Should match 100%.
4. **Price emergence:** Exchange ledger (from my-chara-txt) + blockchain transaction volumes = price discovery + market health.
5. **Validator health (QTH):** Audit stake distribution, slashing events, rewards, gas collection — all in validators.txt and call_log.txt.

### **Audit Tools (to write later)**

- `audit_qtc.sh` — Replay QTC blockchain, verify all blocks, check wallet balances.
- `audit_qth.sh` — Replay QTH blockchain, verify validator rewards, check contract state.
- `price_discovery.sh` — Plot QTC/QTH prices over time, detect manipulation, ensure emergent.
- `gas_analysis.sh` — Analyze gas spending patterns, detect expensive operations, tune VM.

**Example audit output:**
```
QTC Audit (2026-08-01 to 2026-08-01):
  Blocks mined: 2880 (30s target achieved)
  Difficulty trend: [1.0, 1.1, 1.15, 1.2] (smooth adjustment)
  Total coins generated: 144,000 QTC
  Wallet integrity: ✅ PASS (replayed state matches stored state)
  
QTH Audit (2026-08-01 to 2026-08-01):
  Blocks produced: 8640 (10s target achieved)
  Validators: 4 (Alice 100%, Bob 50%, Charlie 30%, Dave 20%)
  Slashing events: 0
  Gas total: 125,000 QAS collected
  Contract execution: 456 calls, 0 failures
  
Price Discovery:
  QTC: started $100, ended $105 (↑5% based on supply/exchange volume)
  QTH: started $50, ended $52 (↑4% based on validator demand + gas demand)
```

---

## 🚀 8. BUILD ORDER (REVISED — QTC backend is DONE, game shell is next)

| Phase | What we build | Status | Verified by |
|-------|---------------|--------|-------------|
| **P1** ⛏️ | QTC backend: reuse 041.pal-chain wholesale | ✅ **DONE** | 21/21 test-harn-same checks pass (`qtc/test-harn-same/`) — real signup/login/wallet flow, real SHA-256 |
| **P2** 🥇 | **Game shell skeleton:** `myne-qrypto/button.sh`, `main.chtpm`, `myne_menu_input.c`/`myne_compose_frame.c`, one real screen — copy `my-chara-txt`'s own P1/P2 build directly, it's the proven template | ❌ **NEXT REAL STEP** | real `button.sh run` renders `main.chtpm`, matches `my-chara-txt`'s own P2 checkpoint shape |
| **P3** 🔌 | Mine screen: shells out to `qtc/ops/+x/chain_miner.+x` (pattern from `chain_menu_input.c`'s own `CHAIN_START_MINING`), Store screen (buy ASICs) | ❌ not started | player can mine QTC from myne-qrypto's own screen, real wallet balance updates |
| **P4** 🪙️ | QTH backend: PoS validator selection, block generation (§4, genuinely new design) | ❌ not started | blocks generated by weighted-stake selection |
| **P5** 🔒 | QTH game-shell integration: Mine screen gains a QTH option, Stake screen | ❌ not started | players can stake QTH from myne-qrypto's own screen |
| **P6** 📜 | Smart contract VM: PAL interpreter, gas metering, storage | ❌ not started | contracts can deploy and execute, gas charged |
| **P7** 🤖 | Contract browser screen in the game shell | ❌ not started | players can call contracts, results returned |
| **P8** 📊 | Audit tools: replay all three ledgers (§7), cross-check | ❌ not started | audit scripts pass, state verified |
| **P9** 💱 | Exchange screen: QTC/QTH prices, real `chain_send.+x`-backed trades | ❌ not started | prices emerge/are set, trades really move wallet balances |

---

## 🔗 9. CROSS-REFERENCES & WIDGET REUSE

### **Reusable Widgets (Key Point!)**

These widgets built here can be **reused in genesis-txt, genesis-zr, and future apps:**

1. **Mining UI widget** (select coin type + GPU count, show odds + reward preview).
2. **Wallet widget** (view balances, history, copy address).
3. **Exchange widget** (buy/sell crypto by price or order placement).
4. **Staking widget** (lock QTH, view rewards, unstake).
5. **Contract browser widget** (list deployed contracts, read contract code, call methods, set params).

**Path:** All widgets in `myne-qrypto/pieces/` as reusable layouts + ops. genesis-txt **imports** these same widgets, adds them to player menus.

### **Related Projects**

- **041.pal-chain⛓️** ← QTC's backend is literally this, unmodified, now living at `myne-qrypto/qtc/`.
- **my-chara-txt** → NOT a consumer of myne-qrypto. Instead, myne-qrypto's own game shell is a direct structural COPY of my-chara-txt's proven CHTPM pattern — a sibling game, not a dependent.
- **genesis-txt** → Once myne-qrypto's single-player game shell works, the SAME widgets (mining UI, wallet UI, exchange UI) can be reused there for a multiplayer crypto mode, same relationship `genesis-txt` already has to `my-chara-txt`.
- **genesis-zr** → Visual layer on top (same pattern + GL rendering), once both txt versions exist.

---

## 🤔 10. QTH CHAIN RULES — DETAILED GUIDANCE

### **Stake Distribution & Validator Selection**

**Model:** Probability-weighted selection by stake.

```
Validators:
  Alice: 100 QTH staked → P(selected) = 100 / 300 = 33%
  Bob:   50 QTH staked  → P(selected) = 50 / 300 = 17%
  Charlie: 150 QTH staked → P(selected) = 150 / 300 = 50%

Block production: 8640 blocks per day (every 10s)
Expected blocks per validator per day:
  Alice: 8640 × 33% ≈ 2851 blocks → 2851 × 2 QTH reward = 5702 QTH earned
  Bob:   8640 × 17% ≈ 1469 blocks → 1469 × 2 QTH reward = 2938 QTH earned
  Charlie: 8640 × 50% ≈ 4320 blocks → 4320 × 2 QTH reward = 8640 QTH earned
```

**Randomness:** Use block hash + slot number to seed PRNG, pick validator. Deterministic but unpredictable per block.

### **Slashing Conditions**

Detect misbehavior and penalize:
1. **Double-signing:** Validator signs two blocks at same height → 5% stake slash.
2. **Equivocation:** Contradictory messages → 5% slash.
3. **Offline too long:** Validator doesn't produce block when selected 3 times in a row → 1% slash (minor penalty to encourage participation).

**Enforcement:** Check each block for evidence. If found, automatically deduct from validators.txt.

### **Gas Model (QAS)**

**Operation costs (tunable):**
- `read_state` — 3 QAS
- `write_state` — 5 QAS
- `transfer_qth` — 5 QAS
- `emit_event` — 1 QAS
- `read_balance` — 2 QAS
- `call_method` — base 10 QAS + operation costs

**Gas price:** Validators set minimum (e.g., "1 QAS = 0.001 QTH"). Users include higher gas price to prioritize.

**Block gas limit:** 100,000 QAS per block (prevents runaway contracts).

### **Contract Lifecycle**

1. **Deploy:** User submits PAL code + init params. VM validates syntax, computes code hash, creates contract_id.
2. **Storage:** Contract gets a state storage root in contract_state.txt. Can store up to 1 MB (tunable).
3. **Call:** User calls method. VM loads code + state, executes, deducts gas, updates state.
4. **Deletion:** Contract can self-destruct (if code includes `halt_and_refund`), freeing storage.

### **Delegation (Advanced, Optional)**

Validators can delegate rewards to other addresses (e.g., "I staked 100 QTH, but Alice manages it"):
- Alice's validator runs, earns rewards.
- Rewards split: 90% to delegator, 10% to Alice (fee).
- Slashing applies to delegator's stake.

---

## 🤔 10.5 OPEN QUESTION ADDED THIS REVISION: do ASICs actually change mining odds?

`chain_miner.c` (the real, unmodified QTC backend) has no hash-rate/ASIC-count parameter at all — it just runs one PoW search loop per process, at a fixed difficulty. Buying more ASICs in myne-qrypto's own Store screen needs to DO something real for the Mine screen to feel meaningful. Options, not yet decided:
1. **Launch N copies of `chain_miner.+x`** (N = ASICs owned) as separate background processes, all racing against the same `qtc/data/blockchain.txt` — more real attempts per second, matches how real mining pools work, but `chain_miner.c`'s own header comment warns about exactly this race condition if not carefully guarded (two miners finding blocks near-simultaneously).
2. **Modify `chain_miner.c`** to accept a hash-rate multiplier arg (e.g., try N nonces per read instead of 1) — a real code change to the reused engine, breaks the "keep it byte-for-byte unmodified" principle from earlier this session (see the `feedback-reuse-ops-dont-rename` memory note) — would need explicit user sign-off to justify deviating from that.
3. **ASICs are flavor/display only for now** — ignore real difficulty, just show a bigger number in the inventory screen. Simplest, but doesn't feel like it matters.

**Ask the user before building the Mine/Store screens** — this materially changes what `myne_menu_input.c`'s `MINE_QTC` handler and `BUY_ASIC` handler actually need to do.

---

## 🏁 11. TL;DR — THE 30-SECOND VERSION

- **myne-qrypto is its own playable game**, structurally identical in playstyle to `my-chara-txt`/`genesis-txt`: one character, `main.chtpm`, Mine/Store/Exchange/Inventory/EndTurn.
- **QTC backend:** real, unmodified `041.pal-chain⛓️` copy at `myne-qrypto/qtc/` — already built, already tested (21/21 harness checks). The game shell's Mine screen shells out to its real ops (`chain_miner.+x`, `chain_balance.+x`, `chain_send.+x`).
- **QTH backend:** genuinely new PoS + PAL-smart-contract design (§4), not built yet.
- **The game shell itself** (the part a player actually navigates — `main.chtpm`/`mine.chtpm`/`store.chtpm`/`exchange.chtpm`) **does not exist yet** — this is the real next build step, and it's a direct copy of `my-chara-txt`'s own already-proven CHTPM pattern, not new design work.
- **Three independent ledgers:** the game's own event ledger, QTC's real on-chain ledger, QTH's real on-chain ledger (once built) — all independently auditable/replayable.
- **Open question before building Mine/Store:** do ASICs really change mining odds, or are they flavor-only for now? (§10.5)

⛏️ Ready to mine and stake? 💎
