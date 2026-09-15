Test Games Roadmap — Shared Systems + Build Order
===================================================
Reference doc, 2026-09-14. VISION doc — nothing here is built yet
except what §2 already marks real/live. Companion to
EVENT-TRIGGER-LAYER-PLAN.md, PLAY-MODE-ENTITY-HARNESS-DESIGN.md, and
CIV-TEST-DESK-AND-DOOR-TRANSFER-PLAN.md, whose real, already-proven
desktop-entity trigger mechanism (cursword walking onto a target
entity, `master_ledger.txt`, the desktop bridge watcher, real DESK-row
persistence, `mr_transfer_desk`) every game below is built on top of -
not a new mechanism per game.

Direct instruction that started this doc: a list of test games to
prove out different genre mechanics on real desks/entities/events, plus
"i want this in a document somewhere as things we are going to do
going forward."

## 1. Why shared systems first, not game-by-game

Every game on this list is really a skin over a small number of real
systems. Building system-by-system (once, generically) instead of
game-by-game (nine times, each reinventing its own clock/combat/economy)
is the whole point of this doc - pick the game that most cleanly proves
a system, build the system for real, then the next game that needs it
is mostly content, not new engineering.

## 2. Shared systems inventory

| System | What it is | Games that need it | Status |
|---|---|---|---|
| Desk/entity/event trigger layer | player-touch Common Events, desk transfer, persistent DESK rows | all of them | **REAL, LIVE** (§ civ-test/castle/door_civ work) |
| Calendar/time-tick engine | a real "week/day advances" clock, other systems subscribe to it | Monster Rancher, Harvest Moon, Desk Street Raider (DSR), Game Dev Story | not started |
| Grid movement + turn-based combat | tactics-range move/attack resolution on a grid | Civilization, TPMOJIO, TSOTS, Monopoly (board movement) | partial (`mr_move_to_entity` exists; no combat resolver yet) |
| Stat/resource + market/trading economy | a real ledger-backed economy: buy/sell/rent/loan/invest | Desk Street Raider (DSR), Harvest Moon (selling), Monopoly (rent), Civilization (trading), Game Dev Story | not started |
| PVP/battle resolution (turn-based, screen-swap) | Pokemon-style: swap to a dedicated battle screen, resolve turns by skill/type | TPMOJIO, Pokemon-mode, TSOTS | not started |
| Real-time action/combat | Zelda-style: live movement + hit detection, no turn swap | Zelda-mode | not started |
| Side-scroller camera mode | camera-5 side-scroll rendering, also the base for fighting-game mode | Mario-mode, fighting-game mode | not started |
| Board-game movement + property/rent | fixed-path tile movement, AI opponents, trading | Monopoly | not started (leans on grid-movement + economy once both exist) |
| Growth/breeding/care stats | stat decay/growth over calendar ticks, care actions | Monster Rancher (creature), Harvest Moon (crops/livestock) | not started (leans on calendar engine) |
| Verse/skill accuracy-based damage | a real "input accuracy vs. an expected pattern" -> damage-scaling mechanic | TSOTS | not started (leans on PVP/battle resolution) |
| 3D voxel/piece placement | Minecraft-style | piececraft-3d, board-viewer's own 3D raymarch work | separate, already-existing track (board-viewer-3d-perf, not part of this doc) |
| Ledger-driven multi-entity economy | many autonomous economic actors (governments/banks/stores/corps/players) each WRITE an intent to a shared ledger; one real manager process reads it and executes (trade, rate change, loan) - the exact `master_ledger.txt` mechanism the desk/event trigger layer already proves, applied to economic actions instead of movement/touch events | Desk Street Raider/DSR (the whole game), Civilization (trading), Harvest Moon (selling), Monopoly (rent/trades), Game Dev Story | not started - see §6, this is DSR's real proving ground |
| Quest/task-board delegation | a single player-managed avatar assigns work to OTHER entities via a real posted-task mechanism (a literal board/list an entity reads, claims, and executes against), rather than the player directly controlling every entity | Dwarf Fortress, Monster Rancher (the avatar's own care-tasks), Game Dev Story (hiring/assigning) | not started - see §7, Dwarf Fortress is the real proving ground |
| Menu/status-screen game UI (nav-driven, no map view) | a real `.chtpm` text/menu screen (nav-numbered folding submenus, a status readout) AS the entire game surface, no grid/sprite rendering at all | Desk Street Raider (DSR) | not started - this house already has the exact right primitive for it (the generic `khtpm_core_render.c` `.chtpm`/`cli_io`/nav-index machinery every taskbar menu already uses), see §6 |
| Z-level (3D layered) simulation | a voxel/layer stack a sim can dig into/build up through, not just a flat 2D grid | Dwarf Fortress (3D), Cataclysm-DDA (2D+3D), piececraft-hq (already has 3D/z-level rendering) | not started as a sim - piececraft-hq's own 3D raymarch rendering is the existing real substrate, see §8 |
| Crafting system | combine/break down held or nearby resources into new items via a real recipe mechanism | Cataclysm-DDA (the whole point), Dwarf Fortress, Harvest Moon (light), Game Dev Story (light) | not started - Cataclysm-DDA is the real proving ground, see §7b |
| Turn-based roguelike movement/combat | tile-based, permadeath-flavored, real-time-with-pause turn resolution over a large open map | Cataclysm-DDA | not started - shares the grid-movement primitive already partial from `mr_move_to_entity`, but needs its own combat/threat resolution, see §7b |

## 3. The full game list, what each proves, and its real system dependencies

1. **Civilization** — tactics-range grid movement, combat, trading, tech
   tree. Broadest system coverage of anything on this list; already the
   furthest along (civ-test desk, castle, door transfer all real/live).
2. **TSOTS** (own IP) — play as a sword possessing Bible characters,
   capture enemies by accurately arranging Bible verses to wear down
   HP. Reuses grid movement + PVP/battle resolution almost directly;
   verse-accuracy is just a new damage-calc input feeding the same
   resolver Civilization's combat would use.
3. **TPMOJIO** (own IP) — PVP matches between pieces with predetermined
   skills. Proves the PVP/battle resolution system in its purest form
   (no board, no economy) - a good second or third build after Civ's
   combat resolver exists.
4. **Monster Rancher** — passage of time as calendar weeks, entity
   care/feeding/training/resting, money via tournaments + online PVP.
   Stands up the calendar/time-tick engine for real; PVP leg reuses the
   battle-resolution system above.
5. **Harvest Moon** — farming: planting, watering, reproduction,
   selling/trading. Rides the calendar engine (growth ticks) and the
   economy system (selling/trading) once both exist from #4.
6. **Desk Street Raider (DSR)** — banking, loans, stocks, options,
   commodities, trading, startups, marketing, R&D. The real proving
   ground for the stat/resource + market/trading economy system.
7. **Game Dev Story** — hiring, studio management. Calendar-engine +
   economy, applied to a management-sim genre instead of a farm/bank.
8. **Pokemon-mode** — battle-screen-style battle (swap to a dedicated
   screen, turn-based, type/skill resolution). Proves the
   screen-swap variant of the PVP/battle resolution system.
9. **Zelda-mode** — real-time battle (live movement + hit detection,
   no screen swap, no turn order). Proves the real-time action/combat
   system, the one genuinely new mechanic class not covered by turn-
   based grid combat.
10. **Mario-mode** — camera-5 side-scroller. Also doubles as the base
    render mode for a fighting-game (same camera, different hit
    resolution rules layered on top).
11. **Monopoly** — buying real estate, collecting rent, AI opponents,
    trading. Board-fixed-path movement + the economy system; mostly
    content once both exist.
12. **Minecraft (piececraft-3d)** — separate, pre-existing track (see
    board-viewer-3d-perf-not-io-bound / board-viewer-3d-perf-ceiling
    memory entries), not sequenced with the rest of this list.
13. **Dwarf Fortress** (added 2026-09-14, direct instruction: "that
    should be the 4th we want to be looking at right away") — the
    connective-tissue game: a single player-managed avatar delegates
    work to other entities via a real quest/task-board rather than
    directly controlling them, the same real pattern Monster Rancher's
    tamagotchi-care needs (manage 1 avatar) generalized to managing
    many (Civ-scale multi-entity play). Also the natural home for
    testing farming mechanics (borrowed from Harvest Moon) and
    z-level/3D digging, feeding directly into piececraft-hq's own
    existing 3D substrate. See §7 and §8.
14. **Cataclysm: Dark Days Ahead** (added 2026-09-14, direct
    instruction: "lets do that fairly early on... we have the source
    code we can look at and copy, as well as the tilesheet") — a
    turn-based (real-time-with-pause) roguelike + crafting + z-levels
    game, meant to embody that exact genre vibe for both 2D and 3D. A
    real, unusually strong head start over every other game on this
    list: the ORIGINAL project's source code and tilesheet are
    available to reference/copy directly, not just design from genre
    memory. Proves the crafting system and turn-based roguelike
    movement/combat, and is the second real z-level consumer alongside
    Dwarf Fortress - see §7b.

## 4. Recommended build order (system-unlock order, not list order)

1. **Civilization** (in progress) — finishes the grid movement + combat
   + trading + tech-tree systems, broadest coverage first.
2. **TSOTS** — same combat resolver, own IP, high motivation, proves
   the verse-accuracy damage-input variant.
3. **TPMOJIO** — same resolver again, no board complexity, cheap third
   proof of the PVP system before moving on to calendar work.
4. **Dwarf Fortress** (promoted 2026-09-14, direct instruction: "look
   at right away") — stands up the quest/task-board delegation system
   AND the calendar/time-tick engine together (a DF-style sim needs
   both from day one, not sequentially). Sequenced ahead of Monster
   Rancher/Harvest Moon because it's the system BOTH of them actually
   need underneath their own single-avatar-care and farming mechanics
   - build the general delegation+calendar substrate here once, then
   #5/#6 below are mostly content on top of it. See §7.
5. **Cataclysm: Dark Days Ahead** — sequenced immediately after Dwarf
   Fortress (direct instruction: "fairly early on"), sharing its own
   z-level substrate and adding the crafting system + turn-based
   roguelike combat. Real source + tilesheet to reference makes this
   cheaper to build accurately than genre-memory alone would allow -
   worth doing while Dwarf Fortress's own z-level work is still fresh,
   not deferred. See §7b.
6. **Monster Rancher** — the single-avatar-care specialization of
   Dwarf Fortress's own delegation system (manage exactly 1 entity
   instead of many); calendar engine already exists from #4. PVP leg
   reuses the battle-resolution system from #2/#3.
7. **Harvest Moon** — farming content, built and proven once inside
   Dwarf Fortress's own 3D/z-level substrate first (see §8's back-and-
   forth plan), then confirmed working as its own standalone desk too.
   Rides the calendar engine (#4) and the ledger economy (#8) for
   selling/trading.
8. **Desk Street Raider (DSR)** — stands up the ledger-driven multi-entity
   economy system AND the menu/status-screen game-UI system. The
   economy piece then feeds back into Civilization (trading), Harvest
   Moon (selling), and Monopoly (rent) - see §6 for the full mechanic.
9. **Game Dev Story** — calendar + economy + delegation (hiring is a
   task-board variant), management-sim skin over #4/#8's systems.
10. **Pokemon-mode** — screen-swap battle variant.
11. **Zelda-mode** — real-time combat, the one truly new system left.
12. **Mario-mode / fighting-game** — side-scroller camera, built once
    both battle systems (turn-based and real-time) exist to draw hit
    rules from.
13. **Monopoly** — mostly content once board-movement + economy exist.

## 6. Desk Street Raider (DSR) — full mechanic (events-only, ledger-driven)

Direct instruction, 2026-09-14, with a real frame dump of the original
game's own status-screen UI as the reference shape (nav-numbered
folding menu over a live status readout - Wallet/Active Corp Balance
Sheet/Financial News/World Status, `[^]`/`[>]` fold markers, a
`Nav > _` prompt) - this is NOT a new UI system to build: it's the
exact same `.chtpm`/`cli_io`/nav-index machinery every taskbar HQ menu
in this house already uses (`khtpm_core_render.c`'s generic tag
vocabulary), just as the entire game surface instead of a popup. No
map/sprite rendering needed for DSR at all.

**The desk**: its own real desk (like `civ-test`), populated with:
- **2 castles** = governments - set interest rates, issue/buy/sell
  bonds.
- **4 banks** = buy bonds from governments, sell loans to stores/
  players.
- **8 stores** = take loans from banks, bank with banks, sell goods to
  the "population pool" (a variable for now - see §7/§8 for turning
  this into real entities later).

**The player** can: start their own companies, buy shares in stores/
banks, buy government bonds, trade commodities/options, short
positions, and manage their own companies' R&D spend, ad spend,
corporate assets, and stock buybacks/issuance.

**The core mechanic, and why it's simple but important**: every
entity on the desk (each castle/bank/store/corp) operates by WRITING
its own intended action to a shared master ledger - the exact same
real `master_ledger.txt` append mechanism (`timestamp|turn|actor|
action_type|details`) the desk/event trigger layer already proves live
(castle's touch-trigger, `desktop_ledger_append()`). A real game-
manager process reads the ledger and EXECUTES each entity's action
(a trade clears, a rate changes, a loan is issued) - entities never
mutate shared state directly, they only ever declare intent through
the ledger, same as every other desk entity in this house already
does for movement/touch. This is deliberately the SAME shape as
`mr_transfer_desk`/`khtpm_desktop_trigger_watcher.c`: an entity fires
an event, a real op/daemon reads the ledger and acts on it - DSR just
uses that shape for economic actions instead of desk transfers.

**Why this matters beyond DSR itself** (direct instruction: "do u see
the plan and how these are all related?"): this ledger-driven,
manager-executes pattern is the same real mechanism every other game
on this list needs for its own economy leg (Civ's trading, Harvest
Moon's selling, Monopoly's rent/trades, Game Dev Story's studio
finances) - DSR is simply the game that proves it in its purest,
deepest form (loans/bonds/shorts/buybacks exercise it far harder than
a farmer selling crops does), so building it for real here means every
later economy feature elsewhere is mostly wiring, not new engineering.

**The stated future extension** (not started, written down now per
direct instruction): the "population pool" variable becomes real
entities, and stores start selling REAL goods/services tied into the
farming sim (Harvest Moon mechanics) and the Civ tech tree sim -
governments gain the ability to wage war (Civ-style) and hold votes.
This is the explicit bridge from DSR's abstract economy to the whole
rest of this roadmap's simulated world being one connected thing, not
twelve separate demo desks.

## 6b. DSR's dual front door: toy AND desk, independently

Direct instruction, 2026-09-14: "i also want to add the toy DSR...
the dsr toy will open an x11-hq window, with a gui like i showed u
before, and also open its own desk but it can be played without the
desk being open, like a normal toy, and can even be set 'not to open
desk'... it can also be played without toy, by just opening desk."

**Why this matters** (real architectural reason, not just a feature
request): every other game on this list has been assumed to be reached
through its desk - walk up to an entity, touch it, a menu/event fires.
DSR is the first one explicitly designed to have TWO independent real
front doors onto the exact same backend state (the ledger-driven
economy, §6):

1. **As a toy** - a real `toy.pdl` entry (this house's own established
   convention, `khtpm_taskbar_manager.c`'s `toys_scan_one_root()`/
   `livedesk_build_toys_menu()` - opt-in by file presence, already
   proven for `mutaclysm`/`my-chara`/`my-lawyer`/`piececraft`), which
   opens DSR's status-screen menu (§6's nav-numbered `.chtpm` UI) as a
   normal X11-HQ window, exactly like any other toy/HQ app. No desk
   required to be open at all for this to work.
2. **As a desk** - `dsr` (or whichever real name it ends up with), the
   same 2-castle/4-bank/8-store desk from §6, walkable/touchable like
   `civ-test`. No toy window required to be open for this to work
   either - opening the desk and interacting with its entities is a
   complete, independent way to play.

**The toggle**: launching the toy has a real setting for whether it
also auto-opens its own desk (default likely on, but explicitly
settable off - "can even be set not to open desk"). The desk, if
opened separately (or already open), and the toy, if opened separately
(or already open), both read/write the SAME real ledger/state - two
views of one game, neither a special case of the other.

**Why this is worth doing, not just building the toy alone**
(understanding the real point, not just the literal ask): it's the
first real, deliberate proof that this house's two entity-access
patterns - the taskbar's own HQ-app/toy convention, and the desk/
entity/event trigger layer - are genuinely interchangeable front ends
onto the same real game state, not two competing architectures. Every
later game that wants BOTH a spatial desk presence and a quick
menu-only way to check in (a status screen, a management panel) now
has a real, proven pattern to copy instead of inventing its own.

**Status**: documented now per direct instruction ("we will make desk
and toy entry and record this in our documentation now") - not yet
built. Real next steps when this is picked up: a real `toy.pdl` for
DSR (title="Desk Street Raider", launch="button.sh" or a direct
`.chtpm` launch), the `dsr` desk itself (empty for now, same as
`tsots-test`/`tpmojio-test`/`dwarf-fortress-test`), and the toggle
state file/flag controlling desk auto-open on toy launch.

## 7. Dwarf Fortress — why it's the 4th game, right away

Direct instruction, 2026-09-14: "the monster rancher aspect, also ties
in where 1 or more entity is managed like a tamagotchi... but that
avatar will, like dwarf fortress, manage other entities thru a quest-
board/task-board type situation... this should be the 4th we want to
be looking at right away."

Dwarf Fortress is the missing connective system between two things
already on this list that looked separate but aren't: Monster
Rancher's "player manages exactly 1 avatar, tamagotchi-style" and
Civilization's "player manages many entities/units directly." Dwarf
Fortress's real mechanic is neither - the player manages ONE avatar
(or a small leadership layer) who posts tasks to a real quest/task-
board, and OTHER entities read that board, claim a task, and execute
it autonomously (mine here, build this, haul that). That's the exact
general form Monster Rancher's own single-avatar care is a
SPECIALIZATION of (the board has exactly one worker: the player's own
avatar) - building the general delegation system once here means
Monster Rancher's own "feed/train/rest" loop is mostly content on top
of it, not a separate system.

It's also the natural place to prove farming mechanics (Harvest Moon)
and z-level/3D digging together, before either is built as its own
separate, standalone game - see §8.

**Real system dependencies**: quest/task-board delegation (new, this
is its proving ground) + calendar/time-tick engine (needed from day
one, not optional here the way it might be added later to a simpler
game) + the existing desk/entity/event trigger layer (a posted task is
just another real event an entity's own trigger can pick up and act
on, same shape as `khtpm_desktop_trigger_watcher.c` already proves).

## 7b. Cataclysm: Dark Days Ahead — why fairly early, and the real head start

Direct instruction, 2026-09-14: "lets add cataclysm, dark days ahead
to that list... we will do that fairly early on and even have the
source code that we can look at and copy as well as have the
tilesheet for that, and it embodies the turn based live action
roguelike + crafting + z levels vibe we are going for that will work
in 2d and 3d."

Sequenced right after Dwarf Fortress (build order #5) rather than
later, for two real reasons:
1. It shares Dwarf Fortress's own z-level substrate - building it
   while that work is fresh avoids re-learning the same 3D/z-level
   integration a second time later.
2. Unlike every other game on this list, this one has a REAL, existing
   reference implementation available to read and copy from directly -
   both the original Cataclysm-DDA source code and its tilesheet - not
   just genre knowledge. That's a real, unusual advantage worth using
   while it's top of mind, not banking for later.

**What it proves**: the crafting system (combine/break down resources
into items via a real recipe mechanism - new, not covered by anything
else on this list) and turn-based roguelike movement/combat (tile-
based, real-time-with-pause turn resolution over a large open map -
related to but distinct from Civ's own tactics-range grid combat).
Both genuinely new systems, not reskins of something already planned.

**2D and 3D both**: like Dwarf Fortress, meant to work in both modes -
the same real back-and-forth plan in §8 applies here too, and the two
games together (both z-level, both close in build order) are the
strongest real test of piececraft-hq's 3D substrate holding up under
more than one genre's worth of mechanics.

## 8. The 3D/piececraft-hq synergy loop

Direct instruction, 2026-09-14: "we may also steal some of the farming
mechanics and try dropping them into piececraft-hq and making sure
they still function in 3d, we may go back and forth loading our dwarf
fortress in 3d and making sure it works with z levels."

piececraft-hq already has real, working 3D/z-level rendering (the
board-viewer 3D raymarch work - see `board-viewer-3d-perf-not-io-bound`
and `board-viewer-3d-perf-ceiling` memory entries, GPU raymarch daemon
already landed). The plan is an explicit back-and-forth, not a one-way
port:
1. Build farming mechanics (planting/watering/growth) as flat, 2D desk
   content first (Harvest Moon, §4 step 6).
2. Drop those same mechanics into piececraft-hq and confirm they still
   function with real 3D placement/z-levels, not just visually but
   mechanically (a planted tile still waters/grows correctly when it's
   a voxel at some z-level, not just a flat sprite).
3. Build Dwarf Fortress's own sim (§7) in 3D from the start, using
   piececraft-hq's existing z-level substrate rather than a new flat
   grid.
4. Build Cataclysm-DDA (§7b) the same way, right alongside it - its
   own real source+tilesheet reference makes it a good second, harder
   test of the same z-level substrate while Dwarf Fortress's own
   integration work is still fresh.
5. Go back and forth between all three (farming, Dwarf Fortress,
   Cataclysm-DDA) - z-level lessons from one feed back into making the
   others more real, rather than building each in isolation and trying
   to unify them later.

This is deliberately mid-term, exploratory work, not a scheduled build
step with a fixed order - the point is discovering real synergies and
paths forward between systems that already technically both exist
(farming content, piececraft-hq's 3D rendering) rather than assuming
in advance how they'll combine.

## 9. Status

Nothing below Civilization (already in progress per
CIV-TEST-DESK-AND-DOOR-TRANSFER-PLAN.md) is started. This doc is the
durable, standing reference for "what's next and why," to be updated
as each system/game lands - not a commitment to build all of it in one
pass.
