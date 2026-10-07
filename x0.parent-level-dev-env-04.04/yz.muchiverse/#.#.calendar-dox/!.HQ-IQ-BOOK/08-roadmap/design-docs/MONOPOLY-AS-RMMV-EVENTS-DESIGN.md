# A full playable ring-board game from RPG Maker events only (KISS reference game)

> **UPDATE 2026-10-07 (owner): avoid the Monopoly name and branding (copyright/trademark).** This engine is now the **neutral ring-board core**; the reference game becomes the owner's own **Footrace Fu / Time-Fu: Buffet** (see `OWN-GAMES-REFERENCE-SUITE-DESIGN.md`). The mechanics below (dice, loop board, buy, rent, bankruptcy) are generic and stay; board data/names are data, so the Time-Fu Buffet board replaces them. Read 'Monopoly' below as 'ring board game'. The file keeps its name only so earlier links still work.

Status: DESIGN + first-build spec, 2026-10-07, claude. Owner: "I think we should get a full 'monopoly' game be playable with the player, using just RMMV events, and even creating the 'layout studio' if needed. Then we will have a full playable example of a game like that. Do you understand? KISS."
Understood as: **one small, complete, boring-on-purpose game** that proves the house can express a whole game with **events + variables + switches + pages + common events + menus + save/load + the clock**, so every later game (DSR included) is a variation, not an invention. It is a **reference example**, not the DSR economy.

## 1. What exists (read, not assumed)
- `TEST-GAMES-ROADMAP.md`: Monopoly is item 11, "board-game movement + property/rent... **not started**, leans on grid-movement + economy once they exist" (it is already on the roadmap as mostly content).
- RPG Maker primitives registered in `#.ref/menu/event_commands.registry.pdl` (variables, switches, `if/else/loop`, `change_gold`, `call_common_event`, `transfer_player`, `shop_processing`, text/choices); the digipet build (alpha, 111 checks) added `change_variable` and `event_page_op`, which runs event pages with **variable/switch conditions** (RPG Maker page conditions).
- Clock with schedules, End Turn, snapshots and restore (alpha); play modes and `game_slot_op`; token/sprite board viewing exists in pc-hq's board viewer.
- `@.apps/deskopoly=FFU/deskopoly=ffu.txt`: an owner note (not a design): "can hai play monopoly... each agent acts independently"; AI play is therefore a later seat type, not v0.
- `HQ-LAYOUT-STUDIO-DESIGN.md`: a **seed doc only (proposed 2026-09-29, not built)**: a visual widget-layout editor. Treated here as **"only if needed"**: v0 uses an existing layout file for the board window.
- **Gaps found by the digipet build** that Monopoly hits too: the registry `if` tests a switch only; prisc has no variable compare or arithmetic beyond a literal add; no random-number command; `play_event.sh` double-runs common-event pages; a common event cannot act on a caller. So v0 needs a few **tiny single-verb ops** (house rule: one rule, one tiny op) rather than pretending the registry covers them.

## 2. KISS scope (v0 = playable, v1 = fuller; no more)
**v0 rules (the "toy Monopoly"):** 40 board spaces in a ring; **2 to 4 human hot-seat players** (a seat can be dormant); start cash 1500; **roll 2d6** (seeded, reproducible), move forward, collect 200 passing/landing on GO; landing on an **unowned property** offers *Buy / Pass* (no auctions); landing on an **owned property** pays **fixed rent** to the owner (rent from a table, no houses/hotels); **tax** spaces pay the bank; **Go To Jail** space and a simple **Jail** (skip 1 turn or pay 50); **Free Parking** does nothing; Chance/Community Chest are **not in v0**; **bankruptcy** = cash < 0 with nothing to sell -> player out, properties return to the bank; **game ends** when one player remains (or after N turns for the harness). One screen: the board, tokens, a turn indicator, a log.
**v1 (only after v0 is green):** color-set monopolies (double rent), houses/hotels, mortgage, trading, Chance/Community Chest cards, auctions, AI seats (a Gemma-picked/weighted FSM seat per `deskopoly=ffu.txt`), the clock as turn timer.
**Explicitly not now:** online play, rating/pot (the sessions/economy docs), custom art beyond emoji.

