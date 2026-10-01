# OPERATING-INCOME MODEL — design for review

**Status: PROPOSAL. Nothing here is built.** This is the paper design the
`operating income is the bottleneck` finding calls for. It is written to be
argued with, not implemented blind.

## 0. Why this document exists

A real playthrough (`7b594a6bc`) proved the plumbing conserves value and then
proved the economy is inert: 50 corporations exist, one ticker trades, total
registered share ownership is 0, and **no op credits a corporation for selling
anything**. The only `revenue` in the tree is `gov_decide.c` — government tax
receipts, not corporate income.

That blocks both seeding modes the user asked for:

- **`prerun_years = 0`** ("nobody starts with anything; people farm") requires
  corporations to earn, or there is nothing to farm *with*.
- **`prerun_years = 100`** ("feels lived in") is currently 100 years of decay —
  seeded cash is spent on R&D, bonds and dividends, never replenished, and no
  corporation ever accumulates enough to buy a stake.

So operating income is the critical path, not the pre-setup menu.

### 0.1 The household layer did not exist either *(fixed, `5b5246d95`)*

Starting this work surfaced a bigger version of the same problem. Three ops —
`pop_tick_idle.c`, `pop_update.c`, `market_quote.c` — all **discover** `pop_*`
pieces, and a live world contained **zero**. There was one template,
`pieces_template/pop_downtown`, that nothing ever instantiated. Every one of
those ops had nothing to act on.

Worse, the household template had **no `cash` field at all** (`total_population`,
`birth_rate`, `food_supply`, `avg_wage`, `unemployment_rate`, …). So even with
the piece present, a household had no money to bid with. The missing field, not
just the missing piece, is why §3's bootstrap could never have started.

`ensure_entities` now instantiates 24 households, idempotently, seeded with
cash, env-overridable via `WSR_PAL_HOUSEHOLDS` / `WSR_PAL_HOUSEHOLD_CASH` for
the complexity tiers to drive later. A household is a **district**
(`total_population=10000`), not an individual, so it buys in bulk.

`market_quote` participant discovery went **57 → 81**, which is the equity ops
finally seeing participants rather than just corps. Seeded cash is a **new
rule** — the legacy specifies no starting household wealth, same as it specifies
none for share ownership — and is meant to be replaced by
`seed_cash_household` from the scenario file.

Building the goods market before this would have produced a market with no
counterparty on the other side.

## 1. Fidelity status: this is NEW, and it must be labelled so

> Verified 2026-09-29: zero matches for `employ`, `unemploy`, `labor`, `jobs` or
> `wage` in any non-stub legacy source. `SOCIETY-ECONOMY-ARCHITECTURE.txt:121-125`
> states real WSR is a pure paper-asset simulation whose "commodities" are five
> tradeable paper holdings.

**The legacy has no operating economy.** No revenue, no wages, no goods market,
no production. There is nothing to port, so nothing here can be unfaithful —
it is all addition, and it is governed by the house principle that additions are
labelled as additions (`PORT-FIDELITY.md` §0).

The one thing that *is* faithful is the refusal to let a formula assign a price.
Legacy `analysis_loop.c` writes `stock_price` from a formula, and that is
recorded as a real divergence (`PORT-FIDELITY.md` Gap 1). The goods market below
must **not** repeat it: goods prices, like equity prices, come only from matched
trades.

## 2. The engine already exists — reuse it, don't build a second one

A goods market is structurally identical to the equity market already built and
fought over in `market_quote.c` / `market_settle.c`:

| | Equities (built) | Goods (proposed) |
|---|---|---|
| Book file | `data/book_<TICKER>.txt` | `data/gbook_<GOODS>.txt` |
| Order row | `bid\|1\|price\|size\|who` | identical shape |
| Clearing | last matched trade sets price | identical |
| Ledger | one balanced row per fill | identical |
| Only-writer rule | `market_settle` | `goods_settle` |

