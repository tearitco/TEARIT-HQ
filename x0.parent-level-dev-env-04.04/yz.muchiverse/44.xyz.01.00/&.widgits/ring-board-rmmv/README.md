# ring-board-rmmv - a ring-board game made only of RPG Maker-style events (template files, not a live entity)

Built 2026-10-07 in alpha (step 1 of `RING-BOARD-AS-RMMV-EVENTS-DESIGN.md`, the file keeps its older name so links work). A neutral engine: **the board, the rules numbers and every name are data** (`entity/board.pdl`, `entity/rules.pdl`), so another loop board with another size, other tile names and other numbers replaces them with no code change. Nothing here is installed anywhere: **never copy it into live user data (`xyzfs/users`)**; the harness works on `/tmp` scratch roots only. Headless core only: no window, no layout studio, no AI seat.

## Files
| Path | What |
|---|---|
| `entity/board.pdl` | 40 tiles: `TILE \| n \| kind \| name \| price \| rent \| group`; kinds `go prop tax jail gotojail parking`; invented generic names (sheds, stores, banks, castles) |
| `entity/rules.pdl` | every number as `key=int`: start cash 1500, bank mint 100000, GO pay 200, jail fee 50, jail turns 1, jail tile 10, board size 40, tax_income 200, tax_luxury 100, turn cap 600, bot policy reserves |
| `entity/event_pkg/pages/page_N/` | the rules as event pages (`condition.pdl` = trigger + variable/switch conditions, `event.pal` = lines of `exec ops/+x/<op>.+x . ...`), `mode.pdl` = `sequential` |
| `entity/meta.pdl` | the token entity's menu rows (METHOD): New Game, Roll, Buy, Pass, End Turn, Pay Jail Fee |
| `entity/variables.txt`, `switches.txt` | seed state (`players`, `dice_seed`; `game_over`, `awaiting_buy`) |
| `common_events/end_turn/` | the clock hook: a schedule row `common:end_turn` forwards to the End Turn pages (`target.pdl` names the game entity) |
| `ops/*.c` | the five tiny compiled ops (one verb each, header = usage and exit codes): `dice_roll`, `var_math`, `cash_transfer`, `tile_lookup`, `var_sweep` |

Referee and harness (outside this folder): `&.widgits/_shared-lib/harness/ops/ring_audit_op.c`, `ring_board_core.pal`, `cases/ring_board_core.pdl`.

## State = variables (`key=int` lines in `variables.txt`, missing = 0)
Per seat `p` (1..4): `pos_p`, `cash_p`, `jail_p` (turns left to skip), `alive_p`. Per tile `owner_n` (0 = bank). Global: `players` (2..4, set by the setup row; seats above it stay dormant and are skipped), `turn`, `current`, `phase`, `dice_seed`, `dice_counter`, `dice1`, `dice2`, `roll`, `winner`, `game_over_reason`, `cash_bank`; switches `game_over`, `awaiting_buy`. `phase`: 0 = start of turn (Roll, or Pay Jail Fee / End Turn to skip when jailed), 1 = Buy/Pass choice open, 2 = turn actions done (End Turn); 3, 4, 5 are Roll-internal. Ledgers (append-only, no timestamps): `dice_ledger.txt`, `cash_ledger.txt`, `audit_ledger.txt`, errors in `op_errors.txt`.

## Rules (v0)
Roll 2d6 (seeded integer hash of `dice_seed` + `dice_counter`, so a restored game rolls the same next dice), move forward, passing **or landing on** GO pays `go_pay` from the bank. Unowned property: Buy (price to the bank) or Pass (no auctions). Property owned by another: fixed rent from the tile to the owner (no houses, no color bonus). Tax tile: `tax_<group>` to the bank. Go To Jail: token to the jail tile, `jail_turns` to skip; a jailed player pays `jail_fee` (then rolls) or ends the turn to skip. Free Parking, GO and Jail do nothing. **Bankruptcy** = a payment the payer cannot cover: `cash_transfer` moves everything the payer has to the creditor and says `bankrupt`; the Bankruptcy page marks the seat out and `var_sweep`s its `owner_*` back to 0. The game ends when one player is left (reason 1, winner = that seat) or `turn >= turn_cap` (reason 2, winner 0). No cards, houses, mortgages, auctions, trading or AI in v0.

