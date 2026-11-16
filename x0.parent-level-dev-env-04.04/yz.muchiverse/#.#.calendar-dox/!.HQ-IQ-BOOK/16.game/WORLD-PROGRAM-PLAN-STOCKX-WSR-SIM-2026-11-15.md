# The world program: stock+x, the WSR sim, headless worlds with many views and controllers (plan, 2026-11-15)

> Owner 2026-11-15 (SAID): "how should we organize this all up and do it. I like the pure broker interface for light sim. But you can open a separate rpg-edit window to view the 2D/3D board and control the sim from either, since it's technically headless and data driven. We could also control the scene with different controllers depending on what is hooked up to the simulated universe at any given time, and multiplayers can just jack in. Even TSC should have a 3D-viewable, 2D-viewable view of just two players facing each other on a 'board', so everything always has an XYZ representation and the ability to render. This is the vision, long term. Make sure I get a copy of our in-house docs that cover all this in nov-15 before we start."
> Also SAID today: stock+x is a **TSOTS** app for now; a **WSR app** follows with the same interface; **TEMPIRE** is the empire and **TEMPEROR** an AI-run empire government company that every other company and government pays tribute to; **real time and game time are parallel calendars and both are taxed**; there is an adjustable platform fee.
> Written by Claude Sonnet 5.5. **READ** = I opened it; **DOC** = my proposal, not decided. Nothing in this plan is built. This file supersedes nothing: it ties the other docs together (the reading list is section 9).

