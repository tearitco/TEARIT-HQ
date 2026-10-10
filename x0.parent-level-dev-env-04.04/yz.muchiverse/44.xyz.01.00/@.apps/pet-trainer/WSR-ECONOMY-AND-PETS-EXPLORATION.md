# WSR economy + AI: what runs today, and how pets buy stocks and bonds (exploration, 2026-10-10)

Status: EXPLORATION. Everything under "What I ran" was run by claude in a scratch copy (nothing in the real WSR trees was changed). Everything under "Pets" is design, not built. Companion to `CHAIN-ECONOMY-DESIGN.md` (section 5 and 10), `WSR_PAL-PREFERED/SOCIETY-ECONOMY-ARCHITECTURE.txt` and `XOD-ROADMAP.md`.

## 1. Two trees, and neither is complete alone
- `44.xyz.01.00/014.wsr-pal...` (**original**): has the entity DATA (`projects/wsr-pal/pieces_template`: **50 corporations `corp_*`, 7 governments `gov_*`, `player_you`, pop, weather, 2 realestate**) and the core ops, but **no order-book market** (`market_quote`/`market_settle`), no shareholder registry.
- `WSR_PAL-PREFERED/` (**preferred working tree**): has the newer economy engine (`market_quote`, `market_settle`, `shareholder_registry`, goods market, `corp_apply_finances`, `econ_calendar`, `day_loop`, XOD) but **its `pieces/` holds only the menus**; its `ensure_entities.sh` expects a source folder (`Mar$.$treetRace.wsr]Q]k32/`) that is not there, so it creates 0 entities.
- To run the engine you need both: the original's `pieces/corp_*`, `gov_*`, `player_you` copied into the preferred tree's `projects/wsr-pal/pieces/`. **Open item: fold the entity data into the preferred tree** (a one-time copy + a `pieces_template`), then it is self-contained.

## 2. What I ran (scratch copy under the job tmp dir)
Recipe (all with `PRISC_PROJECT_ROOT=<scratch root>`):
```
cp -a WSR_PAL-PREFERED  scratch/;  cp -a 014.wsr-pal.../projects/wsr-pal/pieces/{corp_*,gov_*,player_you} scratch/projects/wsr-pal/pieces/
bash scripts/tick_all.sh 3                       # ticks every corp/gov piece: idle -> deciding -> trading   (3 rounds = 1.7 s)
ops/+x/player_trade.+x corp_AFL buy 2            # the player buys: "Bought 2 shares of AFL at $98.71"
ops/+x/shareholder_registry.+x list              # who holds what (holder-kind-agnostic)
ops/+x/market_quote.+x                           # analysts post orders into data/book_<TICKER>.txt
ops/+x/market_settle.+x                          # matches the book; ONLY op that moves cash/holdings; writes market_ledger.txt
```
Results: 3 tick rounds moved prices (AFL 115.04 -> 98.71); the player bought shares; a quote round had **59 participants, 31 orders**; settle made **17 fills at 109.13** and wrote `Time | Debit | Credit | Amount | Event | Ticker | Price | Shares` rows to `data/market_ledger.txt` (append-only, good).
Not run: the interactive terminal game (`button.sh run`), XOD training/tournaments, goods market, derivatives, real estate, news.

## 3. The AI tiers (read in `corp_decide.c`, `gov_decide.c`)
Every corp/gov piece is a 3-state FSM (`current_state` 0 idle, 1 deciding, 2 trading) with `decision_mode`:
| mode | name | what it does |
|---|---|---|
| 0 | preset | fixed |
| 1 | **weighted (real)** | fundamental value from book value per share x market-cap multiplier x leverage x risk bias (`risk_bias` weights), blended 70/30 with momentum; buy if fundamental > price, sell if meaningfully below, else hold. Governments: deficit ratio + debt/GDP -> raise tax / cut spending / hold |
| 2 | rl | **stub, falls back to weighted** (this is where pets/XOD learning would plug in) |
| 3 | llm | one-shot Gemma call over the LAN (`connect_op`); default endpoint was pinned to one machine (header says fixed) |
| 4 | human | parks in `deciding` until a human queues a choice |
`market_quote` forms bids/asks from `fair = book_value/shares`, momentum, a spread that widens when the book is thin, clamped to a valuation band; banks quote too and can lend (`bank_loan_op`). Discovery is a directory scan, so **any new piece dir with a state file becomes a participant** without editing a roster.

## 4. What exists for bonds, loans, dividends
- **Stocks:** yes, real (order book, fills, holdings per holder, pro-rata dividends via `shareholder_registry`; holders may be player, corp or "other" by id prefix).
- **Loans:** `bank_loan_op request|repay` (bank needs 2x reserve in cash, flat, observable rules).
- **Bonds:** `corp_action issue_bonds|buyback_bonds` only changes the **issuer's** `bonds_outstanding` and cash, and `corp_apply_finances` charges a flat `bond_rate` interest as a cost. **There is no bondholder: nobody owns a bond, so the interest is paid to no one.** Pets buying bonds needs a **bond ledger** (new).
- Governments: tax rate, spending, GDP, debt/GDP (no sovereign bonds either).

