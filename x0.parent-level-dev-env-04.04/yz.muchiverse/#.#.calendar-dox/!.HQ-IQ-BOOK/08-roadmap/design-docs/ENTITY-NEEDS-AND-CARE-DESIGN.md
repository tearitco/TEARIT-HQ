# Entity needs and care (hunger, hygiene, sleep, health, weight): design

Status: DESIGN ONLY, written 2026-10-07 by claude. Nothing here is built.
Owner brief (2026-10-07): "the entities will get hungry daily and have to be fed, cleaned, sleep etc, in the future; that will happen accelerated as their days tick by in school, and bad hygiene will contribute to a variable. If these entities pass time without eating (from their inventory / cafeteria / farming etc.) you can tell because their health will get low and they will lose weight. That's all for now. Dying can be turned on/off along with these other things, which should all be events based asap, and have synonyms, weights, the whole nine yards."

Related: `ENTITY-SCHOOL-YEARS-DESIGN.md` (school clock, section 7), `ENTITY-WORD-BANK-DESIGN.md` (aliases, behavior + variables), `HARNESS-BEHAVIOR-BANK-DESIGN.md`, NIGHT 30 (survival-loop shape, per-entity clock gap), NIGHT 22 (concept nodes, GOAP preconditions/effects with concept pointers), `GENESIS_TXT_DESIGN.md`, `PIECECRAFT_XYZ_DESIGN.md`.

