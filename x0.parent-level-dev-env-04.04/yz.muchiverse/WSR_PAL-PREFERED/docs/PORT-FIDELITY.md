# PORT-FIDELITY

Which parts of wsr-pal are faithful ports of the original WSR, and which are
not. Every line here was checked against `MSR-DEPRACATED/` on 2026-09-29 by
reading the original source, not inferred from names or from the port's own
comments.

**The rule being applied:** the original is the golden standard. Where wsr-pal
diverges, the divergence is either (a) a bug, (b) an unexplained loss, or (c) a
deliberate, named decision. Unnamed divergence is the thing this file exists to
kill.

Read this before "fixing" anything in this list. Several items look like obvious
bugs and are actually faithful reproductions of the original's own quirks.

---

## 1. Faithful — leave alone

| Mechanic | Original | Port | Notes |
|---|---|---|---|
| Stock price formula | `analysis_loop.c:99-137` | `ops/corp_update_price.c:87-106` | **Exact match.** Same `book_value/shares_outstanding`, same `market_cap_multiplier = 1 + (log10(mcap)-3)*0.05`, same leverage branch at `debt_to_equity > 1.0`, same `bias_factor = 0.5 + risk/100`, same 0.7/0.3 momentum blend, same `new_price < 0.1` floor. Every constant identical. |
| News generation | `news_loop.c` (236 lines, genuinely implemented) | `ops/wsr_news_op.c` | Real port. This is the **only** `*_loop` in the original that was not a stub. |
| Price history | `analysis_loop.c:176-188` appends old/new/change% | `ops/corp_update_price.c` | Same shape, but see gap 6 on the date field. |
| Corp discovery | `analysis_loop.c:288-318` `opendir("corporations/generated")` | `scripts/tick_all.*` glob `corp_*` | Same dynamic-discovery principle: no hardcoded entity list. |
| 30-day months | `wsr_clock.c:120` `if (g_time.day > 30)` | `ops/econ_calendar.c` `DAYS_PER_MONTH 30` | Reproduced deliberately. Not a bug to fix. |
| Clock rate constants | `wsr_clock.c:92-103` | `ops/econ_calendar.c` `rate_for_speed()` | Carried over as-is, including the mislabelled `day` speed. See gap 7. |

---

## 2. Gaps — mechanical, unfaithful, and worth fixing

Ordered by how much macro health depends on them.

### Gap 1 — prices update on End Turn, not daily *(largest macro impact)*

The original recalculated **every** corporation's stock price from a
**wall-clock daily** schedule, not from a player action:

```c
/* MSR-DEPRACATED/day_loop.c - the whole file */
fptr = fopen("data/day.txt", "a");
fprintf(fptr, "Hello, DAy!\n");
fclose(fptr);
system("./+x/analysis_loop.+x");     // <- the actual payload
```

and `presets/schedule.txt` binds `day_loop` to `1_day`.

wsr-pal runs `corp_update_price` from `scripts/tick_all`, which fires on
**End Turn** (menu item 15). So in the original the market moves while the
player does nothing, every simulated day. In wsr-pal the market is frozen
until the player presses End Turn, and a player who never presses it sees a
permanently static economy.

This is a real mechanical divergence, not a cosmetic one: it changes the
relationship between real time and economic time, and it is the single biggest
reason the current simulation has no macro "pressure".

### Gap 2 — no government bonds, and no bond portfolio display

The original's `financing.c:79-97` wrote a full bond section into the player's
portfolio listing:

```
STOCK & BOND PORTFOLIO LISTING FOR <PLAYER>
BOND PORTFOLIO FOR <PLAYER> (In U.S. Dollars)
PRICE FACE MARKET BOND YIELD
BOND ISSUER DESCRIPTION PAR=100 VALUE VALUE TO MATURITY
<PLAYER> OWNS NO BONDS ....
```

So the original had **bonds as a player-held asset with a yield-to-maturity
column** in the portfolio view. wsr-pal has:

- No government bond issuance at all (`gov_decide.c` has only
  raise_tax / cut_spending / hold).
