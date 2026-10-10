# DSR city: where companies, population and pets live together, and how their data bridges (design, 2026-10-10)

Status: DESIGN. Nothing here is built. Written by claude from the owner's words and from reading the real files (read-only; the owner's live DSR pals were only listed, never written).
Builds on `DSR-ENTITY-GAME-DESIGN.md` (DSR store = a WSR corp wearing an entity body), `DSR-AS-PHYSICAL-WSR-PLAN.md`, `DSR-SIMULATION-DESIGN.md`, `CLOCK-AS-THE-PLAY-SPINE-DESIGN.md`, and in the pet app `WSR-ECONOMY-AND-PETS-EXPLORATION.md`, `CHAIN-ECONOMY-DESIGN.md`, `LEVEL-BUILDER-AND-INT-CAMERA-DESIGN.md`.

## 1. Owner asks (2026-10-10)
- "for wsr, in x11-hq we want to use the dsr infrastructure. we will use the toy's DSR menu and the DSR book with buildings (castles, stores, hotel etc.) to have companies and population on a 2d/3d map."
- "this is where pets and companies will live and coexist, pls pay attention to how to bridge its data."

## 2. What exists: four homes, four formats (read, not assumed)
| Home | Where | Format | Holds today |
|---|---|---|---|
| **DSR toy** (X11-HQ window "Desk Street Raider") | `&.hq-apps/dsr/` (`dsr.xhtpm`, `ops/dsr_manager.c`, `state/dsr_state.pdl`) | `STATE | key | value` rows, ONE `active_corp` (e.g. `corp_AFL`) | the original 34-item WSR action menu + wallet / balance sheet / news / world status, display-first |
| **DSR entities** ("the dsr page", a desk of pals) | the owner's user home `.../livedesk/pals/dsr_<kind>_<id>/` (16: castle a/b, bank a1..b2, store a1..b4, population a/b); rehearsal copy `dsrtest_*` | folder: `entity_uid.txt`, `glyph.txt`, `meta.pdl` (`METHOD | label | command` rows), `variables.txt` (**empty**), `pieces/`, `history.txt` | a **body only**: no economic state at all |
| **WSR pieces** | `014.wsr-pal.../projects/wsr-pal/pieces_template` (data) and `WSR_PAL-PREFERED` (engine) | folder `corp_X/state.txt` (`key=value`), `holdings.txt` (`<ticker>|<shares>`), `price_history.txt`; 50 corporations, 7 governments, `player_you`, pop, weather, 2 realestate | all economic truth: cash, stock price, shares, bonds_outstanding, industry, decision_mode, owned_by, market ledger |
| **Pets** | `@.apps/pet-trainer/state/pets/<id>/` | folder: `variables.txt` (`key=value`), `entity_uid.txt`, inventory dirs, `miners.txt`, chat/ledger | needs, stats, coins (items), rigs, wallet secret |
| **Chain** | `041.pal-chain` (old, preferred) and `state/chain/chains/pet-cones` | `wallets/<id>`, `data/blockchain.txt` (`TX|from|to|mc|ts|id`) | cones |

**Three facts that make the bridge cheap:**
1. **Every home is plain `key=value` files in a folder**, so a bridge is a field map, not a translation layer.
2. **Every living thing already has an `entity_uid`** (64 hex) and the chain derives its wallet from it (`e` + first 24 hex). That is the identity key everywhere.
3. **DSR entities are bodies with no mind**: nothing exists on the DSR side that could disagree with WSR. We are filling an empty `variables.txt`, not reconciling two truths.
- Also found: **there is no `dsr` book on the pc-hq board yet** (`@.apps/piececraft-hq/pieces/system/maps/` has chess, doom, tsots, ... but no `dsr`). The "DSR page" is a desk of free-floating pals. The 2D/3D map the owner wants is a new book.

## 3. The bridge rule: one authority per fact, everything else is a projection
The house rule is one authoritative derivation, never a stored second copy. So every field gets exactly ONE home, and other homes get a **read-only projection** written by a bridge op, refreshed on a change marker (append-only marker file growth, never mtime).
| Fact | Authority | Projected to |
|---|---|---|
| a company's cash, price, shares, bonds, owner, decision mode | **WSR piece `state.txt`** (only WSR ops write it) | the building entity's `variables.txt` (read-only mirror, header `# projection of <piece_id>`); the DSR toy window |
| who holds which shares (pets, corps, player) | **`holdings.txt` per holder** + the registry | pet's `variables.txt` summary line; Exchange/WSR windows |
| where a building/pet stands, what a tile is | **the book desk** (`maps/dsr/<desk>`: glyph grid + events) | WSR/pet windows (position) |
| pet needs, stats, rigs | **pet folder** | map sprite state (hunger icon) |
| cones | **the chain** | pet/company `variables.txt` (`cones=` cached 60 s), Exchange |
| a company's *cash in WSR dollars* vs real cones | **reconciled**, not duplicated: `sum(WSR cash of leased entities) == leased pool - fees` (see 6) | harness invariant |
Never write a projection back. A player or pet *action* on a building (buy shares, set price, take a loan) is an **event on the entity** that calls the WSR op, whose result then flows out through the projection.

