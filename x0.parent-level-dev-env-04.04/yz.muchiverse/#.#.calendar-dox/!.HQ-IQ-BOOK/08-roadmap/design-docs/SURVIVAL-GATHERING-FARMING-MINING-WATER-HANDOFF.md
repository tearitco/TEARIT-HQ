# Handoff for Grok: gathering, farming, mining and water (Cataclysm/Minecraft style)

Status: HANDOFF/SPEC written 2026-10-07 by claude for the Grok lane ("which we will have Grok do soon"). Nothing here is built. Read the related docs before starting; do not improvise a parallel mechanic.
Related: `ENTITY-NEEDS-AND-CARE-DESIGN.md` (the clock and the needs this feeds), `MUTACLYSM-AS-EVENTS-DESIGN.md` (existing CDDA-style mechanics to reuse), `DSR-SIMULATION-DESIGN.md` section 10 (supply and demand), `PLAY-ECONOMY-POT-FAUCETS-DESIGN.md` (rewards), `PRISC-OPS-ARCHITECTURE.md`, NIGHT 30 (the survival-loop shape).

## 1. The shape (NIGHT 30, restated)
Planting, mining, chopping, gathering water are **all the same shape**: **interact with something, wait some time, receive a yield.** Build that **once**, generically, and the mechanics fall out of it:
- a **target** (a field plot, a tree, an ore node, a water source) with a `kind`, a `yield` (item + amount), a `duration` (in ticks of the place clock), an optional `tool` requirement and a `regrow`/`depletes` rule;
- an **interact** event starts it (a `work` row with a start tick), the **place clock** advances it, a **complete** event writes the yield into the actor's inventory (an append-only ledger row, `GATHER | actor | target | item | amount | tick`);
- everything is **event data + registered commands**, no hardcoded mechanic (NIGHT 20/26).
The per-entity timer NIGHT 30 says is missing is **not per entity**: it is **a row with a start tick** read against the **place's day clock** (`ENTITY-NEEDS-AND-CARE-DESIGN.md` section 3). That is the dependency to build first.

## 2. Deliverables in order (each: a pal harness first; develop in alpha, rehearse data in beta)
1. **The generic yield timer** (ledger rows + `work_start` / `work_complete` commands + harness: yields exactly once, resumes after restart, respects duration and tool).
2. **Gathering water** (water source target, container item, yields `water`; feeds the new thirst need).
3. **Farming** (plot: plant -> grows N days with water/season rules -> harvest yields food; spoilage optional later).
4. **Mining and chopping** (node with tool tiers; depleting or regrowing).
5. **Recipes/crafting as event data** (reuse mutaclysm's `recipes.txt` content, write it as `.pdl`; one `craft` command shared with shops).
6. **Item properties as data:** nutrition, hydration, warmth (clothes), tool tier, stack size.
7. **Hand the goods to the economy:** gathered food/water/clothes become **commodities** in DSR's supply/demand (producers = farms/wells/tailors as stores; a gather yield is a production row).

## 3. Interfaces you must use (do not reinvent)
- **Clock:** the place day/phase tick ledger (read by cursor). **Inventory:** an entity's `inventory/` folder items (phone pattern: one folder per item). **Commands:** the event registry (`#.ref/menu/event_commands.registry.pdl`: `COMMAND ... PARAMS ... TEMPLATE exec ...`). **Needs:** write results through `feed` / `drink` / `clean` commands only. **Identity:** items/entities get ids by the house rules (no copied hashes). **Seed:** every random choice takes the game seed so runs are reproducible.
- **House rules that bit others:** ops are compiled C, self-contained, **no shared headers**; pure shared logic is a text-included `.c`; stateful shared logic is a separate op; append-only ledgers, cursor reads, never mtime; **no shell loops** driving mechanics; prisc `exec` ignores exit status (write result rows); a harness must be able to fail; never touch `xyzfs/users` data directly (rehearse on a copy in beta, backup first); never edit `khtpm_core_render.c` for a game; commit only your own paths on your own branch.
- **Lane split** (AGENTS.md): do not edit `ops/` worker/fetch files or `board-viewer/ops/bv_menu_input.c`; if a shared file must change, hand it to the owner/manager.

## 4. Mutaclysm tie-in
Read `MUTACLYSM-AS-EVENTS-DESIGN.md` first: its crafting, eating/drinking, pickup/drop, monster tick and map generation are the **content** this lane should absorb as events, not rebuild. Check with the owner before retiring any of its ops, and never run against a live mutaclysm session.

## 5. Acceptance (what the owner should be able to see)
A scratch page where an entity walks to a well, gathers water (time passes on the clock), plants and harvests a crop, mines an ore with a tool, crafts an item from a recipe, eats and drinks (needs go up), all as **ledger rows** you can replay, with a pal harness that passes and is shown able to fail.

## 6. Open questions (for the owner, before or during the build)
1. Season/weather effects on farming from the start, or later?
2. Water: a finite source (depletes, refills) or infinite wells first?
3. Do gathered goods go straight to the actor's inventory only, or also to a shared stockpile (a store's stock)?
4. Tool durability in v1?
