# Everything starts through entities and the player, built from RPG Maker primitives (events)

Status: DESIGN + first-build spec, 2026-10-07, claude. Owner: "all of it should start through entities and player (play / load + save etc) and use RPG Maker primitives" ... "(events)".
This is the rule for the needs, clock, DSR, sandbox, survival and economy work already designed: **no private mechanic; every rule is an RPG Maker-style event built from the primitives the house already has.**

## 1. The primitives already in the house (read: `#.ref/menu/event_commands.registry.pdl`, 68 registered commands)
| RPG Maker primitive | house form (exists) |
|---|---|
| **Switches** (ON/OFF) | `control_switch` -> `{STATE_DIR}/switches.txt` |
| **Variables** (ints) | `control_variable` -> `{STATE_DIR}/variables.txt` |
| **Self-switches** (per event) | `control_self_switch` |
| **Control flow** | `if / else / end / loop / break_loop / label / jump_to_label / exit_event / wait / comment` |
| **Common events** | `call_common_event` (-> `call_event_op`: runs the page whose `condition.pdl` trigger matches) |
| **Actor parameters** | `change_hp / change_mp / change_tp / change_exp / change_level / change_parameter / recover_all` -> `{STATE_DIR}/actor_<id>_stats.txt` |
| **States** | `change_state` (e.g. Hungry, Sleepy) |
| **Skills / class / equipment / name** | `change_skill / change_class / change_equipment / change_name / change_nickname / change_profile` |
| **Items, gold, shops** | `change_items / change_weapons / change_armors / change_gold / take_gold / shop_processing` |
| **Timer** | `control_timer` |
| **Movement and maps** | `transfer_player`, `set_move_route`, `scroll_map`, `followers` |
| **Messages and choices** | `show_text`, `show_choices`, `input_number`, `select_item`, `scrolling_text` |
| **Menu/save access gates** | `change_menu_access`, `change_save_access`, `change_encounter` |
| **AI / phone hooks (house additions)** | `ai_describe`, `ai_chat`, `phone_send`, `advance_fact`, `apply_range` |
Event **pages** (`event_pkg/pages/page_N/{event.pal, condition.pdl}`) pick the active page by condition and trigger; common events are pages called by name. The map/page/desk layer is `game.pdl`, `mr_transfer_desk`, and **save/load slots** are `game_slot_op`. RPG Maker params (`mhp, mmp, atk, def, mat, mdf, agi, luk, level, exp`) are the stat vocabulary (strength ~ atk, intellect ~ mat as bank aliases).
**Gaps** (no registered command found for these; verify before building): a **per-place day-tick source**, a `feed`/`drink` verb, price lookup for `shop_processing`, per-entity needs variables (hunger/thirst/energy as named variables), and the tick **cursor** read ("apply the ticks I have not seen").

## 2. The rule: entity first, player first
- **Entity-first:** a need, a job, a rule lives on or is called by an **entity's event pages** (a pet, a citizen, a store, a planet body). The entity's own `variables.txt`/`switches.txt`/`actor_*_stats.txt` hold its numbers; shared rules are common events many entities call. No world-global mechanic without an entity (or page) that owns it.
- **Player-first:** every feature must be reachable and testable through the **player path**: *start* a game page (play), *act* (menus and events through the player's possession), *save* and *load* (`game_slot_op` slots), and *resume*. A feature that cannot survive save/load is not done.
- **Same primitives for agents:** an AI entity or phone uses the same commands (the existing law: a model names a command, deterministic code runs it).

## 3. How the earlier designs map onto events (so nothing is a special system)
- **Hunger/thirst/energy:** variables `hunger`, `thirst`, `energy` per entity; **Day Tick** common event (subscribed to the place clock) does `control_variable hunger += 1`, `if hunger > T -> change_state Hungry`, `if hunger > T2 -> change_hp -x` (death off = HP floor via a switch). **Feed** = event page: `select_item food` -> `change_items -1` -> `control_variable hunger -= nutrition`. The critical-need gate and seed reward table (digipet M1/M2) are `if` pages and variable values.
- **Clock:** a **tick row** in the place's ledger; a clock op (the one new compiled piece) writes `TICK | place | n | source=auto|turn` and each entity's **Day Tick** page runs once per unseen tick via cursor.
- **School:** class pages, `change_skill`/`change_class`/`change_level` on a pass, report card derived from the variables; attendance = `transfer_player` in and out.
- **DSR/economy:** stores = events with `shop_processing`, `change_gold`; wages and rent = common events on the day tick; elections = a common event run on the election cycle.
- **Planet/solar sandbox:** `transfer_player` between pages; physics nodes feed variables (gravity into a `gravity` variable) that move/jump pages read.
- **Survival gathering:** an interact event + `wait` + `change_items`.
- **Quests:** switches/variables + `call_common_event` for the check; rewards via `change_gold` or the chain escrow (`phone_send`/op).
So the **new compiled pieces are only**: the clock/tick op, the cursor read, and thin ops where a command is missing (feed/price); everything else is **event data using registered commands**.

## 4. First build (this step): one entity with a hunger loop, playable, savable, loadable
Scope (alpha branch, harness first, no GUI claims):
1. Pick **one pet pal** as the subject (a scratch copy of an existing pal; never live user data).
2. **Variables:** `hunger` (0..), `coins`; **switch:** `death_enabled` (OFF); **state:** Hungry.
3. A **clock op** `place_clock_tick` appending `TICK` rows to a per-place ledger (auto and manual-turn variants, same row type).
4. A **Day Tick common event page** (RPG Maker-style) that applies unseen ticks by cursor: hunger up, state change, HP loss floored while death is off.
5. A **Feed event page** (menu entry on the entity: `Feed`): consumes an item, lowers hunger, spends coins from a food price list (`apple 10`).
6. **Play/save/load:** start through the player path, then save with `game_slot_op`, change state, load, and prove the variables, ticks cursor and states restored exactly; **ticks not double-applied after load** (cursor correctness).
7. **Harness (pal):** hunger rises per tick; threshold sets Hungry; HP floor with death off; feed lowers hunger and coins; replay from ledger = same state; save/load round-trip; cursor idempotent; plus a case shown to fail. Result rows are Watch-style so TEARIT can score them later.

## 5. Open questions
1. Subject entity for step 1: a terumon pet, a dsrtest citizen, or a fresh minimal "digipet" entity (recommended: fresh minimal, in alpha only)?
2. Tick source for the first build: manual End Turn only, or also an auto timer?
3. Where do `variables.txt`/`switches.txt` live for an entity (the registry's `{STATE_DIR}`): per-entity folder or per-page? (to be read in the code first.)
