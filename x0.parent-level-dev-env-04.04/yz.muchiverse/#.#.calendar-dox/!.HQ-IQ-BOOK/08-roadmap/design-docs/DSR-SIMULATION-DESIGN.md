# DSR simulation: two mini civilizations you watch play (design)

Status: DESIGN ONLY, written 2026-10-07 by claude. Nothing here is built. Target page: `dsr-test` (entities `dsrtest_*`); the original `dsr` page is left alone.
Owner brief (2026-10-07): "how do we structure dsr so that it 'plays' a game, where we watch these 2 mini civilizations do loans, wars, schools, rent in the hotels, stores where they work and do marketing, R&D, get stock options from, people shop at, etc. The entities will come out of the hotel and go to work or school every day. Also more 'stores', maybe maid, when pop increases."

Builds on: `DSR-ENTITY-GAME-DESIGN.md`, `ENTITY-NEEDS-AND-CARE-DESIGN.md`, `ENTITY-SCHOOL-YEARS-DESIGN.md`, `WSR_PAL-PREFERED/SOCIETY-ECONOMY-ARCHITECTURE.txt`, `WSR_PAL-PREFERED/docs/LONG-RANGE-SHAPE.md`.

## 1. What already exists on paper and in code (read, not assumed)
- **WSR already designs most of this world** (`SOCIETY-ECONOMY-ARCHITECTURE.txt`): population pools that are **aggregate by default and promoted to an individual `citizen_<id>` only when notable** (section 2); a **labor market** where companies post job listings on the ledger and citizens qualify by curricula passed (section 3); schools as teacher curricula (3); R&D tiering with staffing from research-tier citizens (9); a ledger/auction marketplace and **physical locations** with xyz (6, 7); housing owned by a structure with citizens paying rent (2). Its own caveat: this population/labor/school layer has **zero code precedent in WSR**; it is designed, not built.
- **Built WSR mechanics** (ops over pieces): stock repricing, IPO, buy stake, trade, payroll, merger, bank loans, goods/market quotes and settle, population update, real estate (`realestate_*`), tax loop, shareholder registry, news, derivatives/futures, and the daily `day_loop`.
- **War is the largest unbuilt domain** (`LONG-RANGE-SHAPE.md` 2): "war must cost something real, or it is theatre"; no `at_war` flag; governments exist as `gov_*` pieces (7 governments); territory needs a refactor before war is built on it.
- **Nothing exists** for hotels (0 hits in WSR code/docs), maid-type service stores, or staff stock options as compensation; marketing appears in a few WSR files but I did not check what it does.
- **DSR today** (`dsr-test`, 16 copied entities): 2 castles, 4 banks, 8 stores, 2 population entities; **no game state on any of them**, no hotels, no schools, no citizens. Moving an entity between places already exists (`move_entity_init/tick` + a pal; the entity polls a position marker).