- `ops/player_list_portfolio.c` is **stocks-only** — it computes cost basis,
  gain/loss and totals, and has no bond section, no yield, no maturity.
- `ops/corp_action.c:136` issues *corporate* bonds at a hardcoded `"0.06"`.

The "OWNS NO BONDS" line is the interesting part: the original's own default
state is a player with no bonds, and the game was clearly built to acquire
them. That path was dropped.

### Gap 3 — two financing actions are missing from the menu

The original's financing menu (`financing.c:431-438`) lists eight:

```
1. Startup New Corp          -> ported (corp_ipo / wizard)
2. Capital Contribution      -> ported
3. Public Stock Offering     -> ported
4. Issue Bonds               -> ported (corporate only, hardcoded rate)
5. Buy Back or Call Bonds    -> ported
6. Extraordinary Dividend    -> ported
7. Tax-Free Liquidation      -> MISSING
8. Spin Off Subsidiary       -> MISSING
```

`corp_action.c` implements 1-6 and has no `liquidate` or `spin_off` action.
`SOCIETY-ECONOMY-ARCHITECTURE.txt:329-335` describes both, so the design intent
survived even though the actions did not.

### Gap 4 — no taxes exist, in the original *or* the port

`tax_loop.c` is 19 lines in full:

```c
fptr = fopen("data/tax.txt", "a");
fprintf(fptr, "Hello, TAX!\n");
fclose(fptr);
```

There is no tax logic in the original to port. wsr-pal's substitute
(`gov_trade.c:92`, `revenue += revenue * 0.01f`) invents a self-inflating
revenue number that moves no cash.

**This one is correctly marked unfaithful in the source and is tracked as
ROADMAP Phase 2.3.** It is listed here so the "nothing to port" fact is on
record: the missing tax system is a gap in the *original*, not a regression
introduced by the port.

### Gap 5 — payroll and dividends had no real original either

`dividend_loop.c` and `payroll_loop.c` are both 380 lines and byte-identical
wrong copies of each other. `salary_loop.c` does not exist. So the whole
Phase 2.3/Phase 3 tax-withholding and payroll work is genuinely new modelling,
not a port. `SOCIETY-ECONOMY-ARCHITECTURE.txt:18,454` says the same.

`ops/shareholder_registry.c` and its dividend path are therefore **new**, not
ports — worth knowing before anyone looks for an original to diff against.

### Gap 6 — price history is stamped with turn numbers, not dates

`analysis_loop.c:182-186` stamps each row with a real calendar date:

```c
time_t t = time(NULL);
struct tm *tm_info = localtime(&t);
fprintf(hist_fp, "%d-%02d-%02d,%.2f,%.2f,%.2f%%\n", ...);
```

`corp_update_price.c` substitutes `turn_number` because there was no clock.
Now that `econ_calendar` exists, this should read the game clock. The
original used *wall-clock* time here even though it had `wsr_clock`; using the
game clock is a judgement call and is flagged rather than assumed.

### Gap 7 — the original's `ticker_speed` labels do not describe their spans

Reproduced as-is, but the consequence needs stating, because the next agent
will otherwise "fix" it:

| `ticker_speed` | rate (game cs per real ms) | real time for 1 game day |
|---|---|---|
| `cent` | 36000.0 | 0.2 real seconds |
| `sec` | 360.0 | 0.4 real minutes |
| `min` | 6.0 | 24 real minutes |
| `hour` | 0.1 | 1.0 real days |
| `day` | 0.004167 | **24.0 real days** |

The `day` setting is not a usable game day — one game day takes 24 real days,
and a 30-day game month takes 720. Only `hour` is calibrated to something
intuitive (1 game day per real day). `data/setting.txt` therefore ships
`ticker_speed: hour`, and `econ_calendar.+x show` prints this table so the
numbers are discoverable instead of being a trap.

### Gap 8 — `shares_outstanding` unit handling

`analysis_loop.c:230-237` reads the profile line as a **string** and strips a
trailing `" million"`:

```c
extract_string_value(line, "Shares of Stock Outstanding:", temp_str, ...);
char* pos = strstr(temp_str, " million");
if (pos) *pos = '\0';
corp.shares_outstanding = atof(temp_str);
```

