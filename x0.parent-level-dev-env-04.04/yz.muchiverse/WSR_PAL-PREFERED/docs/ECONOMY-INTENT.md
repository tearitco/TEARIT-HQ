# ECONOMY-INTENT

Where this simulation is going, and why each piece exists. Written 2026-09-29.

This is the design intent behind the auction market and the macro layer. It
exists because the roadmap alone does not say *what the economy is supposed to
be*, and because two of the mechanisms below are **deliberate extensions with no
WSR precedent at all** — that distinction is load-bearing and is recorded rather
than blurred.

For what the *original* does, see `PORT-FIDELITY.md`. For sequencing, see
`ROADMAP.md`. Where this file and the legacy disagree, this file is describing
something new and says so.

---

## 1. The core reframe: price is discovered, not computed

The original's own roadmap (`MSR-DEPRACATED/marst-arch-j23.txt:577-581`) stages
price discovery:

1. static prices at startup
2. daily formula repricing (`analysis_loop.c`) — where wsr-pal sits
3. **"prices could be updated per-trade based on supply/demand (bid-ask
   spread)"** — the destination

So stage 3 is the legacy author's stated intent. We are going there, and the
legacy is the justification.

The reason it matters is a specific behaviour the formula **cannot express**:

> A good company can trade far above book value when analysts and the herd bid it
> up. A company nobody wants can trade far below book value, indefinitely.

A formula that pulls price back toward `book_value / shares_outstanding`
structurally forbids the second case, because the formula *is* the price. In an
auction the price is whatever the last matched order cleared at, so "no demand"
becomes expressible: the bid never comes up to book, the spread opens, and the
stock sits there. **That is a feature, not a stuck price** — the book is
reporting that nobody wants it at book.

Everything below hangs off this.

---

## 2. The valuation chain

Fundamentals are an **input to quoting, never the price**. The chain runs:

```
employment + goods demand
        -> corporate revenue & profit (the real fundamental)
        -> analyst's fair-value view  =  book_value / shares_outstanding
        -> quotes posted around that view, widened by spread and momentum
        -> ORDERS MATCH in the book
        -> price = last matched trade
        -> new book value -> next tick's analyst view -> ...
```

Two properties we want and are designing for explicitly:

- **Above BVP is normal and good.** A strong company with rising demand draws
  bids above book. Analysts and other institutions chasing it is momentum, and
  momentum is a feature of real markets.
- **Below BVP is normal too.** A neglected or shrinking company trades under
  book, and *stays* under book, because the demand side simply isn't there.

The spread is what makes both true. A thin book widens the spread, so an offer
lands far under fair value; a contested book narrows it, so price converges on
the analysts' view. **A wide spread is the market telling you it has no opinion,
not the simulation breaking.**

### Where momentum is bounded, and why that is not a bug

Unbounded positive feedback in a small market compounds to absurdity within a few
hundred ticks, which would make the whole economy unusable. So quotes are bounded
to within a fixed fraction of the analyst's own fair-value view.

This bounds the **analyst's reasonableness**, not the price. A price that runs
outside the band still reports its last trade, and the gap between price and the
band is exactly the signal that the market thinks the analysts are wrong. The
mechanic stays; only the arithmetic blow-up is prevented.

---

## 3. Participants - all discovered, never a roster

Every participant and every traded ticker is found by **directory scan**, the way
the legacy's `analysis_loop.c:288-318` uses `opendir`. No fixed entity count
anywhere, because entities are added over time and a hardcoded list would
silently exclude everything added after it was written.

- **Banks** — corporations whose `industry` is `bank` (`corp_AFL`, `corp_AZN`).
  These are the "analysts": they hold the valuation view *and* they lend
  (`bank_loan_op.c`), so an analyst with a thin balance sheet can still take a
  position on credit. **This is how a short gets funded without magic money.**
- **Corporations** — quote and trade their own and each other's stock.
- **Players** — one more participant in the same book, no privileged access.
- **Governments** — set policy rates; see §5.
- **Population** (`pop_*`) — see §4.

**Banks trade with each other, and with corporations, players and population.**
Inter-bank flow is deliberate: momentum is only *alive* if institutions are
trading against each other, not just against the player. A market where
analysts only face the player is a thin market with no real price discovery.

### The ledger

Every fill appends to `projects/wsr-pal/data/market_ledger.txt` in the
**original's own double-entry format** (`MSR-DEPRACATED/financing.c:256`):

```
Time: YYYY-MM-DD HH:MM:SS | Debit: <payer> | Credit: <receiver> | Amount: N.00 Dollars | ...
```

