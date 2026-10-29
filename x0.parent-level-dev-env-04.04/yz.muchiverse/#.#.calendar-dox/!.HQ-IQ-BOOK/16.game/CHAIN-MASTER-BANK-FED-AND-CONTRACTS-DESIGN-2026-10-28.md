# The chain as master bank and "Fed": tributes, fees, bonds and owner contracts (design, 2026-10-28)

Status: **DESIGN. Nothing in this document is built; every number below is an illustration, not a setting.** Written by Claude (Sonnet 5.5) from the owner's words and the existing chain documents. Labels: **SAID**, **READ**, **RAN**, **MINE**, **OPEN** as in `PIECE-IDENTITY-AND-MASTER-PAL-LIST-DESIGN-2026-10-28.md` (the companion: identity, minting, the Master Pal List). Builds on `@.apps/pet-trainer/CHAIN-ECONOMY-DESIGN.md` (sections 3 to 5, 8, 11, 13), `GAMES-AS-PACKAGES-AND-STORE-DESIGN.md` (sections 13 to 19: disputes, REP, `policy.pdl`, events, signatures) and `08-roadmap/design-docs/AUCTION-SCREEN-DESIGN.md`.

## 0. What the owner said (SAID, 2026-10-28)
- Minting "is an op / event ... devs can **buy / copy / fork depending on the chain**, in exchange by giving [to] the blockchain."
- "The blockchain **takes tributes which let you mine for terumon / pals**, or **buy them directly from the chain / auctions / NPCs for a set price**, but **markets can set their own**."
- "The chain turns into its **own master bank for the players, like a Monopoly game bank**, but it's on its blockchain so it's **uncheatable**. It's going to function like a **Fed** as well: **issuing its own bonds**, and taking a **'master tax' and 'master tx fee' on every purchase**."
- "It also allows **chain owners / managers to do their own financial contracts: taxes, dividends, royalties, annuities, whatever.** Doc that."
(My reading of "giving 2 blockchain" is "giving to the blockchain"; OPEN if you meant something else.)

## 1. What already exists (READ, not run today)
| Piece | State |
|---|---|
| **Old chain** `041.pal-chain` ("preferred cones", 99.98% mined, never reset), chain ops (`chain_send`, `chain_miner`, `chain_new`, `chain_escrow` LOCK / PAYOUT / REFUND with paid + refunded <= locked) | built |
| **Entity wallet** `e` + 24 hex of the uid | derived, no wallet created yet |
| **New high-difficulty `pet-cones` chain**, daily caps, pet mining, miners | designed (CHAIN-ECONOMY sections 3, 8), partly built |
| **NFT transaction types** `NFT_MINT` / `NFT_XFER` | **designed only** (section 4 option A); grep finds none in code |
| **Exchange HQ** (order book, taker/maker fee, preferred-value averaging) and **Auction HQ** (LIST / BID / BUY / CANCEL) | built as **paper ledgers**; chain settlement not built |
| **Events** `ev_chain`, `ev_store` + registry commands; **signatures** `sig_op`; signed escrow | built for test chains, free `get` only, spend caps planned |
| **`policy.pdl`** per chain and per game, owner-signed changes; **REP, disputes, courts** (`my-lawyer` reuse) | designed (store doc sections 13 to 15); the reader is not built |
| **WSR** governments / banks / companies, loans, payroll, dividends, tax loop | built in the WSR stack; the chain-side link is designed |
Not found anywhere: master tax, master transaction fee, bonds, royalties, annuities (grep of the docs: 0 files), and the word **"tribute"** as a defined term (OPEN, section 4).

## 2. The idea in one picture (MINE)
The chain is the **bank of the game**: it holds the **master account**, issues the units, records every payment, and every purchase passes through it, so the **house rules are enforced by replay, not by trust**. Three roles, all data-driven:
1. **Master account (treasury):** receives the master tax and the master tx fee, pays bond coupons and buybacks, funds prizes. Its balance and every movement are chain transactions.
2. **Issuer ("Fed"):** the only party that can create new units, only by a rule in `policy.pdl` (mining rewards, bond sales, prize grants), never by hand.
3. **Registry of value:** the Master Pal List asks the chain for the **NFT mint / transfer** records so a piece's owner and lineage are the chain's, not a game's.
Chain owners / managers (a market, a game, a guild) run **their own contracts on top** inside the limits the master policy allows (section 6).

