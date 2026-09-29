# ROADMAP

Ordered by dependency, not by appeal. Each step lists what "done" means in
terms you can check, so a step is never closed on a clean compile alone.

Status: **DONE** / **IN PROGRESS** / **TODO** / **BLOCKED**

---

## Phase 0 — Windows bring-up and driveability

**DONE.** See `PROGRESS.md` for the evidence.

- [x] install the missing canonical `prisc+x.c`
- [x] fix op-spawn quoting (single quotes through `cmd.exe`)
- [x] fix `button.ps1:196` same-file redirect
- [x] fix op path forward slashes in `shell_cd_run`
- [x] resolve `$(bash scripts/active_corp.sh)` in C on Windows
- [x] prove the equity trade path end to end through the real UI

---

## Phase 1 — Shareholder registry

**DONE**, 2026-09-29. The top task, and the prerequisite for dividends
reaching anyone but the owner and for AI corporations having real
counterparties.

The substrate already existed and was already correctly maintained by a
working trade path. No new storage was invented.

- [x] confirm the substrate: `player_you/holdings.txt` is `<ticker>|<shares>`
      with negatives for shorts; `transactions.txt` is the append-only history
- [x] **settle the open design question** (see below) — the registry is
      holder-kind-agnostic, not player-only
- [x] build the corp -> holders reverse index — `ops/shareholder_registry.c`,
      scanning every piece directory for a `holdings.txt`, writing
      `projects/wsr-pal/shareholders.txt`
- [x] make the registry the single writer of the index AND the only code path
      that moves dividend money
- [x] rewire `corp_apply_finances.c` so a dividend pays **every** registered
      holder pro-rata; the old `owned_by=player_you` special case is gone
- [x] replace the `Shareholder List` STUB (nav 26) with real registry output
- [x] verify: a dividend leaves the corporation exactly once, is credited to
      each holder in proportion to shares held, and the books balance

**Done means:** a dividend leaves the corporation exactly once, is credited to
each holder in proportion to shares held, and the books balance. Not "the op
ran". That is what was verified — see `PROGRESS.md`.

### What the open design question actually was, and the answer

`corp_trade.c`/`corp_buy_stake.c` let a corporation hold a stake in another
corporation, so if AI corps held positions the dividend would have to credit
them too, which changes the schema. Checked rather than assumed:

- `corp_buy_stake.c` writes a stake into the **buyer's own** piece directory,
  so a corp-held position would appear as `pieces/corp_RIV/holdings.txt`.
- Scanning the live tree: **only `player_you` has a `holdings.txt`**. No AI
  corp currently holds anything, so nothing is being mispaid today.

The registry is therefore built kind-agnostic anyway — it classifies a holder
from its piece-id prefix (`player_` / `corp_` / `gov_` / other) and pays a
corp-held row into that corp's own `cash`, exactly like a player. That costs
nothing today and means the first AI corp to go long needs no change to this
code.

### Why reconciliation is NOT against `shares_outstanding`

The original plan said the index should be reconciled against each corp's
`shares_outstanding`. **That is not possible and the plan was wrong.**
`shares_outstanding` is not a share count: `scripts/ensure_entities.{sh,ps1}`
scrapes the first decimal off the profile line "Shares of Stock Outstanding:",
which is in **millions**. Live `corp_AFL` has `shares_outstanding=15.15` while
the registry counts 55 whole shares. `market_cap` is consistent with it only
because `market_cap = stock_price x shares_outstanding` — both are in the same
synthetic unit. Comparing whole-share holdings against a millions-scaled
profile number is meaningless.

Reconciliation is instead against what can be checked: the per-corp sum of
registered rows, and the invariant that a payout's credits sum exactly to what
left the corporation. The index carries `shares_outstanding` per row, labelled
as a different unit, rather than quietly implying the two agree.

Making `shares_outstanding` a real share count is a **separate, still-open**
data-modelling task (it would mean a real IPO share count and a real float).
It is not part of this phase and should not be attempted casually — the price
formula divides by it.

---

## Phase 2 — Macro: calendar, GDP, taxes, government bonds

**IN PROGRESS.** This phase was inserted and moved ahead of payroll on
2026-09-29, reversing the earlier plan to do payroll second. The reason is
that payroll is downstream: wages need a tax system to withhold against, and
money that is never taxed, never borrowed, and never priced is a weak
foundation to hang wages on.

**The diagnosis, verified in code rather than inferred from names:**

- **No calendar exists.** *(RESOLVED 2026-09-29 — see 2.1.)* `wsr_menu_input.c:580`
  used to do `turn_number++` once per End Turn and that was the whole of time.
  There was no year, no quarter, no multi-turn schedule. `weather_update.c` kept
  its own private `season_tick`. Both prior docs referenced a `3_months` schedule
  that was never built. Every periodic mechanic — yearly tax, bond coupons,
  payroll — was blocked on this.
