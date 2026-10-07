# DSR as an entity-based, event-made game that shares WSR_PAL-PREFERED code: design

Status: DESIGN ONLY, written 2026-10-07 by claude. Nothing here is built.
Owner brief (2026-10-07): "do you see the dsr page? there are 'stores' which have 'stock' market value and will tie into wsr_preferred... no death yet. The entities will also get all RPG Maker stats, including strength which can go up, intellect, when learning more; skills are learned. In dsr we are going to make it so it's all made of events; we can start a game and it's like WSR preferred but using entities; we will actually share code when possible, see those?"

Related: `ENTITY-NEEDS-AND-CARE-DESIGN.md` (day tick per place, toggles), `ENTITY-SCHOOL-YEARS-DESIGN.md` (classes, skills learned), `ENTITY-WORD-BANK-DESIGN.md` (aliases, behavior + variables), `AUCTION-SCREEN-DESIGN.md`, `TEST-GAMES-ROADMAP.md` 6/6b, `WSR_PAL-PREFERED/SOCIETY-ECONOMY-ARCHITECTURE.txt`.

## 1. What I see (read, not assumed)
**The DSR page = the `dsr` desk**, 16 entities (pals) in the owner's user home: `dsr_castle_a/b` (2), `dsr_bank_a1/a2/b1/b2` (4), `dsr_store_a1..a4`, `b1..b4` (8), `dsr_population_a/b` (2), laid out on a grid (e.g. castle_a x=800 y=80, bank_a1 x=880 y=80, store_a1 x=1040 y=80, store_a2 x=800 y=240). Desk definition in `sessions/s1/desks/dsr.pdl`. Today these entities have **only a menu, history and position**: no stock, price, cash or any game state of their own.
**The DSR toy** (`&.hq-apps/dsr`, "Desk Street Raider", X11-HQ window) is display-first: one `state/dsr_state.pdl` (`turn`, `active_corp=corp_AFL`, `player_cash`, `corp_stock_price`, news ticker, `population`, `temperature`), the original 34-item action menu with **every action a stub** (`action="void"`). Per `projects/dsr/NOTES.md`: toy and desk are **two front doors onto one ledger**, neither special.
**WSR_PAL-PREFERED** (`yz.muchiverse/WSR_PAL-PREFERED/`, plus `014.wsr-pal`) has the real mechanics as ops over `corp_*`/`gov_*`/`pop_*` **pieces** (a piece is a folder with a `state.txt`): `corp_update_price` (re-prices a corp from fundamentals every day, appends `price_history.txt`), `corp_ipo`, `corp_buy_stake`, `corp_trade`, `corp_decide`, `corp_payroll`, `corp_attempt_merger`, `bank_loan_op`, `goods_quote/settle`, `market_quote/settle`, `pop_update`, `realestate_*`, `player_trade`, derivative/futures ops, and **`day_loop`** (the daily market tick, runs `corp_update_price` over every `corp_*` piece). `SOCIETY-ECONOMY-ARCHITECTURE.txt` section 6 designs the ledger/auction marketplace (the auction screen reuses it).

So: **yes, it makes sense.** A DSR store is exactly a WSR corp wearing an entity body, and WSR already has the market mechanics the stores need.

## 2. The mapping
| DSR entity (a pal on the dsr desk) | WSR piece / mechanic it reuses |
|---|---|
| `dsr_store_*` | `corp_*`: cash, stock price, shares, payroll, trade, IPO, price history |
| `dsr_bank_*` | bank pieces: `bank_loan_op` |
| `dsr_castle_*` | government / owner pieces: `gov_*` |
| `dsr_population_*` | `pop_*`: `pop_update`, retail demand |
| the human player / AI actors | `player_*`, `corp_decide` weighted/RL decision modes |
| a game day | `day_loop` (one tick = reprice every corp) |

## 3. Sharing code, by the house rule (never header + link)
- **Pure logic with 2+ consumers -> text-included `.c`** in `&.widgits/_shared-lib/`: the price formula (`fundamental_value` / `calculate_new_stock_price`), market quote math, loan/interest math, population update. Both WSR ops and DSR ops include the same file.
- **Stateful or process-shaped -> a separate op + fork/exec + file IPC:** `corp_trade`, `corp_ipo`, settle ops. They currently take a `<piece_id>` and read WSR project paths; make them take an **entity directory** (or accept either) so the same binary serves a WSR piece and a DSR entity. One op, two callers.
- **Not** copied: DSR must not fork WSR's logic. If an op must change to take a directory, change it once and keep WSR's tests (`harnesses/`) as the guard (convert its verify scripts to pal harnesses as already planned).