**One market engine, two asset classes.** The bugs already found in the equity
path — the holdings key mismatch and the `shares > 0` guard that silently
dropped shorts — are the bugs to expect here, so the goods engine inherits the
per-fill read-back assertion rather than rediscovering it.

## 3. The bootstrap chain (this is the part that must work)

```
        scenario seed cash
                |
                v
      corps hire labour  ----wages---->  population gets income
                |                              |
                |                        households post BIDS on goods
                |                              |
                v                              v
        corps produce goods --------> corps post ASKS on goods
                |                              |
                +----------> GOODS MARKET <---+
                                 |
                       matched trades only
                                 |
                                 v
                    revenue -> corps cash -> retained earnings
                                 |
                                 v
                   corps can now buy stakes (breadth!)
```

Every arrow is either a trade or a ledger entry. **No step assigns a price.**

The two feedback directions, both of which should be survivable:

- **Viable loop:** revenue → cash → wages → demand → more revenue. Population
  grows (labour supply up), corps hire, market deepens.
- **Death spiral:** nobody sells goods → households' bids go unmatched →
  population starves → demand falls → corps cut production and lay off → wages
  stop → nobody can buy. This is *correct behaviour*, not a bug. It must be
  allowed to happen, and the `food_supply` field `pop_update.c` already reads but
  **no op writes** (frozen at `15.0`) is where starvation would be recorded.

### 3.1 Production must steer on sell-through, not on last tick's sales

Implemented in `ops/goods_quote.c`; measured, and the obvious version fails.

The tempting rule is "produce some fraction of what sold last tick", damped for
stability. **That rule is fatal, and it fails silently** — it looks like a
reasonable heuristic and produces no error at all:

- A firm that sells everything it offers always has `produce < sell-through`,
  because produce is a fraction of what sold and all of it sold.
- So inventory decays `100% → 50% → 25% → 12.5%` and the goods market is
  **empty by tick 6**.
- Confirmed by running it: production `500 → 250 → 100 → 50 → 0`, then a market
  with zero offers and zero fills, with nothing in any log to indicate a fault.

The fix is to steer on the **ratio** `sold/offered` rather than the level,
because volume alone is ambiguous: 20 units sold is healthy demand for a firm
that offered 20 and total failure for one that offered 200.

```
want = sold × (0.5 + sell_through)      sell_through = sold / offered, clamped [0,1]

  sold out  (1.00) → ×1.50   grow into unmet demand
  half sold (0.50) → ×1.00   holds steady
  10% sold  (0.10) → ×0.60   backs off, runs inventory down
  no demand (0.00) →  0      spends no cash, which is correct for an idle producer
```

There is no fixed point to fall through, and the last row matters: a producer
with no demand must produce nothing, because production is a cash expenditure
and an idle producer must not bleed. Capped by `WSR_MAX_PRODUCE_FRACTION` of cash
so a firm cannot spend itself into insolvency to fulfil one order.

`offered` must be registered when orders are *parsed*, not accumulated on fills.
Accumulating fill quantity instead silently makes `offered == sold`, the ratio
collapses to a constant, and you are back to the flat damper above — with a
plausible-looking number in the file.

### 3.2 The wage arrow is designed, and is still the blocking gap

The `wages` arrow in §3 exists in `SOCIETY-ECONOMY-ARCHITECTURE.txt` and in the
diagram above, and **no op implements it**. Confirmed dynamically, not just by
reading: `goods_quote` + `goods_settle` are now a working market, and household
cash falls monotonically because households spend and never earn.

| tick | household cash | corp cash |
|------|---------------:|----------:|
| 1    | 53,095         | 53,608    |
| 2    | 39,836         | 55,658    |
| 3    | 27,486         | 56,807    |
| 4    | 17,579         | 56,198    |

