# rpg-pvp-hq and events (and blueprintz later) - 2026-10-20

Owner's question: do the latest grok game (game-dev-hq) and our rpg-pvp-hq use **events** - event-based scripting - so blueprintz can show and edit them later? This page answers it from the files. Labels: READ = from files; RAN = I ran it today and looked.

## 1. "Event" means three different things in this house
| name | what it is | who writes it | blueprintz sees it? |
| --- | --- | --- | --- |
| **Event page package** (events-hq) | a folder `events/<name>/event_pkg/pages/page_N/` with `event.ir.pdl` (the command list, `NODE \| id=N type=<command> \| args`), `condition.pdl` (the trigger: ui, clock, touch, autorun, ...), `event.pal` (compiled program) and `cmd_N.sh` (wrapper that runs the command). RPG-Maker-style scripting. | events-hq editor; compiled by the house tools | **yes** - this is the format `bp_scan` / `bp_tree.sh` read |
| **Clock event** | a row in `time.pdl` (`AT \| phase_night \| 19:00 \| every:1day`). The house clock fires it and runs the event package of the same name. | `time.pdl` | yes, as the package it runs |
| **Ledger row (S1)** | a line in `state/events.txt`: `EVENT\|ts\|actor\|verb\|target\|at\|queued`, resolved by C code into `EVRES\|...`. It is a **log and a rule resolver**, not a script. See `S1-EVENT-LEDGER.md`. | the op (`ge_append`), a pet, a menu row | **no** (nothing scripts it; there is no page to show) |