- **No taxes are collected.** Grepping every writer of the tax fields: the only
  writer of `tax_rate_adj` is `gov_trade.c:111`, and **nothing reads it**.
  `gov_trade.c:92` does `revenue += revenue * 0.01f` — that is not taxation, it
  is a number growing by 1% of itself, from the template seed `revenue=67.84`.
  No cash moves from any corporation or the player to the government; the
  government's own `cash=42.6` never changes. `gov_decide.c` then makes genuine
  fiscal decisions (deficit + high debt -> cut spending) off a revenue figure
  with no connection to the economy.
- **No government bonds exist at all.** Only *corporate* bonds, at a hardcoded
  `0.06` (`corp_action.c:136`). No treasury, no yield curve, no sovereign, no
  borrowing action in `gov_decide.c` at all.
- **GDP is static.** `gov_decide.c:222` reads `gdp` and `debt_to_gdp` and nothing
  ever writes them. Live values `gdp=426.0`, `debt_to_gdp=25.0` are template
  seeds. GDP is an input to policy that nothing produces, and is unaffected by
  the policy it drives. **This observation is factually right and the conclusion
  drawn from it was wrong** — the original computes GDP once at world generation
  and never again, and ships the identical constant `25.0000` in every
  government. Corrected at 2.2.

Recorded for context, not as a defence: the original source's `tax_loop.c` was a
placeholder ("Hello, TAX!") per `SOCIETY-ECONOMY-ARCHITECTURE.txt:18,454`, and
real WSR treated government as "purely tax-rate-setter + bond-issuer" (`:121`).
So bonds and taxes are precisely the two macro levers the original had, and both
are inert in this port. This phase is not gold-plating; it is the original
game's own macro layer.

### Why the order below is this order

Every later step is downstream of an earlier one, and the dependency runs
calendar -> taxes -> bonds -> rate benchmark, not by appeal. **Real GDP was in
this chain and has been struck** — see 2.2, which is the worked example of why
the legacy gets asked first.

- [x] **2.1 Calendar** — `ops/econ_calendar.c`, the single writer of time.
      **DONE 2026-09-29.** The design changed mid-flight, deliberately: the
      first cut was `turn / year / quarter` advanced once per End Turn, which
      is *not* the original. The original is a wall-clock (`wsr_clock.c`) plus
      a schedule file (`presets/schedule.txt`), and it moves whether or not
      the player acts. Re-implemented as that port: `GameTime` down to
      centiseconds, 30-day months, the original's cascade carry transcribed
      nesting-for-nesting, colon-delimited persistence in
      `projects/wsr-pal/data/wsr_clock.txt`, elapsed real ms scaled by
      `ticker_speed`, `projects/wsr-pal/data/schedule.txt` ported verbatim, and
      the original's SIGINT/SIGTERM save handler and PID file. Started as a
      daemon by `button.ps1`/`button.sh`; `tick_all.*` deliberately does *not*
      call it. Annual and quarterly boundaries are now addressable.
      **Known and accepted:** every op the schedule points at is still a stub,
      so the clock honestly reports `SKIPPED <op>: not built`. That is the
      original's own state, not a claim of working macros. See
      `PORT-FIDELITY.md` §3 for the full design and for the `ticker_speed`
      label trap (at `day`, one game day takes 24 real days — reproduced, not
      corrected, and `data/setting.txt` ships `hour`).
- [x] **2.2 Real GDP — STRUCK, do not implement.** Originally written as
      "computed from actual economic activity instead of seeded". **The legacy
      says the opposite, and the legacy wins.** `dev/setup_governments.c:131,215`
      computes GDP **once at world generation** as `population × cash_per_cap`,
      splits it across governments by a fixed percentage, and never recomputes
      it; `Debt-to-GDP Ratio` is the literal constant `25.0000` in every
      government file. wsr-pal's `gdp=426.0` / `debt_to_gdp=25.0` are therefore
      already faithful, and "static GDP read by policy but written by nothing" is
      a description of the original, not a regression.

      It also would have broken the game. `gov_decide.c:134` acts only when
      `deficit_ratio < -0.02`; the seed 426 sits *precisely* calibrated to trip
      it, whereas the sum of 50 corps' `book_value` is 84,040 and drops the
      ratio to -0.0001, permanently disabling every fiscal decision. And it is
      the wrong basis regardless — GDP is a flow of output per period,
      `book_value` is a balance-sheet stock, and no corp `state.txt` field
      measures a flow (there is no `revenue` field at all).

      Full working in `PORT-FIDELITY.md` gap 11. **Standing rule adopted as a
      result: ask the legacy before designing a mechanic.** See
      `PORT-FIDELITY.md` §0.
