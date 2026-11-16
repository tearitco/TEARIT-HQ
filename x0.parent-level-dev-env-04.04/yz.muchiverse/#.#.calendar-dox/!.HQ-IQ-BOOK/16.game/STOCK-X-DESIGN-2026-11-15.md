# stock+x: a paper-trading broker app for the +phone (design, 2026-11-15)

> Owner 2026-11-15 (SAID): "a new phone app, `stock+x`, that works like the other middle-house stock broker, fetching prices from Yahoo and letting users buy and sell them, and later charts, watchlists, options, fractional stocks, etc. Basically a Robinhood clone, in our framework." Written by Claude Sonnet 5.5. Labels: **READ** = I opened the file or ran the command; **DOC** = my reasoning or proposal (not decided). Nothing in this document is built yet.

## 0. The short version
1. **It is not a greenfield.** A working Yahoo broker already exists: `1.TPMOS_c_+rmmp.0103.0001/projects/yahoo/` (16 C files in `ops/` plus a manager, 3,678 lines in all; fetch, lookup, buy, sell, add credit, portfolio, profit and loss, options pricing, option buy/sell, linear-regression predictions, a manager and a layout). It was migrated and marked OPERATIONAL on 2026-04-28 (READ `REPORT.md`, `summary/sum&suc_yahoo-a28.txt`; its own state file shows a live NVDA lookup at 216.61 on that date). stock+x is the **port of that project into the +phone framework**, fixing the four things in section 3 that would be wrong in our house today.
2. **Its fetch is broken today.** `fetch_stock.c` speaks plain `http://` over a raw socket. Checked 2026-11-15 with one request: `http://query2.finance.yahoo.com/v8/finance/chart/AAPL` answers **301 to https**, and the `https://` URL answers **200 with JSON** (AAPL, USD, `regularMarketPrice` 356.27, no key, no cookie, no crumb). So the fetch op must become an HTTPS call (a `curl` child, the shape `hostlist_op fetch` already uses), not a socket.
3. **Money must move to a ledger.** The old project keeps cash and holdings in mutable `state.txt` files as C `float`s. That is exactly the ghost-data pattern the house forbids (`02-architecture/NO-GHOST-DATA-...`) and floats are wrong for money. stock+x keeps ONE append-only ledger per user, money in integer cents, shares in integer micro-shares (which also gives fractional shares for free), and everything else is a projection.
4. **It is paper trading.** Game money, real quotes. A real-money broker is a regulated business (broker-dealer registration, custody, KYC, market-data licences); that is out of scope here and this document does not design it (section 9).
5. **Same framework as True Swords Clash:** an app package under `&.hq-apps/app-edit/projects/stock-x/`, a rules-and-projector op in C, screens with the tab bar, HQ menu, page dropdown and footer, harness cases that can fail.

## 1. What exists (READ)
### 1.1 The middle-house project `projects/yahoo/` (the thing to port)
| Piece | What it does | Port plan |
|---|---|---|
| `ops/fetch_stock.c` (219) | raw-socket HTTP GET, `http` only, 5 s timeout, 128 KB buffer, user agent `honpwc web_get 1.0` | **replace** with an HTTPS `curl` child (section 4.2) |
| `ops/json_parser.c` (117), `read_price.c` (39) | JSON to `.kvp`, read one price | **replace** with a tiny parser for the v8 chart shape (section 4.2); keep the idea |
| `ops/lookup_stock.c` (107) | fetch or cached price, `CACHE_DURATION 3600` s, writes `last_lookup_*` into the user state | keep the cache idea; the cache becomes a capped projection (4.3) |
| `ops/buy_stock.c` (188), `sell_stock.c` (180) | `<user_hash> <symbol> <shares float>`; rewrites a `stocks=SYM:qty,...` line with `strtok` | **rewrite** as ledger rows; same verbs and UX |
| `ops/add_credit.c` (97) | adds to `balance=` | becomes a `DEPOSIT` row with a rule (4.4) |
| `ops/portfolio_new.c` (77), `profit_loss.c` (120) | holdings summary, profit and loss | become projections of the ledger |
| `ops/options_pricing.c` (190) | Black-Scholes-style prices on a fixed grid: 5 expirations (1 hour, 1 day, 1 week, 1 month, 1 year) times 5 moves (+2/+4/+8/+16/+32 % calls, the negatives for puts) | **port as a pure function** and verify it against known Black-Scholes values before trusting it (6) |
| `ops/buy_option.c` (353), `sell_option.c` (356), `sell_option_inventory.c` (294) | option positions | rewrite on the ledger (phase 4) |
| `ops/predictions.c` (328) | linear regression, 1-day and 1-month forecast | port as a pure function; label it a toy, never advice |
| `manager/yahoo_manager.c` (172), `layouts/main.chtpm` | key-polling manager, terminal-era UI | replaced by the phone screens |
| `test_suite]a0.c` (279) | tests | **not run by me**; read it for cases to carry over |

