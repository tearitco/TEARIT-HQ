# Auction screen (network cell): design

Status: DESIGN ONLY, written 2026-10-07 by claude. Nothing here is built.
Owner brief (2026-10-07): "auction is a screen. it allows you to put things up for auction, or buy things, using blockchain cones, or playmoney for testing etc. it can also work as a coin exchange, and show who's buying (other players; AI can buy/sell also here). There's a search. It is based on the old p2p platform (`@.SHIP.../2.p2p.platform]GROQ]xc0]0000/`) and our current chain/p2p." Earlier: "do auction in network; a way to trade (through phones also); items 2; gate later; events?"

Related: `SOCIETY-ECONOMY-ARCHITECTURE.txt` section 6 (ledger/auction marketplace, in `014.wsr-pal...`), `IRC-FORUM-CHAIN-HQ-WINDOWS.md`, `NETWORK-CELL-HQ-WINDOWS-DESIGN.md`, the TEARIT-HQ Marketplace concept doc (outside the repo, `13.claude-xoxo/`), NIGHT 30/31.

## 1. What exists (read, not assumed)

**Old p2p platform (reference only, 2025-03):** `auction.c` (about 70 lines) appends one line to the local `blockchain_<port>.txt`: `Auction: <item_hash> (<name>) listed by 127.0.0.1:<port> min_bid <n>`; it refuses a hash already listed. The big `p2p...QURE.c` parses those lines into a table of at most 10 auctions (`MAX_AUCTIONS`), skips the node's own, and prints them (`auction <hash> <min_bid>`, `auctions` commands). `send_coins.c` counts balance from `Send:` lines starting at 100. The owner's own notes say: send_coin stopped working and **auctions don't update**, and the chain is "real" only as a start. **There is no bid, no close, no settle, no escrow** in it. Take the idea (an auction is a line on the chain), not the code.

**Current house chain (`041.pal-chain`):** wallets (`wallet.txt`, password hash), global ledger `data/blockchain.txt` (`BLOCK|n|prev|nonce|hash|ts|miner|` + TX lines), `chain_send.+x` appends `TX|from|to|amount_millicones|ts|tx_id` after a balance check, `chain_miner.+x`, `chain_balance.+x` (the one authoritative derivation), `palnet_peer` mesh (outbox/inbox). **No signing** (`tx_id` is a dedup key only, per `chain_send.c`), **no escrow**, only one transaction type (balance transfer), peers are same-machine (see memory cross-machine-networking-plan).

**Event registry:** has `phone_send` and the network cell menus (IRC / Forum / Chain). No auction, bid or trade command.

## 2. Principles (from the lessons)

- Append-only ledger, cursor reads, marker file growth, never mtime. A bid is a row; nobody edits one.
- The screen is a thin renderer (`.xhtpm` + one compiled manager `<module>` + action shims), same three-part shape as `music-player-hq`. No per-app C in `khtpm_core_render.c`.
- Anything that can be exercised must have a pal harness (`_shared-lib/harness/`) before it is called done.
- One authoritative derivation. Auction state (open / highest bid / closed / settled) is **derived** by replaying the auction ledger, never stored a second time.
- Humans, AI entities and phones are all just **actors** writing rows. Models never decide: an AI actor places a bid through the same command a person does (NIGHT 26: deterministic code validates).

## 3. Layers

