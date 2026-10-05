# PORT-FIDELITY

Which parts of wsr-pal are faithful ports of the original WSR, and which are
not. Every line here was checked against `MSR-DEPRACATED/` on 2026-09-29 by
reading the original source, not inferred from names or from the port's own
comments.

**The rule being applied:** the user's direction wins; the legacy is a strong
prior about what worked and what its author intended next, not an authority;
arithmetic is not negotiable. See §0 for the full ordering, which supersedes the
older framing of the legacy as "the golden standard" — that wording caused a
real error on ROADMAP 2.2, which is recorded and corrected rather than quietly
dropped.

Where wsr-pal diverges from the legacy, the divergence is either (a) a bug,
(b) an unexplained loss, (c) a deliberate, named decision, or (d) a **new
mechanic the legacy has no answer for**, which is not a defect. Unnamed
divergence is the thing this file exists to kill.

Read this before "fixing" anything in this list. Several items look like obvious
bugs and are actually faithful reproductions of the original's own quirks.

---

## 0. How to decide what to build: the user, then the legacy, then arithmetic

**The legacy is a good model, not the bible. What the user says overrides it,
always.** The user owns this simulation and knows where it is going; the legacy
is a strong prior about what worked in the original, not an authority.

The order, highest first:

1. **What the user asks for.** This wins, without exception. If the user says the
   economy should have employment, the auction should have inter-bank flow, or
   GDP should grow with the roster, that is the decision — even where the legacy
   has nothing, and even where the legacy did something else.
2. **The legacy's behaviour and its stated intent.** Consulted *first* for any
   question of the form "how should this mechanic behave?", because it usually
   has a real answer, often an unstated one, and frequently a roadmap for the
   very thing being asked for. Two real cases:
   - `marst-arch-j23.txt:580` stages price discovery and names per-trade
     bid/ask supply-and-demand as the destination. Asking the legacy first is
     what established the auction as *fulfilment* rather than invention.
   - `setup_governments.c` computes GDP once at setup. Asking first is what
     **prevented** an unfaithful "improvement" — and also what let the seeded
     basis be discarded once the user said the roster grows. Both halves of that
     were only available because the legacy had been read.