## 4. The link registry (the actual bridge data)
One append-only file `bridge/links.pdl` (rows, never edited, a later row for a key wins; `UNLINK` rows end a link):
```
LINK | entity_uid | piece_id   | kind      | desk | x  | y  | note
LINK | 0a31...    | corp_AFL   | store     | city | 12 | 7  | Aflac as a store
LINK | 9f20...    | gov_Solar_Empire | castle | city | 4 | 3 |
LINK | 66b1...    | pet_66b1c2d3 | house   | city | 20 | 9 | pet Rin's holder piece, wallet e66b1...
```
- `kind` -> WSR family: **castle = `gov_*`**, **bank = `corp_*` with `industry=bank`**, **store/factory = `corp_*`**, **hotel = `realestate_*`**, **population block = `pop_*`**, **pet house = `pet_<uid8>` holder piece**, **exchange / auction = service buildings** (open the Exchange/Auction HQ windows, no WSR piece).
- The same `entity_uid` is the chain wallet identity, so company, pet and building are one key.
- Generation: `bridge_link` op reads a manifest (`city.pdl`: ticker -> building kind/position) and the existing pieces; idempotent (re-run adds only missing links).

## 5. Field map (WSR `state.txt` -> entity `variables.txt`)
Names follow the existing pet/RPG Maker keys where the meaning is the same (`hp`, `mp`, `level`).
| WSR key (authority) | Entity variable (projection) | Unit / note |
|---|---|---|
| `cash` | `wsr_cash` | WSR dollars (float, 2 dp) |
| `stock_price` | `wsr_price` | dollars |
| `shares_outstanding` | `wsr_shares` | millions (WSR unit; unit mismatch with registry counts is a known quirk) |
| `market_cap`, `book_value`, `debt_to_equity` | `wsr_mcap`, `wsr_book`, `wsr_dte` | |
| `bonds_outstanding`, `bond_rate` | `wsr_bonds`, `wsr_bond_rate` | bondholders do not exist yet (design 7) |
| `industry`, `owned_by`, `decision_mode`, `last_action` | `wsr_industry`, `wsr_owner`, `wsr_mode`, `wsr_last` | strings |
| governments: `gdp`, `debt_to_gdp`, `tax_rate_adj` | `wsr_gdp`, `wsr_debt_gdp`, `wsr_tax` | castle menu |
| pop: pop count, temperature | `wsr_pop`, `wsr_temp` | population block |
| (chain) wallet id, cones | `wallet`, `cones` | `e` + 24 hex of uid; cached |
Projection op: `bridge_project <links.pdl>`: for each LINK, if the piece's `state.txt` changed (size/marker) rewrite the entity's `variables.txt` atomically (tmp + rename) and append one line to `bridge/projected_changed.txt` (the marker windows watch).

## 6. Pets and companies trading: the money and the shares
- **Pet as a WSR holder:** `pieces/pet_<uid8>/` with `state.txt` (`cash`) and `holdings.txt`, same shape as `player_you` (the registry already counts any piece dir with a `holdings.txt` as a holder; dividends reach its `cash`).
- **Money bridge (never mints or loses value):** pet's mined cones -> Exchange (averaged rate into preferred value, fee in preferred) -> WSR dollars credited to the pet piece, capped per pet, recorded as `BRIDGE | in | uid | mc | dollars | rate_bp | fee | ts`; the reverse on cash-out (`out`). WSR companies run on **leased preferred cones** (managed accounts, swept back at game end). Harness invariant: `sum(WSR cash of all linked, leased entities) + fees == lease pool`.
- **Orders:** pets post orders into the same `book_<TICKER>.txt` through a `pet_quote` op (same row format), settled by `market_settle` (the only op that moves cash/holdings). A pet's decision uses mode 1 (weighted) first, mode 2 later (RL, paper money first).
- **Bonds:** need the bond ledger (`BOND | id | issuer | face | rate | maturity | holder | ts`); then banks issue and pets buy; coupons go to the holder.