Money is **conserved** — it moves household → corp, corp cash stays roughly flat
— so this is not a printing bug. But it is a one-way transfer with no return leg:
households liquidate within ~4–5 ticks and the goods market then has no buyers.
This blocks ROADMAP 2.2 (real GDP from auction prices), because `calendar →
auction → taxes → bonds → rate benchmark` has no circulating income to tax once
households are broke.

## 4. The accounting identities that must hold

`Assets = Liabilities + Equity` everywhere, and these three must be invariant
under any tick:

1. **Cash conservation** — a fill moves cash buyer→seller; nothing is created.
   Already proven in the equity path (37,150.04 → 37,150.04 to the cent).
2. **Goods conservation** — produced goods enter the market, consumed goods
   leave it. `produced = sold + unsold_stock` at all times. A goods unit must
   never appear from a price change.
3. **Employment conservation** — every wage payment moves cash corp→household
   and decrements exactly one job on exactly one payroll. Payroll is not a
   money printer (`ECONOMY-INTENT.md` §4).

A revenue fill is a *different* ledger shape from an equity fill and needs its
own event name, so the two are distinguishable when auditing:

```
Time: ... | Debit: household_<id> | Credit: corp_<T> | Amount: N.00 Dollars |
  Event: goods_settle.+x | Goods: FOOD | Price: P | Quantity: Q
```

Note revenue is a **sale of a good**, not the creation of earnings. Earnings
(retained earnings) only change when the *cost* side is booked, and the tax and
dividend machinery already in `corp_apply_finances.c` consumes that figure.

## 5. Open decisions — these are the user's, not mine

1. **Unit scale.** The legacy quotes shares in **millions** (`shares_outstanding
   = 16.50` million). Corps have no share count in the same units, and goods
   quantities are physical. Does 1 unit of FOOD = 1 real unit, or does the whole
   economy inherit the millions convention? **This has to be settled before any
   price is meaningful**, and it is the single most likely source of a
   scale error that looks plausible.
2. **Goods taxonomy.** How many distinct goods, and per-corp or global? The
   legacy hints at food plus commodities; §4 calls out `food_supply`. A
   per-industry mapping (`AFL = SHIPPING`) already exists in the corp profiles
   (`AFL.txt`, "Industry Group: SHIPPING"), which is a natural key.
3. **Do governments consume goods?** Taxes are cash-only today. If governments
   buy goods they become demand, which feeds the price signal and gives fiscal
   policy a real lever instead of `gov_trade.c:92`'s `revenue += revenue * 0.01f`.
4. **Wage level.** Fixed per job, or endogenous to labour scarcity? A fixed
   wage makes unemployment the *only* cost of a weak economy; an endogenous one
   makes the labour market a second price-forming market.

## 6. Known risks, stated up front

- **Concentration.** With 50 corps and no holdings seeding, one lucky early
  trade could hand one corp everything. The pre-run is where this shows up, so
  the pre-run summary must **report the ownership distribution**, not just that
  it ran.
- **Performance.** 100 years × 4 quarters = 400 ticks × (50 tickers + N goods)
  × two market passes. This is the hard constraint on `prerun_years = 100` and
  may dictate a headless fast path distinct from the playable one.
- **The dead `food_supply` scalar.** Famine logic in `pop_update.c` is currently
  unreachable. If operating income lands without wiring that field, the
  population layer still cannot respond to scarcity and the "flourishing market
  grows population" loop is only half-connected.
- **Interference with the equity market.** Once corps hold cash from revenue,
  they bid on equities, which changes equity prices — the dependency chain
  `calendar → auction → taxes → bonds → rate benchmark` finally becomes real.
  That is the intent, but it means the goods layer can destabilise the equity
  layer, and both need the per-fill assertions running.

## 7. What is deliberately NOT in this design

- No population growth formula. Growth follows employment and goods actually
  sold.
- No GDP number. GDP must become a sum over discovered real prices, never a
  seeded constant (`ECONOMY-INTENT.md` §5, ROADMAP 2.2).
- No fixed entity count. Everything is discovered from the pieces directory.
- No price formula. Not for goods, not for equities.