## 3. Everything as events (the mapping)
- **Board data** = `board.pdl`: `TILE | n | kind=go|prop|tax|jail|gotojail|parking | name | price | rent | group`. A tile's behavior is a **page** chosen by its kind.
- **State** = variables: per player `pos_<p>`, `cash_<p>`, `jail_<p>`, `alive_<p>`; per property `owner_<n>`; global `turn`, `current`, `dice1`, `dice2`; switches `game_over`, `awaiting_buy`.
- **Turn loop** = a **Turn** common event: *Roll* (random op, seeded) -> *Move* (position = (pos + roll) mod 40, passing GO pays) -> **Land** (page by tile kind: unowned property -> set `awaiting_buy` and show the choice; owned -> **Pay rent** via a transfer op; tax; jail) -> *End turn* (next alive player, `turn += 1`; also the clock's **End Turn** mailbox row).
- **Player input** = the choices **Roll / Buy / Pass / End Turn / Pay jail** are **entity menu rows or choice dialogs** (`show_choices`) on the player token entity; the pages are the same ones an AI seat will later fire.
- **Save/load** = the snapshot store: variables, tile owners, clock epoch and the **seed position (dice counter)** all restore; after load, the next roll equals the roll the uninterrupted game would have produced.
- **Tiny ops needed (each compiled, one verb, harness-tested):** `dice_roll <seed> <counter>` (deterministic, appends the counter), `var_math` (add/sub/mod/compare of two variables into a third), `cash_transfer <from> <to> <amount>` (refuses overdraft or returns bankrupt), `tile_lookup <n> <field>`. They are the **missing registry commands**, added as registry rows after the digipet agent's work is merged in alpha.

## 4. The window ("layout studio only if needed")
A **board window** (X11 phymoji style, one `.xhtpm` + manager, per the house window standard) draws 40 tiles as a ring of emoji cells, tokens as emoji sprites, owner colors, current-player highlight, a dice readout and a message log, read-only from the state files; the manager publishes them (the standard projector pattern). **If the existing layout/grid primitives cannot draw a 40-cell ring, that is the trigger to build the layout studio** (a minimal one, in its own design pass); do not build it before then. The headless engine and harness come first and do not depend on the window.

## 5. Build order (harness first, alpha, claude's lane)
1. **Headless core:** `board.pdl`, state variables, the four tiny ops, the Turn/Land common events as pages run by `event_page_op`, a **scripted full game** in a pal harness: seeded dice, 2-4 players run to **game over** (bankruptcy), checks: determinism (same seed = same winner/log), **cash conservation** (bank + players' cash + purchases accounted exactly), no negative cash except the bankruptcy exit, positions always 0..39, ownership consistent, GO passed pays 200, jail rule, save at turn K / restore / continue equals the uninterrupted run, plus a case shown able to fail.
2. **Player path:** play start (`ticker`/clock hook gap from digipet), entity menu rows/choices for Roll/Buy/Pass/End, driven in the harness through the same METHOD action strings the menu fires.
3. **Board window** (read-only projection) and a relay-driven live check on a **beta copy** (never the owner's live desk).
4. v1 items one at a time; AI seat last (a weighted FSM; the models never decide, per the attrition law).

## 6. Open questions
1. v0 as above (2-4 humans, fixed rent, no cards), or do you want Chance/Community Chest in v0?
2. Board theme: classic property names, or the DSR/house theme (castles, banks, stores as tiles)? (Recommended: house theme, since the tiles can then be the real `dsrtest_*` entities in v1.)
3. Should the **same engine** later run DSR turns (a property = a store, rent = revenue), so Monopoly is literally the first DSR skin?
