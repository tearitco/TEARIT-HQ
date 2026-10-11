# dsr-sim - DSR step 1: game STATE plus a deterministic headless economy (template/scratch files, not a live game)

Built 2026-10-07 in alpha (build order step 1 of `DSR-SIMULATION-DESIGN.md`, lane "dsr-test"). **Nothing here is installed anywhere: never point it at live user data (`xyzfs/users`); the harness works on `/tmp` scratch roots only.** The real `dsrtest_*` entities carry no game state yet; this builds the state and the tick, and generates scratch entities with the same shape.

## Files
| Path | What |
|---|---|
| `scenario.pdl` | SETUP rows (civs, seed, per-civ seat/pop/free buildings) and TUNE rows (WSR constants, DSR starting numbers). Defaults = the dsr-test set: per civ 1 castle, 2 banks, 4 stores, 1 population (16 entities). Last row wins, so a scenario can append an override |
| `ops/dsr_scenario_gen.c` | `dsr_scenario_gen <scenario.pdl> <game_root> [--seed N]`: writes `pals/dsrtest_*` (pal.pdl, meta.pdl, desktop_pos.txt, glyph.txt, instance_id.txt `DT<n>`, livedesk_index.txt, `variables.txt`), `sessions/s1/desks/dsr-test.pdl`, `dsr_world/` (variables, tunables, scenario copy, append-only `world_ledger.txt`). Never overwrites; refuses hotel/school (later steps) before writing anything |
| `ops/dsr_day_tick.c` | one game day, callable as an `lc_clock` event runner. Header = the order of the day, the money invariant, and which WSR formula each step is |
| `ops/dsr_sim_query_op.c` | read-only independent checks (conserved total, history row counts, distinct values, position overlaps ...) used by the harness |
| `common_events/dsr_day_tick/event_pkg/` | the common event package: `target.pdl` (`TARGET | state | dsr_game`) and `event.pdl` (the runner) |
| `schedule.pdl` | the schedule row, human readable (the real row lives on the clock) |
| `build_dsr_sim.sh` | builds the three ops into `ops/+x/` (`-ffp-contract=off`) |

## State (RPG Maker variables: `variables.txt`, `key=int`, money in whole cents)
store `cash price share_price shares stock wage_bill loan_balance loan_bank good_id last_sold last_offered offered_today sold_today rev_today produced_today interest_today tax_today`; bank `reserves loans_out rate(ppm/day) interest_in`; castle `treasury tax_rate(bp/day) tax_in`; population `count cash avg_wage unemployment(permille) food_supply wages_today`; world `day seed civs sink`. History (append-only, one `DAY|n|...` row per entity per day): store `cash|price|share_price|stock|sold|wage_bill|loan_balance`, bank `reserves|loans_out|rate|interest_in`, castle `treasury|tax_rate|tax_in`, population `count|cash|avg_wage|unemployment`; world ledger `MARKET|n|good|fills|units|last_price`, `DAY|n|total|sink|loans_out|loans_owed|wages|tax|interest`.

## Install note (by hand, in a scratch house first)
1. `sh build_dsr_sim.sh`
2. `ops/+x/dsr_scenario_gen.+x scenario.pdl <house>/dsr_game [--seed N]`
3. copy `common_events/dsr_day_tick` to `<house>/common_events/dsr_day_tick`
4. clock: `lc_clock <house> new g1`, then `lc_clock <house> reminder-add g1 86400000 common:dsr_day_tick "" every:1day`; `LC_CLOCK_NO_POPUP=1 LC_CLOCK_EVENT_RUNNER=<ops>/+x/dsr_day_tick.+x lc_clock <house> cmd g1 advance 30d` then `... step 0` (or the daemon). One schedule firing = one day; the schedule ledger is the cursor.
5. check: `ops/+x/dsr_sim_query_op.+x <house>/dsr_game total` (the `total=` value never changes).
Harness: `&.widgits/_shared-lib/harness/dsr_sim_step1.pal`.

## What is conserved
`sum(store cash + bank reserves + castle treasury + population cash) + world sink`, every day, exactly (integer cents); debt (`sum store loan_balance == sum bank loans_out`) separately. Production cost goes to the civ's population as wages (`production_wage_pct`, default 100, a DSR addition) and the rest to `sink` (WSR lets that cost vanish; with `production_wage_pct=0` households drain as WSR itself measured).

## Gaps (what the house cannot express yet)
See the Build report in `DSR-SIMULATION-DESIGN.md`. Short: no registered command does price/auction arithmetic, a `variable >= n` test, or runs a compiled op from a common event; the house default runner `play_event.sh` cannot run `dsr_day_tick` (use the `LC_CLOCK_EVENT_RUNNER` hook); the tick is ONE world event over all entities, not a per-entity page driven by a per-entity cursor.
