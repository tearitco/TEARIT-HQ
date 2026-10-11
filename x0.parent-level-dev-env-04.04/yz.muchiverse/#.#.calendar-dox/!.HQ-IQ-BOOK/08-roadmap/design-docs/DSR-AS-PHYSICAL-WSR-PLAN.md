# DSR as a physical X11 phymoji version of WSR_PREFERED: running it with events, play, and player save/load slots (plan)

Status: PLAN, 2026-10-07, claude. Owner: "I want to run that with events, play, player save/load slots, trying to make a physical X11 phymoji version of WSR_PREFERED." Builds on `DSR-ENTITY-GAME-DESIGN.md`, `DSR-SIMULATION-DESIGN.md`, `RPGMAKER-PRIMITIVES-FOR-EVERYTHING-DESIGN.md`, `CLOCK-AS-THE-PLAY-SPINE-DESIGN.md`, `GAME-SETUP-PDL-DESIGN.md`. Lane: claude's focus is `dsr-test` (owner, 2026-10-07).

## 1. What "physical phymoji WSR" means here
WSR_PREFERED is a **menu-driven terminal game** (`pieces/chtpm/layouts/`: `wsr_main_menu`, `wsr_trade_menu`, `wsr_financing_menu`, `wsr_management_menu`, `wsr_derivatives_menu`, `wsr_search_menu`; **53 ops** in `WSR_PAL-PREFERED/ops`: `corp_*`, `bank_loan_op`, `pop_update`, `realestate_*`, `goods_*`, `market_*`, `gov_*`, `tax_loop`, `weather_update`, `wsr_news_op`, `day_loop`, `econ_calendar`, and more). The "physical" version is the **same game rules**, but **every actor is an entity on the desk (a phymoji window)** and **every menu action is an entity menu row that fires an event**: you right-click a store and act; you do not navigate a text menu. `dsr-test` already has the entities (2 castles, 4 banks, 8 stores, 2 populations); what is missing is the **game state, the events, and the play loop**.

## 2. Mapping WSR menus onto entity menus (read from the layout names and the op list; I have not read every layout)
| WSR menu / op family | physical form |
|---|---|
| Buy/Sell, `corp_trade`, `player_trade`, `corp_buy_stake`, `corp_ipo` | **store/castle entity menu**: Buy shares, Sell shares, IPO |
| Financing, `bank_loan_op` | **bank entity menu**: Borrow, Repay, loan terms |
| Management, `corp_payroll`, `corp_set_owner`, `corp_decide`, `corp_apply_finances` | **store menu**: Hire/pay, Set price, Marketing, R&D budget (human decision rows) |
| Derivatives, `player_open_derivative`, `player_settle_futures` | later (bank/exchange entity) |
| DB Search, `display_search_results` | the **auction/search screen** or a search entity |
| `realestate_*` | hotel/real-estate entity menu (new kinds) |
| `gov_*`, `tax_loop` | **castle menu**: set tax, treasury |
| `pop_update`, `weather_update`, `day_loop`, `econ_calendar`, `*_tick_idle` | **the day tick** (clock-driven events), no menu |
| `wsr_news_op` | news ticker / entity message (phone) |
The WSR ops take a piece directory: the plan is the **entity-dir adaptation** already named in the earlier designs (ops take an entity dir argument and read/write its variables), with the pure price/pop formulas shared by text-include (no header+link).

