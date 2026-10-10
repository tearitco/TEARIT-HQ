# Pets on the chain: wallets, mining, NFTs, banks, exchange (design, 2026-10-10)

Status: DESIGN + owner decisions. Phase 1 is being built next; everything after it is a plan, not code. Written by claude from the owner's words in this session.

## 1. The owner's vision, in their words (kept so nothing is lost)

- "they are supposed to be able to mine from the blockchain on the computer at their house, and then build more rooms and store more ASIC miners, and maintain them. that's their 1 way of making money, 'cones'. this is why they keep the pets, for income from the blockchain. the pets mine instead of the user running the real house blockchain themselves"
- "give each pet a wallet address; each pet is also an NFT on the chain, and so is the food they grow; every tx they make gets stored as a tx. pets can store p2p storage nodes of the chain for node stake rewards as well. later they will be able to start businesses, sell stocks, give dividends etc."
- "that old chain should still exist. those original accounts should be used as 'bank' accounts like in WSR PREFERRED: 'governments' and 'banks' who can loan and start their own companies"
- "those cones should be 'preferred' from the old chain because they are 100% confirmed. the company / game will give those out as prizes and stuff along with the mine chain, which should have much more difficulty so pets don't consume it so fast"
- "we can start building the separate x11-hq auction and crypto exchange apps where users can sell their NFTs (food grown in game, on chain), trade things and send blockchain coins to others. the pets can also trade on the auction and blockchain. that's who is supposed to use it (AI entities on the blockchain, always trading for real needs)... this keeps the blockchain live and honest even though there are no human users"

- "yes, let those WSR governments and companies use the premined chain and accounts and coins to trade in that WSR market. mark that we want to join them somehow to trade from those accounts at start and give all coins back at end of game. (we, tearit-co, owned those premined accounts so we can do what we want, and the keys can't be stolen from us because they are public but the coins are still always owned and managed by our account and can do anything we want. get it? hybrid managed chains"

## 2. What exists today (read, not assumed)

