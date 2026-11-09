# Deskopoly classic (shown to players as Teropoly): how to play it and how to drive it (milestones M0-M6 done, M11 board page done, M7 auctions done 2026-11-08, M8-M10 and M12 open; status table corrected 2026-11-08)
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
| M7 auctions | **DONE 2026-11-08** | `skip` on a tile for sale opens an AUCTION (ledger result `declined;auction`): bidders left of the decliner first, the decliner last, the high bidder is skipped, `bid <amount> [as=pN]` (minimum 10, then standing + 10, at most your cash) and `pass`; the call that records the last bid or pass also appends the bank row `auction_end` (`sold-to=pN-amount` or `unsold`). Rules: `RULE auction_min_bid`, `auction_min_step`, `auctions=off` (the pre-M7 game). Old ledgers (`declined`) replay WITHOUT an auction. Pets bid by a labelled STOPGAP (price x 130% bold, 85% cautious, 100% coin; M10 replaces it). **LIMIT: tiles of a bankrupt player go back to the bank unsold, not to auction.** Case `dsk_auction` (57), live script `tests/dsk-seat-auction.pdl` (28 steps, popup Bid N / Bid N+50 / Pass) |
| M9 trading, ENGINE slice | **DONE 2026-11-08, no screen yet** | verbs `offer <pN> give <assets> get <assets>`, `counter <id> ...`, `amend <id> ...`, `accept|reject|cancel [<id>]`, bank row `expire` (appended by `end_turn`). Never blocks play; `RULE trade_offers_per_turn` (default 3, 0 = off), `trade_max_rounds` (default 6); validity at offer AND accept; atomic swap; a mortgaged tile costs its receiver 10% at once; `accept` waits while an auction is open; pets ANSWER offers (stopgap value rule), pets do not PROPOSE yet. Case `dsk_trade` (113). **NOT BUILT: the trade menu (both inventories + two drop panes), the chat commands, pets proposing trades. A human cannot trade from the screen yet.** |
| M8 debt queue | **PLAN** | until then those tiles resolve as `todo-<kind>` (nothing happens) |
| M10 pets/ui/tick for all phases | **PARTLY** (corrected 2026-11-08; this row used to say PLAN) | pets play and the players bar, footer and popup run through `deskopoly_driver` inside the rpg-edit-hq editor; the pet policy is still a STOPGAP, personalities are not built |
| M11 board page | **done** (corrected 2026-11-08) | `deskopoly page` writes the 33x33 board and the 3D players; case `dsk_page` 42; sky-seam fix and desk guard 2026-11-08 |
| Where to start | | `16.game/TEROPOLY-ONBOARDING-HARNESSES-DRIVING-AND-CPU-LESSONS-2026-11-08.md`, then `06-testing/WHAT-TO-LOOK-FOR-ACCEPTANCE-COMPENDIUM-PER-PROJECT.md` |

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