A fill is **one** line, not two. The original's format already carries both
sides of the entry on a single row - `Debit: <payer> | Credit: <receiver> |
Amount: N.00 Dollars` - so a single line *is* the balanced entry. An earlier
draft of this document said "a fill is two lines summing to zero", which
contradicted the very source it cited; the source wins, per the fidelity
hierarchy. The share transfer is deliberately *not* a second ledger row: it
moves a position, not money, and inventing a cash row for it would create
money out of nothing. Shares move in `holdings.txt`, conservation held by
construction (buyer +N, seller -N).

The original replayed every event through `master_reader.+x`; here
`market_settle.c` is the equivalent and is the **only** thing that moves cash
or holdings. Quoting never writes balances, so an order cannot create money by
existing.

---

## 4. Population, employment, and demand

> **No WSR precedent. This is an extension, and the legacy has nothing to port.**
> Verified 2026-09-29: zero matches for `employ`, `unemploy`, `labor`, `jobs` or
> `wage` in any non-stub legacy source. The original is a pure paper-asset
> simulation — its "commodities" are five tradeable paper holdings, and
> `SOCIETY-ECONOMY-ARCHITECTURE.txt:121-125` says so directly. The labor and
> commodity layer is grounded in other projects, not in the original game.

The intent, since it is being asked for:

- **A flourishing market grows population.** More capital formation, more goods
  sold, more firms profitable → more demand for food and commodities → more
  population, above a subsistence floor.
- **Population growth without jobs creates unemployment.** This is the
  important feedback: growth in the *labour force* outpacing growth in
  *employment at the 50+ corporations* produces unemployment, which suppresses
  consumption, which suppresses goods demand, which feeds back into prices.
  A booming equity market that creates no jobs is self-limiting, and should be.
- **Corporations sell goods**, and that revenue is the **real fundamental** —
  the thing that makes book value mean something rather than being a seed.
- **Employment is a cost** to corporations and income to population, which is
  what makes payroll (ROADMAP Phase 3) a genuine link between the labour side
  and the equity side rather than an isolated money printer.