- **The old chain** `041.pal-chain/` (the "cones" chain): 14,000 blocks, 35 wallets (`walletA_*`, `jb`, ...). Reward 10,500 millicones, halving every 1,000 blocks, cap 21,000,000. **20,996,000 millicones (99.98%) are already mined** (by the test harnesses); the reward at block 14,000 is 0. So the old chain **cannot pay mining income any more**, but its balances are real, confirmed and final: exactly what "preferred" means. It stays untouched.
- **Chain ops** (`ops/`, C, run with `PRISC_PROJECT_ROOT=<chain root>`): `chain_create_wallet`, `chain_balance`, `chain_send` (appends `TX|from|to|mc|ts|tx_id` to `pending_tx.txt`), `chain_miner` (real SHA-256 PoW, `--blocks N`, daily cap per wallet from `chain.pdl`), `chain_new` (makes a new chain root under `chains/<id>/`: kinds test/user, `--difficulty`, `--cap`, faucet options), faucet/escrow ops. A TX only counts when a block that includes it is mined. Inclusion does **not** balance-check a plain `TX`, and nothing is signed (the `agent`/`by` fields of escrow are an honor field). **No NFT transaction type exists.**
- **Entity wallets**: Q005 derives `wallet_id = "e" + first 24 hex of entity_uid` for every entity (pets included) but creates no chain wallet.
- **WSR** (`WSR_PAL-PREFERED/SOCIETY-ECONOMY-ARCHITECTURE.txt`): 50 corporations and 7 governments exist as entities; banks are an entity type that answers `loan_request` posts on a ledger/auction marketplace; companies, stocks, dividends, payroll are designed there. Reuse it; do not invent a second banking model.
- **Auction screen design** `08-roadmap/design-docs/AUCTION-SCREEN-DESIGN.md`: append-only auction ledger (LIST/BID/...), units `cones` and `play`, a settlement layer that is the only thing touching wallets, humans and AI are the same kind of actor. Not built.
- **Stale chain binaries (found 2026-10-10):** the compiled ops in `041.pal-chain/ops/+x/` date from 2026-10-05, the sources from 2026-10-07 (multichain, faucet, escrow, daily cap, `--blocks`). The old `chain_miner.+x` ignored `--blocks N` (it mined until killed) and ignored `chain.pdl` (difficulty 5 instead of the chain's 2). `chain_new` was never built. The Q003 build gate does not cover the chain folder. Rebuild with `scripts/build.sh` (or `gcc ... -lcrypto` per op) before any use; `chain_new` is built now.
- **Open bug** (bug_bounty): `chain_miner` spins at ~98% CPU when its wallet does not exist. Whether the 2026-10-07 source still does this is checked after the rebuild (see build order item 4).

## 3. Decisions (owner direction turned into rules)

1. **Two currencies, two chains.**
   - **Preferred cones** = the old chain. Finite, confirmed, held by the original accounts, which become the **banks and governments** (WSR roles). The game/company hands them out as **prizes**, loans and grants. Nothing mints more.
   - **Mined cones** = a NEW chain, `pet-cones` (id placeholder), made with `chain_new`, with **much higher difficulty** and a per-wallet daily cap so the pets cannot drain it. This is the pets' working currency and the thing they mine.
   - The exchange app (section 6) is where the two units trade against each other.
2. **Every pet has a wallet** on `pet-cones` (id from Q005: `e` + 24 hex of its uid; the password is random, kept in the pet's private state file, never printed or logged, never committed).
3. **The pet's purse is its chain balance.** "Coins" in the pet game are mined cones: 1 coin = 1,000 millicones (a first guess in data). Every purchase, sale, rent, fee and wage a pet makes is a real `chain_send` TX (the chain is the record, per the owner). A payment is "pending" until the next block; a pet may only spend `confirmed balance - its own pending outgoing`, so it cannot double-spend. The item `coin` stops being the money (it may stay as a display).
4. **Mining is a game system with real work behind it.** Each pet's computer is a miner; ASIC miners are bought with coins and **stored in rooms** (slots per room; more rooms = more miners); miners **wear out** and must be **maintained** (cost, and a pet action); a worn or broken miner stops. A pet's "work meter" fills from its working miners; when full, the pet runs ONE real `chain_miner <wallet> --blocks 1` (niced, one miner at a time via a lock), so the CPU cost is real, bounded and serial. Chain difficulty and the daily cap are the supply brakes, the meter is the pacing.
5. **Safety rails for the weak machine:** `nice -n 15`, one chain miner at a time, a per-pet and chain-wide daily cap, and no mining while the machine is loaded.
6. **The old chain is never mined, never reset and never rewritten.** Reading its balances is fine; moving its cones is an owner/bank action behind the exchange/settlement layer, never an AI side effect.

## 4. Assets: NFTs (needs chain work, owner OK first)

- **Pets** (identity + species/seed/stage hash) and **food/crops they grow** (kind, grower, grown_at, quality) are NFTs. Machines (ASIC miners) and buildings are the next candidates.
- The chain has no NFT type. Two ways:
  - **A (recommended):** add transaction types `NFT_MINT | id | kind | owner | meta_hash | ts | tx_id`, `NFT_XFER | id | from | to | ts | tx_id`, replayed with the balances in all three ops that replay blocks (`chain_balance`, `chain_miner`, plus the new ownership query op). Inclusion rules: mint only by the rightful minter, transfer only by the current owner, no double mint. This changes consensus code that three ops duplicate by house rule, so it is a **quest with a locked harness**, not a side effect of the game.
  - B (not recommended): fake NFTs as dust payments. Hacky and unverifiable.
- Until A exists the game keeps a local asset table (food items already exist as inventory folders) and records the intent in the ledger, so nothing has to be thrown away.

## 5. Banks, governments, companies (WSR roles)

- The original accounts of the old chain become named **bank/government wallets** with a role file (`bank_<name>`, `gov_<name>`), the WSR way. They answer `loan_request` ledger posts (credit-line rule from WSR), pay prizes, and can start companies.
- **Pets later**: start a business (a store becomes a company), issue stock, pay dividends from profit, take loans from banks. All as ledger rows settled by chain TXs; the decisions are the WSR deterministic ones, never a model.
- **Honesty:** the owner's point is that tireless AI traders keep the chain live and honest. For "honest" the chain itself must reject bad transactions: balance-check every TX at inclusion (today a plain `TX` is not checked) and sign transactions. Listed as chain-hardening work, a prerequisite for real value.

## 6. Apps (separate X11-HQ windows, built later, in this order)

1. **Exchange** (crypto exchange): send cones to a wallet, balances, order book between `cones` (preferred) and mined cones, shows who is buying. Settlement layer from AUCTION-SCREEN-DESIGN section 3A.
2. **Auction**: LIST/BID/BUY NOW for NFTs (grown food, later pets/machines) and goods; closes by the ledger; humans and AI entities place bids through the same command.
3. **Chain/Bank desk**: loans, prizes, company registry (WSR).
- Pets use the exchange/auction themselves through the same commands: they sell surplus food NFTs, buy what they need (feed, parts, miners) with mined cones. That is the steady traffic that keeps the chain live.

## 7. p2p storage nodes (later)

Pets can run storage nodes for the chain and earn a stake reward. Needs: `STAKE`/`UNSTAKE`/`REWARD` transaction types, a storage proof (a challenge/response the verifier can check), and `palnet_peer` working across machines (today it hardcodes 127.0.0.1; see the cross-machine networking plan). Design after the exchange exists.

## 8. Build order

| # | Item | Harness |
|---|---|---|
| 1 | `pet-cones` chain (high difficulty, daily cap), pet wallets, chain adapter (balance, pay, mine with lock) | `pet_chain` pal on a scratch chain |
| 2 | Miners: kinds, rooms with slots, wear, maintenance, work meter, real block per full meter, income on chain, sidebar + scene | `pet_mining` |
| 3 | Purse = chain balance; trades/rent/fees as TXs with pending accounting; old `coin` items retired | rerun `pet_town`, `pet_econ`, `pet_ai` on a funded scratch chain |
| 4 | Rebuild chain ops (stale binaries), re-check the `chain_miner` missing-wallet spin on the new source, balance-check every TX at inclusion | chain harness |
| 5 | NFT tx types (owner OK) + ownership query; pets and grown food minted | `pet_nft` |
| 6 | Role file for the 35 premined wallets (bank/government/corporation/treasury); lease + sweep ops on a scratch chain copy; tearit-co joins the WSR market; loans/prizes | `chain_lease` pal (lease, trade, sweep, all back to zero) |
| 7 | Exchange HQ (rates table, fee settings, trade feed; section 11), then Auction HQ | per AUCTION-SCREEN-DESIGN |
| 7b | Pets drive the relay in the exchange/auction windows, learn by RL with the house harnesses, propose harness edits, watch human input (section 12): first as paper trading (`play` unit) | `pet_trade_rl` (pal, deterministic fake market) |
| 8 | Staking/storage nodes, businesses, stocks, dividends | later |

## 9. Open decisions for the owner (defaults are in data, change any)

1. Difficulty and daily cap of `pet-cones` (default guess: difficulty 6 hex zeros, cap 6 blocks per wallet per UTC day).
2. Name of the mined unit (chain id `pet-cones` for now).
3. 1 coin = 1,000 millicones? (a block pays 10.5 coins at the first reward).
4. Which old-chain wallets are banks and which are governments (list in the next step, balances only).
5. OK to change the three chain ops for NFTs (item 5), to balance-check every TX (item 4), and to add LEASE/SWEEP and managed-account rules (section 10)?
6. Exchange defaults (section 11): fee %, averaging window and weighting, floor/ceiling, seed value of the mined unit.
7. The treasury wallet for tearit-co (name/which existing wallet), the wallet-to-WSR-role mapping, and the end-of-game trigger that fires the sweep.

## 10. Hybrid managed chains: the premined accounts, WSR and "give it all back"

Owner (2026-10-10): the old chain's premined accounts are **owned by tearit-co**. Their keys are public, so game entities can use the accounts freely, but the coins stay owned and managed by the tearit-co account, which can do anything with them. The WSR governments and companies use those accounts and coins to trade in the WSR market; tearit-co joins them to trade from those accounts at the start, and **all coins are given back at the end of the game**.

**Rule (what "hybrid managed" means here):** the chain is public and the keys are not secret, so safety does NOT come from key secrecy. It comes from the chain's own inclusion rules plus a **custody layer** that tearit-co controls.
- **Custody root:** one tearit-co treasury wallet (to be named) is the owner of record. It may recall (sweep) any managed account at any time.
- **Lease at game start:** for each WSR entity (government, bank, corporation) a **game account** is funded from the premined accounts by a recorded lease: `LEASE | lease_id | from_treasury | game_account | amount_mc | ts`. The entity trades with those coins in the WSR ledger/auction marketplace.
- **Sweep at game end:** every outstanding lease is returned to the treasury: `SWEEP | lease_id | game_account | treasury | amount_mc | ts`, and the lease table must be empty and the game accounts at zero. "Give all coins back" is a checked end-of-game condition, not a hope.
- **Managed policy (target, chain-level):** `chain.pdl` rows mark managed accounts and their allowed counterparties (the WSR market accounts, the other game accounts, the treasury). A managed account's outflow to anyone else is refused at inclusion; a sweep by the custodian is always allowed. Unmanaged accounts (the pets' mined wallets) are ordinary wallets with no such rule.
- **Interim, before any chain change:** the settlement layer does it with ordinary `chain_send`: a lease ledger file (`LEASE` rows), a lease op (treasury -> game account) and a sweep op (game account -> treasury, for all outstanding leases), both append-only and harness-tested on a scratch copy of the chain. Because the keys are public and tearit-co controls the treasury, this is honest for now; the consensus-level rule comes with chain hardening (balance-check every TX, section 5).
- **Join the WSR market:** register tearit-co as a participant entity in WSR (the entity registry and the ledger/auction marketplace of `SOCIETY-ECONOMY-ARCHITECTURE.txt` section 6) so it can post/bid from the leased accounts. Marked as a TODO: how tearit-co "joins" (a company entity, or a seat in the market) is an open design point.
- **Which premined account is which:** the 35 wallets (`walletA_*`, `jb`, ...) get a role file mapping them to WSR roles (government, bank, corporation, treasury). Listing balances only; nothing is moved until the owner approves the mapping.
- **Prizes:** the game/company pays prizes in preferred (old-chain) cones from the same managed accounts, as leases that are not swept (a prize is a gift) or as ordinary transfers, to be decided per prize type.

## 11. Exchange valuation into preferred value

- **Preferred** = the old chain's cones (final, 100% confirmed). Every other chain's unit (the pets' mined cones, test and user chains) has a **preferred value**: the running average of its real trades against preferred cones at our exchange (volume-weighted over a window set in `exchange.pdl`, with a floor and ceiling so one trade cannot move it; a configured seed value until trades exist).
- Using a non-preferred chain through our exchange costs a **fee**, "like a transaction fee": it is paid in preferred cones, or the amount is **auto-exchanged into preferred** at the averaged rate. So all chain activity resolves into preferred value and nothing floats free of it. Fee %, averaging window, floor/ceiling and which chains are accepted are settings in `exchange.pdl`, **editable in the Exchange x11-hq window** once it exists.
- Deterministic code computes and applies the rates (models never decide numbers). The rate history is an append-only ledger (`RATE | unit | preferred_per_unit | window | trades | ts`); every conversion and fee is a ledger row and a chain TX.
- **The Exchange window must show it** (owner: "we wanna see the window for those exchanges"): a per-unit table (last trade, averaged preferred value, change, volume), the fee/rate settings editor, a live trade and order feed saying who traded (pet, WSR entity, human), the fee income account, and the rate history.

## 12. The soul of the ecosystem

The pets are not scripted traders. They are **learners that operate the house the way a human does**:
- They **drive the relay**: they use the exchange, auction, chain and other windows through the same input path a human's keys take (`#.desktop/entity_menu_history/<pid>.txt` KEY_PRESSED / MOUSE_EVENT lines and the windows' state files), with no private API. Every action is a relay line plus the ledger rows it causes.
- They **learn by reinforcement (RL)** with the **in-house models and harnesses**: outcomes become reward/punish (`entity_grade`, the feedback ledger `obs_feedback_log.txt`, the concept bank and skillbook, Laplace score `(reward+1)/(reward+punish+2)`), weights move only through bounded, ledgered edits (`joint_tune`), the models are the house's own (Gemma on the Mac via `ai_backend.pdl`, the HORN provider ladder), and the **harness verdict is the referee**.
- They **use and modify their own harnesses while learning**: a pet may propose new cases, weights and bank rows for the harnesses it is judged by, as candidate edits that must pass `concept_edit_validate` and the locked-harness rules; it never edits the grader that scores it (hash-lock, `DELEGATION-FLYWHEEL-HORN-GHOSTS-DESIGN.md` section 2); promotion tiers and human review apply (preschool/elementary never auto-promote).
- They **watch real human input**: `#.desktop/human_input/<pid>.txt` (real X events only) is demonstration data for trading and every other house task (`IRL-BOOTSTRAP-RECURSION-SPEC.md`). Humans are optional; when they are there, the pets learn from them.
- Why it is the soul: the pets trade for real needs (food, parts, miners, rent), so the exchange has steady, honest, need-driven traffic and the chain stays live **with no human users**; every window is also a training environment; the harnesses grow from use.
**Rails (not optional):** DESCRIBE never CLASSIFY; models never decide numbers or promotion; every change is a diff plus a verdict; paper trading (`play` unit) before real cones; per-pet trading limits, daily loss caps and a kill switch (a flag file the runner polls); managed accounts are only ever reached through capped leases; no keys or wallets in any prompt or log.
