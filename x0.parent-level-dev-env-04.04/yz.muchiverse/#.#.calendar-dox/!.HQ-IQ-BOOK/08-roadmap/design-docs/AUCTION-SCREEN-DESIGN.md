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
