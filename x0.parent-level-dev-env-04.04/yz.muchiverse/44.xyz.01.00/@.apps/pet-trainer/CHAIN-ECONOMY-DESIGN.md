# Pets on the chain: wallets, mining, NFTs, banks, exchange (design, 2026-10-10)

Status: DESIGN + owner decisions. Phase 1 is being built next; everything after it is a plan, not code. Written by claude from the owner's words in this session.

## 1. The owner's vision, in their words (kept so nothing is lost)

- "they are supposed to be able to mine from the blockchain on the computer at their house, and then build more rooms and store more ASIC miners, and maintain them. that's their 1 way of making money, 'cones'. this is why they keep the pets, for income from the blockchain. the pets mine instead of the user running the real house blockchain themselves"
- "give each pet a wallet address; each pet is also an NFT on the chain, and so is the food they grow; every tx they make gets stored as a tx. pets can store p2p storage nodes of the chain for node stake rewards as well. later they will be able to start businesses, sell stocks, give dividends etc."
- "that old chain should still exist. those original accounts should be used as 'bank' accounts like in WSR PREFERRED: 'governments' and 'banks' who can loan and start their own companies"
- "those cones should be 'preferred' from the old chain because they are 100% confirmed. the company / game will give those out as prizes and stuff along with the mine chain, which should have much more difficulty so pets don't consume it so fast"
- "we can start building the separate x11-hq auction and crypto exchange apps where users can sell their NFTs (food grown in game, on chain), trade things and send blockchain coins to others. the pets can also trade on the auction and blockchain. that's who is supposed to use it (AI entities on the blockchain, always trading for real needs)... this keeps the blockchain live and honest even though there are no human users"

## 2. What exists today (read, not assumed)

- **The old chain** `041.pal-chain/` (the "cones" chain): 14,000 blocks, 35 wallets (`walletA_*`, `jb`, ...). Reward 10,500 millicones, halving every 1,000 blocks, cap 21,000,000. **20,996,000 millicones (99.98%) are already mined** (by the test harnesses); the reward at block 14,000 is 0. So the old chain **cannot pay mining income any more**, but its balances are real, confirmed and final: exactly what "preferred" means. It stays untouched.
- **Chain ops** (`ops/`, C, run with `PRISC_PROJECT_ROOT=<chain root>`): `chain_create_wallet`, `chain_balance`, `chain_send` (appends `TX|from|to|mc|ts|tx_id` to `pending_tx.txt`), `chain_miner` (real SHA-256 PoW, `--blocks N`, daily cap per wallet from `chain.pdl`), `chain_new` (makes a new chain root under `chains/<id>/`: kinds test/user, `--difficulty`, `--cap`, faucet options), faucet/escrow ops. A TX only counts when a block that includes it is mined. Inclusion does **not** balance-check a plain `TX`, and nothing is signed (the `agent`/`by` fields of escrow are an honor field). **No NFT transaction type exists.**
- **Entity wallets**: Q005 derives `wallet_id = "e" + first 24 hex of entity_uid` for every entity (pets included) but creates no chain wallet.
- **WSR** (`WSR_PAL-PREFERED/SOCIETY-ECONOMY-ARCHITECTURE.txt`): 50 corporations and 7 governments exist as entities; banks are an entity type that answers `loan_request` posts on a ledger/auction marketplace; companies, stocks, dividends, payroll are designed there. Reuse it; do not invent a second banking model.
- **Auction screen design** `08-roadmap/design-docs/AUCTION-SCREEN-DESIGN.md`: append-only auction ledger (LIST/BID/...), units `cones` and `play`, a settlement layer that is the only thing touching wallets, humans and AI are the same kind of actor. Not built.
- **Bug found while researching** (logged in bug_bounty): `chain_miner` spins at ~98% CPU when its wallet/root does not exist.

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
| 4 | Fix `chain_miner` spin bug; balance-check every TX at inclusion | chain harness |
| 5 | NFT tx types (owner OK) + ownership query; pets and grown food minted | `pet_nft` |
| 6 | Bank/government wallets from the old chain, role files, loans/prizes | WSR-style |
| 7 | Exchange HQ, then Auction HQ | per AUCTION-SCREEN-DESIGN |
| 8 | Staking/storage nodes, businesses, stocks, dividends | later |

## 9. Open decisions for the owner (defaults are in data, change any)

1. Difficulty and daily cap of `pet-cones` (default guess: difficulty 6 hex zeros, cap 6 blocks per wallet per UTC day).
2. Name of the mined unit (chain id `pet-cones` for now).
3. 1 coin = 1,000 millicones? (a block pays 10.5 coins at the first reward).
4. Which old-chain wallets are banks and which are governments (list in the next step, balances only).
5. OK to change the three chain ops for NFTs (item 5) and to balance-check every TX (item 4)?