User state today (READ, `pieces/user_CA33D1/state.txt`): `balance=1000.00`, `watchlist=` (empty in this account; `buy_option.c` reads it as a comma list of followed symbols, no op writes it yet), `stocks=`, `options=`, `history=`, `last_lookup_symbol=NVDA`, `last_lookup_price=216.61`, `last_lookup_time=2026-04-28T00:27:29`. So the starting balance is 1,000 and a watchlist and options fields were planned.

### 1.2 The other middle-house broker, `014.wsr-pal` (not the same thing)
WSR-PAL is a **stock-market game with fictional corporations** (`corp_<TICKER>` pieces with `price_history.txt` rows `tick,price,prev,change%`, AI corporations trading by tier, IPO, dividends, futures and derivatives ops, a player's `holdings.txt` as `ticker|shares`, negative shares = short). Its trades deliberately do not move prices (a flagged v1 simplification). It is a **second quote source** for stock+x, not the base: real tickers from Yahoo, fictional tickers from WSR-PAL (section 5.2).

### 1.3 Framework facts stock+x relies on (READ, built this session)
App package, `phone_op` verbs (`install`, `open`, `relaunch`, `quit`, `flip`, `home`), the tab bar with `HQ ▾` and `phone : <page> ▾`, the footer, `- cancel -` rows, `book` and pages as data, managed windows, the ledger-plus-replay pattern of `tsc_op` (rows verified by replay, an edited ledger refused with exit 3), the harness (`pal` + case `.pdl` + C ops; no Python), `hostlist_op fetch` (a `curl` child with a 10 s limit and a 64 KB cap, no cookies, a generic user agent, no identity sent). See `PLUS-PHONE-PLATFORM-DEV-GUIDE-2026-11-15.md`.

## 2. Goals and non-goals
**Goals (SAID):** real prices from Yahoo; buy and sell; later charts, watchlists, options, fractional shares; a Robinhood-like feel in our framework.
**Goals (DOC, from the same intent):** a quote screen, a buy/sell sheet, a portfolio with day change, an orders and history list, price alerts later; works for several users on one house; runs alongside a browser on a weak CPU (house rule: idle cost near zero, say what a feature costs).
**Non-goals:** real money, real brokerage, margin, shorting (phase 5 at the earliest), after-hours routing, real tax forms (a simulated tax is in section 13, which DOES need cost-basis lots), any claim that a price is current or tradable. Prices are what a free feed returned at a time; the app shows that time.

## 3. What is wrong in the old code for our house (the four fixes)
1. **Mutable state is the authority** (`state.txt`, `balance=`, `stocks=`): ghost data by the house rule. Fix: ledger plus projections (4.1).
2. **`float` money and shares** (`atof`, `%.2f`): rounding drifts and `0.1 + 0.2` problems. Fix: integer cents and integer micro-shares (4.1).
3. **`http://` only:** now a 301 (checked today). Fix: HTTPS via `curl` (4.2).
4. **Blocking menu loop and polling manager:** terminal-era. Fix: the phone screens and a projector op that sleeps when idle.
Also: `1.yfin..._orch]h13.c` (562 lines) is the original monolith; it is a source of behaviour to compare against, not code to carry.

## 4. Architecture (DOC)
### 4.1 The ledger is the only authority
One file per user: `stock-x/<user>/ledger.txt`, append-only, one row per fact, written only through verbs. Money in **cents** (`int64`), shares in **micro-shares** (1,000,000 = one share), prices in **micro-dollars** (so a $356.27 quote is 356,270,000). Rows (first draft; the exact columns are fixed when the replay op is written):
```
OPEN   | n | ts | user | cash_cents                 # the starting account (default 100000 = $1,000.00, the old project's balance)
DEPOSIT| n | ts | cents | reason                      # game credit; a verb with a daily cap, never a free field
BUY    | n | ts | SYM | micro_shares | price_micro | quote_ts | source | fee_cents
SELL   | n | ts | SYM | micro_shares | price_micro | quote_ts | source | fee_cents
WATCH  | n | ts | add|remove | SYM
ORDER  | n | ts | SYM | side | kind(limit|stop) | micro_shares | limit_micro | expires   # phase 3
FILL   | n | ts | order_n | price_micro | quote_ts
```
**The executed price and the quote time are IN the row**, so a replay never needs the network (the same idea as the seed in True Swords Clash's START row). A fill is refused if the quote is older than the freshness limit (default 300 s during market hours). Replay rebuilds cash, holdings and cost basis; an edited ledger is refused (exit 3). Projections (safe to delete, regenerated byte for byte): `portfolio.txt`, `pl.txt`, `watchlist.txt`, `orders.txt`, `ui.txt`. A `state.pdl` declares each file's kind (LEDGER, PROJECTION, CONFIG, PREFERENCE) and an `audit` verb fails on any undeclared file. Each ledger row ends with a short chained hash of the previous row, so a hand-edited middle row is detected even if the totals still add up (DOC; the house `hash_op` exists).

### 4.2 Quotes: one fetch op, one cache
- **`stockx_fetch_op`** (C): `quote SYM`, `chart SYM <range> <interval>`, `search TEXT` (search is unverified, section 7). It forks `curl` with an argument vector (no shell), `https://query2.finance.yahoo.com/v8/finance/chart/<SYM>?range=..&interval=..`, 10 s limit, 256 KB cap, **no cookies, no identity, a generic user agent**, validates the symbol (`[A-Z0-9.^=-]{1,12}`) before it goes into a URL, and refuses redirects to anything but https on the same host. It extracts `meta.symbol`, `currency`, `exchangeName`, `regularMarketPrice`, `chartPreviousClose` and (for charts) the timestamp and close arrays, and writes them as rows of a quote file, never into the ledger.
- **Charts come from the same call** (`range=1d|5d|1mo|6mo|1y|5y`, `interval=5m|1d|1wk`), so the phase-2 chart needs no new source.
- **Quote cache** (a capped projection, `quotes.txt`): `SYM | price_micro | quote_ts | fetched_ts | source`, at most 256 symbols, rewritten atomically. TTL: 60 s while the exchange is open, 3,600 s when closed (the old project used a flat 3,600 s). A request for a fresh-enough symbol makes **no network call**.
- **Budget and politeness (house rules):** the feed is unofficial and publishes no limit, so: at most one request in flight, a minimum gap between requests (default 1.5 s), a hard cap per hour (default 300, counted in a capped file), and on `429` back off for 10 minutes and say so on screen; **never loop to a 429** (the day's free calls were burned once before). Everything niced and idle-silent: no polling when the app is not open.

### 4.3 The app (same shape as True Swords Clash)
Package `&.hq-apps/app-edit/projects/stock-x/`: `manifest.pdl` (`APP | id | stock-x`, title `stock+x`, `SCREEN | home | screens/market.xhtpm`, `SCREEN | home-landscape | ...`), `ops/stockx_op.c` (rules, replay, projector module, UI verbs `ui_quote`, `ui_buy`, `ui_sell`, `ui_digit`, `ui_watch`, ...), `ops/stockx_fetch_op.c`, `screens/*.xhtpm` + css. Screens use the phone bar shape: a tab bar (`HQ ▾`, `phone : stock-x ▾`), a footer, dropdowns with `- cancel -`. **Pages** of the book (the dropdown and the tab): `markets` (search and quote), `portfolio`, `watchlist`, `orders`, later `options`. The book name and page names are data (the usage-framework rule), not hard-coded.
- **Quote sheet:** symbol, price with its time, day change from `chartPreviousClose`, `buy` and `sell` buttons.
- **Trade sheet** (phone-sized, digits like the TSC answer pad): shares or dollars, a review line (`BUY 2.5 AAPL @ 356.27 = $890.68`), confirm; the confirm appends the row.
- **Portfolio:** holdings from the replay, value at cached quotes, day change, cash.
- **A small number of buttons per screen** (the phone is 360 px wide; the TSC lesson: 2 to 6 choices fit).

### 4.4 Cash and the economy
Starting cash is an `OPEN` row. More cash comes only from a `DEPOSIT` verb with a rule (for example a daily top-up, or a payout from another house system later); there is no free "set balance". Fees are a config row (`FEE | cents`, default 0) so the simulation can add a spread later. Cones, wallets and the pet chain (section 31.3 of the phone design doc, `khtpm_phone.c`) are a later bridge: a `DEPOSIT` whose reason is a signed transfer from that wallet; nothing in this design depends on it.

### 4.5 Money, rounding, and the one rule
All arithmetic is integer. `cost_cents = round(micro_shares * price_micro / 1e10)` with a stated rounding rule (half away from zero), the same function used by the BUY check and the replay, tested by a case that replays 10,000 random trades and compares cash to an independent integer sum. A trade that would take cash below zero is refused with a message; selling more than held is refused (no shorting in v1).

## 5. Phases (each ends with a visible before/after and a case that can fail)
| Phase | Scope | Done when |
|---|---|---|
| **P0** | `stockx_fetch_op` (quote + chart) with the cache and budget; case `stockx_fetch` using a **local file server or a canned response file** (no live network in the case), plus ONE live check run by hand and photographed | a quote for a real ticker on a phone screen with its timestamp; a second request within the TTL makes no network call (counted) |
| **P1** | ledger + replay op, `OPEN`/`DEPOSIT`/`BUY`/`SELL`, portfolio and P&L projections, market and portfolio screens | buy 2.5 shares, sell 1, cash and holdings correct on screen; an edited ledger refused; projections regenerate identical |
| **P2** | watchlist, charts (line chart from the chart call), orders list | a watchlist of 5 symbols refreshed within the budget; a 1-month chart drawn |
| **P3** | limit and stop orders (`ORDER`/`FILL`), price alerts | an order fills only when a fresh quote crosses the limit; the fill row carries that quote |
| **P4** | options: port `options_pricing.c` (grid UI first), then `buy_option`/`sell_option` on the ledger, expiry and exercise as rows | option prices match reference values in a case; an expired option settles by rows |
| **P5** | dividends, short selling, a second quote source (WSR-PAL fictional tickers), leaderboards, the wallet bridge | each its own design entry first |
Fractional shares need no phase: micro-shares and the dollars-based trade sheet are in P1.

### 5.2 Quote sources are data (flexibility)
`SOURCE | yahoo | stockx_fetch_op ...` and `SOURCE | wsr | wsr_quote_op ...` rows in a config file; a symbol carries its source (`SYM@wsr`). The ledger rows record the source of each quote. Adding a source is a config row and an op, not a change to the app.

## 6. Tests (cases that can fail, no Python)
`stockx_rules` (replay, refusals, rounding, edited ledger refused, projections regenerate, audit finds a stray file), `stockx_fetch` (canned JSON files for: a normal quote, a missing price, a currency other than USD, a malformed body, a redirect to another host refused, a 429 backs off; plus a **negative**: an oversized body is refused), `stockx_options` (Black-Scholes reference values for a few inputs, put-call parity), `stockx_money` (10,000 random trades against an independent integer sum). Plus a live run on the phone with real taps and photographs (the owner-evidence rule), and the CPU line: idle cost and the cost of one refresh with a browser open.

## 7. Verified today, and not verified (be honest about the feed)
**Verified 2026-11-15 by me (one request each):** `http://` to `query2.finance.yahoo.com/v8/finance/chart/AAPL?range=5d&interval=1d` returned **301 to https**; the `https://` request returned **200**, 1,715 bytes, with `symbol` AAPL, `currency` USD, `exchangeName` NMS, `regularMarketPrice` 356.27. No cookie or key was sent.
**Not verified:** Yahoo's current terms of use (I did not read them; a web search found secondary sources that disagree on how strictly they limit non-personal and commercial use; read the terms before anything beyond a personal or demo project: `https://legal.yahoo.com/us/en/yahoo/terms/otos/index.html`); any rate limit (none is published; a search found only unofficial figures, so the budget above is a guess to be tuned); how long the endpoint stays open without a cookie (it is unofficial and can change or be shut off at any time); the search endpoint; whether quotes are delayed on a given exchange (they often are; the app must show the quote time and never say "live"); the old `test_suite` and every other old op (not run).
**Design consequence:** the fetch layer is a replaceable source (5.2); the app must degrade to the last cached quote with its age shown when the feed fails; a paid or keyed source (several exist, free tiers with limits) can be added as another `SOURCE` row without touching the ledger.

## 8. Cost and runtime files (house rules)
Idle: zero (no timers while the app is closed, none polling while open except the screen's own 5 s projector wake, the same as True Swords Clash). Active: one `curl` child per refresh, niced, budgeted. **Files and caps:** `ledger.txt` (append-only, small: one row per trade; a size cap with a rotate-to-archive verb that writes a signed opening balance), `quotes.txt` (256 rows), `fetch_budget.txt` (a few lines), `charts/<SYM>.txt` (one file per symbol, at most 400 points, at most 32 symbols kept, oldest dropped), `ui.txt`/`portfolio.txt`/`pl.txt` (projections, rewritten). Each gets a row in `03-pitfalls/RUNTIME-FILE-GROWTH-STANDARD.md` and a place in the guard's hygiene sweep when it is built.

## 9. Real money (out of scope, stated so nobody builds it by accident)
Executing real trades for users needs a licensed broker-dealer relationship (or an introducing-broker arrangement with an API provider), customer identification and anti-money-laundering checks, custody of funds and securities, best-execution and disclosure obligations, licensed market data and a lawyer. None of that is in this design; the app must say "paper trading, prices may be delayed, not advice" on its first screen. The ledger design (integer money, replay, no hand edits) is chosen so that a future real-money back end could sit behind the same verbs, but that is a business decision, not an engineering step.

## 10. Open questions for the owner
1. ~~Whose app is it?~~ **ANSWERED 2026-11-15 (SAID): stock+x is a TSOTS app for now** (black and red brand, the phone is the TSOTS interface). A WSR app follows with the same interface (section 14).
2. **Start money and refills:** $1,000 like the old project? A daily top-up, or none?
3. **Fees and spread:** zero for now?
4. **Which users:** one account per LyfeDesk user, per phone, or per phone and user? (Section 31.3 of the phone design doc is the identity question.)
5. **Quote freshness limit** for a fill (default 300 s) and the **hourly request cap** (default 300): acceptable?
6. **Charts:** a plain line chart first (drawn with the existing canvas or text blocks), or wait for a proper chart element?
7. **Do you want WSR-PAL fictional tickers in the same app** (as a second source), or kept separate?
8. **Yahoo terms:** do you accept a personal and demo use, with a keyed provider swapped in if this ever ships to the public?

## 11. Next steps if you say go
P0 first: the fetch op and its cases (canned responses), then ONE hand-run live fetch photographed on a phone screen, so the first thing you see is a real ticker with its time on a phone. Then P1.

## 12. External references (for other agents)
- Old project: `x0.parent-level-dev-env-04.04/1.TPMOS_c_+rmmp.0103.0001/projects/yahoo/` (`REPORT.md`, `summary/sum&suc_yahoo-a28.txt`, `ops/`, `ops/ops_manifest.txt`, `pieces/user_*/state.txt`).
- Second middle-house economy: `x0.parent-level-dev-env-04.04/yz.muchiverse/44.xyz.01.00/014.wsr-pal💸️📌️+2/` (`FEATURES-IMPLEMENTED-j17.md`, `SOCIETY-ECONOMY-ARCHITECTURE.txt`, `ops/player_trade.c`). Middle-house app anatomy: BOOK `20.middlehouse/HOW-TO-RELAY-DRIVE-MIDDLE-HOUSE.md`, `middle-house-conversion.txt`.
- Phone framework: `16.game/PLUS-PHONE-DESIGN-2026-11-15.md` (sections 27 to 32), `16.game/PLUS-PHONE-PLATFORM-DEV-GUIDE-2026-11-15.md`; the app to copy the shape from: `&.hq-apps/app-edit/projects/true-swords-clash/` (`ops/tsc_op.c`, `screens/`); the fetch shape: `&.hq-apps/plus-phone/ops/hostlist_op.c` (`fetch`).
- House rules: `02-architecture/NO-GHOST-DATA-THE-LEDGER-IS-THE-AUTHORITY-2026-11-07.md`, `02-architecture/DESIGN-CHOICE-TEST-AND-DECISION-LOG.md` (add a row before building), `03-pitfalls/RUNTIME-FILE-GROWTH-STANDARD.md`, `03-pitfalls/hq-cpu-safety.md`.
- Outside: Yahoo Finance terms `https://legal.yahoo.com/us/en/yahoo/terms/otos/index.html`; the v8 chart endpoint is unofficial and undocumented (secondary sources: `https://scrapfly.io/blog/guide-to-yahoo-finance-api`, `https://github.com/ranaroussi/yfinance` issues).

## 13. Fees, tribute, tax and rebates (owner 2026-11-15, SAID; TEMPIRE and TEMPEROR are NEW names, no earlier mention anywhere in the tree: grep of the whole repo)
**Owner:** "there will be an adjustable platform fee, a tax (tribute) tx fee for the chain, quarterly estimated tax payments, a year tax, and rebates from TEMPIRE." **TEMPIRE** = the name of the empire; **TEMPEROR** = the name of the emperor.

### 13.1 What the house already designed that this lands on (READ, `16.game/CHAIN-MASTER-BANK-FED-AND-CONTRACTS-DESIGN-2026-10-28.md`, DESIGN, nothing built)
The chain is the master bank ("Fed"): a **master account (treasury)** that receives a **master tax** and a **master tx fee on every purchase**, an **issuer** that creates units only by a rule in `policy.pdl`, **bonds**, and owner **contracts** (taxes, dividends, royalties, annuities). That document says the word "tribute" is not yet a defined term (OPEN). The WSR stack has a built `tax_loop.c` (225 lines in `WSR_PAL-PREFERED/ops`) and governments with `tax_rate_adj`. So the owner's list is not a new subsystem: it is **stock+x being the first customer of the master-bank design**, plus the empire naming.
**CORRECTED 2026-11-15 (SAID, later the same day):** **TEMPEROR is not a person or a signing role: it is an AI-run empire government COMPANY, and every other company and government pays it homage and TRIBUTE.** **TEMPIRE** = the empire (the world those companies and governments belong to). So the tribute is the tax-like payment that flows UP to TEMPEROR from every company and government (and, in stock+x, from each trader), and the **rebates** flow back DOWN from it. DOC consequence: TEMPEROR is an ordinary entity in the economy (a `corp_*`/`gov_*` piece with an entity body, an AI decision mode like the attrition-model entities) that owns the **treasury ledger** and decides its own rates and rebates by rule and by AI, within bounds written in `policy.pdl`; a human can still edit the bounds (owner contracts, per the 2026-10-28 chain doc), but the day-to-day rate and rebate decisions are TEMPEROR'S. It is the empire-level analogue of the WSR `gov_*` ops and `tax_loop.c`, one level above them. (The earlier text of this paragraph, "TEMPEROR = the signing role", was my guess and is withdrawn.)

### 13.2 Five money flows, each a ledger row (never a free field)
| Flow | When | Who sets the rate | Ledger rows (user ledger and treasury ledger, linked by the trade number) |
|---|---|---|---|
| **Platform fee** | on every BUY and SELL | **adjustable** by the platform operator (a signed `policy.pdl` row, with a history of changes so a replay uses the rate that applied then) | `FEE | n | trade_n | cents | rate_id` (replaces the old `fee_cents` column) |
| **Tribute (chain tx fee)** | on every settlement that touches the chain | TEMPIRE policy (TEMPEROR-signed) | `TRIBUTE | n | trade_n | cents | rate_id` |
| **Quarterly estimated tax** | each game quarter | TEMPIRE policy; computed from the user's REALIZED gain so far this year (an estimate, like real estimated payments) | `TAXEST | n | quarter | cents` and the matching treasury credit |
| **Year tax** | year end | TEMPIRE policy; true-up = tax on the year's realized gain minus estimates paid | `TAXYEAR | n | year | tax_cents | est_paid_cents | due_cents` |
| **Rebate** | when policy says (for example a loss year, a fee holiday, a loyalty rule) | TEMPIRE policy | `REBATE | n | reason | cents | rule_id` (money comes OUT of the treasury) |
All five are integer cents, appended through verbs, replayed; the treasury account is a second ledger written by the same verbs so **money never appears or vanishes** (the user ledger debit equals the treasury credit; a case checks the sum of both ledgers is constant).

### 13.3 What taxing needs that section 4 did not have
- **Realized gain** needs **cost basis**: lots per symbol. Default **FIFO** lots (a `LOT` projection from the BUY rows); average-cost as a policy option. Fractional shares work the same (micro-shares).
- **Two parallel calendars (SAID):** REAL time and GAME time run side by side and **both are taxed by TEMPIRE**: a real-time tax (real quarters and a real year, by the wall clock) and a game-time tax (the world clock of each sim). A trade carries both stamps (`ts_real`, `ts_game`); each calendar has its own estimated-payment and year-end rows (`TAXEST | real|game | period ...`). A world with no game clock (stock+x on Yahoo) only has the real calendar.
- **Estimates:** a simple rule first: quarterly estimate = rate x realized gain so far, minus estimates already paid; the year-end true-up can refund (a negative due becomes a REBATE-like row). Losses carry forward? OPEN.
- **Everything adjustable lives in `policy.pdl`** (rates, thresholds, dates), signed by TEMPEROR, read by replay with the policy version in force at each row's time; a user can never edit it.
- **Display:** the portfolio screen shows fees and tax paid and an "estimated tax due" line, so the cost of trading is visible (the Robinhood feel plus the empire).

### 13.4 Open questions for the owner (new)
1. Is the **tribute** (paid UP to TEMPEROR by every company, government and trader) the same payment as the master tx fee in the 2026-10-28 chain doc, or separate from it and from the platform fee? 2. ~~Who is TEMPEROR?~~ ANSWERED: an AI-run empire government company that all other companies and governments pay tribute to; OPEN: which attrition-model/AI decides its rates, and what bounds does `policy.pdl` put on it? 3. Is the chain-as-master-bank a department of TEMPEROR/TEMPIRE, or its own entity that TEMPEROR also pays? 4. ~~Game or real time?~~ ANSWERED: both, in parallel, both taxed; OPEN: are the two taxes independent or does one credit the other? 5. Which events earn a **rebate**? 6. Is tax on gains only, or also on dividends and income (WSR's `tax_loop.c` is the model to read)? 7. Is a user's tax paid from the trading cash or from a separate wallet?

## 14. The WSR app and a shared price service (owner 2026-11-15, SAID)
**Owner:** "we will do a WSR app too. Same interface, but it runs the WSR 'entity price simulation' (which should not be exclusive to the app; other apps should be able to use these prices too), and if we view the `wsr-sim-#` we can see the sim actually being played out by the 'entity bank' in 2D/3D."

### 14.1 What exists (READ)
- **The price engine and the economy ops are built** in `WSR_PAL-PREFERED/ops` (53 ops): `corp_update_price.c` (165 lines, the ported analysis formula, once per End Turn), `corp_trade`, `corp_ipo`, `bank_loan_op.c`, `day_loop.c`, `market_quote.c`, `market_settle.c`, `tax_loop.c` (225), `pop_update`, `realestate_*`, `gov_*`; pieces `corp_<TICKER>/state.txt` and `price_history.txt` (`tick,price,prev,change%`). A bank is a `corp_*` with `industry=bank`.
- **The design for watching it play is written, not built:** `08-roadmap/design-docs/DSR-SIMULATION-DESIGN.md` ("two mini civilizations you watch play": a world clock ledger with phases in a day, buildings as entities with a kind including **bank**, a population pool with a watchable sample, daily routines as data, WSR mechanics through entities, loans with daily interest, **same seed gives the same numbers** (its phase 1 test), a speed control to watch), `DSR-WSR-PETS-BRIDGE-DESIGN.md` (companies, population and pets on a **2D/3D map**; entity_uid bridges every identity; WSR `state.txt` is the authority for company finance and the entity's `variables.txt` is a read-only projection; **no `dsr` book exists on the pc-hq board yet**), `DSR-AS-PHYSICAL-WSR-PLAN.md`. Built today: the DSR toy window (`&.hq-apps/dsr`), the `dsr-rpg` and `dsr-rpg-board` apps, and in the `dsr-test` page 4 banks, 8 stores, 2 castles, 2 population entities (no game state on them yet, per the simulation doc).
- **Not found:** the name `wsr-sim-#`, an "entity bank" as a defined term (the nearest is the DSR bank entities = `corp_*` with `industry=bank` and the chain master bank), and any shared price service. WSR trades do not move prices (a flagged v1 simplification in `player_trade.c`).

### 14.2 The shared price service (DOC, proposal)
A **quote bus**: one folder per world (`prices/`), one append-only quote ledger per SOURCE (`yahoo`, `wsr-sim-1`, `wsr-sim-2`...), rows `Q | n | ts | SYM@source | price_micro | prev_micro | meta`; a projection `latest.txt` (`SYM@source | price | ts | change`) that ANY app reads and none writes. **Producers** write only their own source's ledger: `stockx_fetch_op` (Yahoo) and the WSR sim (its price op appends one `Q` row per corp per tick). **Consumers** (stock+x, the WSR app, a pet shop, the board's building signs, the chain exchange) read `latest.txt`. This is section 5.2's `SOURCE` rows made real and it keeps one authority per fact: the WSR piece stays the authority for company finance, the bus carries only prices.
- Same interface, two apps: **stock+x** (source `yahoo`) and the **WSR app** (source `wsr-sim-<n>`) are one package shape with a different `SOURCE` row, so the second app is mostly data (the usage-framework rule: not two products).

### 14.3 `wsr-sim-<n>` (DOC, proposal)
One simulation world = one numbered instance (like phone instances): `wsr-sim-<n>/` with its seed, its world-clock ledger, its corp and bank entities (WSR pieces with entity bodies per the bridge doc), its quote source, a speed setting. **Viewing it** = the DSR board (2D/3D) opened on that instance: a READ-ONLY projection of the sim ledgers (buildings, citizens walking, the bank lending), never a second authority. The **bank** acts through the same ops the player uses (`bank_loan_op`, deposits) so the viewer shows real ledger events. Determinism: same seed gives the same prices (a harness case, as the simulation doc already specifies). Idle cost: the sim runs only while an instance is open or asked to run at a stated speed (house CPU rule).
### 14.4 Open questions (new)
What does "entity bank" mean precisely (the 4 DSR bank entities, the chain master bank, or the empire's bank)? Should player trades move prices in `wsr-sim` (the old v1 said no)? Several sims at once (`wsr-sim-1`, `-2`) or one world with many banks? Do sim prices and Yahoo prices ever mix in one portfolio (a symbol is `SYM@source`, so yes by design)? Is the sim time the world clock the tax calendar of section 13 uses?

### 13.5 Owner answers, round 1 (2026-11-15, SAID): supersedes parts of 13.2 and 13.3
Three SEPARATE charges (platform fee, chain tx fee, tribute) plus tax; **tribute is paid to the government the business is registered in, governments pay TEMPEROR** (the AI above all, the main AI controlling NPCs through trunking, each NPC with its own personality); **illegal operations do not pay it** (sim or "IRL", OPEN); a trade is taxed on **the calendar of its world: real time for Yahoo-source trades (stock+x), game time for sim-source trades (the WSR app)**. In the ledger: `FEE`, `CHAINFEE`, `TRIBUTE | n | trade_n | cents | to_gov | rate_id` are three row types; a `REGISTER | n | entity | gov` row names the government; `TAXEST`/`TAXYEAR` carry which calendar (`real`|`game`) their world uses. For a plain stock+x trader on Yahoo prices the open question is who the tribute goes to (the phone user's home government? a default empire government?): see the plan section 7a. Full list of new questions: `WORLD-PROGRAM-PLAN-STOCKX-WSR-SIM-2026-11-15.md` section 7a.

## 15. Reading pass, 2026-11-15 (what it changed here)
- **Acceptance list for the port:** the old Yahoo `test_suite` has seven behaviours (lookup, add credit, buy, sell, buy options, sell option, portfolio); the stock+x harness case must reproduce all seven first, then add the new rows (fee, chain fee, tribute, treasury sum constant, replay, forged ledger refused).
- **Tax calendar needs no new code:** quarterly estimate = a `lc_clock` schedule `every:3month` and year tax = `every:1year` on the world's clock (`CLOCK-AS-THE-PLAY-SPINE-DESIGN.md`, built and tested); a real-time world uses a clock at wall rate. Sim trades use the sim clock, Yahoo trades the real clock (owner round 1).
- **WSR tax today** is a yearly flat percent of CASH per government (`tax_loop.c`); section 13's gains-based estimates are NEW, and the cash-based levy stays as the government layer beneath TEMPEROR's tribute.
- Plan section 10 holds the full list and what is still unread.
