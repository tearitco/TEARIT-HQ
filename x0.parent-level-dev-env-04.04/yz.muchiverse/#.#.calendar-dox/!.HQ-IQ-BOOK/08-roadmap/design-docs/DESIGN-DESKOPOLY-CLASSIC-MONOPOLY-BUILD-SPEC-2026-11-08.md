# Deskopoly Classic: the complete build spec, written so a new AI can implement it from scratch and fix what is wrong today
*Filed under nov-8 as asked; written 2026-11-07 23:5x by Claude Sonnet 5.5 for the owner. Owner decision (SAID 2026-11-07): "lets go classic for now, write up a huge design doc so a completely new ai could implement this from scratch correctly even fixing anything we have wrong currently." This is that document.*

**Labels used everywhere:** **SAID** = the owner told an agent · **READ** = I opened that file (path and line given) · **OBS** = I ran it and saw it · **RULES** = the published standard Monopoly rules, written from my memory of the standard US edition, NOT from a file in this repo: a builder must check every RULES number against the official rule sheet before shipping (section 4 says exactly how) · **DOC** = my own reasoning or recommendation.

## 0. How to use this document (read this first)
1. Sections 1 to 3 tell you the **rules of the house** you must obey, **what exists today**, and **what is wrong with it** (25 findings, each with evidence, the classic rule and the fix). Do not skip section 3: three of the findings would make a "classic" game silently wrong.
2. Sections 4 to 6 are the **specification**: data tables (the whole board, every price and rent, both card decks), the ledger grammar, the turn state machine, every verb.
3. Sections 7 to 11 are **trading, auctions, bankruptcy, setup and sign-up, the window and chat front door, and how pets decide**.
4. Sections 12 to 15 are the **proof plan** (harness cases, with negatives), the **migration plan** from the 16-tile engine, caps, and the **open decisions** only the owner can make (each has a recommended default so you are never blocked).
5. Section 16 is the **build order** with an acceptance test per milestone. Do them in order. Every milestone ends committed, with its harness case passing, before the next starts.
6. Nothing here has been built yet. Everything about the CURRENT engine is READ or OBS. Everything about the TARGET is a specification.

## 1. The rules of the house (these override any habit you have)
All of these are in `/home/no/Desktop/github/work/NNEST-12.00/AGENTS.md` (READ, 2026-11-07); the ones that bite this project:
* **Languages:** C (ops), pal (assembly for the prisc VM), `.pdl` (pipe-row data). **No Python, no new `.sh`.** A harness is **pal + a case `.pdl` + compiled C ops** (`&.widgits/_shared-lib/harness/`, read its `README.md` before writing one).
* **The ledger is the only authority for game state (no ghost data, owner 2026-11-07).** A fact about a game lives in ONE append-only ledger row written through a verb. Every other file is a PROJECTION (derived by a named verb, safe to delete, byte-identical when regenerated), a CONFIG (an input, never written by the game) or a PREFERENCE (a window choice that never changes an outcome). Never keep behaviour-deciding state only in memory; derive it by replaying the ledger. A game ships four tests that can fail: replay reproduces it and a hand-edited ledger is refused; files beside the ledger change nothing; `audit` finds a stray file; deleted projections regenerate identical bytes. Standard: `02-architecture/NO-GHOST-DATA-THE-LEDGER-IS-THE-AUTHORITY-2026-11-07.md`.
* **Heavy work asks first:** every compile, engine start or screenshot loop runs as `cpu_gate <house> run <label> -- <cmd>` (`&.widgits/_shared-lib/ops/+x/cpu_gate.+x`), one at a time, niced. **Never wrap `button.sh reset` or `run` in `cpu_gate`** (it took the desktop down on 2026-11-07; they take the lock themselves).
* **Runtime files need a hard cap** and a row in `03-pitfalls/RUNTIME-FILE-GROWTH-STANDARD.md`; every process the house starts is registered (use the funnel, no new `setsid nohup`).
* **Never report a number you did not observe.** `grep -c` on a missing file prints nothing and reads as zero; a results file is append-only, so two runs read as a doubled total (this happened: `dust_gen` showed 230 for 115 checks). Verify that a passing assertion can fail (write the negative).
* **Commit discipline:** commit only your own paths with an explicit pathspec (`commit_scoped.+x <repo> --branch claude --message-file <f> --push -- <paths>`), never `git add -A`, never reset or stash, never touch `xyzfs/users`. Commit at the end of every work block. Mid-work snapshots may be `wip:`.
* **Design-choice test:** before choosing between two shapes ask which is more modular, scalable, long-term and house-aligned, and add an entry to `02-architecture/DESIGN-CHOICE-TEST-AND-DECISION-LOG.md` (the next free number after D52 is **D53**).
* **Names and IP:** the product is LyfeDesk. Use neutral tile names, never the commercial board's street names or card wording (section 4.1 explains, and the owner's own legal note flags the same kind of risk elsewhere). Not legal advice.
* **Never put code, game data, logs or scratch in `XO/`** (docs and media only). Mp3s go to XO, never into the repo.

## 2. What exists today (READ and OBS, 2026-11-07)
**The app** is `44.xyz.01.00/@.apps/deskopoly/` (house root = `/home/no/Desktop/github/work/NNEST-12.00/x0.parent-level-dev-env-04.04/yz.muchiverse/44.xyz.01.00`):
| File | What it is |
|---|---|
| `ops/deskopoly.c` | the whole rules engine, 425 lines of C, one op. Usage `deskopoly <app_dir> <state_dir> <verb> [args]`. Built to `ops/+x/deskopoly.+x` (compiled programs are in no branch; rebuild with `cpu_gate`). |
| `board.pdl` | `TILE | idx | name | kind | price | rent | group`: **16 tiles**, kinds `go prop tax rest`, 4 colour groups, flat rent |
| `rules.pdl` | `RULE` rows (start_cash 1500, pass_go 200, rounds 40, seed 7) and four `PLAYER | id | name | style | param` rows (the roster!) |
| `verbs.pdl` | `VERB` rows (name, takes-args, ui flag, signature, note) and `FLOW` / `BRAIN` rows; `mechanics` projects it |
| `page.pdl` | how the board is drawn as an editor page (atlas tile indexes per group, owner markers, block size 3) |
| `state.pdl` | the no-ghost-data manifest: `ledger.txt` LEDGER, `decisions.txt` LEDGER, `ui.txt` PROJECTION, `mechanics.txt` PROJECTION |
| `tuning.pdl` | `TUNE | tick_ms | 1500` (nothing reads it yet) |
| `layouts.pdl` | `BAR | seats | side_game_seats | right | play | on` (the Seats bar, not drawn yet) |
| `rooms.pdl`, `deskopoly-pchq/pieces/system/maps/deskopoly/{game.pdl,board/*}` | the book the rpg-edit editor opens |
**Verbs (OBS):** `new roll buy skip end_turn autoplay status replay check mechanics page preview seat tick ui ledger_hash audit`.
**State is the ledger** (`state/ledger.txt`, rows `EVENT|round|actor|verb|target|at|result`, the S1 grammar of `16.game/S1-EVENT-LEDGER.md`). Every verb replays the whole ledger, applies one action and appends. Dice are a pure function of `(seed, roll index)` (`die()`, splitmix-style `mix()`), so replay needs no random state. **Replay recomputes the result of every row and refuses a ledger whose recorded result differs.** Money is conserved: `sum(cash) + bank == start_cash x players`, the bank being an account that goes negative when it pays Go money. A second ledger, `decisions.txt` (`DEC|unix|stamp|actor|state|action|target|outcome`), is what the decision bank (`dec_op`) digests.
**Seats:** who drives a seat (`human` or `pet`) is a ledger row (`seat`), legal out of turn; `tick` performs one step for the current seat (a human seat waits); `ui` writes `state/ui.txt` (the window feed; `show_*` keys are `1` or empty).
**Proof today (OBS):** harness case `deskopoly` (`&.widgits/_shared-lib/harness/cases/deskopoly.pdl`, runner `deskopoly.pal`): **105 checks, 0 failed**. Its cases: `rules autoplay replay refusals harsh mechanics lint book page seats ghost`. Pins you will meet: a human who issues the verbs a pet would issue makes the SAME game (334 rows, hash `61ac51a395a63e0d` once the two seat rows are left out; 336 rows, hash `9baa915d6d0846a9` with them); weights from the decision bank matched a hand count (buy 0.9286, skip 0.6667). Re-pinning these on purpose is part of the migration (section 14).
**It opens in the real editor window** (OBS, PNGs in `XO/31.Nov/nov-7/4.` and `5.`), and the editor's edit/play sidebar switch works on it (`ui_mode_op`, 41 checks).