## 2. What each game uses today (READ, counted 2026-10-20)
| game | event page packages | ledger rows | note |
| --- | --- | --- | --- |
| cdda-hq, df-hq, gta-hq, dsr-rpg | 21 each | yes | the same 21: play, stop, int_on, int_off, 4 phase_*, day_tick, greet_* and reply_* for the six pets (door_* packages exist only in rpg-pet) |
| mcl-hq | 22 | yes | |
| rpg-pet | 25 | yes | |
| **rpg-pvp-hq (ours)** | **21** (copied from cdda-hq) | yes | see section 3 |
| **game-dev-hq (grok's latest)** | **0** | yes (`EVENT\|ts\|studio\|hire\|...` appended by `event_row()` in `game_dev_hq.c`) | its verbs (hire, train, rest, start, assign, advance, ship) are C code plus ledger rows. No event pages, so blueprintz would show nothing for it. `bp_tree.sh` already lists `game-dev-hq` but there is nothing to find |

So: **cdda/df/mcl/gta/rpg-pet and now rpg-pvp-hq use event pages for the game's chrome** (start/stop, INT on/off, day phases, greetings); **the gameplay rules themselves are C verbs plus ledger rows in every game**, including grok's latest. Nothing in any game scripts "pick up a gun" or "open a door" as an event page yet (the doom book in piececraft-hq does for its map events, `events.pdl`, a separate store).

## 3. rpg-pvp-hq: what happens now (RAN)
rpg-pvp-hq is **a rename-copy of cdda-hq** (house recipe, same as mcl-hq/df-hq): `INT` mode (turn-based vs real time), `book` (the six pets), `page` (rooms), Shop, Menu, Help, Inventory, Craft, Body, Look, Player (Play/Stop/Save/Load), the game clock (`D1 06:00`) with day/night phases, the pets in the footer, chat, info. I first built a small city viewer window instead of copying; that was the wrong order, the viewer is kept as `rpg-pvp-city.xhtpm` and the real game window is `rpg-pvp-hq.xhtpm`.
- **Events fire.** In the real window I clicked INT (key `1`, Enter): `state/flag_int.txt` became `1`, the window showed `INT on: Rex`, and `events/int_on/master_ledger.txt` got `Play event page_1 (trigger=ui) for int_on`. The clock fired `phase_night` by itself (`trigger=clock`, same ledger). The clock started at D1 06:00 and read D1 17:17 when I looked (rate "sec" = one game hour per second).
- **INT walking works.** With INT on I appended `1001` (right), `1001`, `1003` (down) to `state/interact_relay.txt` (the engine's own key relay, codes in `keybinds.pdl`): Rex went from castle 6,3 to 7,3, 8,3, 8,4 and the other pets kept stepping (STEP rows in `state/events.txt`). Arrow keys sent through the renderer's per-window relay as `KEY_PRESSED: 203` did not move him; that mapping was not investigated.
- **Own switches.** INT wrote `@.apps/rpg-pvp-hq/state/world/switches.txt`; cdda-hq's `switches.txt` was not touched (mtime 2026-10-12 18:19; ours 2026-10-20 03:59).
- **Each game has its own two registered commands** in `#.ref/menu/event_commands.registry.pdl`. I renamed the copy's commands (`rpg_pvp_hq_verb`, `rpg_pvp_pet_says`) but at first did not register them: `bp_scan` (blueprintz's read-only scanner) then flagged **105 `unknown-type`** problems on rpg-pvp-hq (cdda-hq: 0). I added the two COMMAND blocks (same shape as cdda's). Result: **42 events, 0 flags** (`bp_scan <house> --dir ...` over all 21 packages; output kept in `XO/rpgpvp_t/bp/`). The negative case is real: it failed 105 before the fix.
- Known, not mine, same in cdda-hq since Oct 12: every event run logs `[Prisc Error] Could not open ops file: .../101.mutaclsym.../system/default_op.txt` before the line that shows the page ran. The page still ran (the INT flag changed). Not fixed.

## 3b. Downtown, loot and the first gameplay event page (RAN, 2026-10-20 later)
- **Downtown** is a second overmap room (`ROOM | downtown ... | city:downtown.txt`): the glyph map and its LOOT rows come from `ops/rpg_pvp_city` (`downtown.txt`, `downtown.pdl`, seed 10, 40x30: 6 buildings, 11 loot, drawn with the cdda tileset; `#` and `W` block walking). The island city reaches it by a door pair in `rooms.pdl`. A bad map file falls back to the island.
- **Pickup.** Walking onto loot appends an S1 `give_item` EVENT through `try_pickup()` (ledger rows `EVENT|..|rex|give_item|pistol|downtown:5,6` then `EVRES ok`); the chat says "Rex finds a pistol". The Inventory menu has "Pick up here" and **`events/pickup/`** is the event page for it (trigger `ui`, node `rpg_pvp_hq_verb verb=pickup`). Tested in the real window: Rex walked 4,6 -> 5,6 and got `rex:pistol=1`; a second `pickup` there and stepping back onto the cell gave nothing (one ledger row per loot); the page wrapper `cmd_1.sh` picked up loot under Rex; `bp_scan`: 0 flags on 22 packages. Taken loot is read back from the ledger (incremental read), so Save/Load/New game bring it back. Not built: ammo counts and reload (the message says so), shooting, loot respawn.

## 3c. CPU (owner: "the pvp is currently a hog"), measured with /proc ticks over 40-60 s
- **Cause 1, fixed:** the copied clock installs at speed `sec` (1 game hour per second), so a day lasts 24 s and 5 phase/day events (each a sh + bash + prisc launch) fire every ~5 s: **4.5% of a core in the clock's finished children**. Install default is now `min` (time.pdl already said min was the default); the live clock was switched with `clock rate min`: children 4.5% -> 0.0%. Total with INT on: about 10% -> about 3%.
- **Clock label** shows the time rounded to 10 min at `min` (hour at `sec`) because each label change repaints the window.
- **Still heavy, not fixed:** in real time (INT off) with pets walking in the viewed room, `scene.raw` changes about 2.3 times a second and each full repaint of the shared renderer costs 49-96 ms (avg 66 ms, `KH_REDRAW_TRACE`): renderer 7.6% + engine 2.0% of a core. That per-paint cost is the shared renderer's (canvas plus all chrome), the same for every canvas game; the fix is a canvas-only repaint (shared-renderer work, not done). The engine's 100 ms main loop also re-reads about 25 small state files per cycle (about 230 opens/s under strace).
- **The two edits checked:** the dropdown row cap in the renderer is arithmetic only (no cost); the `dropdown_max_rows=0` setting only makes an open dropdown taller. A gprof profile of the renderer was started and cut off; no profile numbers exist.

## 4. The rule for rpg-pvp-hq from now on (so blueprintz can use it)
1. **Gameplay actions are event pages, rules are verbs.** An action the player or a pet can trigger (pick up a gun, open a door, reload, enter a building, spawn a monster, day/night change) is an event package: trigger in `condition.pdl`, steps in `event.ir.pdl`, one of them `rpg_pvp_hq_verb verb=<verb> arg=<arg>`. The verb (C, in `ops/rpg_pvp_hq.c`) resolves it and appends the S1 `EVENT`/`EVRES` rows. Blueprintz then shows the page, its trigger and its switches; the C code stays the one place that decides the result.
2. **Register every new command** in `#.ref/menu/event_commands.registry.pdl` (never hard-code a list). Check with `bp_scan`; 0 flags is the gate (`unknown-type`, `no-condition`, `dead-target`, `abs-path`, `over-128` are the real failures it catches).
3. **No second event format** (BLUEPRINTZ-DESIGN.md section 3): `event.ir.pdl` is the single source of truth; do not store scripts in the op or in a private file.
4. A new verb needs: the verb in the op, an event page (or several) that calls it, a `time.pdl` row if the clock should fire it, and the S1 rule in `GE_RULE_HOOK`. The shooter verbs planned in `RPG-PVP-HQ-DESIGN.md` (`fire`, `reload`, `equip`, `pickup`) follow this.
5. Add the game to `&.hq-apps/blueprintz/ops/bp_tree.sh` (its hard-coded game list) so the B1 tree lists it. Done for rpg-pvp-hq in the same commit as this page.

## 5. What is not done

> **2026-10-20 later:** the blueprintz gap below is analysed and a fix is designed in `EVENT-VERB-PAGES-DESIGN.md` (the real gap has two parts: no pages, and pages that the player's path does not run). **Built for rpg-pvp-hq the same day (route chosen by the owner): 13 gameplay verbs are event pages the window really plays; see section 8 of that page.**
- Only the 21 copied chrome events exist. **No shooter event page exists yet** (pickup, door, reload, spawn). The city generator (`ops/rpg_pvp_city.c`) is still separate from the engine: the engine's map is the copied cdda island. Joining them (city as the overmap room, buildings as rooms through door events) is the next step, see `RPG-PVP-HQ-DESIGN.md` section 6, P1.
- blueprintz itself is B0/B1 (a shell and a read-only tree per its design); editing (B4) is not built, so "use blueprintz later" means: keep to the format above and it can read and later edit these pages. I did not open the blueprintz window.
- game-dev-hq has no event pages: to make it blueprintz-visible grok's verbs would need pages calling a registered `game_dev_hq_verb`. That is grok's to do; I did not touch it.
- The copy still carries cdda-hq's rules (one zed, wounds by body part, craft bandage, cellar). They are placeholders to be replaced by the shooter rules; the wound table by body part is the planned hit-location table.