## 3. The play loop (the part the owner named)
1. **Play start** (taskbar `1.play`, `khtpm_play_mode.state.txt` `mode=on`): validate the scenario (`game.pdl` `SETUP`), seed/refresh the dsr-test entities, **start the main clock** (`running=1`), publish the clock in the taskbar/pc-hq. (Setup menu = `DSR-SIMULATION-DESIGN.md` section 8.)
2. **Events**: the clock's schedule rows fire common events: `dsr_day_tick` (repricing, wages, rent, taxes, interest), monthly rent/earnings, yearly school/election; entity menu rows run event pages that call the ops. Everything is a registered command or a small op behind a TEMPLATE.
3. **Player actions** are menu rows on entities (possession of the player body as in the existing play modes); a refused action (not enough cash, map not available) is a ledger row and a visible message, not a silent no-op.
4. **Player save/load slots** (taskbar "9 player" -> save-game / load-game, 16 slots): **today these are `game_slot_op` v1: a hash manifest only; load restores nothing** (read in its header). So "run DSR with save/load slots" **requires slots to become real**: save = a checkpoint in the snapshot store (`game_snapshot_op`, built in alpha, 118 harness checks) **including the main clock, schedule ledger and game variables**; load = restore it. **Restoring running entity windows is the unsolved step** (`game_snapshot_op` restores files into a destination; quiescing the running entities, restoring and reloading them is not built): see section 5.
5. **Rewind / play-test:** the same checkpoints; play-test mode takes automatic ones (`CLOCK-AS-THE-PLAY-SPINE-DESIGN.md` section 7).

## 4. What exists vs. what is missing (checked or stated)
Exists: dsr-test entities; entity menus (`meta.pdl` METHOD rows); clock with recurrence, catch-up, commands, chaining, restore check (alpha, harnesses passing); snapshot store (alpha); play modes and the `game.pdl` map-access check; WSR ops (the rules). **In progress in alpha:** digipet hunger loop, DSR step 1 (state + 30-day run).
Missing: entity-dir versions of the WSR ops; DSR game state in the live entities; menu rows wiring WSR actions; game_slot_op **v2** (real save/load using the snapshot store + clock files); quiesce/restore/reload of running entity windows; the clock shown in the taskbar/HUD from the master clock; a player body on the dsr-test page (possession) and the tb Player menu entries as live checks.

## 5. The hard problem: restoring a live game
Saving is easy (copy files by hash). **Loading while entity windows run** is the risk: windows hold in-memory state and poll files. Options (smallest first): (a) **stop and relaunch the game's entities** around a restore (the `button.sh`-style quit/reset path already kills the world manager; the entities relaunch from the restored files); (b) a **reload event** each entity handles (re-read its state files, via the existing marker-size-growth rule), no relaunch; (c) a **quiesced load**: pause the clock (`pause`), signal entities to idle, restore files, signal reload, resume. Recommendation: build (c) on top of the clock `pause` command and a `reload` marker, falling back to (a). All rehearsed in alpha/beta against scratch entities first; **live user data is never touched until the owner approves the restore design** (data rules, AGENTS.md).

## 6. Build order (harness first, alpha, claude's lane)
1. **DSR step 1** (state + 30-day deterministic run): *in progress*.
2. **WSR ops as entity-dir ops** for the first loop: price update, wages, taxes, loan interest (reuse WSR formulas), plus harness.
3. **Entity menu rows + event pages** for buy/sell/borrow/set price on dsr-test entities (template files, then `desk_copy_op`-style install into dsr-test only after rehearsal in beta with a backup).
4. **game_slot_op v2**: slots = snapshot checkpoints including clock files and game variables (new op or a flag; the live taskbar call keeps working; v1 behavior preserved for existing slots).
5. **Quiesced load** (pause, reload marker, restore, resume) with a pal harness that proves state equality after load and no double-applied ticks (the clock restore check already exists).
6. **Play start/stop wiring**: start the clock with play, show the master clock in the taskbar/HUD, play-test auto checkpoints.
7. **Live verification** through the relay on a beta tree copy of the owner's data: window opens, menu row acts, save, load, state matches; this is the "physical" proof.
8. Then growth, hotels/maids, schools, politics, war, per the DSR designs.

## 7. Open questions
1. Is the first playable target **DSR as WSR's three-menu core** (trade, financing, management) or the whole 53-op surface? (Recommended: the three-menu core.)
2. Load behavior: **stop-and-relaunch entities** (simple) or **quiesced reload** (smoother)? (Recommended: quiesced, falling back to relaunch.)
3. Does a loaded game replace the dsr-test live entity files, or run on a **separate copy per save slot**? (Recommended: restore into the same dsr-test dir, since slots are per user.)
4. One human seat first (a single player body), with AI civ B?