## 3. Money flow of one purchase (MINE; numbers illustrative)
A buyer pays `P` for a piece. Settlement is **one escrowed transaction group**, all rows or none:
| Row | Amount | To | Rule from |
|---|---|---|---|
| master **tx fee** (for using the chain) | `max(floor, P x f_m)` | master account | master `policy.pdl` |
| master **tax** (on the value of the purchase) | `P x t_m` | master account | master `policy.pdl` |
| market's own fee / tax (a market sets its own) | `P x f_k` | market owner | market `policy.pdl`, must stay within the master's cap |
| royalty (copy / fork / resale of a lineage) | `P x r` | the parent piece's creator | the piece's lineage row |
| **seller proceeds** | `P - all of the above` | seller | arithmetic |
**The buyer sees the full breakdown before paying** (a quote op, section 8). **Fee vs tax:** the fee is the price of *using the chain* on any transaction (even a transfer for free); the tax is on *value bought*. OPEN whether the owner wants both, one, or the fee waived for the first `play`-unit tests.
**Conservation rule (checked on replay, a harness case):** `sum(all balances) + escrow held + bonds outstanding at face = units ever issued - units burned`. A block that breaks it is rejected.

## 4. Tributes and mining for pieces (MINE; the term is OPEN)
I read "tribute" as a **payment to the chain that buys the right to mine a piece**.
- A **tribute** (units paid to the master account, price from `policy.pdl`) creates a **mint right**: a transaction `MINT_RIGHT | id | owner | tier | ts`.
- **Mining** spends the right: the **rarity and the template** are drawn from the **block hash plus the right's id**, with the odds table in `policy.pdl`; the op `piece_mint --source mine --key <right id>` writes the piece. Anyone replaying the chain gets the same piece, so a model or a harness cannot rig it, and the right cannot be used twice (idempotency key).
- **Direct buy** paths: the chain's catalog at a **set price**, an auction (Auction HQ), an NPC shop, or a **market's own price** (a market lists pieces it holds). All end in the same settlement of section 3 and the same `piece_mint` / `NFT_XFER`.
- Rarity is part of the data (`TIER` rows), never a hidden constant.

## 5. Bonds and the Fed role (MINE)
- **Bond (`BOND | id | issuer | face | coupon | maturity | ts`)**, bought for units from the master account, repaid at maturity with coupons from the treasury. Selling a bond **withdraws** units from circulation; buying one back **adds** them: that is the chain's monetary lever, set by **deterministic rules in `policy.pdl`** (target treasury balance, a rate, caps), **never by a model** (house rule: models never decide numbers or promotion).
- **Rails (not optional):** a **paper `play` unit first**; a per-issue and per-day cap; a kill switch flag file the runner polls; every issue, coupon and buyback is a ledger row **and** a signed chain transaction; a **bond is also an NFT** (owner, transfer, resale on the exchange).
- OPEN: who may buy bonds (players, pets, WSR governments); whether the rate floats with the exchange's preferred-value averaging (CHAIN-ECONOMY section 11); whether the master account may run a deficit.

## 6. Contracts for chain owners and managers (the part to document, MINE)
A **contract** is **data plus a deterministic executor**, so every owner gets the same tools and the harness can test each one.
```
CONTRACT | id | kind | owner | parties | terms | schedule | cap | status | sig
```
| Kind | Terms | Executor writes | Typical use |
|---|---|---|---|
| **tax** | `rate`, `base` (purchases / income / holdings), `payee` | `TAX | contract | payer | amount` + chain TX | a market or guild tax within the master's cap |
| **dividend** | a **share table** (holder -> share), `source account`, `schedule` | one `DIV` row + TX per holder | pay profits to piece / share holders |
| **royalty** | `% of resale / copy / fork`, `beneficiary` (creator or lineage) | attached to the piece's lineage; paid inside the section 3 settlement | creators get paid when their pieces are copied, forked or resold |
| **annuity** | `principal`, `payment`, `period`, `count`, `payee` | `ANN` row + TX each period until paid | pay a pet / player a stream; sell a stream as an NFT |
| **loan** (WSR) | principal, rate, term, collateral | the WSR loan mechanics, settled by chain TX | banks and governments (CHAIN-ECONOMY section 5) |
Rules: a contract is **signed by its owner's key** (`sig_op`); **disclosed to the buyer before a purchase** it affects; **bounded by the master policy** (a market cannot tax above the master cap); **changes take effect after a notice period** and are owner-signed; **every execution is a ledger row and a chain TX**; **disputes** go to the REP / court path of the store doc (sections 13 to 14). Because the schedule is data, **blueprintz event pages** can start, pause and inspect contracts like any other event.

## 7. `policy.pdl` per chain (the knobs, MINE; sketch)
```
POLICY | master_tx_fee    | pct=0.5 | floor=1
POLICY | master_tax       | pct=2
POLICY | market_cap       | fee_pct=3 | tax_pct=5
ACQUIRE | kind=pal | via=mine   | price=tribute:tier1 | fee=master | royalty=0
ACQUIRE | kind=pal | via=buy    | price=set:catalog   | fee=master | royalty=0
ACQUIRE | kind=pal | via=copy   | price=pct_of_value:40 | fee=master | royalty=10 to parent_creator
ACQUIRE | kind=pal | via=fork   | price=pct_of_value:60 | fee=master | royalty=5  to parent_creator
TIER    | tier1 | odds=common:80,uncommon:15,rare:5
BOND    | max_issue_per_day=... | coupon_pct=... | kill_switch=state/chain_halt.txt
```
"Buy / copy / fork **depending on the chain**" = the `ACQUIRE` rows a chain publishes. Defaults live in data and are owner-signed; the reader (`policy.pdl` reader with range checks) is on the store roadmap and not built.