## 3. What is wrong with it today (25 findings; fix every one)
Severity: **S** = would make a classic game wrong or break the house rules · **M** = a real gap · **L** = polish.
| # | Sev | Finding (evidence) | Classic rule or house rule | Fix |
|---|---|---|---|---|
| F1 | S | The board is **16 tiles** with four colour groups and a flat rent (`board.pdl`, READ). | Classic is 40 tiles: 22 streets in 8 colour groups, 4 railroads, 2 utilities, 2 taxes, Go, Jail, Free Parking, Go To Jail, 3 Chance, 3 Community Chest. | Section 4.1: the full table as data. Keep the 16-tile board as the `mini` ruleset (F15). |
| F2 | S | **No doubles, no jail** (`do_roll`, `deskopoly.c` lines 117 to 129, READ). | Doubles roll again; the third double in a row goes straight to Jail; Jail has its own turn rules. | Sections 5 and 6. |
| F3 | S | **A bankrupt player's properties go back to the bank even when the creditor is another player** (`pay()` calls `release()`, lines 105 to 111, READ). | Debt to a player: ALL assets pass to that player (mortgaged ones stay mortgaged and the receiver pays 10% or unmortgages). Debt to the bank: buildings sold, properties auctioned. | Section 9. |
| F4 | S | **No auction.** A declined or unaffordable tile just stays unowned (`do_skip`, lines 135, READ; `bold` is refused and logged `blocked`, then skips). | When a player does not buy, the bank auctions the tile to all players. | Section 8. |
| F5 | S | Rent is `T[i].rent x (2 if the owner holds the whole group)` (line 126, READ): no houses, no railroads, no utilities, no mortgages. | Section 4.3: a full rent function. | Section 4.3 and 6. |
| F6 | S | **The roster lives in config.** Who plays (ids, names, styles) is `PLAYER` rows in `rules.pdl`; the ledger `new` row only records a count (`v_new`, lines 220 to 224, READ). | No ghost data: a fact about a game (who is playing) is a ledger fact, not a config input. Editing `rules.pdl` mid-game changes names and styles under an old ledger. | Section 10: `join` rows in the ledger; `rules.pdl` keeps only defaults. |
| F7 | S | **Rules are not pinned to the game.** `new` records only `seed` and `players`. Editing `start_cash`, `pass_go` or a price re-reads old history under new numbers, and a ledger that happens to stay self-consistent is accepted. | The ledger must say which rules it was played under. | Record a **rules hash** and a **schema number** in the first rows (sections 5.2 and 10). |
| F8 | M | `MAXP 4`, and `new` refuses more players than `PLAYER` rows (lines 30, 156, 222, READ). | Classic is 2 to 8 players. | Raise to 8; players come from `join`. |
| F9 | M | `rounds = 40` ends every game (`do_end`, line 140, READ). | Classic ends when one player is left. A round cap is a house rule for pets and watchers. | `rounds = 0` means unlimited, with a **safety cap** (default 400 rounds) that ends the game as a recorded `stalemate` won by net worth. |
| F10 | M | `net()` counts cash plus tile prices only (line 182, READ). | Net worth = cash + unmortgaged price + half price of mortgaged + buildings at cost. | Section 4.2. |
| F11 | S | **Silent truncation can hide a ghost.** `ap()` appends with `vsnprintf(res+l, cap-l)`; `v_status` uses a 4096-byte buffer, `v_mechanics` 24576, `v_page` 64x64 cells (lines 113, 183, 225, 238, 296, READ). Output past the end is silently cut. | A status or audit that is truncated can omit exactly the row that shows a problem. | Size for 8 players and 28 properties with buildings (32 KB), and make overflow a **loud failure** (exit 1), never a silent cut. |
| F12 | M | The pet decisions are hard-coded in C: `wants_buy()` knows `bold`, `cautious`, `coin` (lines 197 to 202, READ). | D48/D52: a brain is data (BRAIN rows, the decision bank, personality rows); the engine knows no pet style. | Section 11: `personalities.pdl` and `decide` rows; the C keeps only a deterministic evaluator. |
| F13 | M | `ui` offers only Roll, Buy, Skip and End. | Classic needs build, sell, mortgage, jail choices, auction bids, trade menus. | Section 10.3. |
| F14 | M | `check` proves money conservation, cash >= 0, position range, owners (lines 227 to 233, READ). | Classic adds houses and hotels supply, mortgage flags, even building, deck integrity, jail counters, open trades and auctions. | Section 5.6 (the invariant list). |
| F15 | S | **The old harness pins depend on the 16-tile board** (334 rows, hash `61ac51a395a63e0d`, 105 checks). Replacing the board silently breaks 105 checks, or tempts a builder to edit the pins to match. | Never edit a pin to make a test pass. | Ship **rulesets**: `rulesets/mini/` (today's board and rules, unchanged) and `rulesets/classic/`. The `start` row names the ruleset, so the old case keeps running against `mini` byte for byte. |
| F16 | M | `page` assumes `group & 7` and owner markers for at most 4 players (`PG_group[8]`, `PG_owner[MAXP]`, lines 264, 293, READ). | Classic needs 10 groups (8 colours, railroads, utilities) and 8 owners. | Section 10.5. |
| F17 | L | The header comment bounds the ledger at "rounds x players x 4 rows, about 60 KB" (line 14, READ). | With 40 tiles, cards, auctions and trades the bound is different. | Section 13. |
| F18 | M | `die()` mixes `idx*16 + k` and `nrolls` counts only `roll` rows (lines 91, 128, READ). | Doubles, jail rolls, card rolls and the opening order roll all consume dice. | One **dice index** counts every row that consumes dice (section 5.3). |
| F19 | M | `tick` and `autoplay` call `act_simple` for buy/skip/end only. | New verbs need a decision for every phase. | Section 11. |
| F20 | L | A refused action prints to stderr and exits 2 (correct), but only the human buy logs a `DEC ... blocked` row (line 216, READ). | Every refusal that a decision maker caused should be learnable. | Log a `blocked` DEC row for any refused decision by a seat that has a brain. |
| F21 | M | The tax tiles take a fixed `price` field and no choice (line 121, READ). | Standard edition: Income Tax is a flat amount (older editions: 10% or $200); Luxury Tax is a flat amount. | Data values; the choice is a recorded owner decision (section 15). |
| F22 | M | `tuning.pdl tick_ms` is read by nothing (READ). | A watched game must advance at a human pace. | Section 10.4 (the tick loop) reads it. |
| F23 | L | The `rest` tile is Free Parking with no pot (correct for the official rules) but is named `Rest` and treated as inert. | Official: nothing happens. Many house rules add a pot. | Keep the official behaviour; offer the pot as a **disabled** rule (section 15). |
| F24 | M | **The verbs carry no actor.** `roll`, `buy`, `skip` and `end_turn` act for whoever is the current player (`v_roll`, `act_simple`, lines 207 to 219, READ). Only `tick` refuses a human seat (line 338). A daemon and a window can both act on the same seat; nothing in the row says which of them did. | A row should say who acted and in which capacity. | Every action verb takes `as=<player id>` and is refused unless it names the current player (or, for responses, the addressee); the row records it. The front door (window, chat, tick) decides whether the caller may use that seat's driver. |
| F25 | M | **A finished game writes no `over` row.** Game over is implied by a flag set inside `do_end` (` over` appended to the end_turn result, line 141) and the winner is printed by `autoplay` only (lines 359 to 360, READ). | The ending and the winner are facts. | A bank row `over` with `winner=<id>` or `stalemate=<id>` (section 5.2); the winner is never stdout only. |

**Reading list for the builder, in order:** `AGENTS.md` (house rules) · `08-roadmap/design-docs/PROPOSAL-ONE-DOOR-DRIVING-AND-THE-MASTER-LEDGER-2026-11-07.md` (D49: one door, the master ledger) · `PROPOSAL-ONE-BRAIN-PER-GAME-HARNESS-AND-PETS-2026-11-07.md` (D48: BRAIN rows) · `PROPOSAL-PLAY-MODE-SEATS-AND-GAME-SIDEBARS-2026-11-07.md` (seats, play mode, sidebars) · `02-architecture/NO-GHOST-DATA-THE-LEDGER-IS-THE-AUTHORITY-2026-11-07.md` · `16.game/S1-EVENT-LEDGER.md` · `&.widgits/_shared-lib/harness/README.md` and `cases/deskopoly.pdl` (how the current proof is written) · `02-architecture/DESIGN-CHOICE-TEST-AND-DECISION-LOG.md` (D48 to D52).

## 4. The classic rules as data (RULES; verify against the official sheet, see 4.12)
**Why neutral names:** the commercial game's mechanics (turn order, price tables, the way rent grows) are rules, but its street names, board art and card wording are protected by its publisher. The house already flags the same kind of risk in its own lore notes (a shop quarter whose draft name was too close to a famous one). So the numbers below are the classic numbers, and **every name and every card text is the house's own** (colour-and-number names; two decks called Luck and Fund). The owner may rename any tile in `board.pdl` without touching code. Not legal advice; the owner decides (section 15, decision O1).

### 4.1 The board: 40 tiles, clockwise from Start (idx 0 to 39)
Columns: `idx | id | kind | group | price | rent[0..5] (base, 1 to 4 houses, hotel) | house cost | mortgage`. Group ids: 0 brown(2 tiles) · 1 sky(3) · 2 pink(3) · 3 orange(3) · 4 red(3) · 5 yellow(3) · 6 green(3) · 7 navy(2) · 8 rail(4) · 9 util(2). Money is whole dollars. Mortgage value is half the price in every row.
| idx | id (name in `board.pdl`) | kind | grp | price | rent: base, 1h, 2h, 3h, 4h, hotel | house |
|---|---|---|---|---|---|---|
| 0 | Start | go | - | - | collect pass_go (200) when you land on or pass it | - |
| 1 | Brown 1 | street | 0 | 60 | 2, 10, 30, 90, 160, 250 | 50 |
| 2 | Fund 1 | chest | - | - | draw from the Fund deck | - |
| 3 | Brown 2 | street | 0 | 60 | 4, 20, 60, 180, 320, 450 | 50 |
| 4 | Levy A | tax | - | 200 | pay 200 to the bank | - |
| 5 | Line 1 | rail | 8 | 200 | 25, 50, 100, 200 by lines owned | - |
| 6 | Sky 1 | street | 1 | 100 | 6, 30, 90, 270, 400, 550 | 50 |
| 7 | Luck 1 | chance | - | - | draw from the Luck deck | - |
| 8 | Sky 2 | street | 1 | 100 | 6, 30, 90, 270, 400, 550 | 50 |
| 9 | Sky 3 | street | 1 | 120 | 8, 40, 100, 300, 450, 600 | 50 |
| 10 | Jail | jail | - | - | just visiting, or in jail | - |
| 11 | Pink 1 | street | 2 | 140 | 10, 50, 150, 450, 625, 750 | 100 |
| 12 | Power | util | 9 | 150 | 4x or 10x the dice by utilities owned | - |
| 13 | Pink 2 | street | 2 | 140 | 10, 50, 150, 450, 625, 750 | 100 |
| 14 | Pink 3 | street | 2 | 160 | 12, 60, 180, 500, 700, 900 | 100 |
| 15 | Line 2 | rail | 8 | 200 | as Line 1 | - |
| 16 | Orange 1 | street | 3 | 180 | 14, 70, 200, 550, 750, 950 | 100 |
| 17 | Fund 2 | chest | - | - | draw from the Fund deck | - |
| 18 | Orange 2 | street | 3 | 180 | 14, 70, 200, 550, 750, 950 | 100 |
| 19 | Orange 3 | street | 3 | 200 | 16, 80, 220, 600, 800, 1000 | 100 |
| 20 | Free | free | - | - | nothing happens (no pot by default, see O4) | - |
| 21 | Red 1 | street | 4 | 220 | 18, 90, 250, 700, 875, 1050 | 150 |
| 22 | Luck 2 | chance | - | - | draw from the Luck deck | - |
| 23 | Red 2 | street | 4 | 220 | 18, 90, 250, 700, 875, 1050 | 150 |
| 24 | Red 3 | street | 4 | 240 | 20, 100, 300, 750, 925, 1100 | 150 |
| 25 | Line 3 | rail | 8 | 200 | as Line 1 | - |
| 26 | Yellow 1 | street | 5 | 260 | 22, 110, 330, 800, 975, 1150 | 150 |
| 27 | Yellow 2 | street | 5 | 260 | 22, 110, 330, 800, 975, 1150 | 150 |
| 28 | Water | util | 9 | 150 | as Power | - |
| 29 | Yellow 3 | street | 5 | 280 | 24, 120, 360, 850, 1025, 1200 | 150 |
| 30 | Go Jail | gotojail | - | - | go straight to Jail (idx 10), no Start money | - |
| 31 | Green 1 | street | 6 | 300 | 26, 130, 390, 900, 1100, 1275 | 200 |
| 32 | Green 2 | street | 6 | 300 | 26, 130, 390, 900, 1100, 1275 | 200 |
| 33 | Fund 3 | chest | - | - | draw from the Fund deck | - |
| 34 | Green 3 | street | 6 | 320 | 28, 150, 450, 1000, 1200, 1400 | 200 |
| 35 | Line 4 | rail | 8 | 200 | as Line 1 | - |
| 36 | Luck 3 | chance | - | - | draw from the Luck deck | - |
| 37 | Navy 1 | street | 7 | 350 | 35, 175, 500, 1100, 1300, 1500 | 200 |
| 38 | Levy B | tax | - | 100 | pay 100 to the bank | - |
| 39 | Navy 2 | street | 7 | 400 | 50, 200, 600, 1400, 1700, 2000 | 200 |
**Counts to assert in the harness:** 40 tiles; 22 streets; 4 rails; 2 utils; 2 taxes; 3 Luck; 3 Fund; Start, Jail, Free, Go Jail once each; groups 0 and 7 have 2 tiles, groups 1 to 6 have 3, rail 4, util 2; the sum of all 28 purchase prices and the sum of all rents must be **computed from the data file and pinned from that computation**, never copied from this document.
**Data shape (new `board.pdl` rows, `|` separated, one row per tile):** `TILE | idx | id | kind | group | price | r0 | r1 | r2 | r3 | r4 | r5 | house_cost`. The loader must refuse: indexes not 0..n-1 in order, a street without six rents, a group id with a different house cost in two rows, a rail or util without a group, a kind it does not know, fewer than 4 or more than 64 tiles, tile 0 not Start. (The current loader reads 7 fields; keep it working for the `mini` ruleset by version, section 14.)

### 4.2 Money, values and supplies
* Start cash **1500** each. Pass Start pays **200**. The bank has unlimited cash; its balance is an account that may go negative (today's engine already does this and `check` proves `sum(cash) + bank == start_cash x players`).
* **Mortgage value** = price / 2. **Unmortgage cost** = mortgage value + 10%, rounded UP to the next whole dollar (60 -> 30 -> 33; 150 -> 75 -> 83).
* **Supply:** 32 houses and 12 hotels in the bank. A hotel is the 5th building; buying it returns four houses to the supply. Building when the supply is empty is **refused** (the official shortage auction is a decision, O6).
* **Net worth** (used for the stalemate winner and the status line) = cash + for each owned tile (price if unmortgaged, price/2 if mortgaged) + building cost for each house (a hotel counts as 5 houses).

### 4.3 The rent function (one pure function, `rent_due(tile, owner, last_dice_total)`)
1. Tile unowned, or owned by the payer, or the owner is bankrupt: **0** (owner bankrupt cannot happen: its tiles were transferred).
2. Tile **mortgaged**: **0**.
3. **Street:** with `b` buildings (0 to 5, 5 = hotel) rent = `rent[b]`. With `b = 0` and the owner holding **every tile of the group**, rent = `2 x rent[0]` (official; if one tile of the group is mortgaged the doubling still applies, decision O7, verify).
4. **Rail:** `25 x 2^(n-1)` where `n` = rails the owner owns (mortgaged ones count as owned): 25, 50, 100, 200. The **Luck "go to the nearest line"** cards make the payer pay **twice** that.
5. **Util:** `n = 1` -> `4 x dice`; `n = 2` -> `10 x dice`, where `dice` is the total of the roll that brought the payer here. The **Luck "go to the nearest utility"** card, when the util is owned, makes the payer roll again (a new dice row) and pay `10 x` that total regardless of `n`.
6. Rent is paid immediately and only when the landing is resolved (a payer who cannot pay enters the debt state, section 9).

### 4.4 Doubles and Jail
* A roll of two equal dice is **doubles**. After the landing is fully resolved (rent paid, card applied, tile bought or auctioned) the same player **rolls again**, unless they were sent to Jail. Count the doubles of the current turn: the **third in a row sends the player straight to Jail without moving** (the third roll's landing is not resolved).
* **Going to Jail** (from Go Jail, the third double, or a card): token to idx 10, the turn ends at once (even after doubles), **no Start money**.
* **A turn in Jail:** before rolling the player may (a) **pay 50** to the bank and roll normally, (b) **use a Get Out card** (it returns to the bottom of its deck) and roll normally, or (c) **roll**: doubles frees them and they move that roll's total (**no extra roll** for those doubles); a non-double keeps them in Jail. A player who has failed on **three turns** must pay 50 and moves by the third roll's total. Jail time counter lives in replay state (derived from rows, never a file).
* While in Jail a player still collects rent, may build, mortgage, trade and bid in auctions.
* **Just visiting** (landing on idx 10 normally) does nothing.

### 4.5 The two card decks (house wording; effects are the classic effect kinds)
**Data shape:** `CARD | deck | n | kind | a | b | text` (deck is `luck` or `fund`; `n` 1 to 16; `kind` one of `move_to`(a = tile idx; pass Start pays if it wraps), `nearest_rail`, `nearest_util`, `move_back`(a = steps), `go_jail`, `keep_card`, `collect`(a), `pay`(a), `collect_each`(a, from every other active player), `pay_each`(a), `repairs`(a per house, b per hotel)). Deck order is a **seeded shuffle** (Fisher-Yates over the `mix()` of `(seed, deck id, i)`) done once at `start`; cards are drawn from the top and go to the bottom, except a kept Get Out card (it leaves the deck until used). The deck position is **derived by replay** (the count of draws), never stored.
| n | Luck deck: effect | kind | a | b |
|---|---|---|---|---|
| 1 | Advance to Start and collect | move_to | 0 | |
| 2 | Advance to Red 3 | move_to | 24 | |
| 3 | Advance to Pink 1 | move_to | 11 | |
| 4 | Advance to the nearest utility | nearest_util | | |
| 5 | Advance to the nearest line, pay double rent if owned | nearest_rail | | |
| 6 | (second copy) nearest line | nearest_rail | | |
| 7 | The bank pays you 50 | collect | 50 | |
| 8 | Get out of Jail free (keep it) | keep_card | | |
| 9 | Go back three tiles | move_back | 3 | |
| 10 | Go to Jail | go_jail | | |
| 11 | Repairs: 25 per house, 100 per hotel | repairs | 25 | 100 |
| 12 | Pay a 15 fine | pay | 15 | |
| 13 | Take a trip to Line 1 | move_to | 5 | |
| 14 | Advance to Navy 2 | move_to | 39 | |
| 15 | Pay every other player 50 | pay_each | 50 | |
| 16 | A loan matures: collect 150 | collect | 150 | |
| n | Fund deck: effect | kind | a | b |
|---|---|---|---|---|
| 1 | Advance to Start and collect | move_to | 0 | |
| 2 | Bank error in your favour: collect 200 | collect | 200 | |
| 3 | Pay a 50 fee | pay | 50 | |
| 4 | Sell a share: collect 50 | collect | 50 | |
| 5 | Get out of Jail free (keep it) | keep_card | | |
| 6 | Go to Jail | go_jail | | |
| 7 | Holiday fund matures: collect 100 | collect | 100 | |
| 8 | Refund: collect 20 | collect | 20 | |
| 9 | Your birthday: collect 10 from every other player | collect_each | 10 | |
| 10 | Insurance matures: collect 100 | collect | 100 | |
| 11 | Pay 100 in hospital fees | pay | 100 | |
| 12 | Pay 50 in school fees | pay | 50 | |
| 13 | Collect a 25 consulting fee | collect | 25 | |
| 14 | Street repairs: 40 per house, 115 per hotel | repairs | 40 | 115 |
| 15 | You win a contest: collect 10 | collect | 10 | |
| 16 | An inheritance: collect 100 | collect | 100 | |
**Card landing rules:** drawing happens when the landing is resolved on a Luck or Fund tile. A `move_to` that wraps past Start pays Start money; a `move_to` or `nearest_*` landing is then resolved like a roll landing (buy, auction, rent) but **does not grant doubles again and never recurses into another card draw beyond one level** (a card that moves you onto a card tile draws that deck once; the harness must cover `move_back 3` from Luck 3 (idx 36), which lands on Fund 3 (idx 33), a card tile: the classic famous case; and from Luck 1 (idx 7), which lands on Levy A (idx 4), a tax tile). `go_jail` and a third double never collect Start money.

### 4.6 Buildings
* A player may build only on a **street** of a group they **fully own, with no tile of that group mortgaged**.
* **Even building:** after any build the building counts of the tiles in the group differ by at most 1; the same for selling. A hotel needs four houses on every tile of the group first.
* **Cost:** the group's house cost, paid to the bank, per house; a hotel costs one more house cost and returns four houses to the supply.
* **Selling:** a house sells back to the bank for **half** its cost; a hotel sells for half a house cost and the player takes four houses from the supply back **if the supply has them**, otherwise the hotel must be sold all the way down (decision O6). Buildings can never be traded: they must be sold before a tile leaves the owner (trade, bankruptcy to the bank).
* Building and selling are allowed whenever it is the owner's turn and the debt state is clear (and during a debt, selling is how you raise cash).

### 4.7 Mortgages
* Only a tile with **no buildings in its whole group** may be mortgaged: the owner receives the mortgage value; the tile pays no rent while mortgaged.
* To lift it the owner pays the unmortgage cost (4.2). A mortgaged tile can be traded or transferred in bankruptcy; the receiver pays **10% of the mortgage value at once** (rounded up) and either lifts it now (paying the mortgage value plus another 10%) or leaves it mortgaged and pays another 10% when they lift it later (the official rule; verify, decision O8).

### 4.8 Taxes, Free and the end of the game
* Levy A 200 and Levy B 100 are paid to the bank (flat; the "10% or 200" choice of older editions is decision O3).
* Free does nothing.
* The game ends when **one player is left** (that player wins). The optional **safety cap** (`safety_rounds`, default 400) ends the game as a `stalemate` and the winner is the highest net worth (ties go to the lower player index); the ledger records which (section 5.2).

### 4.9 Players
2 to 8 players, each with an id (`p1`...`p8`), a display name, a token index (0 to 7) and a **driver** (`human` or `pet`); the order of play is decided by an opening dice roll (section 10.1). A bankrupt player is **out**: they never act again, their row stays in the roster, and a replay never forgets them.

### 4.10 Turn order inside a turn (summary; the full state machine is section 5.4)
`start of turn` -> (if in Jail: jail choices) -> `roll` -> resolve landing (pay rent / pay tax / draw a card / go to Jail / offer the tile) -> if the tile was offered: **buy or decline (auction)** -> if doubles and not in Jail: roll again -> otherwise the **action phase** (build, sell, mortgage, unmortgage, trade) -> `end_turn`. Building, mortgaging and trading are also legal **before the roll**.

### 4.11 What the classic game does NOT include here
No bank-run house rules (Free Parking pot, double Start money for landing exactly on it, no rent in jail, etc.): they are `RULE` flags that default OFF (section 15). No timers: a human may think as long as they like; pets are paced by the tick.

### 4.12 Verify list (do this before you call the numbers right)
Open the official rule sheet of the standard edition (the owner may hold a physical copy) and confirm, and write the result next to each line in the decision log: (1) all 28 purchase prices, base rents and building costs; (2) the two tax amounts and whether Income Tax is flat or 10%; (3) 16 cards per deck and that the effect kinds above match the official deck's kinds (wording stays ours); (4) 32 houses and 12 hotels; (5) unmortgage interest 10% and the rounding; (6) the mortgaged-property transfer rule in 4.7; (7) doubling rent while one tile of the set is mortgaged; (8) the auction rules (section 8); (9) the three-failed-turns Jail rule; (10) the third-double rule (go to Jail without moving). A number you could not check stays marked DOC in the data file's comment.

## 5. The ledger, the dice, the turn state machine and replay
### 5.1 Files (target layout of `@.apps/deskopoly/`)
| Path | Kind | Notes |
|---|---|---|
| `ops/deskopoly.c` -> `ops/+x/deskopoly.+x` | code | one op, sectioned (decision D53: stay a single file until it passes about 2,500 lines, then split by text-include of PURE parts only; never header-plus-link) |
| `rulesets/<name>/board.pdl`, `rules.pdl`, `cards.pdl` | CONFIG | `rulesets/mini/` = today's files unchanged; `rulesets/classic/` = section 4. The ledger names the ruleset and its hash. |
| `personalities.pdl`, `assets.pdl`, `tuning.pdl`, `layouts.pdl`, `page.pdl`, `verbs.pdl`, `state.pdl`, `rooms.pdl` | CONFIG | verbs.pdl lists every verb (the `mechanics` projection and the `audit` read it) |
| `state/ledger.txt` | LEDGER | the only game authority |
| `state/decisions.txt` | LEDGER | the DEC rows the decision bank digests (section 11) |
| `state/ui.txt`, `state/mechanics.txt` | PROJECTION | rewritten atomically; regenerated byte for byte |
`state.pdl` stays the manifest (`FILE | name | kind | writer | regenerate_with`); `audit` fails on any undeclared file. **Never add a file next to the ledger that holds a fact.**
### 5.2 The ledger row (unchanged shape, so the master ledger and every existing reader keep working)
`EVENT|round|actor|verb|target|at|result` : seven `|`-separated fields, no `|` inside a field, one row per line, newline-terminated, at most 1024 bytes (today's limit is 512; raise it). `round` = the round counter at the time; `actor` = a player id (`p1`...`p8`) or `bank`; `at` = the tile index where the actor stands after the action (0 for setup rows). **`result` is the engine's own deterministic computation of what the action did; replay recomputes it and refuses a ledger whose recorded result differs** (this is how a hand-edited ledger is caught). A result is a `;`-joined list of tokens (5.2.2).
#### 5.2.1 Every verb's row
| verb | actor | target | result (tokens) | when |
|---|---|---|---|---|
| `lobby` | bank | `-` | `ruleset=<name>,schema=2,rules=<hash16>,max=<n>` | the FIRST row of a schema-2 game |
| `join` | the NEW id | display name (no `|`) | `driver=<human\|pet>,token=<0..7>,personality=<id\|->` | LOBBY, 2 to 8 times |
| `leave` | id | `-` | `left` | LOBBY |
| `start` | bank | `-` | `seed=<n>,players=<n>,rules=<hash16>` | LOBBY with 2 to 8 players; **replaces `new`** |
| `order` | id | `d1,d2` | `total=<n>` | ORDER: each player in join order throws once (dice index consumed) |
| `first` | bank | `-` | `<id>` | after the highest total is unique (ties among the tied re-throw first) |
| `roll` | id | `d1,d2` | tokens 5.2.2 | PRE, not in Jail |
| `jail_pay` | id | `-` | `paid-50` | PRE in Jail |
| `jail_card` | id | deck (`luck` or `fund`) | `card-returned` | PRE in Jail holding a card |
| `jail_roll` | id | `d1,d2` | `free;to=<idx>;...` or `stay;fails=<1..3>` or `forced-fine-50;to=<idx>;...` | PRE in Jail |
| `buy` | id | tile idx | `bought-<price>` | BUY |
| `skip` | id | tile idx | `declined` (then the engine opens an auction, see `bid`) | BUY |
| `bid` | id | tile idx | `bid=<amount>` | AUCTION, only the bidder whose turn it is |
| `pass` | id | tile idx | `pass` | AUCTION, same |
| `auction_end` | bank | tile idx | `sold-to=<id>-<amount>` or `unsold` | appended by the SAME call that ended the auction |
| `build` | id | tile idx | `houses=<n after>;cost=<c>` (a hotel: `hotel;cost=<c>`) | PRE (before the roll) or POST |
| `sell` | id | tile idx | `houses=<n after>;get=<c>` | PRE, POST or DEBT |
| `mortgage` | id | tile idx | `mortgaged;get=<v>` | PRE, POST or DEBT |
| `unmortgage` | id | tile idx | `lifted;pay=<c>` | PRE or POST |
| `offer` | proposer | addressee id | `id=<k>;give=<assets>;get=<assets>;round=1` | PRE or POST (section 7) |
| `counter` | addressee | offer id | `id=<k2>;re=<k>;give=<assets>;get=<assets>;round=<r>` | while the offer is open |
| `accept` | addressee | offer id | `done;give=<assets>;get=<assets>;interest-<a>;interest-<b>` | while the offer is open |
| `reject` | addressee | offer id | `rejected` | while open |
| `cancel` | proposer | offer id | `cancelled` | while open |
| `expire` | bank | offer id | `expired` | appended by `end_turn` for each open offer of the ending player |
| `pay_debt` | id | `-` | `paid-<n>-><creditor id\|bank>` | DEBT, once cash covers it |
| `bankrupt` | id | creditor id or `bank` | `out;cash=<n>;props=<k>;cards=<k>` | DEBT only (section 9) |
| `end_turn` | id | `-` | `next=<id>;round=<n>` plus `;over` when the game ended | POST, no pending decision, no debt |
| `over` | bank | `-` | `winner=<id>` or `stalemate=<id>` | appended by the call that ended the game (F25) |
| `seat` | bank | player id | `human` or `pet` | any time before the end; unchanged from today |
**Assets** (offers and counters) are `kind:value` joined with `+`: `prop:<idx>`, `cash:<n>`, `card:<deck>` (a held Get Out card). Further kinds (`item:<id>`, `entity:<uid>`) are declared in `assets.pdl` and are DISABLED for classic (section 7.5).
#### 5.2.2 The tokens of a `roll` (and `jail_roll`) result, in this order
`to=<idx>` (new tile) · `go+<n>` (passed or landed on Start) · then exactly one landing outcome: `for-sale` (an unowned street, rail or util: the state becomes BUY) · `own` · `free` · `start` · `visit` (Jail tile, just visiting) · `tax-<n>` · `rent-<n>-><id>` · `card=<deck>:<n>` followed by that card's effect tokens (`collect+<n>`, `pay-<n>`, `each+<n>`, `eachpay-<n>`, `repairs-<n>`, `back<k>`, `keep=<deck>`, `jail`, plus the landing outcome of any move the card made, after another `to=<idx>`) · `jail` (the Go Jail tile or a Jail card) · `jail3` (the third double: sent to Jail without moving) · `cardroll=<d1>,<d2>` (the extra roll a "nearest utility" card forces on an owned utility; it consumes dice) · `doubles=<k>` (this roll was the k-th double of the turn, k is 1 or 2; not present on a non-double or in `jail3`) · `debt=<n>-><id\|bank>` (the payer could not pay in cash; state becomes DEBT) · `bankrupt-><id\|bank>` is NOT a roll token (bankruptcy is its own verb).
### 5.3 Dice and shuffles are pure functions of the ledger (replay needs no random state)
* Keep one **dice index** `di` (starts at 0). Every action that throws dice consumes one value of `di`: an `order` row, a `roll` row, a `jail_roll` row, and a `cardroll` token. The two dice of a throw are `die(seed, di, 0)` and `die(seed, di, 1)` with today's `die()` (splitmix64 `mix()` over `seed * 1000003 + idx * 16 + k`, modulo 6, plus 1). **The row stores the dice in `target` and replay refuses a mismatch** (exactly what today's `roll` does).
* The deck shuffles use the same `mix()` with distinct domain constants (`deck id * 7919 + i`), are computed once at `start`, and are never stored. The draw count per deck is derived from the rows (a `card=<deck>:<n>` token is one draw).
* A pet's random choices (the `coin` personality, tie-breaks) use `mix(seed, di, domain)` too, so a whole game is reproducible from `(ruleset, seed, the rows that were typed)`.
### 5.4 The turn state machine (what is legal when)
Derive the state from the ledger by replay; never store it. `cur` = the current player; `doubles` = doubles thrown this turn; `pending` = a tile offered for purchase; `debt` = the open debt record; `auction` = the open auction; `offers` = open offers.
| Phase | How you are in it | Legal verbs (by whom) |
|---|---|---|
| **LOBBY** | no `start` row yet | `join leave start seat` |
| **ORDER** | `start` is in, no `first` yet | `order` (by the next player in join order who has not thrown) |
| **PRE** | start of a turn (also after a double, before the next roll), no pending tile, no debt | `roll` (not in Jail) · `jail_pay jail_card jail_roll` (in Jail) · `build sell mortgage unmortgage offer` by `cur` · `accept reject counter` by an addressee · `cancel` by the proposer · `seat` |
| **BUY** | the landing offered an unowned tile | `buy skip` by `cur`; nothing else (no building or trading until decided) |
| **AUCTION** | `skip` was just recorded, or a bankruptcy to the bank is selling tiles | `bid pass` by the bidder whose turn it is (starts left of `cur`, clockwise, the decliner last); trades and building are refused |
| **DEBT** | a payment could not be made in cash | `sell mortgage offer(accept/counter by others)` · `pay_debt` once cash covers it · `bankrupt` |
| **POST** | landing resolved, no pending tile, no debt, no further roll owed | `build sell mortgage unmortgage offer` by `cur` · `accept reject counter cancel` · `end_turn` by `cur` |
| **OVER** | an `over` row exists | `status replay check mechanics ui audit ledger_hash page preview` only |
**Transitions (the whole engine in one table):** `roll` lands -> BUY (unowned street/rail/util) | DEBT (could not pay) | PRE again (doubles, not Jail) | POST. `buy` -> POST or PRE (if doubles are owed). `skip` -> AUCTION -> (`auction_end`) -> POST or PRE (if doubles are owed). `pay_debt` -> where the debt began. `bankrupt` -> POST for the next player (via `end_turn` logic) or OVER. Doubles: after the landing is fully resolved and the player is not in Jail, the same player is in PRE again with `doubles` incremented; a non-double goes to POST. `end_turn` increments the round counter when the turn passes from the last player to the first (skipping out players), expires the ender's open offers (an `expire` row each), then PRE for the next player. **A human seat's UI offers only the verbs of the current phase; the engine refuses everything else with a plain sentence on stderr and exit 2, appending nothing.**
### 5.5 Replay (the algorithm; the same code serves every verb)
```
reset(): cash[i]=start_cash, pos[i]=0, jail[i]=0, owner[t]=-1, houses[t]=0, mortgaged[t]=0,
         supply: houses=32 hotels=12, deck order = shuffle(seed, deck), draws[deck]=0,
         kept[deck]=-1, di=0, round=0, phase=LOBBY, offers=[], auction=none, debt=none, bank=0
for each EVENT row, in order:
    verify the row's shape (7 fields), the actor exists (or is bank where required), the verb is legal in `phase` for this actor
    apply the verb with the row's target; compute the result string
    if verb dices: compare the recorded dice with die(seed, di, 0/1); di++
    REFUSE (exit 1, "ledger line N: ...") if the computed result != the recorded result
    append the row to the 5-row log ring
after the last row: run no verb; the derived state is the game
```
A verb call = `replay()` + one legality check + compute the action + append the row(s) + (if the action ended an auction or the game, append the bank rows in the SAME call so a crash cannot leave a half-finished consequence). Writes use `O_APPEND` and one `write()` per row. **A refusal appends nothing.**
**Cost:** replay is O(rows x tiles) with tiny constants. Measure it (section 12, case `speed`): the target is a 4-player classic game (about 2,500 rows) replaying in well under 50 ms on this machine; if a measured run is slower, fix the code, do NOT add a cache file (a cache is ghost data).
### 5.6 Invariants (`check` must prove all of these; each is one harness assertion with a negative)
I1 money conserved: `sum(cash) + bank == start_cash x players` · I2 `cash[i] >= 0` for every player at all times (a shortfall is a `debt` record, never a negative cash) · I3 every position is in 0..n-1, and a jailed player stands on the Jail tile · I4 only streets, rails and utils have owners, and no owner is `out` · I5 buildings only on streets, 0 to 5 each, only in a group fully owned by one player with no mortgaged tile · I6 building counts within a group differ by at most 1 · I7 `houses on board <= 32` and `hotels on board <= 12`, and `supply + on board == 32 / 12` · I8 a mortgage flag only on an owned tile with no buildings · I9 deck integrity: each of the 16 cards of each deck is in exactly one place (deck order or a player's hand) and `draws` matches the rows · I10 jail counter 0..3 and only while jailed · I11 open offers: at most one per proposer, all parties exist and are active, ids strictly increasing · I12 at most one auction, its tile is unowned, bids strictly increase, all participants active · I13 exactly one current player, active · I14 at most one debt, its debtor is the current player (or the player being bankrupted) · I15 `doubles` is 0 to 2 and belongs to the current player · I16 an `out` player has cash 0, owns nothing, holds no card · I17 the first rows are `lobby`, `join`..., `start`, `order`..., `first` (schema 2) and `start` appears once · I18 the dice index equals the number of dice throws counted from the rows · I19 the rules hash in `lobby` and `start` equals the hash of the current ruleset files · I20 an `over` row exists iff the game is over, and its winner is consistent · I21 round numbers never decrease · I22 each `EVENT` row is at most 1024 bytes.
Exit codes stay: 0 ok, 1 replay or invariant failure, 2 usage, bad data, or a refused action.

## 6. The core algorithms every verb uses (pseudo-code a builder can type in)
Define once and reuse: `active(i)` = player i is not out. `next_active(i)` = the next player after i (wrapping) that is active. `owned_in_group(p, g)` = tiles of group g owned by p; `full_set(p, g)` = owns every tile of g. `buildings(t)` = 0..5. `liq(p)` = **liquidation value** = `cash[p]` + sum over p's tiles of (mortgage value if the tile is unmortgaged and its group has no buildings) + sum over p's streets of `(buildings x house_cost / 2)` + (for a street whose group has buildings, its mortgage value too, since selling the buildings frees it).
```
land(p, dice_total, via):                          # via = roll | card | jail_roll
    t = tile[pos[p]]
    switch t.kind:
      go:       tok("start")
      street, rail, util:
          if owner[t] == -1:  pending = t; tok("for-sale")                       # phase BUY
          elif owner[t] == p: tok("own")
          else:               due = rent_due(t, owner[t], dice_total); charge(p, due, owner[t]); tok("rent-<paid>-><owner>")
      tax:      charge(p, t.price, BANK); tok("tax-<n>")
      free:     tok("free")
      jail:     tok("visit")
      gotojail: send_to_jail(p); tok("jail")
      chance, chest: draw(p, deck_of(t))        # section 6.2; may call move/land once more, never a second draw
charge(p, amount, creditor):                       # creditor = player index or BANK
    if cash[p] >= amount: cash[p] -= amount; credit(creditor, amount); return paid
    debts.push({debtor: p, creditor, amount})      # phase DEBT; nothing moves yet
    tok("debt=<amount>-><creditor>")
send_to_jail(p): pos[p] = JAIL_IDX; jail[p] = 1 (first turn); doubles = 0; the turn ends after this action (phase POST, with end_turn the only exit)
```
### 6.1 Roll (and the dice rules)
```
roll(p):                       # phase PRE, p == cur, not jailed
    (d1,d2) = dice(di++); tok order: to=, go+, landing...
    if d1 == d2 and doubles == 2:  send_to_jail(p); tok("jail3"); done            # third double: no move, no landing
    pos' = (pos + d1 + d2) mod n; if pos + d1 + d2 >= n: cash += pass_go; bank -= pass_go; tok("go+200")
    land(p, d1 + d2, roll)
    if d1 == d2: doubles += 1; tok("doubles=<doubles>")     # only if not sent to jail and not in debt/BUY (those resolve first)
    phase after the whole call: BUY | DEBT | PRE (doubles owed and none of the above) | POST
```
Doubles are owed even if the player is in BUY or DEBT; they roll again after the decision resolves (state `doubles_owed = 1` derived: the last roll row's `doubles=<k>` token with no later `roll` row in the same turn).
`jail_roll(p)`: dice; if doubles: `jail[p] = 0`, move by the total, `land()`, **no further roll** (doubles do not repeat from Jail); else `jail[p] += 1`; if this was the 3rd failed turn: pay 50 (via `charge`; a shortfall is a debt), leave Jail and move by the total; else stay (`stay;fails=<n>`). `jail_pay` and `jail_card` clear Jail and leave the player in PRE to `roll` normally.
### 6.2 Cards
```
draw(p, deck):
    c = deck_order[deck][draws[deck] mod 16]; draws[deck]++        # kept cards are skipped
    tok("card=<deck>:<c.n>")
    switch c.kind:
      move_to:       move_with_go(p, a); land(p, last_total, card)
      nearest_rail:  q = next rail tile ahead of pos[p] (wrapping, strictly ahead); move_with_go(p, q); land with "double rent if owned"
      nearest_util:  q = next util tile ahead (wrapping); move_with_go(p, q); if owned by someone else: (d1,d2)=dice(di++); tok("cardroll=d1,d2"); charge(p, 10*(d1+d2), owner)
      move_back:     pos = (pos - a) mod n; land(p, ..., card)        # no Start money going backwards
      go_jail:       send_to_jail(p); tok("jail")
      keep_card:     kept[deck] = p; tok("keep=<deck>")            # the card leaves the deck order until jail_card returns it to the bottom
      collect:       cash += a from bank; tok("collect+<a>")
      pay:           charge(p, a, BANK); tok("pay-<a>")
      collect_each:  for each other active q in seat order: charge(q, a, p); tok("each+<a>")   # q may enter the debt queue
      pay_each:      for each other active q in seat order: charge(p, a, q); tok("eachpay-<a>")
      repairs:       charge(p, a*houses(p) + b*hotels(p), BANK); tok("repairs-<n>")
```
**Edge cases the harness must hit:** a card that moves the player onto a card tile (draw once more, once); `nearest_rail` from Luck 3 (idx 36) wraps past Start and pays Start money; `nearest_util` on an unowned util offers it for sale (BUY) with no dice; a `collect_each` where one payer is short (that payer enters DEBT out of turn); `repairs` with no buildings is `repairs-0`.
### 6.3 Building, selling, mortgaging (verb-level rules)
`build t`: refuse unless t is a street, p owns every tile of its group, no tile of the group is mortgaged, `buildings(t) < 5`, p can pay the house cost (a hotel: the same cost), the even rule holds after the build (`buildings(t) + 1 - min(group) <= 1`), and the supply has a house (or, for the 5th building, a hotel). Effect: `cash -= cost; bank += cost`, supply updated (a hotel returns 4 houses). `sell t`: refuse unless t has buildings and the even rule holds after the sale; effect: `cash += cost / 2`, supply updated; a hotel sold with fewer than 4 houses in the supply: refuse (sell it down in the order the engine offers: the owner picks tiles one house at a time, so this cannot trap them; the harness covers the shortage case, decision O6). `mortgage t`: refuse unless p owns t, it is unmortgaged and no tile of its group has buildings; `cash += price / 2`. `unmortgage t`: refuse unless mortgaged and `cash >= unmortgage_cost`; effect as 4.2. Every refusal is a sentence on stderr and exit 2, nothing appended.
## 7. Trading (the chat commands and the menu, same verbs)
**What the owner asked (SAID 2026-11-07):** "trade in chat as commands suggesting and rebuttal on a menu, deal till deal is made, allowing selection of either entities, inventory, money."
> **AS BUILT 2026-11-08 (M9 ENGINE slice; the screens are NOT built).** Verbs `offer`, `counter`, `amend`, `accept`, `reject`, `cancel`; the bank row `expire` is appended by the `end_turn` call (skipped when that call ended the game). State derived by replay (`OF[]` open offers, `tr_used[]` allowance, no file). Assets are written canonically (tiles ascending, then cash, then cards, `-` = nothing) so a hand-edited row is refused. **Deviations / decisions:** (1) `accept` waits while an AUCTION is open (money and tiles would move under the bidders); the offer stays open and everything else stays legal, so the 7.8 non-blocking rule holds. (2) An offer whose proposer or addressee went bankrupt is dead (derived, no row). (3) `counter` closes the countered offer and opens a new one (`re=<id>`). (4) The accept result lists the interest each side paid: `interest-<paid by the proposer>;interest-<paid by the addressee>`. (5) Invariant I20 in `check`. (6) Pets ANSWER offers by a labelled STOPGAP rule (tile price, tripled when the swap completes or breaks a colour group; accept a positive gain, bold and coin also a zero gain; removal condition M10). Pets do not propose yet. Proof: case `dsk_trade` (113). **Not built: the menu (7.7), the chat parser (7.6), pet proposals.**
### 7.1 The negotiation as ledger rows
`offer` (proposer -> addressee) opens a negotiation `k`. The addressee may `accept`, `reject` or `counter`. A `counter` is a NEW offer from the addressee back to the proposer (`re=<k>`, `round=<r+1>`), which the original proposer may now accept, reject or counter, and so on. **The negotiation ends** with `accept` (a deal), `reject`, `cancel` (the proposer withdraws), `expire` (the proposer's turn ended with the offer open), or when `round` would exceed `trade_max_rounds` (default 6: then the engine appends `expire` instead of allowing another counter). **Only one open negotiation per proposer**; opening a new `offer` while one is open is refused.
### 7.2 Validity (checked when the offer is made AND again when it is accepted; the second check is the one that matters)
Each side's assets: a `prop:<idx>` must be owned by that side, unmortgaged or mortgaged (both may be traded), and **no tile of its group may have buildings** (sell them first); a `cash:<n>` must satisfy `1 <= n <= cash` at the moment of the check; a `card:<deck>` must be held by that side. Both parties must be active, distinct, and the offer must not be empty on both sides. **At accept time the engine re-validates everything** (things change between offer and accept: the cash was spent, the tile was mortgaged-then-sold-buildings, the card was used) and refuses a stale offer with a sentence; the row is not appended.
### 7.3 The swap is atomic, and mortgaged tiles carry a charge
`accept` moves every asset in one step: tiles change owner (keeping their mortgage flag), cash moves, cards move. For **each mortgaged tile that changes hands**, the receiver pays the bank **10% of the mortgage value, rounded up, at once** (section 4.7); the result row lists it (`interest-<n>`). If the receiver cannot pay that interest in cash after the swap's cash movement, the accept is **refused** (they must first raise cash). The receiver may lift a mortgage in a later `unmortgage` action, paying the normal unmortgage cost. After the swap, a full-set check is recomputed by `rent_due` automatically (nothing is stored).
### 7.4 Who may trade, and when
Only the **current player** may open an offer, and only in PRE or POST (never in BUY or AUCTION); a **debtor** (the first in the debt queue) may also open an offer during DEBT, because trading is how they may raise cash. The **addressee** may answer at any time the offer is open, including out of turn (the same way `seat` is legal out of turn). A debtor may accept an offer that gives them cash. Trades with the bank are not trades (use mortgage and sell).
### 7.5 The asset kinds (entities, inventory, money) and how they extend
`assets.pdl` (CONFIG): `ASSETKIND | id | enabled | note`. Classic ships `prop`, `cash`, `card` enabled and `item` and `entity` disabled, so the grammar already has the slot: an `item:<id>` is an inventory item of the player's pet entity, an `entity:<uid>` is a pet or piece (a piece is one uid in the user's `pals/`, see the house's pieces notes). Enabling them needs a **resolver** op that proves ownership from the owner's inventory ledger (never from a file the trade writes), and a rule that an asset leaves one ledger and enters another only through that ledger's own verb. **Do not enable them in this pass**; the point of declaring them now is that the UI's asset picker and the chat parser are written against a kind list, not against three hard-coded kinds.
### 7.6 The chat front door (text in, the SAME verbs out)
The editor's game chat (the pets' chat bar that Play now shows) is one door; a menu is another; both end in the canonical verbs of 5.2.1. **Never write a second path that appends rows.** The chat parser lives in the window layer (it is a projection of keystrokes into a verb call) and is a pure function with its own harness case.
| Chat text (case-insensitive, extra words ignored) | Canonical verb |
|---|---|
| `offer p3 give Navy 2 and 100 get Pink 1` | `offer p3 give prop:39+cash:100 get prop:11` |
| `offer p3 give 200 get Line 1` | `offer p3 give cash:200 get prop:5` |
| `counter 4 give 150 get Pink 1` | `counter 4 give cash:150 get prop:11` |
| `accept` or `accept 4` | `accept <the one open offer addressed to me, or 4>` |
| `reject` / `cancel` | `reject <id>` / `cancel <id>` |
| `bid 120` / `pass` (in an auction) | `bid <tile> 120` / `pass <tile>` |
| `build Red 2` / `sell Red 2` / `mortgage Line 3` / `unmortgage Line 3` | the matching verbs with the tile idx |
| `roll` / `buy` / `skip` / `end` | `roll` / `buy` / `skip` / `end_turn` |
Name resolution: tile ids and names from `board.pdl` (the 40 `id` values), case-insensitive, unique prefix allowed; money words are plain integers; an ambiguous or unknown name is **refused in chat with the candidates listed** and appends nothing. The parser reports what it understood (`offer p3: you give Navy 2 + 100, you get Pink 1`) before sending, in the chat, so a human sees exactly what the row will say.
### 7.7 The menu (the rebuttal loop)
When an offer addressed to a human is open, the `ui` feed (10.3) sets `show_offer=1` with the two asset lists spelled out, and offers **Accept, Reject, Counter**. **Counter opens the asset picker**: two columns (what you give, what you get), each a checklist of that player's tradable tiles (greyed with the reason when a tile cannot be offered: buildings in its group), a cash entry in steps of 10, a card toggle; the picker's Send button calls the `counter` verb. The loop repeats until a deal or an end (7.1). Pets run the same loop through `decide` (section 11), one move per tick.
### 7.9 Research notes, 2026-11-08 (what the web search did and did NOT find)
Thin: no source describes digital Monopoly trade UI. Found and used: (a) the standard rule for a mortgaged tile changing hands (the receiver pays 10% of the mortgage value at once) is stated in the Hasbro rule sheets (see https://fgbradleys.com/wp-content/uploads/rules/Monopoly_Rules.pdf and https://monopoly.fandom.com/wiki/Trade); (b) an open-source Monopoly simulator (https://arxiv.org/pdf/2103.00683) allows ONE pending offer at a time and cancels competing offers when a trade completes: we keep one open offer per proposer and validate at accept time, so a competing offer for a moved asset is refused as stale; (c) concurrent negotiation research (https://homepages.cwi.nl/~baarslag/pub/Bargaining_Chips-Coordinating_One-to-Many_Concurrent_Composite_Negotiations.pdf) supports non-blocking pairwise negotiations. Nothing found on a PER-TURN offer allowance: that rule is our own design (the owner's 7.8). Nothing found on a drag-and-drop trade panel for this engine: the menu is designed from the editor popup's own capabilities.
> **MENU DESIGN DECISION (owner 2026-11-08, SAID: "menu to show both traders inventory and 2 empty panes for dropping things in or out for proposing deals" and "can use nav to click in or out somehow").** No drag and drop: the editor popup has text rows and nav-numbered buttons only, and a nav digit + Enter or a click presses a button. So: left pane = the two traders' tradable inventories (tiles, cash steps, cards; greyed with the reason when not offerable), right = two EMPTY offer panes (what I give / what I get). Pressing an inventory item MOVES it into the matching offer pane; pressing an item in an offer pane MOVES it back out; cash is a stepped control (+10 / -10). Buttons: Send (calls `offer`, or `counter` when answering, or `amend` on my open offer), Cancel. Every button runs the SAME canonical verbs (7.6). The picker state is a PREFERENCE (a draft that changes no outcome), never the ledger. **STATUS: BUILT 2026-11-08 for offer / accept / reject (four panes `tpa tpg tpr tpb` placed by `canvas-overlay-at` rows in the editor layout, filled from the driver feed; 4 tiles per page; popup moved under them). NOT built: Counter / amend buttons, a harness case. Found live: `deskopoly_ctl` rejected digits and + (now allowed).**
### 7.8 Owner requirement, 2026-11-08: trading must never block play, with a per-turn offer allowance and editable terms (SAID; "that is a big deal")
Owner: "a trading menu system that doesn't block gameplay but lets players offer some amount of trades per turn and to modify terms." Status: **NOT BUILT** (M9). This subsection is binding for the M9 build and adds three rules to 7.1-7.7:
1. **Non-blocking.** An open offer is never a phase. While an offer is open, every other legal verb stays legal for everyone (roll, buy, build, mortgage, bid, end_turn), and the engine, the pets and the clock keep running. A human addressee deliberating costs nothing: the game does not wait for an answer, and an unanswered offer simply stays open until it ends (7.1: accept, reject, cancel, expire, or the round cap). The only things refused while an offer is open are what 7.2 already refuses (a stale accept), never a turn action. Test case: with an offer open to a human seat, three other players complete a full round and the ledger keeps growing.
2. **A per-turn allowance.** `rules.pdl` gets `RULE | trade_offers_per_turn | 3` (owner may change it; 0 turns trading off). A proposer may open at most that many NEW offers in one of their own turns; counters inside a negotiation do not count against it. The `offer` row that would exceed it is refused with `result=blocked:offer-allowance` and the ui feed shows the remaining allowance (`trade_left=<n>`). The counter is derived from the ledger by replay (offers by `actor` since their last `turn` row), never stored.
3. **Editable terms.** The proposer may change their own open offer with `amend <k> give <assets> get <assets>` (same validity as `offer`, the addressee is told it changed, and `round` does not advance; an amend counts as no new offer). The addressee changes terms with `counter` (a new offer back, 7.1). Every edit is a ledger row, so a pet or a harness can replay the whole negotiation.
Proof cases to add to `dsk_trade`: the allowance (3 opened, the 4th refused, a counter not counted, a new turn resets it), non-blocking (a full round with an open offer to a human), amend (terms change, `round` unchanged, the stale accept of the old terms refused).

## 8. Auctions (the video-game way)

> **AS BUILT 2026-11-08 (M7).** Phase `auction` (7); verbs `bid <amount> [as=pN]` and `pass`; `skip` records `declined;auction`; the call that records the last bid or pass appends the bank row `auction_end` (`sold-to=pN-amount` or `unsold`). The state (standing bid, high bidder, who passed, whose turn) is DERIVED by replay: no state file. Defaults in code (minimum 10, step 10) so existing rule hashes are unchanged; optional `RULE auction_min_bid`, `auction_min_step`, `auctions=off`. A ledger written before M7 (`declined`) replays without an auction. Invariant I19 in `check`. Pets bid by a STOPGAP policy (minimum raise while within price x 130% bold / 85% cautious / 100% coin and the cash; removal condition: M10 personalities). **Not built: auctioning the tiles of a bankrupt player (they return to the bank unsold).** Proof: case `dsk_auction` (57), live editor script `tests/dsk-seat-auction.pdl` (28 steps, PNGs opened). Driver: Bid N / Bid N+50 / Pass buttons, the amount is read from the feed at click time; bid and pass are "quiet" for the page rebuild.
**When:** the instant a player records `skip` on a tile offered for purchase (a player who cannot afford it can only skip), and sequentially for every tile of a player who goes bankrupt to the bank (9.3). **Rules:** all active players, including the one who declined, may bid. **Order:** starting with the player after `cur` (clockwise), the decliner last. On their turn a bidder must `bid <tile> <amount>` or `pass <tile>`. A bid must be an integer, at least `auction_min_bid` (default 10) for the first bid and at least the standing bid plus `auction_min_step` (default 10) afterwards, and **at most the bidder's cash** (so a won auction can always be paid; nothing else moves cash during an auction because building and trading are refused). A `pass` drops that player out of this auction for good. **End:** when every player except the standing high bidder has passed, the engine appends `auction_end` with `sold-to=<id>-<amount>` and moves the cash (the bank receives it, the tile gets the owner); when every player has passed without a bid, `auction_end` is `unsold` and the tile stays with the bank (it will be offered again on the next landing). A player who can afford nothing must still `pass`; pets do it automatically on their tick. **No timers** (a human may think as long as they like); the engine never auto-passes a human.
**Refusals to test:** a bid below the minimum, below the standing bid plus the step, above cash, by a player who already passed, out of bidding order, on a tile that is not the auction's; `buy`, `build` and `offer` while an auction is open.
## 9. Debts and bankruptcy
### 9.1 The debt queue
`charge` that cannot be paid in cash appends a record to the **debt queue** (`{debtor, creditor|bank, amount}`); the result carries `debt=<n>-><creditor>`. The phase is DEBT for the **first** debtor in the queue. While in DEBT the debtor may `sell`, `mortgage`, `offer` (and others answer), `pay_debt` (legal only when `cash >= amount`: it pays, pops the record, and returns to where the action began) or `bankrupt`. No one else acts except to answer the debtor's offers. Several debtors can queue (a `collect_each` card: every short payer, in seat order); they resolve one after another.
### 9.2 When is bankruptcy allowed or forced
`bankrupt` is refused while `liq(debtor) >= amount` ("you can still pay: sell or mortgage"), because the player is not insolvent. When `liq(debtor) < amount`, the engine refuses everything but `sell mortgage offer pay_debt bankrupt` for them; a human may try to trade their way out, or declare `bankrupt`; a pet's `debt` decision declares it as soon as no trade is open and `liq < amount`. (An optional `RULE | auto_bankrupt | 1` makes the engine append the `bankrupt` row itself; default 0, so a human is never pushed.)
### 9.3 What `bankrupt` does (the fix for F3)
* **To a player (creditor = a player):** the debtor's buildings are first sold to the bank for half cost (the cash goes to the debtor, then everything the debtor holds goes on); then **all** remaining cash, all tiles (mortgage flags preserved), and all Get Out cards go to the creditor; the creditor pays the 10% interest for each mortgaged tile at once (a shortfall puts the creditor into DEBT to the bank, which resolves normally). The debtor is marked `out`. If several creditors queued (a `pay_each` card), the first creditor in seat order receives the assets (decision O9).
* **To the bank:** buildings are sold to the bank, mortgage flags are cleared, Get Out cards return to the bottom of their decks, and **each tile is auctioned in tile-index order** (section 8; the auction's bidders exclude the bankrupt player). Cash that reached zero stays zero.
* The row: `bankrupt | id | <creditor or bank> | tile | out;cash=<n>;props=<k>;cards=<k>`. After it, if only one player is active the engine appends `over` (`winner=<id>`); otherwise play continues with the next active player in PRE (they must roll; CORRECTED 2026-11-08: the first draft said POST, the engine and its tests do PRE).
## 10. Sign-up, setup, the window and the tick
### 10.1 Sign-up (the lobby) and the opening
A schema-2 game starts from an **empty state folder**. `deskopoly <app> <state> lobby <ruleset>` appends the `lobby` row (records the ruleset name and its hash) and opens the lobby. A player is added by `join <name> <human|pet> [personality] [token]`: the engine assigns the next free id (`p1`..`p8`) and the lowest free token index, appends `join`, and **prints the id**. Rules: 2 to 8 players, names unique and 1 to 16 characters from `[A-Za-z0-9 _-]`, no `|`; a `pet` seat's personality must exist in `personalities.pdl` (default: the first); `leave <id>` is legal only before `start`. `start [seed]` is refused with fewer than 2 or more than 8 players; it appends `start` (with the seed, the player count and the rules hash) and shuffles the decks. **Defaults for a window's "New game" button:** the human who pressed it joins as `human`, three `pet` players join (so a person can watch or play at once), then `start`. Then the **opening order**: each player in join order appends `order` (dice consumed); the highest total moves first; a tie re-throws among the tied only; the engine appends `first`. The `sign-up` UI is a Seats bar plus a "Add player" menu that calls `join` and `leave`; a human may take over any pet seat later with `seat` (a ledger row, as today).
### 10.2 What replaces the roster in `rules.pdl` (finding F6)
`rules.pdl` keeps `RULE` rows only (`start_cash`, `pass_go`, `max_players`, `jail_fine`, `auction_min_bid`, `auction_min_step`, `trade_max_rounds`, `safety_rounds`, house rules, `schema`, `features`). The four default `PLAYER` rows are removed from `classic` (they stay in `mini`, labelled as a regression fixture with its removal condition: when the owner retires the old harness).
### 10.3 The window feed (`ui` verb): a PROJECTION, rewritten atomically
Keep every key the current `ui` writes (`game, n_players, round, over, current, current_name, current_driver, human_turn, show_roll, show_buy, show_skip, show_end, pending_tile, pending_price, <pid>_name/_driver/_cash/_pos/_out/_props/_turn, log_0..4`) so the existing template keeps working, and add: `phase` (lobby, order, pre, buy, auction, debt, post, over) · `doubles` · `show_jail_pay`, `show_jail_card`, `show_jail_roll` · `show_build`, `show_sell`, `show_mortgage`, `show_unmortgage` (each `1` or EMPTY) with `buildable_tiles`, `sellable_tiles`, `mortgageable_tiles`, `unmortgageable_tiles` (comma lists of idx) · `auction_tile`, `auction_high`, `auction_high_by`, `auction_turn`, `show_bid`, `show_pass`, `bid_min` · `debt_amount`, `debt_to`, `show_pay_debt`, `show_bankrupt` · `offer_id`, `offer_from`, `offer_give`, `offer_get`, `offer_round`, `show_offer` (for the addressee), `show_counter`, `show_accept`, `show_reject`, `show_cancel` (for the proposer) · per tile `t<idx>_owner`, `t<idx>_houses`, `t<idx>_mort` (so the window can draw the board without parsing the ledger) · `winner`. **Rule (house):** a `show_*` key is `1` or EMPTY, never the string `0` (a `0` string counts as visible). Size the buffer for 8 players and 40 tiles (32 KB) and **fail loudly on overflow** (F11).
### 10.4 The tick (watching a game)
`tick` performs ONE step for the current seat: a **pet** seat performs its next decision (`decide`, section 11) and the engine appends the row; a **human** seat waits (`TICK waiting ...`, exit 0). The editor's daemon calls `tick` every `tick_ms` (read from `tuning.pdl`; default 1500, the cdda family's pace; the machine is weak, so never lower it without measuring idle and active CPU with a browser open, and state the cost in the commit as AGENTS.md requires). `autoplay` stays: it loops `tick` until a human is up or the game ends, and now prints the winner from the `over` row. **Auctions, debts and trade answers are ticks too:** when it is a pet's turn to bid or answer, the next tick is that pet's decision, so a watched game never stalls on a pet and always stalls on a human (by design).
### 10.5 Drawing the board (`page`, the editor page; a projection)
The ring of 40 tiles has side `n = 11`; with `block 3` the page is 33 x 33 cells (the page cap is 64 x 64). Colour by group: ten groups (eight colours, rails, utils): `page.pdl` gets `TILEMAP | group0` .. `group9` and `owner_p1` .. `owner_p8` (today it has `group0..7` and `owner_p1..4`; the loader reads `group%d` with `g < 8` and `owner_p%d` with `g <= MAXP`: raise both). Choose atlas indexes from the 43-tile atlas already in `deskopoly-pchq/.../cells.rgba` (indexes in use: 1 grass, 2 planks, 7 sand, 32 dirt, 33 sword sign, 35 orange, 36 rough stone, 37 dark boards, 38 blue tile, 39 and 42 parquet, 40 orange diamond, 41 white stone) and never invent an index outside it (the loader refuses). **Buildings:** raise the extrusion depth value of a tile's cells with its building count (`cd = 2 + houses`, hotel = 7) so the 3D engine shows buildings as height. The page is a projection of `board.pdl` plus the replayed ledger: it decides no rule.
### 10.6 Sidebars (play mode)
`layouts.pdl` already declares the Seats bar (`side_game_seats`, right, play only). Add `Trade` and `Auction` bars (right slot, play only, **same slot and group as Seats so they are exclusive**, shown by the `ui` keys `show_offer` and `show_bid`). The editor's play/edit switch (`ui_mode_op`) and the page hooks (`page_hooks.pdl`) already exist.
## 11. How pets decide (and why the engine contains no pet style)
**Finding F12 and decisions D48/D52:** the brain is data; the engine is a deterministic evaluator. **Personalities:** `personalities.pdl` (CONFIG), rows `PERSONALITY | id | name | greed | caution | aggression | builder | trader | jail_patience | note`, each trait an integer 0 to 100. The three old styles become rows: `bold` (greed 90, caution 5, aggression 80), `cautious` (greed 40, caution 80), `coin` (a 50 percent random buy through `mix`). **Decisions** (each is a BRAIN row in `verbs.pdl`: `BRAIN | dec_key | verb | phases | ok_outcomes | bad_outcomes | note`, the D48 grammar): `buy` (`buy` or `skip`), `bid` (`bid` or `pass`, and how much), `build`, `sell`, `mortgage`, `unmortgage`, `jail` (`jail_pay` or `jail_card` or `jail_roll`), `trade` (propose; answer with `accept` or `reject` or `counter`), `debt` (what to sell or mortgage first, when to `bankrupt`). **Evaluator (deterministic):** `score(action) = base(action, state) + sum(trait x factor) + w x bank_weight(action)` where `bank_weight` is the Laplace weight `(reward + 1) / (reward + punish + 2)` the decision bank already computes from `decisions.txt` (`dec_op`), `w` is a data constant, and ties are broken by `mix(seed, di, domain)`. The numbers (`base`, factors, `w`) are DATA in `personalities.pdl` and `rules.pdl`, with a worked example per decision; the C only evaluates. **Outcomes:** every pet decision appends one `DEC|unix|stamp|actor|state|action|target|outcome` row (states `forsale auction build jail debt trade`; outcomes `ok blocked`), a refused decision is logged `blocked` (F20). **Learning policy is per game** (owner 2026-11-07: "change traits or learn is game type dependent"): `RULE | learn | fixed` for deskopoly (traits never change; the bank weights still inform choices); other games may say `bank_weights` or `proposals_reviewed` (TSC's P7: changes arrive as reviewed proposals). **A trait change is always a ledger row** (who proposed, who approved), never an edit of `personalities.pdl` by the game.
## 12. The proof plan (every claim in this document becomes a harness assertion that can fail)
### 12.1 How a harness is written here (read `&.widgits/_shared-lib/harness/README.md` first; the traps below cost real runs)
A harness is **pal + a case `.pdl` + compiled C ops**: `<name>.pal` is three lines (`exec ./ops/+x/harness_case_op.+x cases/<name>.pdl`, `exec ./ops/+x/harness_verdict_op.+x cases/<name>.pdl`, `halt`); the case file has `RESULTS`, `ENV`, `NEEDENV`, and `CASE | name` blocks with `SCRATCH |` (a `/tmp/hc_*` dir called `$T`), `COPY` (from `$HOUSE` into `$T`, keeps the mode), `WRITE`, `RUN`, `CAPTURE`, `EXPECT_EXIT / EXPECT_OUT / EXPECT_OUT_IS / EXPECT_HAS / EXPECT_LACKS / EXPECT_FILE / EXPECT_NOFILE`, `WAIT_HAS`, `SLEEP`. **Traps (all hit on 2026-11-07):** (1) `EXPECT_HAS` and `EXPECT_LACKS` are **silently dropped** when the case has no `SCRATCH |` (no pass, no fail); (2) they read only the **first 16 KB** of a file: for a big file use `RUN | /usr/bin/grep | -qF | <text> | <file>` plus `EXPECT_EXIT`; (3) a literal `|` inside a case row is written `\p`, a newline `\n`; (4) the results file is **append-only**: `rm -f results/<name>.txt` before a run or two runs read as a doubled total; (5) a fixture file that has pipes is easier as a file under `harness/fixtures/<case>/` and a `COPY` row; (6) the `CAPTURE` buffer is 16 KB: use the `ledger_hash` verb instead of capturing a ledger; (7) every refusal case needs its own `EXPECT_EXIT | 2`; (8) prove a check can fail: for each new family write one deliberately wrong expectation, watch it FAIL, then delete it (record that you did in the commit message). Run a case as `cd harness && rm -f results/X.txt && ./ops/+x/harness_case_op.+x cases/X.pdl >/dev/null; ./ops/+x/harness_verdict_op.+x cases/X.pdl | tail -1`, behind `cpu_gate` when it compiles anything. **Never put a number from this document into a pin**: a pin is the number you observed on the first run, with the command that made it written in a `NOTE` row.
### 12.2 The cases (names are suggestions; keep the coverage). Each opens with a comment listing what it pins
| Case | What it proves (each family needs at least one negative) |
|---|---|
| `dsk_data` | the classic loader: 40 tiles; the counts of 4.1; every price, rent row and house cost against the table (the harness reads the data file, so this pins the **data** to the spec); the loader refuses a short rent row, a missing group, out-of-order idx, a bad kind, a 41st tile |
| `dsk_setup` | `lobby`, `join` (ids assigned in order, tokens unique, names validated, 2 to 8 players), `leave`, `start` refused with 1 or 9 players, `order` ties re-throw, `first` is the highest, rules hash recorded |
| `dsk_dice` | dice are a pure function of (seed, di); doubles roll again; the third double goes to Jail without moving; a roll whose recorded dice differ is refused |
| `dsk_jail` | every way in (Go Jail, third double, a card) and out (fine, card, doubles, third failed turn forced fine); no Start money on the way in; rent still collected while jailed |
| `dsk_rent` | street base, doubled set, 1 to 4 houses, hotel; rail 1 to 4; util 4x and 10x; the nearest-rail card doubles; mortgaged pays 0; own tile pays 0; Start money on pass and on landing |
| `dsk_cards` | each deck is a permutation of 16 for several seeds; every effect kind; the edge cases of 6.2 (card onto a card tile, wrap past Start, short payer in `collect_each`, repairs 0) |
| `dsk_build` | build only on a full unmortgaged set; the even rule on build and sell; the supply (32 and 12) and the shortage refusal; a hotel returns four houses; sale at half |
| `dsk_mortgage` | mortgage and unmortgage costs and rounding; no mortgage with buildings in the group; mortgaged tile pays no rent |
| `dsk_auction` | opens on `skip`; order of bidders; minimum and step; cannot bid above cash; pass is final; ends with a winner or unsold; refusals of 8; bankruptcy to the bank auctions in tile order |
| `dsk_trade` | every validity refusal of 7.2, a stale accept refused, atomic swap, the mortgaged-tile interest, counter rounds capped at `trade_max_rounds`, `expire` on `end_turn`, one open negotiation per proposer, an offer refused in BUY and AUCTION, a debtor may offer in DEBT |
| `dsk_debt` | a debt queue with two debtors; `pay_debt` only when cash covers; `bankrupt` refused while `liq >= amount`; bankruptcy to a player transfers cash, tiles (mortgages kept), cards and charges the interest; to the bank sells buildings and auctions |
| `dsk_replay` | tamper tests: change one result, one die, one owner token, delete a middle row, truncate the last row, edit `rules` after the game started (hash mismatch), a row before `lobby`; each is **refused with a line number** |
| `dsk_ghost` | `audit` flags an extra file; invented files change nothing; deleted projections regenerate identical bytes; `state.pdl` manifest complete |
| `dsk_seats` | a human issuing the verbs a pet would issue makes the SAME game (compare `ledger_hash seat`); `tick` waits on a human, steps a pet, and also bids, answers offers and handles debt for a pet |
| `dsk_ui` | for every phase the `ui` keys are right, `show_*` are `1` or EMPTY, nothing is offered that the engine would refuse, the buffer does not truncate at 8 players with buildings (overflow fails loudly) |
| `dsk_chat` | the parser table of 7.6, name resolution, ambiguity refused with candidates, nothing appended on a parse failure |
| `dsk_fuzz` | `FUZZ_N` seeded games (default 20; 200 for a nightly) for 2 to 8 players through `autoplay`, `check` after every 50 events and at the end; every game ends with an `over` row; I1 to I22 hold; no refusal in a pets-only game (a pet never proposes an illegal move) |
| `dsk_mutate` | for each invariant I1 to I22, corrupt a copy of a finished ledger so exactly that invariant breaks and assert `check` fails and names it |
| `dsk_mini` | the OLD `deskopoly` case, untouched, against the `mini` ruleset: 105 of 105 and the same hashes (see 14) |
| `dsk_speed` | a 4-player classic game replays within the measured budget (record the measurement, do not invent it); the ledger and `ui.txt` sizes stay under their caps |
| `dsk_page` | the 40-tile page: size 33 x 33, colours by group, owners for 8 players, building height, the page is byte-identical when regenerated, an atlas index outside the atlas is refused |
| `dsk_live` (LIVE, gated by `LIVE_OK`) | the editor opens the book, a window shows the board and the phase buttons, relay clicks play a turn (roll, buy), PNGs saved; restores every live file by hash afterwards |
### 12.3 The live proof and the presentation
When M1 to M12 pass: launch the real editor on the deskopoly book **on a scratch copy** (never the owner's pages), drive it with the per-pid relay (not xdotool), photograph with `dump_frame_png_op.+x`, and record: a start, a roll, a purchase, an auction in progress, a trade offer with the menu, a bankruptcy. Then make a narrated deck with the existing pipeline (`10-user-docs/PRESENTATION-VIDEO-PIPELINE.md`: `harness/cases/deck_video.pdl`, `video_op`, narration on the LAN host) whose slides state only what was run, and export the video to `XO/<month>/<nov-N>/`.
## 13. Files, caps and performance (the runtime-file standard applies)
| File | Bound | Why |
|---|---|---|
| `state/ledger.txt` | one game per state folder; a game ends (`stalemate`) at `safety_rounds` (default 400 rounds) OR at `max_rows` (default 60,000 rows, about 6 MB), whichever comes first; the engine appends the `over` row before the cap and refuses any later verb | 8 players x 400 rounds x about 10 rows is about 32,000 rows; a typical 4-player game is about 2,500 rows (measure it; do not trust this estimate) |
| `state/decisions.txt` | at most one row per pet decision, same bound | |
| `state/ui.txt` | rewritten atomically, at most 32 KB | |
| `state/mechanics.txt` | rewritten atomically, at most 64 KB | |
Add one row per file to `03-pitfalls/RUNTIME-FILE-GROWTH-STANDARD.md`, with files and bytes per hour: a watched game appends at most one pet decision per `tick_ms`, so at 1.5 s that is at most 2,400 rows per hour (about 240 KB per hour) per game and only while a window is open. `cpu_guard`'s `hygiene_sweep()` is the second guard: add the state folder to it. **CPU:** each tick forks the op once, which replays the ledger; measure idle and active cost with a browser open (`cpu_gate check` and the house's footprint line) and write them in the commit message.
## 14. Migration from the 16-tile engine (so nothing that passes today breaks, and no pin is edited to make a test pass)
1. **Baseline first:** build today's op (`cpu_gate`), run `cases/deskopoly.pdl`: expect 105 of 105 and note the commit. Tag it `deskopoly-mini-baseline` in your notes (do not push tags; the owner merges).
2. **Rulesets:** move `board.pdl` and `rules.pdl` to `rulesets/mini/` unchanged (keep a copy at the old path until step 4 passes so the old case keeps its paths, or update the case's `COPY` rows in the SAME commit and say so). The engine takes `<app>/rulesets/<name>/`; a ledger whose first row is `new` is **schema 1** and replays under `mini`; a ledger whose first row is `lobby` is **schema 2** and replays under the ruleset it names.
3. **Features behind flags** (`RULE | features | doubles,jail,cards,buildings,mortgage,auctions,trading,rent_model=classic` listing what is ON): **with `features` empty and `schema 1` the new engine must reproduce the old ledgers byte for byte.** That is the acceptance test of milestone M1: the OLD case runs unchanged against `mini` and passes 105 of 105 with the same hashes (`61ac51a395a63e0d` for 334 rows without the seat rows, `9baa915d6d0846a9` with them). If any hash moves you changed old behaviour: stop and find out why.
4. **Then** add the classic ruleset and each feature, milestone by milestone (section 16). Re-pin only NEW cases.
5. The `mini` roster stays in `rules.pdl` (`PLAYER` rows) **only for `mini`**, labelled a regression fixture with its removal condition (decision O10); `classic` has no roster in config (F6).
6. **What stays:** the ledger row grammar, `seat`, `tick`, `ledger_hash`, `audit`, the DEC grammar, the `ui` keys that exist, the editor book, the Seats bar and the play/edit switch. **What changes:** results gain tokens, `new` is replaced by `lobby ... start` for schema 2, the row limit grows to 1024 bytes, buffers grow with loud overflow, the decision logic leaves C for data.
7. **Consumers to re-check after every milestone:** the harness menu row `deskopoly` (`#.desktop/harness_menu.pdl`), the editor book opening (`taskbar_harness_menu`, `book` and `page` cases), `mechanics.txt`, the decision bank (`dec_op`), the master ledger idea (D49).
## 15. Decisions only the owner can make (each has a recommended default so you are never blocked; record the answer in the decision log)
| # | Question | Recommended default |
|---|---|---|
| O1 | Tile and card names: keep the neutral house names, or rename to the owner's own theme (lore-friendly names)? | Neutral names now; the owner renames in `board.pdl` and `cards.pdl` any time, no code change |
| O2 | What does a window's "New game" create? | One human plus three pets, started at once |
| O3 | Income Tax: flat 200, or "10% or 200, your choice"? | Flat 200 (the current edition) |
| O4 | Free Parking: nothing (official) or a pot? | Nothing; the pot is a `RULE` that defaults OFF |
| O5 | Auction minimums | Opening bid 10, step 10 (both in `rules.pdl`) |
| O6 | House shortage: refuse (simple), or the official auction among the players who want the last houses? | Refuse in this pass; the official auction is a later milestone |
| O7 | Does a mortgaged tile in a set stop the doubled rent on the others? | No, it does not stop it (verify against the sheet, 4.12 item 7) |
| O8 | Interest when a mortgaged tile changes hands | The official rule in 4.7 (verify, 4.12 item 6) |
| O9 | Bankruptcy with several creditors (a `pay_each` card) | Assets go to the first creditor in seat order |
| O10 | Retire the `mini` ruleset and the old 105-check case? | Keep until the classic proof (M12) has passed and the owner says so |
| O11 | Safety cap | 400 rounds and 60,000 rows, ending as `stalemate` by net worth |
| O12 | Trade windows | Only the current player (or a debtor) proposes; the addressee answers any time (7.4) |
| O13 | Enable `item` and `entity` assets in trades | Not in this pass (7.5) |
| O14 | Auto-bankrupt when hopeless | Off; a human is never pushed |
| O15 | Tick speed | 1500 ms (the cdda family's pace), measured before changing |
## 16. The build order (do them in order; each ends committed and pushed, with its case passing)
| M | What | Done when |
|---|---|---|
| M0 | Read section 1's documents; build today's op; run `deskopoly` 105 of 105; add decision D53 (one op, sectioned; rulesets by version) | baseline recorded |
| M1 | `rulesets/`, schema and features flags, the engine reads a ruleset by name | the OLD case passes unchanged on `mini` with the same hashes (`dsk_mini`) |
| M2 | classic data loader and `dsk_data`; `lobby join leave start order first`; rules hash; row limit 1024; loud buffer overflow | `dsk_data`, `dsk_setup`, the hash part of `dsk_replay` pass |
| M3 | roll, land, Start money, tax, free, own, rent for streets only (no doubles, no cards), `rent_due` for rail and util | `dsk_rent` (streets, rails, utils) passes |
| M4 | doubles and Jail | `dsk_dice`, `dsk_jail` pass |
| M5 | cards | `dsk_cards` passes with the edge cases |
| M6 | buildings, mortgages, supply, net worth | `dsk_build`, `dsk_mortgage` pass |
| M7 | auctions | `dsk_auction` passes |
| M8 | debt queue and bankruptcy | `dsk_debt` passes |
| M9 | trading and the chat parser | `dsk_trade`, `dsk_chat` pass |
| M10 | `ui` for every phase, `tick` for every phase, personalities, BRAIN rows, DEC rows | `dsk_ui`, `dsk_seats` pass |
| M11 | `page` for 40 tiles, 10 groups, 8 owners, height; Trade and Auction bars | `dsk_page` passes |
| M12 | fuzz, mutation, ghost, speed, caps and growth rows | `dsk_fuzz`, `dsk_mutate`, `dsk_ghost`, `dsk_speed` pass; the whole suite is run twice with `rm -f results` between |
| M13 | the live proof with PNGs, the narrated deck, the XO export, D53, README rows | pictures viewed (not just written), the deck's checks pass |
| M14 | the owner's call on `mini` (O10) | decision recorded |
After EVERY milestone: commit only your paths with `commit_scoped.+x`, push, and in the message say what you ran, what you did not, and any number you observed. If a milestone cannot be finished, commit a `wip:` snapshot and write what blocks it at the top of the next work block.
## 17. Traps from this project's own history (so you do not repeat them)
* Verifying by reading is not verifying: a clean compile is not evidence; run the case, then run a deliberately failing variant.
* A results file counted twice is a doubled number: always delete before a run (the harness menu now counts only the last run).
* Never report a count from a file that may not exist; check existence first.
* Never wrap `button.sh reset` or `run` in `cpu_gate` (an outage on 2026-11-07).
* Build every binary and read its exit code BEFORE a reset: a failed build leaves the desktop stopped by design.
* Do not touch the owner's live pages: use a scratch copy of a book, fingerprint the real one before and after.
* Restore live files by hash after a live test; end windows and engines by pid, never by pattern.
* A refused action appends nothing; an action that half-succeeds is a bug.
* A fact you keep in memory or in a side file is ghost data; derive it from the ledger.
* Do not trust a table in a document (this one included) over the data file and the official rules: pin the data file, then verify the data against the sheet.
## 18. References
House: `AGENTS.md` · `@.apps/deskopoly/` (the current app) · `&.widgits/_shared-lib/harness/` (README, `cases/deskopoly.pdl`, `cases/harness_menu.pdl`, `cases/ui_mode.pdl`) · `&.widgits/_shared-lib/ops/{harness_menu_op.c,ui_mode_op.c,cpu_gate.c}` · `16.game/S1-EVENT-LEDGER.md` · `08-roadmap/design-docs/{PROPOSAL-ONE-DOOR-DRIVING-AND-THE-MASTER-LEDGER,PROPOSAL-ONE-BRAIN-PER-GAME-HARNESS-AND-PETS,PROPOSAL-PLAY-MODE-SEATS-AND-GAME-SIDEBARS,HANDOFF-NOV-7-ONE-VOCABULARY-PETS-ACT}-2026-11-07.md` · `02-architecture/{NO-GHOST-DATA-THE-LEDGER-IS-THE-AUTHORITY-2026-11-07,DESIGN-CHOICE-TEST-AND-DECISION-LOG}.md` · `03-pitfalls/RUNTIME-FILE-GROWTH-STANDARD.md` · `10-user-docs/PRESENTATION-VIDEO-PIPELINE.md` · XO `31.Nov/nov-7/` (the proof pictures and the progress video). **Outside the house:** the official rule sheet of the standard edition of the commercial game (the owner may hold a physical copy): it is the authority for every RULES number here.
**Glossary.** *Ledger:* the append-only file that is the only game authority. *Projection:* a file derived from the ledger by a named verb. *Replay:* recomputing the whole state from the ledger on every verb call. *Ruleset:* a folder of board, rules and cards data, named in the first rows. *Phase:* where the turn state machine is (section 5.4). *Debt queue:* payments that could not be made in cash. *Liquidation value:* what a player could raise by mortgaging and selling. *Pin:* a number a harness check asserts, taken from an observed run. *Ghost data:* a fact kept anywhere but the ledger.