- [ ] **2.3 Yearly tax collection** — on the calendar's year boundary, debit
      corporations and the player at `tax_rate_adj` and credit the government.
      Revenue becomes real cash and the deficit becomes real. Replaces the
      `gov_trade.c:92` self-inflation, which must go, not be left alongside.
      **This is now the first open macro item**, and it is *new* modelling, not a
      port: the original's `tax_loop.c` is 19 lines that write
      `"Hello, TAX!"`. The original's governments were tax-setters and
      bond-issuers (`PORT-FIDELITY.md` gap 4), so the lever is the game's own even
      though the loop was never written.

      **UNITS WARNING, measured before starting.** Tax receipts will be real
      money, but the `deficit_ratio` denominator is the original's *seeded* GDP
      (426), not a real one, because 2.2 is struck. Real tax money is on a
      completely different scale from 426, so a naive implementation drives
      `deficit_ratio` deeply negative and `gov_decide.c` will raise taxes
      forever. The tax amount and the ratio basis have to be reconciled
      deliberately — do not discover this halfway through and "fix" it by
      fudging GDP back into fiction.
- [ ] **2.4 Government bonds** — treasury issuance, a yield curve driven by
      debt/GDP and a policy rate, annual coupons. The existing `debt_to_gdp`
      becomes live because issuing debt finally costs something. The original
      also had bonds as a **player-held asset with a yield-to-maturity column**
      in the portfolio view (`financing.c:79-97`); wsr-pal has neither the
      securities nor the display. See `PORT-FIDELITY.md` gap 2.
- [ ] **2.5 Rate benchmark** — `bank_loan_op.c`'s hardcoded `0.08` and
      `corp_action.c`'s hardcoded `0.06` become spreads over the sovereign yield
      instead of constants. This is what turns "real loans" into real credit.
- [ ] **2.6 Re-sequence the finance pass onto the clock** — the clock is
      wall-clock driven but `corp_apply_finances`/`corp_update_price` still run
      per End Turn. Those are two different cadences, so once 2.3/2.4 have real
      consumers, a `3_months` event can fire on a turn boundary that has not
      been recomputed yet. Tracked explicitly rather than smuggled in with 2.1,
      because the clock port was supposed to be a faithful port and nothing
      else.

**HARD CONSTRAINT, carried from Phase 1:** interest must be computed on the
*dollar* `bonds_outstanding` field. Never on `shares_outstanding`, which is a
millions-scaled market-profile figure, not a share count (see Phase 1).

**Done means:** money moves. Tax revenue is traceable from a payer to the
treasury, a treasury issue raises a real cash balance, a coupon payment is
observably debited, and `debt_to_gdp` changes because of a decision rather than
because a template said so. Not "the op ran".

---

## Phase 3 — Payroll

**TODO**, and deliberately last among the economy work. Nothing about it exists
in the ops yet; `SOCIETY-ECONOMY-ARCHITECTURE.txt` has the design notes only.

- [ ] design employee storage — genuinely new modelling, there is no
      headcount/wage data anywhere
- [ ] gross -> net, **with tax withholding against the Phase 2.3 tax system**
- [ ] deduct from corporation cash, with the same solvency chain as dividends
      (cash -> credit line -> bankruptcy)
- [ ] periodic run on the Phase 2.1 calendar

This remains the largest single piece of new data modelling in the project, and
it is worth more *after* Phase 2: payroll then feeds GDP (2.2), is withheld
against (2.3), and sits in a world with a real cost of money (2.5). Doing it
first means building wages in a vacuum where none of that exists.

---

## Phase 4 — Presentation and proof

**TODO**, and cheap to do alongside anything else.

- [ ] capture PNG snapshots per feature via `k3_frame_capture.ps1`
- [ ] build `presentation.mp4` via `make_presentation_video.py`
- [ ] per the 2026-08-25 house instruction, one presentation per major feature
      as it lands, paced to be watchable

---

## Phase 5 — Fill in the STUB menu

**TODO.** 22 of 31 main-menu methods are `STUB`. Lower priority than the
economy — a menu of buttons that do nothing is worse than a smaller menu that
works, so do this **after** Phases 2-3, and prefer hiding a stub over shipping
it. Note that Phase 2 will want a `Government`/`Treasury` menu item, which is
the first new METHOD row rather than another stub removal.

---

## Explicitly not planned

- **Merging the legacy k32 work forward.** `MSR-DEPRACATED/` has real
  documentation value but its data model and architecture are superseded. Its
  `wsr_econ.h` design notes are the only thing worth carrying across, and only
  conceptually.
- **A price-impact / order-book model.** `player_trade.c` deliberately does not
  move `stock_price`; that is a large piece of market design and is called out
  as out of scope here. It is the natural next phase after the macro layer, not
  part of it.
- **Commodities and production chains.** `SOCIETY-ECONOMY-ARCHITECTURE.txt` §4
  specifies these in real detail (a composable chain, `bulk_bank` fallback, the
  `craft.c` recipe mechanism reused) and §11 orders the build starting with ONE
  commodity (wheat) + one agricultural company + the population pool consuming
  it, explicitly to prove supply/demand pricing before adding more. It is
  **not scheduled here**: it depends on the labor/population layer, which is
  not in Phases 2-3. The only trace of it today is `pop_update.c` reading a
  `food_supply` field that **no op writes** (frozen at the template's 15.0), so
  the famine logic can never fire. That dead scalar is a known loose end, not
  a working supply/demand model.
- **Committing this tree wholesale.** See `PROGRESS.md` — the checkout carries
  300+ other agents' live modifications.