## 5. Gaps found (to log in bug_bounty / fix)
1. **`market_settle` cannot rebuild the shareholder registry**: it runs `./+x/shareholder_registry.+x` relative to the current directory ("not found"; warning "dividends will use a stale index"). Fix: resolve the op path from its own location (`ops/+x/`).
2. **`player_trade` is hard-wired to `pieces/player_you/`** (holdings, state, transactions). A pet cannot trade through it; it needs a holder argument.
3. **No bondholders / no bond market** (above).
4. **Mode 2 (rl) is a stub**; XOD's evolution/GOAP is separate and drives the terminal UI via keys, not the ops.
5. **Tree split** (section 1) and a stale compiled `ops/+x` (Oct 5): rebuild before relying on it.
6. Units: WSR cash is "Dollars" (floats in `state.txt`); the chain is integer millicones. Needs one explicit bridge.

## 6. How pets plug in (design)
**A pet is a WSR holder piece.** For each pet create `pieces/pet_<id>/` with `state.txt` (`cash`, `owned_by`, ...) and `holdings.txt` (`<ticker>|<shares>`, the same file format as `player_you`). The registry already classifies by id prefix, so the pet is a holder; dividends reach its `cash`.

**Money bridge (never mints or loses value):**
```
pet's mined cones (pet-cones chain)
   --> EXCHANGE (averaged rate into preferred value, fee in preferred)  [CHAIN-ECONOMY-DESIGN section 11]
   --> WSR dollars credited to pieces/pet_<id>/state.txt cash   (recorded as a ledger row, capped per pet)
   ... pet trades in WSR (stocks, loans, bonds) ...
   --> on sell / maturity / "cash out": WSR dollars -> exchange -> pet-cones, ledger row
```
The WSR side runs on the **leased preferred-cone** pool (managed accounts, swept back at game end, `CHAIN-ECONOMY-DESIGN` section 10), so the real chain balance of the pool and the sum of WSR cash must reconcile: a reconciliation harness checks `sum(WSR cash) == leased - fees` every tick.

**Verbs a pet gets** (new ops, generalised from the player ones): `pet_trade <pet> <ticker> <buy|sell> <shares>` (player_trade with a holder), `pet_quote` (posts the pet's orders into the same `book_<T>.txt`, same row format), `pet_loan request|repay <bank>`, `pet_bond buy|sell`.

**Bonds (new, minimal):** `bonds.txt` ledger: `BOND | id | issuer | face | rate | maturity_day | holder | ts`. `issue_bonds` creates rows offered at par; a holder (pet, corp, bank) buys by paying cash to the issuer (market-style fill); `corp_apply_finances` pays each coupon to the **holder** (not into the void) and repays face at maturity (or default row if cash short). Bank-issued bonds are the safest first product for pets (banks have reserves).

**How a pet decides** (rails from CHAIN-ECONOMY-DESIGN section 12): start with the deterministic `weighted` fundamental-value rule (mode 1) scaled by the pet's own `risk_bias` and needs (hunger/rent are attrition: sell to eat). Then **mode 2 becomes real**: the pet's RL policy proposes orders; the harness referee scores outcomes; paper money (`play` unit) before real cones; per-pet loss cap and kill switch; the pet drives it through the **relay** in the Exchange/Auction windows.

**Tournaments (XOD):** pets as XOD agents: isolated session per pet, fitness = preferred value of (cash + holdings + bonds) per day, GOAP over a behavior bank (`buy_dip`, `hold`, `take_loan`, `sell_to_eat`), winner declared per generation. Needs XOD pointed at the pet holder pieces instead of `corp_ORB` and run through the ops, not the terminal UI. (XOD itself was not re-run by me.)

## 7. Build order
| # | Step | Proof |
|---|---|---|
| 0 | fold entity data into the preferred tree; rebuild its ops; fix market_settle path; scratch harness `wsr_scratch` (copy, tick, quote, settle, assert prices moved + ledger rows) | pal harness |
| 1 | `pet_<id>` holder pieces + `pet_trade`/`pet_quote` (holder arg) + reconciliation harness | harness: pet buys, registry lists it, dividend reaches its cash |
| 2 | money bridge with a fixed rate (no exchange yet): fund from a capped lease, cash-out, reconcile | `sum(cash) == lease - fees` |
| 3 | bond ledger + coupons to holders + maturity/default; banks issue, pets buy | harness: coupon paid to holder, maturity repays |
| 4 | pet AI mode 2: deterministic weighted first, then RL on paper money with harness verdicts | harness + paper-trading tournament |
| 5 | exchange rate averaging replaces the fixed rate; Exchange HQ shows it | PNG + trades feed |
| 6 | pets drive the relay in the windows; XOD tournaments over pet pieces | tournament dashboard PNG |

## 8. Questions for the owner
1. Fold the entity data into `WSR_PAL-PREFERED` now (recommended)?
2. First pet product: **bank bonds** (safest, needs the bond ledger) or **stocks** (works today after `pet_trade`)? Recommended: stocks first (days), bonds second.
3. Cap on how much WSR cash one pet can hold at start (suggest 100 coins' worth, set in data)?
4. Should pets' WSR trades be visible as their own feed in the Exchange window (yes recommended: "who traded")?