There is one dead loose end here today, recorded so it is not mistaken for
working code: `pop_update.c` reads a `food_supply` field that **no op writes**
(frozen at the template's `15.0`), so its famine logic can never fire. That is a
dead scalar, not a supply/demand model.

---

## 5. Governments respond to the real economy

The macro layer is not decoration; it is downstream of §2 and §4.

- **Policy rates.** The legacy has a `Prime Rate` field (`ai_manager.c:178`) that
  is **hardcoded to 2.5** and only ever displayed. Making it live — and making it
  respond to inflation and growth — is new work. Rate decisions then feed
  `bank_loan_op.c`'s hardcoded `0.08` and `corp_action.c`'s `0.06` as spreads over
  the policy rate (ROADMAP 2.5), which is what turns constant rates into real
  credit.
- **Taxes** are on the clock's year boundary, debiting corporations and players
  at `tax_rate_adj` and crediting the government (ROADMAP 2.3), replacing
  `gov_trade.c:92`'s self-inflating `revenue += revenue * 0.01f`.
- **GDP must grow.** An earlier note struck "real GDP" as never-to-be-built,
  because the legacy computes it once at setup (`setup_governments.c:131`) and
  never again. **That reasoning assumed a fixed roster and is superseded.**
  Corporations and banks are being added over time, so a constant GDP is simply
  wrong — the same class of defect as a static `debt_to_gdp`. Total market value
  becomes a sum over *discovered* live prices, so it grows as the roster grows.
  This is why GDP follows the auction rather than preceding it: the auction is
  what makes market value real instead of seeded.
  - Note the fiscal-rule constraint that follows: `gov_decide.c:134` compares
    `deficit_ratio` against -0.02 using the *seeded* GDP basis. Real tax money
    against a stale denominator drives the ratio deeply negative and makes
    governments raise taxes forever. The tax amount and the ratio basis must be
    reconciled deliberately. **The fix is not to invent a GDP.**

---

## 6. Where the numbers come from - and where they don't

> **The legacy's financial figures are NOT the source. They are placeholder
> scaffolding.** Stated explicitly on 2026-09-29 because they look authoritative
> and are not, and because I initially treated them as data.

The legacy ships `financial_profile.txt`, `balance_sheet.txt` and the
`Equity (Net Worth):` field as if they were real accounts. They are not, and
they do not reconcile. Worked example —
`MSR-DEPRACATED/governments/generated/Red African Union/balance_sheet.txt`:

| line | value |
|---|---|
| Cash | 42.60 |
| Other Assets | 34.08 |
| **Cash + Other Assets** | **76.68** |
| **Total Assets (as stated)** | **140.58** |
| Total Assets − Debt (85.20) | 55.38 |
| **Net Worth (as stated)** | **14.91** |

The assets do not sum, and the net worth does not follow from them. Every
government in the legacy is like this. These numbers may be used for **shape** —
what fields a statement has, roughly what order of magnitude is plausible — and
for nothing else.

**All accounting in wsr-pal is computed, and the identity is asserted.** From
here on, book value is a *derived* quantity that satisfies
`Assets = Liabilities + Equity` by construction, and an imbalance is a bug that
is reported loudly rather than smoothed. The scraped `Equity (Net Worth):` field
is treated as a seed for the opening balance only, never as a live value.

This is the difference between a scaffold and a simulation, and it is why
`PORT-FIDELITY.md` gap 10 ("`book_value` is sourced differently") is a symptom
rather than the disease.

---

## 7. Information asymmetry is the source of alpha

**Banks do not share one valuation. They each hold a view, and the views
differ.** This is the single most important property of the market design, and
it is what makes the auction a market rather than a formula.

In the real world, banks seek *alpha*: return above what the information
supports, earned because they know something others do not, or because they
model risk differently, or because they are first and better-informed. The
realistic simulation of that is straightforward and is how this sim does it:

- **Each bank maintains its OWN fair-value estimate per ticker**, produced by
  its own model over its own inputs, with its own error.
- **Different methods produce different numbers.** A bank weighting recent
  momentum, one weighting book assets, one weighting margin, one modelling
  cyclicality — they will disagree, and *they should*.
- **The disagreement is the profit opportunity.** A bank whose view is
  genuinely better than the crowd's discovers it by trading against the crowd
  and finding out it was right.
- **The market converges on the views that were RIGHT, and punishes the ones
  that were wrong.** That convergence is emergent — nothing computes a "fair"
  price. It falls out of banks bidding on their own beliefs and being shown to
  be mistaken when the price moves against them.

So: **there is no single authoritative fundamental.** Valuation is per-analyst.
The `fair` value in `market_quote.c` is *that bank's* view, and it must not
become a shared constant again — a shared fundamental is precisely the thing
that made the old formula incapable of expressing disagreement, below-book
clearing, or alpha.

This also resolves how momentum and valuation coexist honestly: momentum is what
one bank sees in recent flow, valuation is what another sees in the accounts.
A bank weighting momentum bids a name up; a bank weighting assets does not. The
spread between them *is* the market.

---

## 8. Dividends reduce retained earnings, not just cash

**A dividend is a distribution from equity.** Under GAAP it reduces retained
earnings within shareholders' equity, and cash leaves the balance sheet. A
dividend that only debits cash leaves equity untouched, so book value per share
never falls, and a stock can pay dividends forever while its book value stays
inflated.

That is not a stylistic point — it is fatal to this simulation. Valuation is
BVP-based, so a dividend that does not touch equity inflates every future
valuation, and the auction converges on a number that drifts upward with every
payout. The dividend path must debit equity as well as cash, or the whole market
slows becomes fictional.

Implemented in `shareholder_registry.c` and `corp_apply_finances.c`.

---

## 9. BVPS follows the SEC/Yahoo convention

```
BVPS = total common equity attributable to common shareholders
       --------------------------------------------------
                shares outstanding
```

Real-world specifics that this sim adopts:

- **The numerator is COMMON equity.** Preferred equity is excluded. Only the
  common/ordinary share class participates in book value per share.
- **The denominator is shares OUTSTANDING** — the actual count of common shares
  in existence, not a weighted average and not an authorised figure.
- **Treasury stock is excluded from the denominator** (US GAAP treats it as a
  contra-equity account, and it is not outstanding). So a buyback that retires
  shares raises BVPS mechanically, which is the correct real-world result.
- **Issued vs outstanding is a real distinction.** A corporation may have issued
  shares that are not yet outstanding; only outstanding ones divide.
- **The legacy's `shares_outstanding` is millions-scaled and is NOT a share
  count** (`PORT-FIDELITY.md` gap 8). It cannot be used as-is for this ratio.

So the sim needs a genuine **share count** distinct from the legacy's
millions-scaled market-profile figure. Once entity count and issuance grow, the
two must be reconciled deliberately, and the registry's cap table is the natural
home for the real count. This is a prerequisite for a correct BVPS, not a
refinement.

**Shorts** (per the user's direction, modelled on real practice): a short
position is a **marginable liability of the seller to the buyer**, marked to
market, not an asset. It does not create equity. So shorting does not change
BVPS by itself — it changes the *price* the market clears at, and it changes the
seller's balance sheet. This is why shorts can push a price below book without
any accounting inconsistency: the price is a market fact, not a book fact.

---

## 10. Spend is investment in earning power, and should compound

Three distinct spends, three distinct accounting treatments, chosen to match
real practice:

- **R&D** — investment in developing new or better products and systems. Under
  GAAP this is **expensed as incurred** (US GAAP, since ASC 730) unless it
  qualifies for capitalisation. Default here: **expensed**, hitting the income
  statement immediately. Its return is not immediate revenue; it raises future
  *earning power*.
- **Marketing** — builds **goodwill and demand**, and is what lets a company
  charge a *higher price for the same product*. So marketing's measurable payoff
  is a **pricing premium**: it raises the price the firm can charge, which shows
  up in revenue and margin, not in a one-off cash kick.
- **Growth** — investment in **increasing assets and employees**. Capitalised:
  it adds to assets on the balance sheet (and brings headcount, which feeds
  employment and payroll). It is not an expense.

**Earning power, not a fudge factor.** The current code nudges `book_value` by
`growth_pct / 1000` and pokes the price by `marketing_pct / 1000`. Those are
placeholders of exactly the kind §0 rejects. Real mechanics:

- Spend → **assets, headcount, or goodwill** on the balance sheet (a real entry).
- Spend → **capability**: R&D and marketing raise a firm's *pricing power* and
  *cost efficiency*, which then show up in **revenue and margin on the income
  statement**.
- Earnings power is **persistent and cumulative** — a firm that has invested
  keeps earning more, which is why incumbents compound and why a one-tick bonus
  is not a model of investment at all.

The honest constraint: the sim must not let spend create earnings with no
mechanism behind it. Every dollar has to be traceable from a cash outflow to a
balance-sheet entry to a later income-statement effect, or it is decoration.

---

## 11. Abstraction should be switchable, at both levels

> **The sim will have BOTH an abstracted economy and an explicit one, and the
> switch is a first-class feature. Documented as intent because it is a big
> deal: it determines the data model, not just a rendering setting.**

**Level 1 — the economy's abstraction switch.** Corporations will eventually own
**real things**: real estate, food, computers, commodities, machinery. They sell
to other corporations, to governments, and to the public pool.

- **Abstract mode (default first):** a corporation is an earnings engine with no
  inventory. It sells a *quantity of output* it produces from capital and
  labour, at a price its market power allows, into a single aggregated demand
  pool. Fast, tractable, and enough to make the auction and macro layers real.
- **Explicit mode:** actual goods with names, quantities, perishable/lumpy
  supply, individual buyers, and a spot market for each. Corporations can
  actually run out of wheat, and a shortage means something specific.

Both must produce the **same income statement shape** — revenue is revenue — so
that switching modes does not invalidate any accounting, the ledger, or the
auction. That means the abstraction has to live *behind* the income statement,
not in it. Designing the interface so the switch is real, and building the
abstract side first, is the intent.

**Level 2 — governance's ambition.** Governments are to be *accurately*
simulated and prepared for the heavier layer once the basics hold:

- treaties, tariffs, tax changes, wars, **drafts**, policy, **elections**
- **government type changes** (in the vein of Civilization)
- **government bankruptcy** — a government can fail, default, or be restructured

Their books must therefore be real three-statement accounts from the start,
because every one of those acts is a balance-sheet event. A government that can
go bankrupt is a government whose debt is real, and whose `debt_to_gdp` is a
measurement rather than the hardcoded `25.0` the legacy ships.

This is why the accounting foundation is not optional groundwork — it is the
substrate the entire macro layer is written against.

---

## 12. Fidelity ledger - what is port vs. what is new

Kept explicit so nobody goes looking for an original to diff against.

| Mechanic | Status |
|---|---|
| Order book / auction, per-trade price | **Legacy intent**, stage 3 of `marst-arch-j23.txt:580` |
| Ledger double-entry format | **Port** of `financing.c:256` |
| Dynamic entity discovery | **Port** of `analysis_loop.c` opendir pattern |
| Wall clock, 30-day months, schedule | **Port** of `wsr_clock.c` + `presets/schedule.txt` |
| Stock price formula constants | **Port**, exact — now demoted to analyst valuation input |
| Shorts (negative share counts) | **Pre-existing** in the registry; legacy has an `Options Long/Short` line |
| Analyst quotes / market making | **New** — banks as market makers, no legacy analogue |
| Momentum term | **New** — the legacy formula has a momentum blend, but not as a quoting input |
| Population, employment, unemployment | **New** — zero legacy precedent, verified |
| Goods sales as the fundamental | **New** — no legacy analogue |
| Live policy rate | **New** — legacy's Prime Rate is hardcoded 2.5, display-only |
| Real GDP / growing market value | **New** — legacy seeds it once; needed because the roster grows |
| Taxes, payroll | **New** — `tax_loop.c` is a stub, `salary_loop.c` absent |
