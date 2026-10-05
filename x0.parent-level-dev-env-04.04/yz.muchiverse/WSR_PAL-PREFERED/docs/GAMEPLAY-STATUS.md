# GAMEPLAY + ECONOMY STATUS

What is actually working, what a player can actually do today, and what is
still broken. Written 2026-10-01. Every number below is measured, not projected.

**Read this first:** the economy engine is real, tested and self-consistent, but
**none of it runs during actual gameplay.** A player pressing End Turn does not
see the goods market or wages at all. That gap is the single most important
thing on this page.

---

## 1. Gameplay today

The main menu (`projects/wsr-pal/pieces/wsr_main_menu/piece.pdl`) has **27 items,
of which 21 are `STUB`**. Menu items are data-driven `METHOD|label|command` rows,
so this is a content gap, not a code gap.

### The 6 items that actually work

| # | Item | Command | Does |
|---|------|---------|------|
| 8 | Select Corp. | `CYCLE_CORP` | cycle the active corporation |
| 14 | End Turn | `TICK_ALL:1` | run one turn |
| 19 | TICKER | `TOGGLE_TICKER` | ticker display toggle |
| 21 | List Portfolio | `player_list_portfolio` | your holdings |
| 25 | Shareholder List | `shareholder_registry summary` | corp shareholder index |
| 27 | New Game | `START_WIZARD:new_game` | 5-step new-corp wizard |

### The 21 that do nothing
File, Game Options, Settings, Help, Select Player, Culture, Entity Info, History,
General, Tools, Private, Other Trans, Misc. Menu, Chart, Auto, Watchlist,
Research Report, Financial Profile, List Options, Earnings Report, My Corporations.

So the honest state of play: you can start a game, pick a corporation, press End
Turn, and look at your portfolio. You cannot research a stock, read an earnings
report, chart a price, manage a watchlist, or read your own financials. Those are
the screens that would make a market simulator playable, and they are the 21.

### What End Turn actually runs

`scripts/tick_all.ps1` does, per turn:
- `corp_apply_finances` + `corp_update_price` per corp (bonds/loans, R&D,
  marketing, dividends, stock price)
- `shareholder_registry` dividends, `econ_calendar` time/season
- `wsr_news_op` (top movers), `player_settle_futures`
- FSM rounds for corp / gov / pop / weather pieces

**It does not call `goods_quote`, `goods_settle` or `corp_payroll`.** Those three
are registered in `default_op.txt` as *"invoked directly"* — nothing invokes
them. Also missing from `default_op.txt` entirely: `corp_payroll`, and the equity
`market_quote` / `market_settle` pair.

**Consequence: the entire goods-and-wage economy described below runs only inside
`scripts/test_goods_loop.ps1`. It is invisible to the player.**

---

## 2. The goods economy (built, tested, not yet in the game)

### What it is
29 goods derived from the real legacy `Industry Group` field, so 50 corporations
produce across 29 markets (6 insurers, 4 internet corps, 3 air transport...).
One `FOOD` market was the tempting simplification and it was wrong — exactly one
corp produces it, giving a 1-producer market.

Prices are **discovered**, never assigned: every producer and household posts its
own view into a per-good book, and only matched orders trade. A single `FOOD`
market or a formula price would both violate that, which is why neither was used.

### Measured, 16 ticks from a clean world

| tick | produced | fills | sell-through | wages | corp_cash | pop_cash | units |
|-----:|---------:|------:|-------------:|------:|----------:|---------:|------:|
| 1 | 0 | 50 | 1.000 | 171 | 38,434 | 118,886 | 1,000 |
| 4 | 3,270 | 65 | 1.000 | 556 | 39,230 | 111,084 | 8,006 |
| 8 | 8,765 | 125 | 0.922 | 1,310 | 41,488 | 80,741 | 36,091 |
| 12 | 9,143 | 186 | 0.857 | 1,335 | 44,329 | 41,692 | 72,298 |
| 16 | 6,922 | 291 | 0.449 | 656 | 42,246 | 11,193 | 104,881 |

Reproduce with `powershell -File scripts/test_goods_loop.ps1 -Ticks 16 -Reset`.

### What is genuinely working
- **Goods conservation.** A unit only exists because cash was spent to produce it,
  so `produced = sold + unsold` is checkable. Every fill is refused unless the
  seller actually holds the units, and both legs plus both cash balances are read
  back before a trade is accepted.
- **Production steering.** Chases sell-through, and it converges: production
  plateaus near 9,100 and then backs off as sell-through falls. It does not
  oscillate, and it does not die.
- **Corporate solvency.** Corp cash is stable (38,434 → 42,246) instead of
  collapsing, because wages are a share of gross *margin*, not of revenue.
- **The wage leg.** Revenue → wages → households → goods purchases, double-entry,
  cent-exact, read back per payment.

---

## 3. What is broken or open

### 3.1 The economy does not run in the game *(highest priority)*
See §1. Fix is small — add the ops to `tick_all.ps1` and register `corp_payroll`
in `default_op.txt` — but it is a real behavioural decision: goods trades every
End Turn is a lot of state movement, and the original game ran its clock on real
time. That call should be made deliberately, not by omission.

### 3.2 Households liquidate their opening wealth
`pop_cash` 118,886 → 11,193 over 16 ticks. Nobody goes formally broke yet, but
the trend is one-way. The cause is structural: **production chases *sales*, and
those sales are funded by drawing down household savings rather than by income.**
Wages total ~1,300/tick against ~8,650 of goods bought per tick.

The honest fix is to size household bids off *income* rather than accumulated
savings, so demand falls when wages fall and production follows it down instead of
the buyer base going bust. This is a change to the bidding rule and is the next
real piece of work.

### 3.3 Payroll under-charges producers that cannot sell
Wages are sized on `revenue − sold_units × cost`, so a producer that makes units it
cannot sell has a real loss the payroll op cannot see. Margin is optimistic
exactly when sell-through is poor, which is backwards. Correct fix: `goods_quote`
writes production per producer (`data/gprod_<GOOD>.txt`) and cost is charged on
that.

### 3.4 Equal-split wages ignore demand
Every household gets the same wage regardless of what it bought, so employment
does not yet follow demand. Labelled a v1 simplification in the source. Correct
version weights each household by what it bought from that corp.

### 3.5 21 of 27 menu items are stubs
See §1. Not a code problem — the handler ops for most of these do not exist yet.
This is the gap between "simulator that runs" and "game".

### 3.6 Equity market ops are also unwired
`market_quote` / `market_settle` are registered but not called from `tick_all.ps1`,
so the auction-driven *equity* market — the thing that is supposed to set
`stock_price` — is not running in the turn loop either. `corp_update_price` still
sets price by formula. That contradicts the core design rule (price is discovered,
not computed) and should be resolved alongside §3.1.

---

## 4. Honest bottom line

The economy is **structurally sound and numerically stable**, with conservation
enforced by construction and three real bugs found and fixed by measurement rather
than assumption (the fatal 0.5 production damper, the revenue-vs-margin payroll
trap, and a stale-binary "rebuild" that was silently running old code).

Gameplay is **thin**: 6 of 27 menu items work, and the whole goods/wage economy is
not connected to the turn loop. The gap is wiring and menu content, not the
simulation core — which is the good kind of gap, but it is still the gap.

Priority order: wire the ops into the turn loop (§3.1) → income-sized household
bids (§3.2) → menu items (§3.5) → equity market (§3.6).