## 4. Everything made of events
Per NIGHT 20/36 and the owner: game mechanics are **event data**, not C. The day tick is a clock-ledger row per place (`ENTITY-NEEDS-AND-CARE-DESIGN` section 3); an **event page** on each store/bank/population entity subscribes to the tick and calls registered commands (working names: `corp_reprice`, `corp_trade`, `corp_ipo`, `bank_loan`, `pop_update`) that are the existing ops behind registry `TEMPLATE`s. Starting a game = a `game.pdl` (the per-game setup file from the play-mode work) naming the desk, the seed entities and their starting state, the clock and the toggles. The toy and the desk read the **same** state (each entity's own `state.txt`) instead of one `dsr_state.pdl`, so the toy becomes a view over entities.

## 5. Stocks and stores
Each store entity gets private state: `cash`, `shares_outstanding`, `stock_price`, `price_history` (append-only, same row shape as WSR's), owner. The **market** is derived by replaying those histories (one authoritative derivation, never a stored second copy). Stock trades are ledger rows; **the same ledger shape the auction screen uses**, so a share can later be put up on the auction screen. Real-value (cones) settlement stays behind the signing gate; **play money first**.

## 6. RPG Maker stats and skills
- **Stat keys:** RPG Maker MV actors/enemies have 8 base parameters (`mhp`, `mmp`, `atk`, `def`, `mat`, `mdf`, `agi`, `luk`) plus `level`/`exp`. "Strength" and "intellect" are not MV key names. **Proposal:** store the 8 MV keys as the real fields, and give `strength` (about `atk`) and `intellect` (about `mat`) as **bank aliases with weights** (ENTITY-WORD-BANK-DESIGN), so text and menus can say either. Confirm this mapping with the owner.
- **Stats go up when learning:** passing a class (school design) awards `exp`; level-ups raise the parameters by a growth curve in tunables (RPG Maker's class "param curve" idea); all as events (`gain_exp`, `level_up`), logged as append-only rows, derived and rebuildable.
- **Skills are learned:** a class lesson can **teach a skill** (RPG Maker "class learnings": skill X at level N). A skill is event data (a page/command set) in the entity's skill list; learning one is a `LEARN_SKILL` row written after the class exam passes. The skill's effect is an event, so games author skills without C.
- Existing pets already carry `hp`, `hp_max`, `mp`, `energy`, `stamina`; reuse those names where they mean the same.
- **No death yet:** `death_enabled` stays off (the needs design's default); HP floors, nothing dies.

## 7. Build order (each step: pal harness first; rehearse in beta; develop in alpha)
1. Pull the price formula and market math into a text-included `_shared-lib` file; make `corp_update_price` (and one trade op) accept an entity dir; keep WSR's results identical (harness: same input, same price).
2. Give the 8 DSR stores/banks/populations a `state.txt` (scratch copy in beta), a day-tick event page, and run N ticks headless; prices move, history appended.
3. Toy reads entity state instead of `dsr_state.pdl`; actions wired to registered commands one at a time (replace stubs).
4. Stats/exp/level-up/skill events; school class awards exp and teaches a skill.
5. Start-a-game: `game.pdl` with seed entities, clock and toggles; save/load by game-data only (the earlier save-slots design).
6. Share more: bank loans, IPO, merger ops.

## 8. Open questions for the owner
1. Confirm the stat mapping (8 MV keys, `strength`~`atk`, `intellect`~`mat` as aliases)?
2. Do DSR entities and WSR pieces keep **separate** state folders (shared ops, separate data) or become one data format immediately? (recommended: shared ops first, one format later)
3. Is the DSR day the **world clock** tick (all stores on the desk follow it), and does a school-enrolled entity ever trade?
4. Which WSR mechanics first: stock repricing + trade (recommended) or banks/loans?
5. Should a game start from a hand-made `game.pdl` per scenario (castle A vs B rivalry, as the dsr desk is laid out) or a generator?