**A. Currency (`unit=`).** Every price is `<amount> <unit>`. Units: `cones` (the real chain's millicones, via wallets) and `play` (a play-money balance, one ledger per desk, for testing). The auction code never touches wallets directly; it calls a **settlement adapter** with two implementations: `play` (writes a play-money ledger row) and `chain` (calls `chain_send.+x`). Until chain signing and escrow exist, `chain` stays **gated off** and the screen shows "play money only". This is the "gate later" from the owner.

**B. Auction ledger** (one append-only file, new, e.g. `auction/ledger.txt`; location to be decided with the network cell):
```
LIST   | id | seller | item_kind | item_ref | unit | min_bid | reserve | ends_at | ts
BID    | id | bidder | amount | ts
CANCEL | id | seller | ts
CLOSE  | id | winner | amount | ts          (written by the manager when ends_at passes, from derived state)
SETTLE | id | status=ok|failed | tx_ref | ts (written after the adapter reports)
```
Derived state per auction: highest valid bid (>= min_bid and > previous high), closed if CLOSE exists, settled if SETTLE exists. Replaying the file always reproduces the same state.

**C. Items ("items 2").** `item_kind` is the extension point. I read "items 2" as **two kinds first**; recommended: `entity` (a creature, robot or object that already has a folder, so the existing entity inventory/ownership applies) and `coins` (the coin-exchange case: sell cones for playmoney, or the reverse). Pages, curricula and tech trees from the marketplace concept doc are later kinds, because their packaging formats do not exist. **Confirm with owner which two.**

**D. Coin exchange.** A `coins` listing is just an auction (or fixed-price listing) where `item_ref` is an amount of one unit and `unit` the other. The "who is buying" board is the derived list of open bids with bidder names, newest first.

**E. Search.** A filter over the **derived** state: by `item_kind`, unit, price range, seller kind (player / AI / school), text match on `item_ref` name. First version = a plain scan with filters in the manager (the ledger is small). A bank-weighted synonym search (NIGHT 22/31 machinery) is a later option, not a requirement.

**F. Actors.** Player, AI entity, robot, phone. Each action is an event COMMAND in the registry (`auction_list`, `auction_bid`, `auction_cancel`, `auction_close`) so a pal, a menu row, a phone message or an AI actor can cause it identically. The phone path is `phone_send` carrying a short command (`bid <id> <amount>`) that an inbox watcher turns into the same ledger row; it needs the actor's identity check (see Gate).

**G. Screen.** A network-cell window (`auction-hq`): search bar, results list (id, item, price, ends, high bidder), a "my bids / my listings" tab, a "Buy / Bid" and "List an item" panel, and the buyers board. Rows are bounded choices (house rule: structured rows, not free chat). Whether it is its own `<window class>` or a tab of the Chain window: open question 3.

## 4. The gate (explicitly later)

Cones can only move for real once these exist (none do today): **(1) signed transactions** (chain_send source names this gap), **(2) an escrow transaction type** (lock the high bid at CLOSE, release to the seller on item transfer, refund otherwise), **(3) cross-machine peers**, **(4) an authenticated actor identity for phones and AI** (a bid must prove who sent it). Until then: `unit=play` only, and `unit=cones` rows may be *shown as valuations* but never settled. The settlement adapter is the single place that flips.

## 5. Build order (each step with its own pal harness first)

1. Ledger + derivation op (`auction_state.+x`: replay -> state file) and a case file covering list / bid / low bid / cancel / close / double-list / replay determinism.
2. Play-money ledger + `play` adapter (balance check, hold on bid, release or pay on close).
3. Registry commands + shims.
4. Manager `<module>` + `.xhtpm` screen (read-only list and search first, then bid/list actions).
5. Item transfer for `entity` kind (uses existing entity ownership; needs a read of how ownership is stored before designing).
6. AI actor bid command + phone path.
7. `chain` adapter, behind the gate.

## 6. Open questions for the owner

1. Which two `item_kind`s first (recommended: `entity` and `coins`)?
2. Fixed ledger location and whether bids are per desk or world-wide (the network cell implies shared; same-machine peers only today).
3. Own window or a tab inside the Chain window?
4. Fixed-price "buy now" listings as well as auctions? (the brief says "buy things")
5. Anti-sniping / end rules: fixed `ends_at` only, or extend on late bids?
6. Play money: who mints it, and is it per user or per desk?
7. Does the curriculum/page marketplace (concept doc) become later `item_kind`s of this same screen, or a separate store?

## 7. Exchange window + valuation + who uses it (owner, 2026-10-10)

The auction and a separate **crypto exchange** are x11-hq windows. Users sell **NFTs** (food grown in the game, on chain; later pets and machines), trade things and send chain coins to others. Pets and WSR entities are the main users: AI entities on the chain, always trading for real needs, which keeps the chain live and honest with no human users. New units besides `cones` (preferred, old chain) and `play`: `mined` (the pets' chain). Full plan: `44.xyz.01.00/@.apps/pet-trainer/CHAIN-ECONOMY-DESIGN.md`.

### Exchange valuation: everything is averaged into preferred value (owner, 2026-10-10)
- **Preferred** = the old chain's cones (final, 100% confirmed). Every other chain's unit (the pets' mined cones, test and user chains) has a **preferred value**: the running average of its real trades against preferred cones at our exchange (volume-weighted over a window set in `exchange.pdl`, with a floor and ceiling so one trade cannot move it; a configured seed value until trades exist).
- Using a non-preferred chain through our exchange costs a **fee**, "like a transaction fee": it is paid in preferred cones, or the amount is **auto-exchanged into preferred** at the averaged rate. So all chain activity resolves into preferred value and nothing floats free of it. Fee %, averaging window, floor/ceiling and which chains are accepted are settings in `exchange.pdl`, **editable in the Exchange x11-hq window** once it exists.
- Deterministic code computes and applies the rates (models never decide numbers). The rate history is an append-only ledger (`RATE | unit | preferred_per_unit | window | trades | ts`); every conversion and fee is a ledger row and a chain TX.
- **The Exchange window must show it** (owner: "we wanna see the window for those exchanges"): a per-unit table (last trade, averaged preferred value, change, volume), the fee/rate settings editor, a live trade and order feed saying who traded (pet, WSR entity, human), the fee income account, and the rate history.

### The soul of the ecosystem (owner, 2026-10-10: "that's the point ... it's the soul of the ecosystem")
The pets are not scripted traders. They are **learners that operate the house the way a human does**:
- They **drive the relay**: they use the exchange, auction, chain and other windows through the same input path a human's keys take (`#.desktop/entity_menu_history/<pid>.txt` KEY_PRESSED / MOUSE_EVENT lines and the windows' state files), with no private API. Every action is a relay line plus the ledger rows it causes.
- They **learn by reinforcement (RL)** with the **in-house models and harnesses**: outcomes become reward/punish (`entity_grade`, the feedback ledger `obs_feedback_log.txt`, the concept bank and skillbook, Laplace score `(reward+1)/(reward+punish+2)`), weights move only through bounded, ledgered edits (`joint_tune`), the models are the house's own (Gemma on the Mac via `ai_backend.pdl`, the HORN provider ladder), and the **harness verdict is the referee**.
- They **use and modify their own harnesses while learning**: a pet may propose new cases, weights and bank rows for the harnesses it is judged by, as candidate edits that must pass `concept_edit_validate` and the locked-harness rules; it never edits the grader that scores it (hash-lock, `DELEGATION-FLYWHEEL-HORN-GHOSTS-DESIGN.md` section 2); promotion tiers and human review apply (preschool/elementary never auto-promote).
- They **watch real human input**: `#.desktop/human_input/<pid>.txt` (real X events only) is demonstration data for trading and every other house task (`IRL-BOOTSTRAP-RECURSION-SPEC.md`). Humans are optional; when they are there, the pets learn from them.
- Why it is the soul: the pets trade for real needs (food, parts, miners, rent), so the exchange has steady, honest, need-driven traffic and the chain stays live **with no human users**; every window is also a training environment; the harnesses grow from use.
**Rails (not optional):** DESCRIBE never CLASSIFY; models never decide numbers or promotion; every change is a diff plus a verdict; paper trading (`play` unit) before real cones; per-pet trading limits, daily loss caps and a kill switch (a flag file the runner polls); managed accounts are only ever reached through capped leases; no keys or wallets in any prompt or log.
