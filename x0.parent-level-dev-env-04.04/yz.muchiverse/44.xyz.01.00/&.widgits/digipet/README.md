# digipet - the first NEEDS loop (template files, not a live entity)

Built 2026-10-07 in alpha (backlog item 4; spec `RPGMAKER-PRIMITIVES-FOR-EVERYTHING-DESIGN.md` sections 1-4, `ENTITY-NEEDS-AND-CARE-DESIGN.md` digipet M1/M2/M11/M12). Everything here is RPG Maker-style event data on the house clock; the one compiled piece is `ops/event_page_op.c`. Nothing here is installed anywhere: **never copy it into live user data (`xyzfs/users`)**; the harness works on `/tmp` scratch roots only.

## Files
| Path | What |
|---|---|
| `entity/` | the entity folder to copy: `meta.pdl` (METHOD rows `Feed`, `Status`), state files (`variables.txt` hunger/coins/days/last_feed/fed_total, `switches.txt` death_enabled=OFF/dead, `items.txt` item_apple, `actor_1_stats.txt` hp/mhp, `actor_states.txt` state_1:1 = Hungry), data (`needs_tunables.pdl`, `food_prices.pdl` = `apple=10`, `reward_table.pdl` = the seed M2 rows, data only, `states.pdl`), `event_pkg/pages/page_1..7` (Feed 1-6, Status 7) |
| `common_events/day_tick/` | the Day Tick common event: pages 1-5, `mode.pdl` (`sequential`) |
| `schedule.pdl` | what runs each tick (M3), human readable; the real row lives on the clock |
| `ops/event_page_op.c` | page runner with variable/switch page conditions (see its header) |

## Rules (all numbers in `needs_tunables.pdl`)
Direction: **hunger 0 = full and rises** (the design doc recommended 100 = full; flipped because the owner's spec says `hunger += 1` per tick and Feed lowers it).
Day Tick (trigger `parallel`): p1 `days += 1`, `hunger += hunger_step`; p2 hunger >= `hungry_at` sets state Hungry (critical-need gate, M1); p3 hunger >= `starving_at`, death OFF, HP would go below 1: HP = 1 (floor); p4 same with death ON: HP = 0 and switch `dead` = ON (the `entity_died` event is NOT built, only the switch); p5 hunger >= `starving_at`, HP >= `hp_loss`+1: HP -= `hp_loss`.
Feed (trigger `feed`): p1 reset result, p2 pantry empty and coins >= price: buy one (coins -= price from `food_prices.pdl`, `last_feed=2`), p3 pantry empty and coins < price: refuse (`last_feed=0`), p4/p5 eat (change_items -1, hunger -= `nutrition_apple`, floor 0, `fed_total += 1`), p6 clear Hungry once hunger < `hungry_at`. Status (trigger `status`) writes `status.txt`.
Page semantics: `mode.pdl` `sequential` = every page whose conditions hold at that moment runs in number order (a page sees the earlier page's changes); default `highest` = RPG Maker's single highest page.

## Install note (by hand, in a scratch house first)
1. `gcc -std=gnu11 -Wall -Wextra -O2 -o ops/+x/event_page_op.+x ops/event_page_op.c`
2. copy `entity/` to a desk entity folder `<ent>` and `common_events/day_tick` to `<house>/common_events/day_tick`; put `TARGET | state | <path of <ent>, absolute or relative to the house>` in `<house>/common_events/day_tick/event_pkg/target.pdl`.
3. clock: `lc_clock <house> reminder-add <clock> <first-day-ms> common:day_tick "" every:1day` (the schedule row; manual End Turn = `lc_clock <house> endturn <clock> 86400000`, timer = `lc_clock <house> cmd <clock> rate x86400`; both feed the same schedule ledger).
4. `LC_CLOCK_EVENT_RUNNER=<house>/&.widgits/digipet/ops/+x/event_page_op.+x EVENT_PAGE_TRIGGER=parallel` for the daemon. The house default runner `play_event.sh` cannot run these pages (see Gaps).

## Idioms (prisc has addi/beq/bne only, no register add/sub, no blt)
- add/subtract a tunable: load the amount into x2, load the variable into x12, loop `beq x2,x0,done; addi x12,x12,+-1; addi x2,x2,-1; j loop`, then store. Used by Day Tick p1/p5 and Feed p2/p5.
- conditions (>=, <, ==) are NOT in pal: they are `COND | kv | file | key | op | rhs` rows read by `event_page_op`.
- the pals use relative file names (run with cwd = the entity state dir) instead of the registry's `{STATE_DIR}` (events-hq bakes an absolute path at compile time).

## Gaps found (house cannot express yet)
`control_variable` only SETs (added registry row `change_variable`, add/subtract a literal); registry `if` tests a switch only (no variable compare, no `>=`); a page `condition.pdl` knows `trigger` only and `play_event.sh` ignores anything else; `play_event.sh` run on a common-event dir runs the page twice (entity block, then the all-common-events loop) and runs EVERY on-click common event, so `event=common:<x>` through lc_clock's default runner double-applies; a common event acts on its own dir, not on a caller entity (play_event.sh KNOWN LIMITATION), so `target.pdl` is new; no tunable-sized change command (needs the loop idiom); no `feed`/price-lookup command (done as pages + data); prisc exits 0 when it cannot open a program, so a failed page looks like success (event_page_op pre-checks the file only); events-hq cannot open these pages (no `event.ir.pdl`, hand-written); no `entity_died` event; no play-to-clock hook (play on = `ticker <clock> on` by hand).
Harness: `&.widgits/_shared-lib/harness/digipet_needs.pal`.