3. **Arithmetic and internal consistency.** Not negotiable. If unbounded positive
   feedback compounds to absurdity in a few hundred ticks, the sim is broken
   regardless of what the user or the legacy asked for. When a bound is needed,
   put it on the *mechanism that is fictional* (an analyst's rationality) rather
   than on the *behaviour that is wanted* (a price that runs up), so the
   mechanic survives intact. See gap 11's anchor, and the momentum clamp in
   `wsr_market.h`.

**What this is NOT.** "Ask the legacy first" is not "defer to the legacy." The
failure mode of treating it as an authority is real and happened here: I struck
"real GDP" as *never to be built* on legacy grounds, when the reason it mattered
was that the roster was growing. Reading the legacy is what should have revealed
that, not what should have closed the question. The legacy tells you what the
original did and what its author intended next; it does not tell you whether
that is right for a simulation that is going somewhere the original never went.

**When the legacy is silent, it is not permission to stop** — it is permission
to build new, and to label it new. `ECONOMY-INTENT.md` §6 keeps that ledger
honest, and it is worth updating it every time a mechanism is added, precisely
so "port" and "new" never blur together.

This is also why several items below are recorded as gaps *in the original*
rather than omissions here. That framing is only correct because the user
confirmed the direction — check it with them before treating it as settled.

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

### Gap 11 — GDP is a setup-time constant in the original

> **Correction, 2026-09-29.** This entry originally concluded "leave it seeded,
> do not implement," on legacy grounds. **That was wrong and has been reopened**
> as ROADMAP 2.2: the roster is growing, so a constant GDP is wrong on its own
> terms. What follows is kept because the *arithmetic* still holds and still
> constrains any implementation — see the deficit-ratio table. Read this section
> for the measured numbers, not for the verdict. Rule corrected in §0.

**The original never computes GDP during play.** `dev/setup_governments.c` derives
it once at world generation and writes it into the government file:

```c
/* setup_governments.c:129-131 — once, from the population preset */
if (strcmp(pool_name, "humanoid_bank") == 0) {
    total_population = pool_pop;
    total_gdp = pool_pop * cash_per_cap;
}
/* :212-215 — split across governments by a fixed share */
double gdp_frac = atof(gdp_share_perc) / 100.0;
gov.gdp = (long long)(gdp_frac * total_gdp + 0.5);
```

Nothing recomputes it. `governments/generated/Red African Union/Red African
Union.txt:3` holds `GDP: 426` from then on, and `financial_profile.txt:53` holds
`Debt-to-GDP Ratio: 25.0000` — the identical constant in **every** government,
from Farland (GDP 287) to Solar Empire (GDP 2611). It is a seed, not a computed
ratio.

**So wsr-pal's `gdp=426.0` and `debt_to_gdp=25.0` are already faithful.** They
are not unfinished business.

**Why "just compute it from the economy" is the wrong fix, measured:**

| basis | gdp | net_operating | deficit_ratio | 2% rule fires? |
|---|---|---|---|---|
| seeded (current) | 426 | -8.84 | **-0.0208** | **yes** |
| sum of 50 corps' `book_value` | 84,040 | -8.84 | -0.0001 | **never** |

`gov_decide.c:134` acts only when `deficit_ratio < -0.02`. The seed 426 is
calibrated so the rule trips; a real corporate-equity GDP is ~200x larger and
silently disables every fiscal decision forever. Two independent reasons it is
wrong:

1. **Wrong basis.** The original's GDP is `population × cash_per_cap`. Summing
   corporate book value is not that, and is not GDP under any definition —
   `book_value` is a balance-sheet *stock*, and GDP is a *flow* of output per
   period. No runtime field in corp `state.txt` measures a flow; there is no
   `revenue` field at all.
2. **It would break a working rule.** The fake GDP is currently *load-bearing*,
   and swapping it for an honest-but-incommensurable number replaces one dead
   input with a more plausible-looking dead input.

**Verdict: the legacy seeds GDP once, and that is what a fixed roster implies —
but this roster is growing, so 2.2 is REOPENED rather than struck.** See the
correction at the head of this section and ROADMAP 2.2. What survives from the
original analysis is the constraint, not the verdict: a GDP derived as a naive
sum of corporate book value is both the wrong basis and silently fatal to
`gov_decide.c`'s fiscal rule, so whatever real GDP gets built has to be
reconciled with that rule on purpose.

Note what is *not* in this gap: the legacy's lack of taxes (gap 4) and its
non-functional dividend/payroll loops (gap 5) are genuine holes in the original
*and* are being filled as new mechanics under the user's direction — so they
belong in `ECONOMY-INTENT.md` §6 as "new", not under a "never implement" verdict.

This does not mean GDP is a good number. It means it is the *right* number for
this game, and any real fiscal mechanic has to be compared against the
original's seeded basis or it will not fire. That constraint carries forward
into 2.3 and 2.5 — see the units warning in gap 4.

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

## 5. The auction, and what a real playthrough exposed

Everything above was established by reading source. This section is what came
out of actually **running** the thing, which is a different and much less
flattering kind of evidence. Until this pass, no one had launched the game:
every prior claim about the market rested on headless builds and scratch trees.

### 5.1 What landed

- `ops/market_quote.c` — each participant quotes from **its own** view and posts
  on the one side that view implies. The offset is a deterministic FNV-1a hash
  of the piece id, scaled by a per-type skill (bank 0.45 → pop 2.20). It writes
  `data/book_<TICKER>.txt` and **never touches a price**.
- `ops/market_settle.c` — matches the book, prints at the resting price, moves
  cash and shares, appends the original's own ledger row, and sets
  `stock_price` from the last matched trade. It is the **only** op that moves
  balances; quoting cannot create money by existing.

The separation is the whole point: the only route to money is a matched trade,
and the only route to a matched trade is a book written by a different binary.

### 5.2 Gap 12 — the world came up **empty**, silently *(fixed)*

`scripts/ensure_entities.ps1` looked for its entity data at
`$SCRIPT_DIR\MarS.StreetRace.wsr]Q]k32\corporations\generated` — *inside* the
project. That tree was renamed away on 2026-09-26 to drop a `$` metacharacter
and never existed here afterwards. The sibling `MarS.StreetRace.wsr]Q]k32`
still exists but its `generated/` subtrees are empty; the data lives in
`MSR-DEPRACATED`.

The failure was **silent**: the script printed `corporations: 0 created, 0
already existed` and exited 0. No error, no warning. The world had **one**
piece (`wsr_main_menu`) and no corporations, so no playthrough was possible and
nothing said so.

Fixed to read the sibling, plus a hard `exit 1` if a source tree is missing —
the lesson is that this class of bug must be *loud*. Now: **50 corporations,
7 governments**, matching the intended roster.

`scripts/ensure_entities.sh` carries the same stale path (worse: it still
escapes a `$` that is no longer in the name). **Still unfixed** — Windows only,
untested here.

### 5.3 Gap 13 — share ownership is not seeded, so the market has no real holders *(open)*

This is the most serious thing the playthrough found, and it is **pre-existing**,
not introduced by the auction work.

`projects/wsr-pal/shareholders.txt` — the dividend source of truth — advertised
`corp_AFL|AFL|player_you|player|55|100.0000`. But `player_you` has **no
`holdings.txt` at all**, and `pieces_template/player_you/` ships only
`state.txt`. So 55 shares were recorded in the index with **no backing record**.

The registry is not a second store of ownership: it is a reverse index
**rebuilt** from every piece's `holdings.txt` (`shareholder_registry.c:247`,
its `rebuild` mode). Running that rebuild on the seeded world produced
`registered=0` and **deleted the player's 55 shares**, because nothing had ever
backed them.