## 8. Reuse surfaces (menus, events, other agents)
| Op / event | What it does | Where it shows |
|---|---|---|
| `chain_fee_quote` | the full breakdown of section 3 for a price, a buyer and a market (read-only) | Auction / Exchange / Store HQ before "Buy"; any agent |
| `chain_settle` | the escrowed settlement group; calls `piece_mint` / `NFT_XFER` | buy buttons, auctions, NPC shops |
| `tribute_pay`, `mint_right` | pay a tribute, hold a right | a "Tribute" menu row; the miner |
| `bond_issue`, `bond_buy`, `bond_redeem` | the Fed role | an Exchange HQ "Bonds" tab; the treasury view |
| `contract_new`, `contract_run`, `contract_status` | section 6 | a "Contracts" HQ window, blueprintz pages |
Each is a **C op with a stable CLI**, an **events-hq page**, a **registry command**, and a **pal harness with mutation tests**, so the same thing runs from a menu, a script or another agent.

## 9. "Uncheatable": what this design gives, and what it does not
**Gives:** signed transactions; balance checked at inclusion (a CHAIN-ECONOMY hardening item, not built); the **conservation rule** checked on replay; **verifiable randomness** from block hashes; escrow with a dispute window; every rule readable as data.
**Does not give by itself:** tamper-proofness on **one machine**. If one person holds the keys and the only copy of the chain, they can rewrite it; "uncheatable" needs **independent verifying copies** (p2p storage nodes, CHAIN-ECONOMY section 7) and **key custody** rules (the wallet key must not be reused for social signing). Until then the honest claim is "**cheating is detected on replay by anyone holding a copy**", not "impossible".

## 10. Build order (each step: a pal harness, a negative test, a scratch chain, paper `play` unit first)
1. **Chain hardening (CHAIN-ECONOMY item 4):** rebuild stale ops, balance-check every TX at inclusion, signatures; the conservation rule as a harness case.
2. **`NFT_MINT` / `NFT_XFER`** (owner OK given 2026-10-28 by "minting is an op") + ownership query; then **`piece_mint`** on top.
3. **`policy.pdl` reader** with range checks and owner-signed changes; `chain_fee_quote`.
4. **Settlement** `chain_settle` with the master tax and tx fee into the master account; Exchange / Auction HQ stop being paper-only.
5. **Tribute + mint right + mining draw**; tier tables.
6. **Contracts** (tax, dividend, royalty, annuity), then **bonds**.
7. Windows: master treasury view, contracts, bonds.

## 11. Open questions for the owner
1. Master **tax and fee**: both, or one? Percentages and floors are yours; the defaults I use are placeholders.
2. **Tribute**: is it "pay to get a mining right" (my reading)? Does the tribute go to the master account or to a prize pool?
3. Bonds: who may buy; fixed or floating coupon; may the master run a deficit?
4. May a **market owner** set a fee above zero on pieces they did not mint (a pure reseller)?
5. **Royalty on copy / fork**: to the parent's creator only, or the whole lineage chain with a decay?
6. Which chain is "the" master chain: the new `pet-cones` chain, a third chain, or both with the exchange converting?
7. Real value: this stays **paper `play` units** until the chain hardening of section 10 step 1 is done and you say go.

## 12. Owner answers of 2026-10-28 (to section 11)
- **Q1 master tax and fee: BOTH** (SAID: "5 both"). Percentages and floors stay in `policy.pdl`; the numbers in section 3 are still placeholders.
- **Q2 tribute (SAID: "some kind of payment? suggestions?")** - my proposal, not yet decided: a tribute is a payment to the master account that buys a **mint right** at a **tier**; each tier has a price and an odds table in `policy.pdl`; the roll uses the hash of a block a few blocks AFTER the payment is confirmed (the payer cannot pick it, anyone can replay it); most of the tribute goes to the master account and a small share to a prize pool (split in the policy). Alternatives that fit the same `ACQUIRE` rows later: a recurring tribute that rents a mining slot; a tribute paid in mined cones by pets.
- Still open: bonds (who may buy, coupon, deficit), reseller fees, royalty depth, which chain is the master chain, paper `play` units until chain hardening.

## 13. Not verified
Everything is design. I did not run the chain ops, read the chain replay code in full, or check `ev_chain` beyond its documentation; `NFT_MINT`, fees, tax, bonds and contracts do not exist in code (grep). The tribute reading is my guess.
