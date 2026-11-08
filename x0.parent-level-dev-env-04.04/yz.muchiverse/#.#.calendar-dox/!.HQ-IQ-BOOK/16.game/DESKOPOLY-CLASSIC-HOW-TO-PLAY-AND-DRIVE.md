# Deskopoly classic: how to play it and how to drive it (state at milestone M3, 2026-11-08)
*Plain help for people and for the next AI. In-engine help: `deskopoly <app> <state> help` (all verbs, from `verbs.pdl`), `help <verb>`, and in a classic game it also prints the phase and what is legal right now. Full design: `08-roadmap/design-docs/DESIGN-DESKOPOLY-CLASSIC-MONOPOLY-BUILD-SPEC-2026-11-08.md`. Labels: **OBS** = I ran it; **PLAN** = in the spec, not built.*

## 1. What exists (OBS, 2026-11-08)
| Milestone | State | Proof |
|---|---|---|
| M0/M1 mini ruleset (the old 16-tile game, schema 1) | done | case `deskopoly` 105 checks |
| M2 classic data + lobby (`lobby join leave start order first`) | done | case `dsk_classic` 79 checks |
| M3 roll, landing, Start money, tax, rent (street, rail, util), buy, skip, end_turn, bankruptcy (to a player or the bank), over (winner or stalemate) | done | case `dsk_play` 48 checks |
| M4 doubles (roll again, third double to Jail), Go To Jail, the Jail turn (`jail_pay`, `jail_roll`; `jail_card` since M5), forced fine on the third failed turn | done | case `dsk_jail` 47 checks |
| M5 the Luck and Fund cards (seeded shuffle, all 11 effect kinds, the kept Get Out card and `jail_card <luck|fund>`, a card move onto a card tile draws once more, then stops) | done | case `dsk_cards` 42 checks |
| M6 buildings (even rule, 32 houses and 12 hotels as data in rules.pdl), mortgages (unmortgage costs +10 percent rounded up), `pay_debt`, net worth, bankruptcy that sells buildings and charges mortgage interest, repairs cards | done | case `dsk_build` 59 checks |
| M7 auctions · M8 debt queue · M9 trading and chat · M10 pets/ui/tick for all phases · M11 board page | **PLAN** | until then those tiles resolve as `todo-<kind>` (nothing happens) and `skip` just declines |

## 2. Run it by hand (no window needed)
```
OP=<house>/@.apps/deskopoly/ops/+x/deskopoly.+x      APP=<house>/@.apps/deskopoly      S=/some/empty/folder
$OP $APP $S lobby classic            # opens a game; first ledger row records the ruleset and its rules hash
$OP $APP $S join Ann human           # prints p1   (name 1..16 chars, driver human|pet, optional personality and token 0..7)
$OP $APP $S join Bo pet              # prints p2   (2..8 players; `leave pN` before start)
$OP $APP $S start 7                  # closes the lobby; 7 is the seed
$OP $APP $S order                    # repeat: each player throws for the opening; highest total moves first, ties re-throw; the last throw also writes `first`
$OP $APP $S roll                     # the current player rolls (add as=p2 to refuse unless it is p2's turn)
$OP $APP $S buy | skip               # when the landing said for-sale
$OP $APP $S end_turn                 # when you are done
$OP $APP $S bankrupt                 # only while a debt cannot be paid
$OP $APP $S status | check | replay | ledger_hash | audit | help
$OP $APP $S tick | autoplay          # pets act (a human seat makes tick wait); autoplay plays to the end. The pet policy is a STOPGAP until M10
```
Exit codes: 0 ok · 1 the ledger does not replay or an invariant failed · 2 usage, bad data, or a refused action (a refusal appends nothing and says why on stderr).

## 3. The rules as built (RULES numbers are from memory of the standard edition and are NOT yet checked against the official sheet: spec 4.12)
* 2 to 8 players, 1500 each, 200 for landing on or passing Start. The bank is an account that may go negative; `check` proves `cash + bank` never changes.
* Rent: a street pays its base rent, **double when the owner holds the whole colour**; a rail pays 25/50/100/200 by rails owned; a utility pays 4x the roll (one owned) or 10x (both). Levies are paid to the bank.
* A debt you cannot pay (not enough cash) puts the game in DEBT: only `bankrupt` is legal for now (selling and mortgaging arrive in M6). **Bankrupt to a player: every tile and the cash go to that player** (the old engine gave them back to the bank: fixed). To the bank: tiles go back to the bank.
* The game ends when one player is left (`over ... winner=pN`) or at 400 rounds (`stalemate=pN`, the richest by cash plus tile prices).

## 4. Where the facts live (no ghost data)
`state/ledger.txt` is the only authority. `state/ui.txt` and `state/mechanics.txt` are projections; `audit` fails on any other file. The ruleset (`rulesets/classic/board.pdl` and `rules.pdl`) is config: its hash is in the first row, so changing it after a game began makes every verb refuse (try it: that is a test in `dsk_classic`). Players are `join` rows, not config.

## 5. Editing the game through the editor (so harnesses and metrics are real)
* The deskopoly book has its own harness rows: `@.apps/deskopoly/harness.pdl` (`classic data`, `classic play`). They appear in the editor's **Harness** tab when the deskopoly book is open and, with the global rows, in the top taskbar **harness** cell. Each runs the real case behind `cpu_gate` and shows the last verdict beside its label (`classic play [46 ok]`).
* Change a price, a rent or a tile name in `rulesets/classic/board.pdl` (the editor's data page for the book, or any text editor), then press the harness row: the data case pins the counts and sums from the file, and the rules hash makes any game begun before the edit refuse to replay, so an edit can never silently rewrite history.
* Add a rule as a new verb row in `verbs.pdl` (with a FLOW row) and a case with a negative that can fail; `help` shows it automatically because it reads `verbs.pdl`.

## 6. Traps (all hit so far)
* In a case file a literal `|` inside an argument must be written `\p` (an unescaped bar splits the argument: a sed command and a grep pattern both failed that way). In an ERE grep a bare `|` is alternation.
* `rm -f results/<case>.txt` before a run: the results file is append-only.
* A replay that cannot reproduce a recorded result is a refusal (exit 1), not a repair. Never edit the ledger to make a test pass.