## How a page calls an op, and the registry rows
Pages run with cwd = the entity folder (`event_page_op`), so an op is `exec ops/+x/var_math.+x . add cash_{current} cash_{current} 50` (state dir `.` first, then the verb arguments). An operand is an integer or `[file:]name` read as a variable (e.g. `rules.pdl:go_pay`); a name may hold `{var}` (`cash_{current}`, `owner_{cur_pos}`; no nesting). Page conditions are `COND | kv | <file> | <key> | <op> | <rhs>` rows (rhs an int or `@file:key`), so a compare is computed first (`var_math ge can_buy cur_cash tile_price`) and tested after. Registry rows (append-only, `#.ref/menu/event_commands.registry.pdl`): `variable_math`, `roll_dice`, `cash_transfer`, `tile_lookup`, `variable_sweep` (TEMPLATE form with `$ENT` as state dir).

## Player path
The menu rows in `entity/meta.pdl` are the same METHOD form as the digipet: `sh -c 'exec "$1/&.widgits/digipet/ops/+x/event_page_op.+x" "$0" "$1" --trigger roll'` with `$0` = the entity folder and `$1` = the house root; triggers `setup roll buy pass endturn payjail`. The bot seat used by the harness is the trigger `autoturn` (pages 70-76: pay jail when cash >= fee + `policy_jail_reserve`, Roll, buy when cash >= price + `policy_reserve`, else Pass, End Turn); it calls the same triggers by `exec ops/+x/event_page_op.+x`, so it needs `EVENT_PAGE_PRISC` in the environment. Manual End Turn through the clock works: `lc_clock <house> reminder-add <clock> <ms> common:end_turn "" every:1day`, runner `event_page_op` with `EVENT_PAGE_TRIGGER=endturn`, then `lc_clock <house> endturn <clock> 86400000` (the pages refuse it until the current player has finished: Roll and Buy/Pass).

## Install note (by hand, scratch house first)
1. build the ops: `cd ops && for o in dice_roll var_math cash_transfer tile_lookup var_sweep; do gcc -std=gnu11 -Wall -Wextra -O2 -o +x/$o.+x $o.c; done` (and `ring_audit_op` in the harness ops folder, and the digipet `event_page_op`; the `+x` programs are git-ignored).
2. copy `entity/` to a desk entity folder `<ent>`; make `<ent>/ops/+x/` and copy the five ops and `event_page_op.+x` into it (a pal `exec` cannot name the house root, so the pages find the ops relative to the entity).
3. write the state into `<ent>/variables.txt` (`players=2`, `dice_seed=<n>`), run the menu row New Game once, then play.
4. optional clock hook: copy `common_events/end_turn` to `<house>/common_events/end_turn` and put `TARGET | state | <ent>` in its `target.pdl`.

## Gaps found (what the house cannot express yet)
No variable compare / arithmetic in pages (hence `var_math`); no random number command (`dice_roll`); no money move with overdraft rules (`cash_transfer`); no board data lookup (`tile_lookup`); a page cannot loop over variable names (`var_sweep` for the bankruptcy return); a COND rhs is a literal or one kv value (no `a >= b + c`, so the bot sums into a scratch variable first); `{var}` templates do not nest; a pal `exec` cannot name the house root (ops are copied beside the entity) and hides the op's exit code (ops append `ERR` rows to `op_errors.txt`); `event_page_op` pages cannot call a common event by name (the bot uses nested `event_page_op` calls and needs `EVENT_PAGE_PRISC`); a page has exactly one trigger (the Land pages are one trigger `roll` with a `phase` variable); the registry editor shows only two fields per command; events-hq cannot open these pages (hand-written, no `event.ir.pdl`).