`scripts/ensure_entities.{sh,ps1}` instead scrape the first decimal off the
line, keeping the millions scale. The original kept the same scale too (it
never converted to whole shares either), so the *value* is faithful, but the
original did at least know and name the "million" unit, and the port silently
dropped the marker. Consequence: `shares_outstanding=15.15` is not a share
count and cannot be reconciled against whole-share holdings — see
`ROADMAP.md` Phase 1.

### Gap 9 — `risk_bias` is not clamped

`analysis_loop.c:69-96` reads `weights.txt`, clamps to 1-100 and defaults to
50. `scripts/ensure_entities.ps1:51-55` applies the value with no clamp and no
50 default. Minor, but it is a divergence in a value that feeds the price
formula directly.

### Gap 10 — `book_value` is sourced differently

`analysis_loop.c:267` computes `book_value = total_assets - total_liabilities`
from two balance-sheet lines. `ensure_entities` reads
`Equity (Net Worth):` directly. These agree only if the source profiles are
internally consistent; the original derived it, the port trusts it.

---

## 3. Time and the schedule — now ported (was the biggest gap)

This was, until 2026-09-29, **the** unfaithful area: wsr-pal had no calendar
at all, only `turn_number++` in `wsr_menu_input.c`, and every periodic mechanic
in the design docs referenced a `3_months` schedule that did not exist.

`ops/econ_calendar.c` is now a port of `MSR-DEPRACATED/wsr_clock.c` plus
`presets/schedule.txt`:

- `GameTime { year, month, day, hour, minute, second, centisecond }` — the
  original's exact field set, with the original's cascade carry
  (`wsr_clock.c:108-131`) transcribed nesting-for-nesting.
- `key:value` **colon** format in `projects/wsr-pal/data/wsr_clock.txt`,
  matching the original's clock file rather than the `key=value` of the
  surrounding `state.txt` files.
- Wall-clock driven, scaled by `ticker_speed` from
  `projects/wsr-pal/data/setting.txt` — same vocabulary, same rate constants.
- The original's SIGINT/SIGTERM save-and-exit handler and
  `data/wsr_clock.pid` PID file.
- `projects/wsr-pal/data/schedule.txt`, ported verbatim including all six rows
  and the `1_hour / 1_day / 3_months / 1_year` interval vocabulary.
- Default epoch `2025-01-01`, matching `wsr_clock.c:36-44`.

One new piece of code, flagged as such: `total_centiseconds()`, a pure helper
that lets the scheduler compare two `GameTime` values by subtraction without
re-implementing the cascade. It uses the original's own field semantics and
30-day months.

**Every command the schedule points at is still a stub** — see gap 4 and gap 5.
The clock reports each unbuilt row as `SKIPPED <op>: not built` rather than
pretending it ran. That is a faithful reproduction of the original's state, and
it is the honest signal that the macro layer still has no consumers.

### Known consequence of choosing the original's design

The clock is wall-clock driven; the rest of the economy is End Turn driven.
Those two cadences are genuinely different, so once `tax_loop`/`dividend_loop`
do get built, a `3_months` event can fire on a turn boundary that has not been
recomputed yet. Fidelity was chosen over convenience. Re-sequencing the finance
pass onto the clock is tracked as **ROADMAP Phase 2.6** rather than smuggled in
with the clock port.

---

## 4. Not mechanical — out of scope for this file

`SOCIETY-ECONOMY-ARCHITECTURE.txt` §2-§4 (population, labor, commodities,
production chains) has **no WSR precedent at all** — the same document says so
at `:121-125`: real WSR is a pure paper-asset simulation, its "commodities" are
five tradeable paper holdings, and the whole labor/commodity layer is grounded
in other projects rather than in the original game.

So a "commodity" or "GDP" mechanic cannot be unfaithful to the original,
because the original has none. Judged against it, they are additions. The only
trace of it today is `pop_update.c` reading a `food_supply` field that **no op
writes** (frozen at the template's `15.0`), so its famine logic can never fire.
That dead scalar is a known loose end, not a working supply/demand model.