## 2. The structure (layers; each is data + events, no new hard-coded mechanics)
**L0 Clock.** One **world clock per game**: an append-only ledger of ticks, with **phases inside a day** (proposed: morning, work, evening, night = 4 ticks/day). Everything subscribes by cursor (the needs design's per-place clock). A fixed **game seed** makes runs reproducible (harness: run N days headless, compare key numbers).

**L1 Places (buildings).** A building is an entity with `kind` (hotel, store, school, bank, castle, townhall ...), a position, a `capacity`, an owner, and its own `state.txt` (cash, shares, price history, rent, tuition, stock of goods). Stores/banks/castles already exist; **hotels and schools are new kinds**, and a **maid/cleaning service** is just another store kind (it sells the `clean` action from the needs design).

**L2 People: pool plus a watchable sample.** Follow WSR section 2: each civilization has a **population pool** (aggregate: count, average wage, unemployment, demand per commodity) that carries the economics cheaply. A small number of **named citizens** (proposed 4-8 per civilization to start) are promoted to individual entities so you can **see** them. Cost warning: in this house an entity is a process and a window; hundreds are not affordable on this machine. So: citizens exist as **data rows** in the world state and are **drawn as sprites on the board view** (the same viewer the pc-hq board uses), and only a selected citizen opens as a real entity window. (Decision for the owner, section 7.)

**L3 Daily routine = a data table, not code.** A citizen has a `home` (a hotel room, rent paid per day), a `job` (store/school/castle) or `student` status, and a routine of rows per phase, e.g. morning: leave hotel -> go to workplace/school; work: work (store pays wage, school marks attendance); evening: eat/shop at a store (store earns revenue), maybe go to a bank; night: return to the hotel and sleep. Each row is an **event** (`go_to <place>`, `work`, `attend`, `buy <good>`, `sleep`) using the existing Move for travel. Routines are weighted choices among valid options (tunable weights; a model never decides).

**L4 Economy = WSR's mechanics through entities** (`DSR-ENTITY-GAME-DESIGN.md` 3/4): daily repricing, trade, IPO, payroll, loans (a bank lends to a store or citizen; interest due daily), rent (citizen -> hotel owner), shopping (citizen -> store revenue), **marketing** and **R&D as spending categories of a store's budget**: marketing shifts the store's share of local retail demand; R&D (rare, tiered, per WSR section 9) raises its quality/tech tier. **Stock options** = a ledger row granting an employee the right to buy shares at a strike price after N days; vesting and exercise are derived from the ledger.

**L5 Government and war.** A castle is a government: it taxes (existing tax loop), holds a treasury, and can fund an **army**. War is an event chain with **real cost** (treasury spent, units lost) and a **real prize** (a parcel/store/hotel changes owner, so its rent/revenue changes). No `at_war` flag; treaties move real resources. Do this last; it needs the territory/ownership refactor LONG-RANGE-SHAPE warns about. v1 stand-in: a "raid" event that takes a fixed amount of cash between treasuries.

**L6 Schools.** A school is a building that citizens-students attend daily (an `ATTEND` row), with classes and exams as in the school design; passing a grade gives an **expertise tag** that job listings require (WSR section 3), so schooling changes wages. **Decision for the owner:** the school design says "in school = in the school's inventory (moved folder)", which is wrong for a daily commute; proposal here: **daily attendance is a location + ledger row** (the student walks in and out), while **enrollment with accelerated ageing** stays the inventory model for long-term residents.

**L7 Growth and new stores.** Population changes by births/migration (a pool rule driven by wages, housing and food). A **demand rule** founds a new store when a service's demand exceeds the capacity of its providers for N days (`pop x per-capita need` vs provider capacity, the same supply/demand shape WSR already uses): an event spawns a new building from a **template entity** (like `desk_copy_op` spawns copies) with a founder/owner and starting capital. So more people -> more hotel rooms needed -> a new hotel; more dirt -> a maid service store; more students -> another school.

## 3. Watching it play
You do not drive it; you observe: a **speed control** (days per second, pause), the board view with citizen sprites walking between buildings, the existing DSR **toy window** as the dashboard (turn, wallet, stock price, news ticker, world status) fed from the real state instead of the stub file, and an **append-only world ledger** window (who did what each day) with the news headlines (WSR's news op: biggest movers). Two civilizations A and B compete in the same market and can trade or fight.

## 4. Why this fits the house rules
Mechanics are **events and registered commands**; state is **append-only ledgers read by cursor**; the clock is **per place**; stateful ops are **separate ops** (shared with WSR by taking an entity dir); pure formulas are **text-included**; every step gets a **pal harness** (deterministic seeded runs) before it counts; development happens in **alpha**, rehearsal and data in **beta**, and **dsr-test** is the only DSR page we change.

## 5. Build order (small, each step visible and testable)
1. World clock + phases + seed; pool state on the 2 population entities; store cash/price/wages; run 30 days headless, harness checks (prices move, histories append, same seed = same numbers).
2. Hotel entity kind + 3 citizens per civilization as board sprites; routine table; the walk hotel -> store/school -> hotel; rent and wage rows.
3. Shopping revenue, a bank loan with daily interest, stock price from fundamentals; stock options ledger.
4. School building: attendance rows, an exam as a harness case, expertise tag changes a wage.
5. Growth rule: a demand shortage founds a maid-service store from a template.
6. Marketing and R&D as budget lines with visible effects.
7. Observer UI: speed control, world ledger window, dashboard on real state.
8. War, last (cost model + territory), after the ownership refactor.

## 6. Reuse (do not rebuild)
`day_loop`, `corp_update_price`, `corp_trade`, `corp_ipo`, `bank_loan_op`, `realestate_*`, `tax_loop`, `shareholder_registry`, `wsr_news_op`, `pop_update`, `move_entity_*`, `desk_copy_op` (template spawning), `phone_ensure_op` (identity), the harness framework, the needs/school/word-bank designs.

## 7. Open questions for the owner
1. **Citizens as data + board sprites** (recommended, cheap) or as real entity windows (a handful only)?
2. **Daily attendance as location + ledger row** for schools (recommended), with the inventory model only for long-term enrollment?
3. How many citizens per civilization to start (3-8)? And fixed names or generated?
4. **Hotels:** one per civilization at first? Rent paid daily to whom (the castle, a private owner, the hotel's own cash)?
5. Is a **fixed game seed** per scenario (reproducible) what you want, or free randomness?
6. War v1: the stand-in "raid" (treasury transfer) acceptable until the ownership refactor?
7. Do the two civilizations share one market and one bank system, or each have its own?

## 8. The start menu: New Game setup (owner: "ideally we can access that menu on start of this game")
I read "that menu" as the setup choices raised as open questions in section 7. They become **options on a New Game screen** shown when the game starts, so the owner decides per game (and I stop guessing).

**What the house already has for this:** the per-game setup file `game.pdl` (`GAME-SETUP-PDL-DESIGN.md`) with `GAME` / `MAP` / `CELL` / `ROW` / `EDIT` rows, where a game can add its **own taskbar cells and dropdown rows** ("game title, new menus"); the play-start and reset behavior from the play-modes design (a starting-position entity; in-game variables restart); and the DSR toy's own menu bar (File, Game Options, Settings, Help) with the parallel-track **New / Load / Save / Save As** menu class (`2026-09-18/2do.md`, `projects/dsr/NOTES.md`).

**Design:**
- **Entry points:** *File -> New Game* in the DSR toy, and a game cell from `game.pdl` (`CELL | dsr-new | New Game | ... | cmd=...`) so it is also reachable from the desk/taskbar. Starting the game (play mode on) with no setup yet opens the screen first.
- **The screen** is an X11-HQ panel (a manager `<module>` + `.xhtpm`, no renderer C), **bounded choices only** (steppers, toggles, pick lists; no free typing except a name), every option with a **default** so `Start` works immediately. Presets at the top: **Quick demo** (2 citizens, fast), **Standard**, **Custom**; a custom setup can be saved as a named preset file.
- **Options (from sections 2 and 7), each defaulted:** citizens per civilization (3-8, default 4) and their representation; game seed (fixed number or random); hotels per civilization; shared market vs separate markets; schools: daily-attendance model; growth rule on/off; **war on/off (default off until built)**; needs toggles (hunger, hygiene, sleep, **death off**); day speed. Options a running game must not change (seed, civilization count) are locked after Start and shown read-only under *Game Options*; speed, pause and toggles stay adjustable.
- **Where it is stored:** new `game.pdl` rows `SETUP | <key> | <value>` (proposed row type; legacy-safe: no `SETUP` rows = the defaults, so every existing game is unchanged). The setup, the seed and the clock start are the first rows of that game's world ledger, so a run is **reproducible** and a saved game restores its own setup.
- **Start does:** validate -> write the `SETUP` rows -> create/seed the dsr-test entities from templates for the chosen counts -> reset the clock and all in-game state (the play-start reset) -> open the observer view (section 3).
- **Reuse:** the New/Load/Save menu class and `game_slot_op` (save/load slots) for Load/Save; `desk_copy_op` and template entities for seeding; the harness framework for tests (defaults produce the same numbers as no `SETUP` rows; each option changes exactly what it names; locked options refuse changes after Start).

**Build order addition (before step 1 of section 5):** define the `SETUP` rows and their defaults in the game setup parser (extend `khtpm_game_setup.c`, with pal harness cases), then the screen, then wire Start to the seeding. Open: is the New Game screen reached from the toy menu only, from a taskbar cell too, or both (recommended: both)?
