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

## 3. Participants — all discovered, never a roster

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

A fill is two lines summing to zero. The original also replayed every event
through `master_reader.+x`; here `market_settle.c` is the equivalent and is the
**only** thing that moves cash or holdings. Quoting never writes balances, so an
order cannot create money by existing.

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

## 6. Fidelity ledger — what is port vs. what is new

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