## 1. The vision in seven rules (DOC, distilled from the owner's words)
1. **A world is headless data.** A world (the WSR sim, a TSC match, a Teropoly game, a pet town) is a folder of append-only ledgers plus a seed, changed only through verbs; everything else on disk is a projection (the ledger rule). It runs with no window open.
2. **Every interface is a projection.** The **broker interface** (phone: quotes, portfolio, buy/sell) is the *light* view; a **board window** (2D/3D, the rpg-edit/pc-hq family) is the *heavy* view; a CLI, a harness and an AI agent are views too. None owns state.
3. **Controllers are interchangeable.** A controller is anything that can append verbs to a world: the phone UI, the board window (INT keys, the xelector), the relay and harness, an AI agent, a network peer. Which controls exist at any moment depends on **what is connected** to the world (a session).
4. **Jack in.** Multiplayer is a session joining a world (over the p2p peer already proven with two phones), taking a seat or a role; leaving is a row too.
5. **Everything has an XYZ representation** ("always the ability to render"): every world can export a **scene** (entities with x, y, z, kind, label, sprite). Even True Swords Clash: two players facing each other on a board. A view that is not open costs nothing.
6. **One price bus, many producers and consumers.** Yahoo, the WSR sim (`wsr-sim-<n>`), any later source write quotes; stock+x, the WSR app, a pet shop, a board sign, the chain exchange read them.
7. **Money flows to an empire (revised by the owner's answers, section 7a).** Three separate charges (platform fee, chain tx fee, tribute) plus taxes; tribute goes to the government the business is registered in, and governments pay TEMPEROR, the AI above all; rebates flow back down; a trade is taxed on the calendar of its world (real time for Yahoo-source trades, game time for sim trades).
(Standing house rules apply to all of it: one authority per fact, integer money, caps on runtime files, CPU niceness, harness cases that can fail, no Python, the usage-framework rule: names and rates are data.)

## 2. What already exists (READ) and how each piece is reused
| Need | Existing piece | Status | Use |
|---|---|---|---|
| Yahoo prices, buy/sell, options, predictions | `1.TPMOS_c_+rmmp.0103.0001/projects/yahoo/` (16 ops + manager) | operational 2026-04-28; **its http fetch now gets a 301** (checked today), https works with no key | port, do not rewrite (STOCK-X doc sections 1 to 4) |
| The WSR economy: prices, IPO, loans, payroll, tax, population, real estate, governments | `WSR_PAL-PREFERED/ops` (53 ops: `corp_update_price`, `bank_loan_op`, `day_loop`, `market_quote/settle`, `tax_loop`, `gov_*`, `pop_*`, `realestate_*`); `014.wsr-pal` (middle-house copy with its terminal UI) | built; state in mutable `state.txt` files (pre-ledger rule); trades do not move prices | the sim's rules; wrap behind ledgers (S3) |
| A watchable simulated town, a world clock, banks as buildings | `08-roadmap/design-docs/DSR-SIMULATION-DESIGN.md`, `DSR-WSR-PETS-BRIDGE-DESIGN.md`, `DSR-AS-PHYSICAL-WSR-PLAN.md`, `DSR-ENTITY-GAME-DESIGN.md`, `CLOCK-AS-THE-PLAY-SPINE-DESIGN.md` | **designs**; built: the DSR toy window (`&.hq-apps/dsr`), `@.apps/dsr-rpg`, `dsr-rpg-board`, the `dsr-test` page (4 banks, 8 stores, 2 castles, 2 population entities, no game state yet); no `dsr` book on the pc-hq board | the world and the board view |
| The chain as master bank, master tax and tx fee, bonds, contracts | `16.game/CHAIN-MASTER-BANK-FED-AND-CONTRACTS-DESIGN-2026-10-28.md`, `@.apps/pet-trainer/CHAIN-ECONOMY-DESIGN.md`, `PIECE-IDENTITY-AND-MASTER-PAL-LIST-DESIGN-2026-10-28.md` | designs; old chain and exchange/auction HQ built as paper ledgers | the treasury and tribute rails (S2, S8) |
| A ledger game projected on a 2D/3D board, driven by harnesses | Teropoly (`deskopoly`), `16.game/TEROPOLY-ONBOARDING-...`, harness cases `deskopoly`, `dsk_driver`, `live_dsk_seat` | built and tested; "the game is data + ledger; the screen is a projection" | the pattern for S5 and S7 |
| New games from data on one shared binary, kinds ledger / realtime / editor | `16.game/GAME-FOUNDATION-GENERATOR-HOW-TO-AGENT-GUIDE-2026-11-09.md`, `ops/game_new_op.c`, `game_core` | built; the live Play half unverified | a way to stamp a world and its board |
| The 2D/3D board, camera, controls, the rpg-edit window | `@.apps/rpg-edit-hq`, `pchq-board`, `bv_render_3d`, skill `rpg-space-board-driving`, `02-architecture/CENTROID_GOLD_STD.md` | built (the owner's editor) | the heavy viewer and a controller |
| Phone apps, tab bar, HQ, ledger replay, p2p between two phones, host list | `&.hq-apps/plus-phone`, `app-edit/projects/true-swords-clash` (`tsc_op.c`), `hostlist_op.c`, `palnet_peer` | built and seen live today | the light viewer, the jack-in transport |
| Rules of the house | `NO-GHOST-DATA-...`, `DESIGN-CHOICE-TEST-AND-DECISION-LOG.md`, `RUNTIME-FILE-GROWTH-STANDARD.md`, `hq-cpu-safety.md`, `AGENT-TOOLBOX-AND-CONTROLS.md`, `20.middlehouse/HOW-TO-RELAY-DRIVE-MIDDLE-HOUSE.md` | standards | all slices |
**Not found anywhere (grep of the whole repo):** the name `wsr-sim-<n>`, "entity bank" as a defined term, a shared price service, the names TEMPIRE and TEMPEROR before today. The one-scene-description / 2D-3D parity principle (owner 2026-10-27) and the `scene` row format are NOT yet read by me in the board code: **slice S5 starts by reading the existing formats (`events.txt` rows, `cells.marker`, pc-hq pieces) and extending them, not inventing a new one.**

## 3. Target architecture (DOC)
```
 CONTROLLERS (append verbs)                         VIEWS (read projections)
 phone broker UI | board window (INT, xelector)     phone broker screens  | board window 2D/3D
 harness / relay | AI agent | network peer          CLI / harness reads    | a phone's small scene view
        \                |                                 ^                          ^
         v               v                                 |                          |
        ┌──────────────── SESSION (who is jacked in, role, seat: ledger rows) ───────────────┐
        │                                                                                     │
        │  WORLD  = seed + clock ledger + entity ledgers + verbs   (headless, deterministic)  │
        │     │ writes quotes                │ exports                 │ pays/receives        │
        │     v                              v                         v                      │
        │  PRICE BUS (quote ledgers per   SCENE (x,y,z,kind,sprite)  TREASURY of TEMPEROR     │
        │  source, latest.txt projection)  one description, 2D + 3D   (fees, tribute, tax,    │
        └─────────────────────────────────────────────────────────── rebates; policy.pdl)────┘
```
Five small CONTRACTS make this real; each gets a short spec and a harness case before any app depends on it:
- **C1 World:** folder `worlds/<kind>-<n>/` (for example `wsr-sim-1`, `tsc-7`), `world.pdl` (kind, seed, tick rate), `clock.ledger`, entity ledgers, `verbs`. Same seed gives the same ledgers (the DSR simulation doc's own phase-1 test).
- **C2 Scene:** rows `ENT | uid | kind | x | y | z | facing | sprite | label | state` (to be reconciled with the formats the board already reads; S5 starts there).
- **C3 Session:** `JACK_IN | n | ts | who | role | seat | controller_kind`, `JACK_OUT`, role permissions as data. Transport = the p2p peer (proven: two phones, forged rows refused by replay).
- **C4 Quote bus:** `Q | n | ts | SYM@source | price_micro | prev_micro | meta` per source, plus `latest.txt` (STOCK-X doc 14.2).
- **C5 Treasury and policy:** a second ledger for TEMPEROR's account, rows `FEE`, `TRIBUTE`, `TAXEST`, `TAXYEAR`, `REBATE`, `policy.pdl` bounds (STOCK-X doc 13); real and game calendars both present.

## 4. The order of work (vertical slices; each ends with something you can see and a case that can fail)
| Slice | What | You see | Size |
|---|---|---|---|
| **S0** | This plan; the docs copy in XO; decision-log row D86; open questions answered or defaulted (section 7) | the docs | done today |
| **S1** | **stock+x P0/P1:** `stockx_fetch_op` (https, cache, budget), quote bus v0 (source `yahoo`), ledger and replay, market and portfolio screens, buy/sell, integer cents and micro-shares. Shared `ui.txt` writer text-include as a side task (3 phone ops convert first) | a **real ticker with its time on a phone**, then a bought and sold position with correct cash | M |
| **S2** | **Treasury v0:** TEMPEROR account ledger, platform `FEE` and `TRIBUTE` rows on every trade, `policy.pdl` v0 with signed history, case that user plus treasury sums are constant | the portfolio shows fees and tribute paid; the treasury balance | S to M |
| **S3** | **`wsr-sim-1` headless:** a small world (3 corps, 1 bank, 1 population pool), clock ledger, `corp_update_price` and `bank_loan_op` behind ledger verbs, quotes to the bus; determinism case (same seed, same prices); no window | prices moving in a text projection with no UI at all | L |
| **S4** | **WSR app:** the same package as stock+x with source `wsr-sim-1` (mostly data) | the same broker screens trading fictional tickers; both apps on one phone | S |
| **S5** | **Scene and board:** the sim exports a scene; the rpg-edit/board window opens world `wsr-sim-1` (2D then 3D) as a read-only projection; **two controllers**, the phone and the board, append verbs to the same world; the bank visibly lends | the town on the board while the phone shows the same prices, and either one controls it | L |
| **S6** | **Sessions and jack-in:** `JACK_IN` rows, roles, a second machine or phone joins a world over the peer | two phones (or phone and board) in one world | M |
| **S7** | **TSC scene:** two players facing each other on a board, 2D and 3D, from the TSC ledger | the proof that "everything has an XYZ representation" | M |
| **S8** | **Taxes and the empire:** quarterly estimates and year-end true-up on the real calendar and the game calendar, rebates, TEMPEROR's AI decisions within `policy.pdl` bounds | a quarter closing with a tax row on each calendar | L (the AI part depends on the attrition-model layer, which the 2026-11-08 audit found has designs and no callers) |
**ANSWERED (round 1): S1 and S3 run in parallel** (two tracks; the sim track could be given to a separate agent). S2 follows S1. Dependencies: S4 needs S1 and S3; S5 needs S3 and the scene reading; S6 and S7 need the scene contract from S5; S8 needs S2 and a clock (S3).

## 5. How we organise it (so it stays one program, not twelve)
- **One program doc** (this file) updated at the end of every slice: status table, what was SEEN, what was not. **One design doc per contract** (C1 to C5) written when its slice starts, in `16.game`, with an index row in `16.game/00-INDEX.md`; **one decision-log row per real choice** (D86 now).
- **ROADMAP.md** gets one row per slice with the harness case name that backs it (a `[x]` is a claim until a harness name or a dated observation backs it).
- **Harness first:** every contract has a case that can fail with a negative control (the `tsc_owner` lesson: a check that passes without its feature proves nothing). Visual slices end with a photograph the owner can open (human-IO evidence rule), saved in XO.
- **Where code lives:** stock+x and the WSR app are app packages under `&.hq-apps/app-edit/projects/`; worlds are data under a `worlds/` folder (runtime, gitignored, with caps and rows in the growth standard); shared ops under `&.widgits/_shared-lib` only when two consumers exist (the house rule: text-include for pure code, op plus IPC for stateful).
- **Commits and checkout:** my branch only, scoped paths, push as authorised; the old middle-house code is read, never modified; anything that touches the owner's live editor or windows is asked first.
- **XO:** the docs that cover all this are copied into `XO/31.Nov/nov-15` (section 9) and refreshed from the book with `xo_export` at the end of each slice; the book file is the one authority.

## 6. Risks (and what we do about each)
| Risk | Plan |
|---|---|
| Yahoo's feed is unofficial and its terms are unread | personal/demo use; the source is replaceable (`SOURCE` rows); cached quotes with their age shown; read the terms before anything public |
| Shared renderer changes (footer, dropdown pass) touched every window | already checked on one second real window (Casino Island); open rpg-edit once before S5 |
| Scope: the vision is large | vertical slices, each useful alone; no slice depends on a later one |
| The WSR ops keep state in mutable files | wrap with ledger verbs in S3; never write a projection back; fixture worlds only in scratch |
| TEMPEROR as an AI | start with a rule-based TEMPEROR inside `policy.pdl` bounds (S2); AI later (S8), only through the verbs |
| Two calendars (real and game) | every row carries both stamps from S2 on, so S8 has the data |
| CPU on a weak machine | sims run only when asked, at a stated speed, niced; viewers cost nothing when closed |
| Names (TEMPIRE, TEMPEROR, `wsr-sim`) | recorded exactly as the owner said; no lore added; asked where unclear |

## 7. Decisions, with the default I would take (round 1 answered in 7a: items 1, 6, 7 and 8 below are SUPERSEDED by 7a)
1. **First slice:** S1 (stock+x P0/P1) then S3 and S2 together. *Default: yes.*
2. **Start money / refills:** $1,000, no refills. 3. **Fees:** platform fee 0.5 percent adjustable, tribute 0.1 percent adjustable, both in `policy.pdl`. 4. **Cost basis:** FIFO. 5. **Quote freshness for a fill:** 300 s; **Yahoo budget:** 300 requests per hour. 6. **Tribute vs master tx fee:** treat them as the same payment (the chain doc's master tx fee, paid up to TEMPEROR) until told otherwise. 7. **Calendars:** every row carries `ts_real` and `ts_game`; taxes on both, independent. 8. **Entity bank:** the DSR bank entities (a `corp_*` with `industry=bank`) acting through `bank_loan_op`; the chain master bank is a separate rail. 9. **Worlds folder:** `worlds/<kind>-<n>/` next to the house, gitignored. 10. **Brand:** TSOTS (black and red). *All defaults are mine, not yours; each is a one-line data change later.*

## 8. What I will do first if you say go
S1, in this order: (a) read `projects/yahoo/test_suite]a0.c` and the old orchestrator for behaviour to carry over; (b) write `stockx_fetch_op` and its canned-response case; (c) **one live fetch, photographed on a phone screen** (a real ticker with its time); (d) the quote bus v0; (e) the ledger and replay op with the rounding case; (f) the screens in the True Swords Clash shape. I stop and show you after (c).

## 9. The docs copy in XO nov-15 (what covers all this)
Listed in `XO/31.Nov/nov-15/21.READ-ME-FIRST-world-program-reading-list.md` (generated with this plan): the plan, the stock+x design, the chain-as-master-bank design, the three DSR designs plus the entity-game, clock and needs designs, the WSR economy architecture and economy intent, the middle-house conversion list and relay-drive guide, the old Yahoo project's report and succession note, the sim-manager and attrition survey, the identity and chain economy designs, the Teropoly onboarding and game-foundation guides (the "ledger game on a board" pattern), the phone design and platform guide (current copies), the house rules (ledger rule, design-choice log excerpt).

## 7a. Owner answers, round 1 (2026-11-15, SAID) and what they change
1. **First slice:** **S1 and S3 in parallel.** (Replaces default 1.)
2. **Charges:** **three SEPARATE charges**: the adjustable **platform fee**, the **chain tx fee**, and the **tribute**; tax is a fourth, separate flow. (Replaces default 6: the tribute is NOT the master tx fee.)
3. **TEMPEROR is the AI above everything.** In the game it is the **main AI that can control all NPCs through trunking functions; each NPC has its own personality that the main AI manager fires through.** (The only trace of "trunking" in our docs is the attrition-model definition, "sharing trunked intelligence banks", `wsr-sim-manager-ai.md` line 29 quoting `RUSSIAN_DOLL_HOUSE_DESIGN/ATTRITION_DIAGRAM.md`; the mechanism itself is not designed, and the 2026-11-08 audit found the AI layer has designs and no callers.) The "entity bank" is therefore not one named thing: it is the banks of the town acting under TEMPEROR's manager.
4. **Tribute goes to the government the business is registered in**, and governments pay TEMPEROR ("always above all"). **Illegal operations** (on the run, organized crime) do **not** pay it; that "can happen in a sim or IRL". New consequences: a business has a **registration** (a ledger row naming its government; changing it is a verb), an **illegal flag** (a state of the entity, set by what it does or by decision), and something must **detect and punish** evasion (TEMPEROR's manager and the government's NPCs: police, courts: the chain doc's disputes/courts design is the nearest). Crime is a game mechanic of the sim first; "IRL" is OPEN (see below).
5. **Calendars:** a trade is taxed on **the calendar of its world: real time for Yahoo-source trades, game time for sim-source trades.** (Replaces default 7: not "both on every row".) Rows still carry both stamps; the tax rule picks one by world.
**New open questions from these answers:** (a) Does the stock+x trader pay tribute to a government too (which one: a default empire government, TEMPEROR directly, the phone user's home government?) or only companies do? (b) How does an entity register and re-register (flags of convenience?) and who can see it? (c) What exactly does "IRL" mean for crime: a real-world trader on the Yahoo source can be "off the books" (an unreported trade?), or only sim entities? (d) How is evasion detected: a probability, an audit by NPCs, TEMPEROR's decision? (e) What is TEMPEROR's trunk: which existing layer (concept bank / A-TEARIT / FSM-GOAP / the ai_backend) does the manager fire NPC personalities through, and where do personalities live (a `personality.pdl` per NPC?). I will read `17.ai/` and the concept-bank and attrition docs before S8 and propose the shape; S3 starts rule-based (no AI) so it does not wait for this.
