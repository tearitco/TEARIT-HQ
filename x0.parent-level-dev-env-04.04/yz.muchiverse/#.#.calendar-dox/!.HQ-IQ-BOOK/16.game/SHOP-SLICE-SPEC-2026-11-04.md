# Shop slice spec (early-survivor step 6), 2026-11-04

Status: spec written BEFORE the code (the checklist rule). Built in the editor's QUARANTINED book `@.apps/rpg-edit-hq/books/ledger-lab/` as `ops/shop_op.c`. Not connected to the editor's Shop tab, to exchange-hq, or to any player. Parent doc: `EARLY-SURVIVOR-AND-COMMON-EVENTS.md` ("Order to build" 6). Checklist: `../08-roadmap/GRAND-ROADMAP-CHECKLIST-2026-11-04.md`.

## Decisions (owner, 2026-11-04)
1. **Coins are items on the ledger.** One inventory writer (`item_ledger_op`), one conservation rule, one replay. The editor Shop tab's own coin counter (`house.txt` BUY rows in `petslike_core.c`) is a different, older system; it is not touched here and is not bridged yet.
2. **Three item ids stay three** (`cdda.*`, `df.*`, `mcl.*`).

## What exchange-hq is, and is not (READ in `&.hq-apps/exchange-hq/ops/exchange_manager.c` and `exchange.pdl`)
- It trades CURRENCY UNITS (`cones`, `mined`, `play`) in an order book (price-time priority, fills at the resting price, fees, a loans market) and averages every unit's real trades into a "preferred cones" rate.
- It is a PAPER ledger: its own header says balances are not checked and no chain coins move yet (settlement waits for the lease layer, `CHAIN-ECONOMY-DESIGN` 3/10).
- It knows nothing about game items. It has no `cdda.stone` / `mcl.cobble`. So there is NO cross-id item exchange there.
- Consequence for the shop: the coin is named after an exchange unit (`coin.play` = the exchange's `play` paper unit) so a later bridge can mint / burn `coin.<unit>` against SETTLED exchange fills. That bridge is not built and not designed here.

## Cross-id rule
- No direct swap of one item id for another. There is no verb for it.
- Indirect exchange works by construction: every item has its own price row; sell `df.stone` for coins, buy `cdda.rag` with the coins. Two trades, each conserving its own id and the coin id.
- Spread on every item (`sell` price strictly below `buy` price) so a buy-then-sell loop loses coins and nobody can print money by cycling. A `shop.pdl` with any `sell >= buy` is refused when loaded.

## Files (book dir)
- `shop.pdl`: `COIN|coin.play` (the coin item id, must be defined in `items.pdl`); `SHOPKEEPER|shop` (the actor that holds the stock and the till); `PRICE|<item>|<buy>|<sell>` (buy = coins the player pays for 1; sell = coins the shop pays for 1; both integers >= 1, sell < buy).
- `items.pdl`: gets `ITEM | coin.play | Play coin`.
- `state/shop_events.txt`: readable log, capped (64 KB, trimmed to the last 32 KB). Items live ONLY in `state/ledger.txt`.

## Verbs (`shop_op <book> ...`)
- `list`: each priced item with buy, sell and the shop's stock.
- `buy <actor> <item> <n>`: one ledger `trade`: the actor gives n*buy coins, the shop gives n of the item. Refused (nothing written) if: the item has no PRICE row, n is not 1..100, the actor has too few coins, or the shop has too little stock.
- `sell <actor> <item> <n>`: one ledger `trade`: the shop gives n*sell coins, the actor gives n of the item. Refused if: no PRICE row, bad n, the actor lacks the item, or the shop's till holds too few coins ("the shop cannot pay").
- The shop never mints. Stock and the till are filled by the owner with `item_ledger_op mint ... gift` (or later by crafting / the exchange bridge). A shop with no coins simply cannot buy.
- Exit: 0 ok, 2 usage, 3 refused (reason on stdout), 4 not quarantined.

## Conservation
- Each trade is the ledger's atomic `trade` (both legs checked, written together or not at all), so per item id and for the coin id, sum of balances = minted - burned always holds; `item_ledger_op check` replays it.
- Harness proves: a refusal changes no balance; total coins unchanged after every trade; buy-then-sell loses exactly the spread; a bad `shop.pdl` is refused.

## Not in this slice
Dynamic prices (supply and demand), shop inventory restock rules, a real player or pet as customer, the editor Shop tab, exchange-hq settlement, taxes / fees, per-user wallets (`WALLET-CONVENTIONS-AND-OPEN-CALLS-2026-11-02.md`), multiple coin ids.