## 7. The map: a `dsr` book on the pc-hq board (2D and 3D)
- **Book `dsr`** (like `doom`, `tsots`, `chess`): desks `city` (outdoors, e.g. 64x48), `downtown`, later `harbor`; each desk = glyph grid + `extrusion.pdl` (building height) + `events.pdl` + atlas, drawn by the existing 2D/3D board renderer (POV 1-4, yaw, pitch, height), so "companies and population on a 2d/3d map" needs no new engine.
- **Glyph legend (proposal):** `C` castle (gov), `B` bank, `S` store, `F` factory, `H` hotel, `P` population block, `E` exchange, `A` auction house, `h` pet house, `r` road, `g` grass, `w` water, `D` door.
- **Building = event page** (`on-touch`/click): opens that building's menu. The DSR toy's 34-item action menu is **split across the building kinds** (Trade/Search -> exchange/auction + stock board, Financing -> bank, Management -> store/factory, Derivatives -> exchange later, Government -> castle). Actions call the WSR ops with the linked `piece_id` (entity-dir adaptation: ops accept a piece id OR an entity dir via `links.pdl`).
- **Pets live on the same desk:** a pet house `h` is a building like any other; the pet village (`PET-VILLAGE-GAME-DESIGN`) is the residential district of this city, and Doom/TSOTS desks remain separate books reached by door events. Pets walk as events (sprites 16x24); doors are teleports (already built for the pet house).
- **Population:** `pop_*` pieces drive demand per district; blocks `P` show density; a store's customers are the pop block it touches (adjacency rule in `city.pdl`).
- **2D/3D of the same data:** the pc-hq renderer reads the desk; a building's look comes from the atlas and its height from `extrusion.pdl` (could scale with market cap in tunables, tweaking `extrusion.pdl` via the projection).

## 8. Time and order of operations (the clock is the spine)
One game day = one clock event `day_tick` (common event):
1. `tick_all` over linked WSR pieces (idle -> deciding -> trading) and `pop`/`weather` updates;
2. `market_quote` then `market_settle` (orders from companies and pets);
3. `corp_apply_finances` (interest, dividends via the registry, payroll);
4. `bridge_project` (changed pieces -> entity variables, marker bump);
5. the pet side (`ai_step`, `mine_tick`) runs on its own cadence and only touches pet files and the chain; its trades enter at step 2 next day.
Never project mid-tick. Every day boundary is a restore point (the snapshot layer of the play spine).

## 9. The x11-hq window: "WSR HQ" is the DSR toy, fed by projections
- Reuse `&.hq-apps/dsr` (`dsr.xhtpm`, `dsr_manager.c`): change its source from the single `dsr_state.pdl` to the projections of the **active building** (the link of the entity you select on the map) so the same 34-row menu works on any company.
- Add tabs: **Market** (order book + last fills from `market_ledger.txt`, same style as the Exchange rates table), **Holdings** (registry for the selected holder, pets included), **Bonds** (after the ledger exists), **Map** (opens/focuses the pc-hq `dsr` book).
- Exchange HQ and Auction HQ (built) are the service buildings' windows; WSR stock prices appear there as a second table.

## 9b. Safety (the live-data rule)
- The owner's DSR pals live under `xyzfs/users/<uuid>/...` (tracked on data branches, not in code). **All rehearsal on `dsrtest_*` copies (`desk_copy_op`) or scratch dirs; the bridge writes into live entities only after a backup + verified file count** (`AGENTS.md` user-data rule).
- Projections only ever write `variables.txt` (one file per entity); authorities are untouched.
- The bridge never moves cones; only the Exchange/lease layer does.

## 10. Build order (each step has a harness; scratch only until the last)
| # | Step | Proof |
|---|---|---|
| 0 | fold the WSR entity data into `WSR_PAL-PREFERED`, rebuild ops, fix `market_settle` registry path; `wsr_scratch` pal harness (copy, tick, quote, settle) | prices move, ledger grows |
| 1 | `bridge/links.pdl` + `bridge_link` (from a `city.pdl` manifest) + `bridge_project` + marker; harness on `dsrtest`-shaped scratch entities | project is idempotent, one-way, atomic; a changed piece updates exactly its entity |
| 2 | pet holder pieces + `pet_trade`/`pet_quote` + money bridge + conservation invariant | `sum(cash)+fees == pool` every tick |
| 3 | `dsr` book: `city` desk, glyph legend, atlas, building events that open menus (2D first, then 3D with the board renderer) | PNG 2D + 3D, click path to a store menu |
| 4 | WSR HQ window: DSR toy re-pointed at the selected building; Market/Holdings tabs | PNG, click path |
| 5 | bond ledger; banks issue, pets buy; day tick as a clock event with restore points | harness |
| 6 | pets live in the city (pet house `h`, village as district); RL on paper money; XOD-style tournaments over pet pieces | tournament dashboard PNG |

## 11. Questions for the owner
1. Rehearse the bridge on `dsrtest_*` (recommended), or link the real `dsr_*` pals right away?
2. Hotel = `realestate_*` (the 2 realestate pieces exist): OK, or should hotels be corps?
3. City size and layout: one big `city` desk (suggest 64x48) with districts, or one desk per district?
4. Which of the 50 corporations are shown as buildings first (suggest 12: 4 banks, 6 stores/factories, 2 hotels) and the 7 governments as castles?
5. Should building height follow market cap (live, via `extrusion.pdl` projection)?