## 1. What exists (read, not assumed)
- **Pets already carry need-like state**: the a-z-pets harness state files hold `hp`, `hp_max`, `mp`, `hunger`, `energy`, `stamina` (`0.a-z-pets-plan/proof/.../after_state.txt`, e.g. `hunger=78 energy=100 hp=20`). Which direction `hunger` moves (full vs empty) is **not documented** in the plan files and one before/after pair (76 -> 78) does not settle it: verify before reusing the key.
- **Genesis-txt design**: a daily `tick` "advance day, apply hunger/status": health decays (5 per day), eating gives `health += 5 x nutritional_value` from inventory grain; its own open question 4: "if health hits 0, is the player eliminated or just badly off?" (the owner's death toggle answers it).
- **Piececraft-xyz design**: `hunger`/`health` on hero state, "N ticks per hunger point" so it does not need input constantly.
- **NIGHT 30**: the survival loop (interact, wait, yield: planting, mining, chopping) does not exist, and the piece underneath it, **a clock for one character**, does not exist either. Needs run on days, so this design supplies the clock (section 3).
- **Terumon/concept bank**: `hunger` and `satiation` were used as illustrative concept nodes in NIGHT 22/26 but were never created as real master files (the real bank has only energy/force/motion).

## 2. State (private numbers, in the entity)
A small `needs.pdl` in the entity folder, next to its other private state (same private-numbers rules as the word bank/school record):
```
hunger       0..100    (define direction once and keep it: recommended 100 = full, falls with time)
hygiene      0..100    (100 = clean)
energy       0..100    (the pets files already have `energy`; sleep restores it)
health       0..health_max   (existing `hp`/`hp_max` names to be reused if hp means the same thing)
weight       number    (drifts with sustained hunger; bounded; display only)
sickness     0..100    (the "variable" bad hygiene feeds)
last_tick    <clock cursor>
```
Weights and rates are **tunables** (`needs_tunables.pdl`: decay per day, health loss per day at hunger 0, weight loss per starving day, hygiene effect on sickness), never hardcoded, same "named joints" rule as the server design.

## 3. Time: one clock per place, entities read it by cursor
A **day tick** is an **append-only ledger row** written by a clock owner: the **world clock** (game-time, saved with the game as decided earlier) or a **school clock** (`school.pdl: time_scale`, the accelerated one). An entity applies the ticks it has not yet seen, tracked by `last_tick` (cursor, append-only size growth, never mtime). This is the house's existing cursor-poll pattern (`move_entity_tick`, `world_manager.pal`). It means **no per-entity clock is needed**: the clock belongs to the place, and entities subscribe. Being in a school's inventory = following the school clock; otherwise the world clock. Entities taught only over the phone are not in a school and follow the world clock.

## 4. Events-based, as soon as possible
Mechanics are **event data**, not C: each need rule is an event page (`event.ir.pdl`, compiled by events-hq) that runs on the day tick; each action is a **registered event COMMAND** (names are working names): `needs_tick`, `feed <item>`, `clean`, `sleep <n>`, `entity_died`. A command is one small op behind a registry `TEMPLATE` (the registry already supports `PARAMS` and `{placeholders}`), so a game can edit the rules without recompiling, same RPG-Maker shape as everything else here. The tick driver is a pal loop calling ops (house law: no shell loops driving the game loop). Check first whether the registered `advance_fact` / `apply_range` commands can already express a decay/restore before adding any new op.

## 5. Rules (first version)
- **Daily decay** at each tick (scaled by the clock): hunger falls, hygiene falls, energy falls.
- **Feeding:** `feed` consumes a food item (an inventory item with a `nutrition` value, the genesis shape) and raises hunger. **Sources:** the entity's own inventory; a **cafeteria** (a food container in a school's inventory that its students may draw from, optionally automatic when hungry); **farming** later (needs NIGHT 30's interact-wait-yield timer).
- **Cleaning:** `clean` restores hygiene. **Low hygiene raises `sickness`** (the owner's "contributes to a variable") and multiplies health decay.
- **Sleep:** `sleep` restores energy over time; low energy slows other things (details open).
- **Health** falls when hunger is empty (and faster with high sickness / low energy). **Weight** falls while hunger stays empty, rises back slowly when fed, bounded. **"You can tell"**: health and weight are shown in the entity's menu/phone status and on the report card; low health is a visible state, not a hidden number.
- **Death is a switch.** If `death_enabled` is **off** (default), health floors at a minimum ("badly off", never zero) and the entity keeps going; if **on**, health reaching 0 fires the event `entity_died`. What death does (grave, ghost; see GRAVEYARD-GHOSTS-DESIGN.md) is separate and not decided here.
- **Toggles, all default OFF** so every existing entity and game behaves as today (the legacy-safe rule used for play modes): `needs_enabled`, `hunger_enabled`, `hygiene_enabled`, `sleep_enabled`, `death_enabled`, resolved most-local-wins: **entity > school > world**. Each is a plain `.pdl` row, so a game author turns them on per game, school or entity.

## 6. Banks, synonyms, weights ("the whole nine yards")
- **Behaviors with signatures and per-entity variables** (ENTITY-WORD-BANK-DESIGN 11): `action:feed` (variable: food item, amount), `action:clean`, `action:sleep` (hours), in a shared bank set (hidden layer) with aliases (feed, eat, give food, bathe, wash, nap, go to bed ...); the entity's own variables (what it likes to eat, how long it sleeps) are private.
- **Concept nodes** (hand-authored masters in a `care` corpus, not the physics ones): `hunger`, `hygiene`, `rest`, `health`; **GOAP preconditions/effects carry concept pointers** exactly as NIGHT 22 describes ("increases hunger"), so a planner can reason "I am hungry, food raises hunger".
- **A decision about when an AI/robot entity eats** is a **weighted choice among valid options** (tunable weights), never a model deciding (server design rule 3, NIGHT 26).
- **Words/status text** ("hungry", "starving", "dirty", "tired") are bank rows with weights, so chat and menus can talk about care using the same synonym machinery; their scores are hand-scored at creation like any entity word.
- **Learning:** whether an entity "got fed in time" is a Watch record with a valence, like any other (NIGHT 31); nothing here auto-promotes anything.

## 7. Tie to school and report cards
Care data feeds the **report card** as an informational line (attendance, average health, weight trend, hygiene), never a grade (a grade is counted from exams). An enrolled entity eats at the cafeteria and follows the school's accelerated clock.

## 8. Build order (each step: pal harness first; rehearse in beta; develop in alpha)
1. Day-tick ledger + `needs.pdl` core (text-include): apply N ticks, bounds, toggles (all off = no change), `entity_died` only when enabled; harness cases (decay, feed, clean, starve -> health down + weight down, death off floors health, death on fires event once, cursor applies each tick once, toggle precedence entity > school > world).
2. Register the commands and wire an event page that runs on the tick.
3. Scratch entity + scratch school in beta; show the state in a read-only window/tab.
4. Cafeteria container + auto-eat; bank rows/aliases/weights for the care behaviors; care concept masters.
5. Farming only after NIGHT 30's interact-wait-yield timer exists.

## 9. Open questions for the owner
1. `hunger` direction (100 = full recommended) and whether to reuse the pets' `hp`/`energy` keys as is.
2. Default decay numbers (per day) and the school scale; one day = how many ticks of the clock?
3. Weight: a number in kg-like units, or a relative index? Does it affect anything besides display?
4. On death (when enabled): grave/ghost, or just flagged?
5. Cafeteria: automatic feeding on, or a manual `feed` by the teacher/owner only?
6. Do robots/AI entities without a body get needs at all (phone-only learners)?

## Digipet comparison: what it changes here (2026-10-07)
Another agent compared the old digipet project against the house: `x0.parent-level-dev-env-04.04/#.ref/digipet-compairison/DIGIPET-VS-HOUSE-COMPARISON.md` (+ `.html`). I read the `.md`; I did not re-read the digipet source, so its LOC counts and "mock" findings are **its claims, unchecked by me**. What matters for this design:
- **Confirms the clock design.** The digipet's `watch.txt` is already an append-only tick ledger; its flaw is polling by modification time. Our per-place day-tick ledger with cursors is the same idea done safely. Keep both tick sources: **auto and manual End Turn write the same row type** with `source=auto|turn` (its M4), so stepped and real-time play share the rules (see `REAL-3D-PLANET-EXPLORATION-AND-REALTIME-MODE-DESIGN.md`).
- **Adopt first (small, tested in a toy):** M1 a **critical-need gate** (a visible threshold rule above weighted choice: starving overrides everything; the threshold is a tunable), M2 the toy's **reward table as seed weights** (eat when hungry +5, eat when full -2, rest while starving -4; real changes still go through candidate EDIT and review), M3 a readable **per-place `schedule.pdl`** (what runs each tick), M11 a **food price list plus coin purse** (feed/cafeteria), M12 **one need = one tiny op** (matches one registered command per rule).
- **Adopt later:** M5 traits (strength, speed) to RPG stats via bank aliases; M10 building-glyph interaction table for the dsrtest stores, banks, castles and a school; M6/M7 mood and pain once needs run; M8 breeding last. M9 (pointer sheets) is the hidden-layer pointer we already use.
- **Do not copy** (its section 6): mtime polling, `system()` loops with no timeout, whole-file state rewrites, relative `chdir` paths, counting mocks as features, another machine's absolute paths.
- **Its four owner questions, with my recommendation (not decisions):** (1) body sheets (genetics, brain, pain, chemistry) stay deferred until needs run; (2) one `schedule.pdl` per place is worth it, as the readable list that points at event pages; (3) yes, `turn` and `tick` as one row type with a `source` field; (4) its glyph interaction table is the same idea as the school entity plus a lookup, so merge them under the building-glyph table (M10).