Consequence for the auction: with total registered ownership at 0, the market
only ever trades positions it created itself. A first run produced 16 fills,
all offsetting long/short pairs summing back to zero — real cash moving, real
ledger rows, and **no shares outstanding**. Conservation held perfectly and the
market was still meaningless.

This must be fixed at world seeding by materialising starting ownership into
`holdings.txt`, not by patching the index. Until then, treat any dividend or
market result from a fresh world as unreliable.

### 5.4 Market breadth is one ticker

`market_quote` discovers tradeable tickers from the registry. On the seeded
world exactly **one** ticker qualified (AFL), so `50 created` corporations
produce a **one-stock market**. Quotes: `fair=165.80 bid=178.46 ask=130.73`.

Not a code bug — a consequence of Gap 13 plus the absence of cross-holdings.
Banks trade with each other only if something establishes those stakes; right
now nothing does.

### 5.5 Multiplayer: legacy has hot-seat, the port has none

The legacy `MSR-DEPRACATED/dividend_loop.c` implements it:

- `:281` `num_players` from argv, `:288` one `Player` per player
- `:364` **menu case 17** — `current_player_index = (current_player_index + 1)
  % num_players`. Hot-seat handoff. No separate process, no lock, no
  authentication: whoever is at the keyboard is that player.
- `:373` **menu case 22** — toggles `current_player->ticker_on`, which
  **starts or kills `wsr_clock.+x`** (`:366`/`:374` start, `:368`/`:376` kill)
- `:365-370` on handoff, the clock is started or stopped according to the
  **incoming** player's `ticker_on`

So the legacy clock is **per-player**, and it is opt-in per player.

**The stall logic the user identified is real, and the legacy has no defence
against it.** A player with `ticker_on` set is on a real-time clock — the
world advances while they deliberate, so stalling costs them. A player with
`ticker_off` has their clock **killed**, so they can sit at the keyboard
indefinitely at zero cost. The incentive to move (you need positions to earn)
is real but unenforced; nothing times out a turn and nobody's holdings move
while they think. A hot-seat game with one shared screen inherits every
"whoever is holding the keyboard" ambiguity that implies.

**Port status: none of this exists.** There is no setup menu for player count,
no `num_players`, no hot-seat rotation, no `ticker_on`. `button.ps1` has no
multiplayer action. This is unported, and the ~5-moves-per-turn budget the user
described is not present in either the legacy source I read or the port.

### 5.6 Realism — not yet assessable

A real run does now produce trades, conserved cash, balanced ledger rows and a
price that tracks rather than spirals (fair 165.80, print 156.55, last 121.73,
flagged `[below BVP]`). But with Gap 13 open and one ticker, **this is not yet
evidence of a realistic economy** — it is evidence the plumbing conserves
value. Realism cannot be judged until ownership is seeded and breadth exists.

Known and accepted: trades print at the resting ask, so price sits a few
percent under fair rather than exactly at it. That is the deliberate
anti-self-marking property; a participant cannot mark its own book by quoting
wide.
