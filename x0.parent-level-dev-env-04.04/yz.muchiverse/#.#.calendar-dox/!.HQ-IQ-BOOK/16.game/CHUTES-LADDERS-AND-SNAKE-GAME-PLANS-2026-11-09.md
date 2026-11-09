# Chutes and Ladders, and Snake: two small games on the Teropoly stack (two plans, 2026-11-09)

Owner request, 2026-11-09: two more games, built the way Teropoly is built. (A) CHUTES AND LADDERS: a race to square 100, no decisions, pure luck, 2 to 4 players. (B) SNAKE: a trail of entities runs round a board, eats a newly spawned entity, grows, and dies on a wall or on itself; real time. The owner's words were "same game and events"; **I read that as "the house game stack": the ledger engine op, the driver, the editor's Play button, the event pages and the clock hooks, not as "one shared code base for the three games".** If the owner meant something narrower (for example literally the same event pages), say so; section 3 and the event tables (sections 17 to 19) are where that would change the plan.

Status: **PLAN ONLY, nothing built, no code written, no harness run for this document.** Every number below that is not marked FACT is a guess or an estimate and says so. Labels used throughout:

* **FACT** = I read it in the repo today (file named) or in a doc I name.
* **MEMORY** = from my memory of the real-world game; NOT checked against an official board or rule sheet.
* **PROPOSAL** = my design, nothing exists.
* **OPEN** = an owner decision, or something I could not verify.
* **NEW** = work that does not exist yet.

Companion files: decision D59 in `02-architecture/DESIGN-CHOICE-TEST-AND-DECISION-LOG.md`, class `1-1.HARNECIENT.SMOL/DAY_28_TWO_SMALL_GAMES.txt`, XO copies in `31.Nov/nov-9` (`6.chutes-ladders-and-snake-plans.md`, `7.DAY_28_TWO_SMALL_GAMES.txt`).

---

## 1. The short version

| | Chutes and Ladders | Snake |
|---|---|---|
| What it stresses in the engine | a game with NO decisions: the whole ledger is dice rows, so it is the cleanest test of replay, seeded randomness, seats and the page | REAL TIME: ticks, steering input, many moving entities, collisions; the ledger question (a row per tick is not allowed) |
| Ledger | one row per spin (about 40 spins per player per game, MEMORY) | INPUTS plus a few consequence rows, never a row per tick (section 12) |
| Where the data goes | a Markov chain: the data studio can compute the exact answer and compare played games with it | feature tables for a snake AI trained on another machine, plus play statistics |
| New engine work | small (a spin verb, a board file, a theory op) | medium (a pure tick function, a live host, a steering hook, a CPU budget) |
| Effort rank | 1 (build first) | 2 |

Both are **new apps** (`@.apps/chutes`, `@.apps/snake`) with their own small engine op and ruleset, sharing a few pieces extracted from Teropoly by text-include (section 3). Neither extends `deskopoly.c`.

---

## 2. What the Teropoly stack gives us (FACT, each row read today)

| Piece | FACT (file) | Reused by |
|---|---|---|
| Ledger shape | `EVENT\|round\|actor\|verb\|target\|at\|result`, seven `\|` fields, O_APPEND, one `write()` per row; the first row records the ruleset name and a rules hash (a real ledger: `@.apps/rpg-edit-hq/state/games/deskopoly/game/ledger.txt`, `EVENT\|0\|bank\|lobby\|-\|0\|ruleset=classic,schema=2,rules=662402adec97dd60,max=8`; spec `08-roadmap/design-docs/DESIGN-DESKOPOLY-CLASSIC-MONOPOLY-BUILD-SPEC-2026-11-08.md` 5.2) | both |
| Replay is the authority | replay recomputes every `result` and REFUSES (exit 1) a ledger whose recorded result differs; a refused action appends nothing (spec 5.5, `HOW-TO-PLAY-AND-DRIVE` section 2) | both |
| Seeded dice | `die(seed, idx, k) = mix(seed*1000003 + idx*16 + k) % 6 + 1` with a splitmix64 `mix()` (`@.apps/deskopoly/ops/deskopoly.c` lines 178-179); the dice index is derived from the rows, no stored random state (spec 5.3) | Chutes (spinner), Snake (food spawn) |
| State files declared | `state.pdl` rows `FILE \| name \| kind \| writer \| regenerate_with`, kinds LEDGER, PROJECTION, CONFIG, PREFERENCE; `audit` fails on an undeclared file (`@.apps/deskopoly/state.pdl`) | both |
| Verb manifest | `verbs.pdl` `VERB` / `FLOW` / `BRAIN` rows; `help` is generated from it | both |
| The window feed | a driver writes `state/game_ui.txt` (`gm_on`, `gmt_<i>_text`, `gmb_<i>_label/_act`, party and chat keys) which the editor draws over its own side bars; nothing else is needed for new bars (`deskopoly_driver.c` header; memory note "editor game feed overrides") | both |
| Play starts the game | the book's `game.pdl` rows: `GAME \| driver \| <program> {house} {book} {editor}` (started by the editor's Play, one instance, ends when the editor is gone), `GAME \| chat \| <program>` (typed line offered first; exit 3 = not a command), `GAME \| clock \| pause` (the world clock is paused while the feed says `gm_on=1`) (`@.apps/deskopoly/game.pdl`; `book_driver()`, `game_chat_hook()` and the clock hold in `&.widgits/_shared-lib/petslike_core.c`) | both |
| A driver watchdog | while `running=1` and the open book has a `GAME \| driver` row, a feed that does not say `gm_on=1` restarts the driver (`petslike_core.c` near line 1883) | both |
| The board page | `deskopoly page <desk> <base atlas>` writes `cells.txt` for a 33x33 page (ring of 40 tiles, block 3); `page.pdl` rows `HEADER`, `PAGE \| block`, `TILEMAP`, `LABELS`, `LABELCOLOR`, `LABEL \| <tile id or kind> \| <text>`; a tile with no LABEL row shows its own id; names are baked into the floor texture by `label_atlas()` with a 5x7 font in 5 size tiers (`deskopoly.c` lines about 1080-1220; `rulesets/classic/page.pdl`). The page cap is 64x64 (spec 10.5) | both (the ring geometry is Monopoly-only) |
| Figures | the driver writes the characters to the play desk's `events.txt`, one row per figure `x y r g b People1 <idx> <dir> <pat> <glyph>`; the 3D engine re-reads it every frame, the 2D picture blits it; the glyph MUST be a letter; walking is animated cell by cell by the driver (`deskopoly_driver.c` lines 101-134, memory note "editor game feed overrides") | both |
| Camera | `cam_snap`: brings the follow camera (mode 2) to the human seat's figure on their turn and on `Find me`; respects a free camera; hard-coded 33x33 clamps today (`deskopoly_driver.c` lines 258-320, decision D57) | Chutes (parametrize the size) |
| Table talk | one function turns a replayed ledger row into a sentence (`talk_row`), the driver pre-wraps one line per row (`TUNE talk_wrap 24`) (decisions D55, D56) | Chutes; Snake for rare events |
| Pets | `join <name> pet`; a pet seat acts on `tick` (`TUNE tick_ms 1500` in `tuning.pdl`); decisions are rows plus `DEC` rows for the bank (spec 11) | Chutes (autopilot), Snake (AI snake) |
| Harness | pal + case `.pdl` + C ops, `dsk_*` cases with negative controls, live scripts `tests/dsk-seat-*.pdl` driven by `relay_drive` (`see`, `press label:`), `nav.pdl` (HOW-TO-PLAY section 1, `ls @.apps/deskopoly/tests`) | both |
| Size of the model | `deskopoly.c` is 1,566 lines, the driver 481, the chat hook 436 (`wc -l`); decision D53: one file until about 2,500 lines | scale reference |

**Differences from the request text.** The task description gave the row shape as `EVENT|seq|actor|verb|args|round|result`; the real shape in the spec and in a real ledger is `EVENT|round|actor|verb|target|at|result` (no `seq` field; `at` is the board position after the action). Both new games keep the REAL seven-field shape so the same readers, the master ledger and the data exporters work.

---

## 3. The design-choice test: new apps, or extend `deskopoly`? (decision D59)

**Question:** build Chutes and Snake as extra rulesets inside `@.apps/deskopoly` (it already has `rulesets/<name>/` and `lobby <ruleset>`), or as new apps with their own engine op?

1. **One authority per fact?** Teropoly's engine holds Monopoly facts in static tables (`T[]` tiles, owners, cash, houses, offers, auctions, debts; the `ui` keys are `t<idx>_owner`, `p<i>_cash`, `show_buy`, ...). A third ruleset would put "squares", "snakes" and "ticks" into the same state struct. Two games sharing one struct is two vocabularies for the same file names. New apps keep one vocabulary each.
2. **Modular?** A `chutes` ruleset would need branches in `roll`/`ui`/`page`/`talk`/`invariants` (per-project knowledge in a shared op); `page` assumes the ring (`ring_xy`, `4*(side-1)`, 33x33 clamps in the driver). Snake is real time with a different ledger discipline (inputs only): it cannot be a ruleset of a turn-based engine at all.
3. **Scalable (10th game)?** With rulesets, every game pays the line count and the pinned cases of every other game (Teropoly has 22 harness cases and 1,563 checks in the last fresh run, per the class text DAY 27; a change to a shared function risks all). With new apps plus extracted PURE helpers, the 10th game copies a folder layout and includes the helpers it needs.
4. **Long term?** The roadmap points at "games are data + ledger; the screen is a projection" and at one data studio reading many providers (D58). Small separate apps are the providers the studio wants; the second provider with no studio code change is exactly its P7 acceptance test.
5. **House shapes?** Pure shared logic = text-include (2+ consumers), stateful = op + fork + IPC, data in `.pdl`, no header-plus-link (memory rule; the pure helpers below). A harness case that can fail per piece. Capped runtime files.
6. **Cost and risk of the better choice?** Extracting helpers out of `deskopoly.c`/`deskopoly_driver.c` touches pinned code. **Mitigation: extract at the SECOND consumer, never before, and gate every extraction on the existing Teropoly cases staying byte-identical (`dsk_labels` 56, `dsk_camera` 75, `dsk_page` 42, the pinned three-pet game hash).** A copy-paste would be cheaper today and would add to the audit's duplicate groups (83 at 2026-11-08, memory note on the codebase audit); not chosen.

**Chosen:** new apps `@.apps/chutes` and `@.apps/snake`, each with its own small engine op (`chutes.c`, `snake.c`), ruleset folder, `game.pdl`, `verbs.pdl`, `state.pdl`, `page.pdl`, `tuning.pdl`, `harness.pdl`, `tests/`, driver and chat hook; the file set mirrors `ls @.apps/deskopoly` (FACT: `deskopoly-pchq game.pdl harness.pdl layouts.pdl nav.pdl ops page.pdl rooms.pdl rulesets shots state.pdl tests tuning.pdl verbs.pdl`). **Shared pieces extracted by text-include into `&.widgits/_shared-lib/` (PROPOSAL, each at its second consumer):**

| Shared piece (proposed file) | Today | Who needs it |
|---|---|---|
| `game_rng.c`: `mix()`, a pick-by-index helper, the die formula | static in `deskopoly.c` lines 178-179 | Chutes spinner, Snake food, both `sim` verbs |
| `game_ledger.c`: row parse and validate (7 fields, 1024 bytes, no `\|`), O_APPEND write, rules-hash check, state-hash fold, `audit` of `state.pdl` | inline in `deskopoly.c` (append at line 756) | both |
| `page_labels.c`: the 5x7 font, `label_wrap`, `label_mask`, per-tile atlas writer | inline in `deskopoly.c` (about 1080-1220) | Chutes (100 numbers), Snake optional |
| `game_driver_core.c`: flock single instance, owner-pid exit, `running` switch poll, `action.txt` byte cursor, atomic feed write, archive of the last 3 games | inline in `deskopoly_driver.c` | both |
| `game_camera.c`: `cam_snap`, `fig_cell`, parametrized board size | inline in `deskopoly_driver.c` | Chutes; Snake if a follow camera is wanted |

**Not chosen:** (a) rulesets inside `deskopoly` (above); (b) header-plus-link libraries (house rule); (c) one generic "game engine" op that all three call (premature: three games, three different shapes of time; revisit at the fourth); (d) copying the code (duplicate groups).

**What stays per game (NEW C each):** the engine op, the board/page generator geometry, the driver's feed layout, the talk sentences, the tests.

**Not shared on purpose:** the S1 event ledger `state/events.txt` (`EVENT|ts|actor|verb|target|at|queued` + `EVRES`, `&.widgits/_shared-lib/game_events.c`, used by df-hq, cdda-hq, mcl-hq): a different shape for pets-like boards. Teropoly does not use it (FACT: it has its own `state/ledger.txt`); the new games follow Teropoly.

---

# PART A. CHUTES AND LADDERS

## 4. The rules and the board as DATA

**Name warning (OPEN):** "Chutes and Ladders" is a trademark of a toy company; "Snakes and Ladders" is the old public game. The house already renamed a product to avoid a trademark clash (AGENTS.md, LyfeDesk). Proposal: internal id `chutes` (like `deskopoly` stays internal), the player-visible title chosen by the owner (candidates: "Slide and Climb", "Snakes and Ladders", or a lore name). Same for token art.

**The game (MEMORY):** 100 squares in 10 rows of 10, numbered from the bottom left in a serpentine ("boustrophedon") path: row 1 runs left to right (1 to 10), row 2 right to left (11 to 20), and so on, so 100 is at the TOP LEFT. Each turn a player spins a 1 to 6 spinner (or throws one die) and moves that many squares. If you stop on the foot of a ladder you climb to its top; if you stop on the head of a chute you slide to its tail. First to reach square 100 wins. In the commonly sold version you must reach 100 exactly (MEMORY; this varies by edition, hence the option below). 2 to 4 players. No choices anywhere.

**The jump table, from MEMORY of the common US board. UNCHECKED: verify every row against an official board before this ruleset ships (same label as the Monopoly numbers in spec 4.12).**

| Ladders (foot to top), 9 | Chutes (head to tail), 10 |
|---|---|
| 1 to 38 | 16 to 6 |
| 4 to 14 | 47 to 26 |
| 9 to 31 | 49 to 11 |
| 21 to 42 | 56 to 53 |
| 28 to 84 | 62 to 19 |
| 36 to 44 | 64 to 60 |
| 51 to 67 | 87 to 24 |
| 71 to 91 | 93 to 73 |
| 80 to 100 | 95 to 75 |
| | 98 to 78 |

**Kept as DATA (PROPOSAL):** `@.apps/chutes/rulesets/classic/board.pdl` (the same grammar family as `rulesets/classic/board.pdl` of Teropoly, which has `TILE` rows with a comment block naming the grammar), editable by hand or in the editor's data page:

```
# board.pdl - chutes classic. FROM MEMORY, NOT CHECKED against an official board.
BOARD | cols   | 10
BOARD | rows   | 10
BOARD | path   | serpentine        (start bottom-left, odd rows left to right, even rows right to left)
JUMP  | 1  | 38 | ladder | L1
JUMP  | 16 | 6  | chute  | C1
...
```
`rules.pdl` (RULE rows only, no roster, like Teropoly's):

```
RULE | spinner      | 6          faces of the spinner, 1..N
RULE | finish       | exact_stay   exact_stay | bounce | reach   (see below)
RULE | max_players  | 4
RULE | six_again    | off        a variant: a six spins again (off in the classic game, MEMORY)
RULE | chain_jumps  | off        off = a jump's landing square may not itself start a jump (audit refuses such a board)
RULE | safety_spins | 2000       a game ends as `stalemate` after this many spins
RULE | max_rows     | 5000       hard cap on ledger rows
```
**The finish rule is the ruleset option the owner asked for:**
* `exact_stay`: you must land on 100 exactly; a spin that would pass it leaves you where you are (turn lost). Longest games.
* `bounce`: you move to 100 and walk back the surplus (from 97 a 6 goes to 100, then back 3, ending on 97; MEMORY of the usual bounce wording; define it in data as `new = 100 - (square + spin - 100)`).
* `reach`: any spin that reaches or passes 100 wins. Shortest games.
The ledger's first row records the ruleset name and its hash, so editing `board.pdl` or `rules.pdl` after a game began makes every verb refuse (FACT for Teropoly: `HOW-TO-PLAY` section 4, a test in `dsk_classic`). `audit` additionally checks the board file itself (section 9, case `cs_data`): 100 squares, no jump from or to 0 or from 100, ladders go up and chutes go down, no square is the foot of two jumps, no chain unless `chain_jumps on`.

**Square to cell:** for square n (1 to 100): row `r = (n-1) div 10` from the bottom, column `c = (n-1) mod 10` when `r` is even, `9 - ((n-1) mod 10)` when `r` is odd. Square 0 is the off-board start (tokens wait there before their first spin). PROPOSAL for the page: one extra "Start" strip row below row 1, so the page is 10 blocks wide by 11 tall.

## 5. The engine (`@.apps/chutes/ops/chutes.c`)

**Phases (three, tiny):** `LOBBY` (no `start` row yet), `PLAY`, `OVER`. No ORDER phase: seat order is join order (OPEN: the real game's "youngest first" is not data we have; a ruleset row `first = join|spin_off` can add a spin-off later). Compare Teropoly's eight phases (spec 5.4).

**Verbs (PROPOSAL; each appears in `verbs.pdl`, `help` is generated):**

| verb | who | ledger row (`target`, `at`, `result`) |
|---|---|---|
| `lobby classic` | anyone | `EVENT\|0\|bank\|lobby\|-\|0\|ruleset=classic,schema=1,rules=<hash16>,max=4` |
| `join <name> <human\|pet> [token]` | LOBBY, 2 to 4 times | `EVENT\|0\|p1\|join\|A\|0\|driver=human,token=0` |
| `leave <pN>` | LOBBY | `left` |
| `start <seed>` | LOBBY, 2 to 4 players | `EVENT\|0\|bank\|start\|-\|0\|seed=7,players=3,rules=<hash16>` |
| `spin [as=pN]` | PLAY, the current player | `EVENT\|<round>\|p2\|spin\|<value>\|<square after>\|from=47;roll=3;to=50` plus ONE of: nothing, `ladder=9>31`, `chute=47>26`, `stay` (finish overshoot), `bounce=...`, `win` |
| `seat <pN> <human\|pet>` | any time | a control change, legal out of turn (as Teropoly) |
| `over` | appended by the SAME call that ended the game | `EVENT\|<round>\|bank\|over\|-\|100\|winner=p2;spins=118` |
| read-only: `status`, `replay` (state hash), `check`, `ledger_hash`, `audit`, `ui`, `talk`, `page`, `help` | | |
| `tick` / `autoplay` | a pet seat spins on a tick; autoplay plays to the end | |
| `sim <seed0> <count> <players>` | read-only, writes NO ledger | prints one summary line per game (section 10) |
| `markov` | read-only | the exact theory tables (section 10) |
| `export <dir>` | read-only projection | the data-studio tables (section 10) |

**The spinner is a pure function of the ledger:** `spin_value = die(seed, si, 0)` with `si` = the number of `spin` rows so far (the same formula Teropoly uses, FACT); with `RULE spinner N` the modulus is N. The row stores the value in `target` and replay refuses a mismatch (the same defence Teropoly's `roll` has). **Nothing a player does can change a spin**, so *who* presses Spin, a human or a pet or the harness, never changes the game: the same seed gives the same game for any seating. That is a test (case `cs_seats`).

**Invariants `check` proves (each a harness assertion with a negative):** I1 every position is 0 to 100; I2 `spin` value is 1 to N and equals the recomputed one; I3 `si` equals the count of `spin` rows; I4 exactly one current player, and turns rotate in join order; I5 rounds never decrease; I6 a player on 100 exists iff an `over` row exists; I7 rules hash in `lobby` and `start` equals the hash of the files; I8 every row is at most 1024 bytes; I9 no jump chains unless allowed.

**Replay cost:** a game is about 7 setup rows plus roughly 40 spins per player (MEMORY figure, section 10), so 2 to 4 players is about 100 to 200 rows; replay is a few microseconds per row. Measure in case `cs_speed` (PROPOSAL target: a 4-player game replays in well under 50 ms, the same target as spec 5.5).

## 6. Seats and pets: what a pet even means here

There is no decision, so a pet has nothing to decide. **A pet seat is an autopilot:** `tick` presses Spin for it at the game's pace (`TUNE tick_ms`), exactly as the driver does for Teropoly pets (FACT: `tuning.pdl`). The ledger row is identical whether a human or a pet pressed Spin (the actor id and the value are all that is written). There is no pet policy, no `BRAIN` row, no `DEC` row and no stopgap (Teropoly's policy is a labelled stopgap, `HOW-TO-PLAY` M10).

What a pet seat adds that is not rules: (1) the figure and the name in the pets bar (the game's players ARE the pets-bar party, FACT: feed keys `party_<i>_*`), (2) optional flavour in the table talk, chosen from the seat's `personality` row by the talk function, never by the engine (OPEN: pet sprites as tokens). The game is therefore a **control experiment**: any difference between seats that the data studio finds must come from the structure of the game (the first-mover advantage, section 10), not from skill.

## 7. The window: feed, popup, table talk

**Feed (PROPOSAL, same keys as Teropoly's `ui`):** `game`, `phase` (lobby, play, over), `round`, `current`, `current_name`, `current_driver`, `human_turn`, `show_spin` (`1` or EMPTY, never the string `0`: house rule, spec 10.3), `p<i>_name/_driver/_pos/_token`, `last_roll`, `last_kind` (none, ladder, chute, stay, bounce), `last_from`, `last_to`, `winner`, `log_0..4` and `logt_0..4` (raw and sentence).

**Popup:** `Spin` (human turn only), `New game`, players `-`/`+`, `Find me` (reuses decision D57's explicit-press rule), speed. The setup choices (players, speed, seed) are window PREFERENCES; who plays a seat is a `join`/`seat` row (the no-ghost rule, FACT for Teropoly: driver header).

**Table talk sentences (PROPOSAL; one function `talk_row`, like Teropoly's; names are the join names; the numbers below are from the MEMORY board):**

* `A spun 4: square 3 to 7`
* `B spun 3 and climbed a ladder from 9 to 31`
* `C spun 5 and slid down a chute from 16 to 6`
* `A spun 6 but needs exactly 3: stays on 97` (`exact_stay`)
* `B spun 6 on 97: bounced back to 97` (`bounce`)
* `C reached square 100 and wins after 41 spins`
* `B is up (round 12)`

The ledger is never changed; sentences are a projection (`talk` prints them for the harness and the video).

## 8. The board page, tokens and camera

**Page (PROPOSAL, reuses the `page.pdl` grammar and the label baker, FACT above):** the generator is a NEW function `page` in `chutes.c` for the serpentine 10 by 10 geometry (Teropoly's `page` is a ring and is not reused; only the baker is). Block 3 gives 30 by 33 cells with the Start strip (page cap 64x64, spec 10.5). Square ids are `S1` to `S100` (ids are 1 to 16 characters, FACT), and **because a tile with no `LABEL` row shows its own id (FACT: `page.pdl` comment "no row = the tile's own id"), the numbers need no LABEL rows if the tile id is the number** (OPEN: use ids `1` to `100` rather than `S1`; check the id grammar in the engine before choosing). Colours alternate by `TILEMAP | floor_a`, `floor_b`, `floor_c`, `floor_d` (a four-colour checker, indexes picked by eye from the 43-tile atlas; stand-ins, not art, as Teropoly's comment says).

**Ladders and chutes on the page:** three steps, cheapest first.
1. *Data only, zero new code:* the foot and the top of a jump get a second label line, for example square 9 reads `9\n^31` and square 31 reads `31\nv9`, and the foot/head squares use a different `TILEMAP` index (`ladder_foot`, `ladder_top`, `chute_head`, `chute_tail`). FACT: `\n` is a line break in `LABEL` text.
2. *NEW code:* draw each jump as a thick line baked into the floor texture, the way the label baker writes text into a per-tile copy of the base tile (it already writes one atlas entry per labelled tile). Ladders and chutes get two colours and a rail pattern. OPEN: the atlas size limit with 100 labelled tiles plus about 150 tiles crossed by lines (`atlas_tiles` is a header; I did not read the loader's cap).
3. *Polish:* the figure slides along the line (below).

The owner's memory-note preference is "all blocks one voxel high" (no ramps or stilts), so no 3D height for jumps.

**Tokens as figures:** `events.txt` rows `People1` with letter glyphs `A` to `D` (FACT format), shared-square spread offsets as Teropoly's `write_events` does. Walking: one cell per step along the serpentine, pace from `W_MS`. A ladder or a chute is a *jump* in the first version (Teropoly's rule: a move that is not a short forward walk is a jump, `deskopoly_driver.c` line 107) with the sentence in the table talk; a slide along the line is the polish step.

**Camera follow:** `game_camera.c` extracted from the Teropoly driver (section 3), with the board size passed in (today the clamps are the literal 32; the 2026-11-09 clamp comment in the driver explains why: a figure on the top row was aimed at y=-4 and the view was empty sky). The whole board is 30 by 33 cells, so the 2D view (key `0`) shows all of it; the follow camera is for 3D. Cases `dsk_camera` (75) must stay green after the extraction.

## 9. Harness cases and live scripts (every case has a negative control that must FAIL)

| Case | What it proves |
|---|---|
| `cs_data` | `board.pdl` and `rules.pdl` counts and sums are pinned (100 squares, 9 ladders, 10 chutes in the MEMORY table, which is a pin of OUR data, not of the official board); audit refuses a self-loop jump, a jump from 100, a ladder that goes down, two jumps from one square; an edited `board.pdl` makes verbs refuse |
| `cs_play` | a scripted 2-player game: spin values equal the recomputed ones, positions, ladder and chute resolution, the win on exactly 100, `over` appended by the same call; a hand-edited spin value or result is refused (exit 1) |
| `cs_finish` | the three finish rules on constructed positions (97 with a 6; 98 with a 2; 99 with a 1): `exact_stay` stays, `bounce` bounces, `reach` wins; the rules hash changes with the option |
| `cs_seats` | the same seed under all-human, all-pet and mixed seating, with `seat` rows in between, gives identical positions row for row; a mutant that lets the actor influence the value fails |
| `cs_ghost` | files beside the ledger change nothing; a stray file fails `audit`; deleted projections (`ui.txt`) regenerate byte for byte |
| `cs_talk` | each sentence form for a constructed row; the long forms wrap in the pane (the `dsk_talkwrap` pattern) |
| `cs_theory` | the `markov` op against a hand-solved tiny board (3 squares, spinner 2, answer worked by hand in the case) and against `sim` on the real board within a tolerance computed from the standard error with a FIXED seed range (deterministic, cannot flake); a mutant that miscounts a chute fails |
| `cs_page` | the page bytes: 100 numbered squares at the expected cells, jump markers on the expected squares (pixel probes on the atlas, as `dsk_labels` does for text), `LABELS off` gives the plain page |
| `cs_driver` | feed keys per phase; `show_spin` only on a human turn; a pet seat spins on a tick and a human seat makes the tick wait |
| `cs_export` | the exporter's tables tie to the ledger (row counts, positions, final squares); the same ledger gives the same bytes twice |
| `cs_speed` | replay time of a 4-player game |

**Live scripts (`@.apps/chutes/tests/`, driven like `dsk-seat-turns.pdl`):** `cs-open-play.pdl` (open the book, Play, the setup popup), `cs-seat-turns.pdl` (a human presses Spin three times, two pets answer; `see` the figure and the sentence), `cs-find-me.pdl`, `cs-stop.pdl`. **Method rules from the Teropoly onboarding apply:** `relay_drive` gentle mode, assert what is ON SCREEN, open the PNG, a null result needs a positive control.

## 10. DATA: Chutes is a Markov chain, so the data studio can know the answer

(PROPOSAL, nothing built; the data studio itself is a plan, D58.)

**The model.** One player's position is a Markov chain on squares 0 to 100, with transition probability 1/6 per spinner face, the jump table applied after landing, square 100 absorbing, and the finish rule changing only the rows near 100. For a single player the number of spins T to reach 100 has an exact distribution: with Q the transition matrix among the non-absorbing squares and r the vector of one-step probabilities into 100, `P(T = t) = e0 Q^(t-1) r`; the mean is the row sum of `(I - Q)^-1`. `markov` computes these by power iteration until the remaining probability is below 1e-12, printing fixed decimals.

**Several players are independent chains that share only the turn order.** With n players taking turns in seat order, let `f(t) = P(T = t)` and `S(t) = P(T > t)`. Seat k wins at its t-th turn exactly when every earlier seat has not finished in t turns and every later seat has not finished in t-1: `P(seat k wins) = sum over t of f(t) * S(t)^(k-1) * S(t-1)^(n-k)`. The game length in rounds has `P(rounds = t) = sum over k of f(t) S(t)^(k-1) S(t-1)^(n-k)`. This derivation is mine (PROPOSAL); the case `cs_theory` checks it against `sim` and a hand-solved tiny board, which is how it becomes a fact.

**The well-known figure (MEMORY, UNCHECKED):** "about 39 turns for one player to finish the common US board". I recall that number and I do not know which finish rule it assumes. It is NOT used anywhere in the plan; `markov` will print the real figure for OUR board file and rule, and a mismatch with the memory figure is a finding about my memory or about the board table, not a bug to hide.

**Tables the exporter writes (`chutes export <dir>`, projections, byte-identical on regeneration; the data-hq plan's `GAME | export | <program>` opt-in row, D58):**

| Table | Rows | Columns |
|---|---|---|
| `games` | one per game | `game, seed, players, ruleset, rules_hash, finish, spins, rounds, winner_seat, winner_driver, class (played, simulated)` |
| `spins` | one per spin row | `game, seq, round, seat, driver, roll, from, to, kind (none, ladder, chute, stay, bounce), jump_id` |
| `squares` | one per square and game batch | `square, landings, ladder_hits, chute_hits` (observed) |
| `theory_square` | one per square and player count | `square, expected_landings, p_chute_if_here, p_ladder_if_here` |
| `theory_length` | one per player count and rule | `players, mean, median, p90, p99, variance, tail_over_100_rounds` |
| `theory_seat` | one per player count and seat | `players, seat, p_win` |
| `index` | one per archived or simulated batch | the data-hq history row |

**Questions it answers (each is a notebook cell; "observed" needs many games, so `sim` supplies simulated games labelled `class=simulated`, kept apart from the few games people play):**

1. *Mean and median game length*, by player count: observed vs `theory_length`.
2. *Longest game:* the observed maximum, next to the theoretical tail (how often does a game last more than L rounds). Under `exact_stay` the support is unbounded, so a "longest possible game" does not exist; only the tail does.
3. *Chute frequency by square:* observed `chute_hits / landings` per head square and the share of all slides that come from each chute (which chute is the biggest game-changer: the one with the largest drop times the highest landing probability; 98 to 78 and 87 to 24 are the candidates by eye, MEMORY, unchecked) vs `theory_square`.
4. *Variance by player count:* the spread of the game length for 2, 3, 4 players (more players, shorter and tighter games, as the minimum of independent chains).
5. *Seat advantage = first-mover advantage:* win share by seat vs `theory_seat`. The first seat wins ties of timing, so its share exceeds 1/n; the size depends on the finish rule (`reach` favours it most). **Sample size warning (my arithmetic):** to see a one-percentage-point difference between two shares near one third at 95 percent confidence needs on the order of 9,000 games (`n = 1.96^2 * p(1-p) / d^2` with p about 1/3, d = 0.01 is about 8,500). That is a thing `sim` does in seconds and people never do by hand, which is exactly why the studio computes the theory and shows both.
6. *Theory vs observed:* a z-score per square and a goodness-of-fit for the length distribution. **NEW stat op needed:** the data-hq `stat` vocabulary lists mean, median, quantiles, correlation, regression, group-by aggregates, moving average, rank (D58 plan section 4); a chi-square or Kolmogorov-Smirnov test is not in it. One small C op to add, and a decision-log row.
7. *Is the spinner fair?* The same chi-square on the 1 to 6 counts of the `die()` function over a million indices tests the house dice function itself (the modulus bias of `mix % 6` is negligible; the test would show a real defect if there were one). A free by-product.

**Honest limit:** nothing here is built; the two numbers I quote (39 turns, the 9,000 games) are a memory and my own arithmetic; the board table is from memory.

## 11. Chutes milestones (each ends with a case that can fail, a PNG opened, a scoped commit)

| M | Deliverable | Case | A human must SEE (PNG) |
|---|---|---|---|
| C0 | app skeleton, `board.pdl`, `rules.pdl`, `lobby join leave start`, audit | `cs_data` | none (text); the rule file in the editor's data page |
| C1 | `spin`, replay, `over`, `status/check/replay/ledger_hash`, `autoplay` | `cs_play`, `cs_seats`, `cs_ghost`, `cs_finish` | none (text) |
| C2 | extract `game_rng.c` and `game_ledger.c` (Teropoly cases stay green) | `dsk_*` unchanged | none |
| C3 | `markov`, `sim` | `cs_theory` | none; the printed theory next to a `sim` run |
| C4 | `ui`, `talk`, popup, `game.pdl`, driver, chat hook | `cs_talk`, `cs_driver` | the editor window with the Chutes popup and the sentences |
| C5 | `page` (numbers, ladder and chute markers), `page_labels.c` extracted | `cs_page` | the 2D and 3D board with 100 readable numbers |
| C6 | figures and walking, camera follow (`game_camera.c` extracted) | `cs_page` figures, `dsk_camera` green | tokens on the board mid-game, camera on the human's token |
| C7 | live scripts: a full human game with two pets | `cs-seat-turns.pdl` | the winning screen |
| C8 | exporter and the data-hq provider | `cs_export`, studio P7 | the dashboard: length histogram with the theory curve over it |

---

# PART B. SNAKE

## 12. The key question: a real-time game with a ledger, without a row per tick

**The rules that bite (FACT):** the ledger is the only authority for game state, derived state is a projection, behaviour-deciding state is not kept only in memory (AGENTS.md "No ghost data"; `02-architecture/NO-GHOST-DATA-THE-LEDGER-IS-THE-AUTHORITY-2026-11-07.md`); anything written at runtime needs a cap, a second guard and a row in `03-pitfalls/RUNTIME-FILE-GROWTH-STANDARD.md`, and "a commit that adds a writer states its files or bytes per hour". A tick every 150 ms is 24,000 ticks an hour per game. (150 ms is my proposed default; Teropoly's pace is 1,400 to 1,500 ms per decision, FACT `tuning.pdl`.)

**Options (design-choice test):**

| Option | Rows per hour (PROPOSAL arithmetic) | Replay | Tamper detection | Verdict |
|---|---|---|---|---|
| (a) a row per tick | 24,000 per snake, with 4 snakes 96,000 (about 4 MB an hour at 40 bytes a row) | trivial | strong | **Rejected:** it breaks the runtime-file standard and fills `max_rows` in minutes |
| (b) INPUTS only (`start` + seed, `turn` rows with tick numbers) | about 1,000 to 8,000 per snake | a pure function of the rows | WEAK: hand-editing a `turn` row silently makes a different valid game | Rejected alone: it fails the "refuse a hand-edited ledger" requirement |
| (c) inputs plus CONSEQUENCE rows (`eat`, `die`, `over`) whose results replay must reproduce | adds about 400 to 1,000 eats an hour | same | good for the rows that exist; an edit shows at the next consequence row | close |
| (d) = (c) plus periodic `ckpt` rows (a state hash every K ticks and at every pause or stop) | adds 3600 / (K * 0.15 s) per hour: K = 64 gives about 375, K = 16 about 1,500 | same, plus a seek point | strong: any edit changes the hash at the next ckpt | **Recommended** |
| (e) a hash chain in every row | as (b) | same | local only; whoever edits can recompute it | not chosen: adds bytes, adds no security on a machine where the owner can edit any file |

**Recommendation (decision D59): (d).** The ledger records INPUTS (the player's and the AI's direction changes with their tick numbers), a few CONSEQUENCE rows whose result strings replay recomputes and must match (the same defence Teropoly's `result` field gives: spec 5.2, 5.5), and CHECKPOINT rows carrying a hash of the full simulated state; everything else (every tick of movement, every body position, the food positions) is derived by deterministic REPLAY.

**Rows (PROPOSAL; the seven-field shape, `round` holds the logical TICK, `at` holds the head cell `x,y` after the action):**

| verb | actor | target | result | note |
|---|---|---|---|---|
| `lobby` | bank | `-` | `ruleset=classic,schema=1,rules=<hash16>,max=4` | first row |
| `join` | pN | name | `driver=human\|pet,policy=<id\|->,color=<n>` | LOBBY |
| `start` | bank | `-` | `seed=<n>,players=<n>,rules=<hash16>` | closes the lobby; tick 0 |
| `turn` | pN | `L`, `R`, `U`, `D` | `applies=<tick>` (accepted) | appended just before the tick it applies to; a reversal into the neck is refused and appends NOTHING |
| `eat` | bank | pN | `len=<n>;score=<n>;food=<x,y>;next=<x,y>` | appended by the tick that ate; replay recomputes it; `next` is the newly spawned food |
| `die` | bank | pN | `cause=wall\|self\|other:pM\|headon:pM` | appended by the tick that killed |
| `spawn` | bank | pN | `len=3;at=<x,y>` | only with `on_death=respawn` |
| `ckpt` | bank | `-` | `hash=<hex16>` | every `ckpt_every` ticks, and at every pause, stop and exit |
| `seat` | bank | pN | `human\|pet` | control change, any time (as Teropoly) |
| `over` | bank | `-` | `winner=<pN\|none>;ticks=<n>;hash=<hex16>` | appended by the SAME call that ended it; carries the final state hash |

**Time is logical, not wall-clock.** The tick counter advances by one each time the host performs a step; wall-clock never enters the ledger. A key press is stamped with the NEXT tick (up to two buffered presses, stamped T+1 and T+2). So:
* **Pause and resume need NO rows.** While the editor's `running` switch is 0 (FACT: it is the one authority for pausing, driver header) the host does not step, so the logical clock stands still. A `ckpt` row is written at the moment of the pause or stop so the exact tick is durable. (A ruleset row `log_pauses on` could record pauses for analytics: OPEN, off by default.)
* **A slow machine slows the game; it never skips ticks** (at most one step per loop). The game is a pure function of the rows whatever the machine did.
* **Speed-up is not a fact:** pace as a function of the score (shorter `tick_ms` every N foods) is derived by the host from the replayed state through ruleset data; ticks are logical, so it changes how fast the screen moves and nothing in the ledger. The base pace is a window PREFERENCE (Teropoly's `speed_next`, FACT).

**Food spawns are derived:** the k-th food goes to `free[ mix(seed*1000003 + k*16 + 7) % nfree ]` where `free` is the list of unoccupied cells in row-major order at the moment it spawns. If no cell is free the board is full: `over` with `winner` = the longest snake. `k` counts spawns (a replay variable, derived). The `eat` row names `next` only so a human reading the ledger and the exporter do not need to replay; replay checks it.

**Row counts per hour, per snake (PROPOSAL arithmetic, to be MEASURED in case `snk_rows`):** turns 1,000 to 6,000 (a turn every 4 to 24 ticks), eats 400 to 1,000, ckpt about 375 (K = 64 at 150 ms), so roughly 2,000 to 7,500 rows, about 40 bytes each, **under 0.3 MB an hour per snake**. A game normally ends by death within minutes (a 5-minute game is about 300 to 600 rows). Caps: `RULE max_rows 20000` (the engine appends `over cause=limit` before the cap and refuses later verbs, as Teropoly does with `max_rows`, spec section 13), `RULE max_ticks 200000`; the state folder goes into `cpu_guard`'s `hygiene_sweep()` and a row in the growth standard (both are NEW rows to add when it is built). One game per state folder; the last 3 finished games are archived (FACT: the Teropoly driver does that).

**Replay cost (estimate, not measured):** an hour of play is 24,000 ticks; each tick moves each head, drops each tail, looks a cell up in an occupancy grid; with four snakes of length up to 200 that is on the order of millions of operations, tens of milliseconds. PROPOSAL target as Teropoly's: a full replay well under 50 ms; case `snk_speed` measures it. If it is slower, fix the code; never add a cache file (a cache is ghost data, spec 5.5).

**Hash pin and refusal of hand edits:** the `over` row carries the hash of the final state; `ckpt` rows carry intermediate hashes; `replay` prints the state hash and `ledger_hash` the hash of the file, and a pinned fixture game (seed, rows, hash) is the regression anchor, like Teropoly's pinned three-pet game (the class text DAY 27 quotes one: seed 6, 831 rows, a fingerprint starting 7244). An edited `turn` row changes the replayed state, so the next `eat`, `die`, `ckpt` or `over` row no longer matches: exit 1, nothing repaired. **Limit, stated plainly:** rows after the last `ckpt` of a still-running game can be edited without detection until the next checkpoint (at most `ckpt_every` ticks, about 10 seconds with K = 64). This is protection against accident and drift, not against an owner who edits files; a rated PvP leaderboard (the Tear-it promise) would need signed ledgers: **OPEN, out of scope**.

**Crash behaviour:** the host's in-memory state is a cache of the replay. After a crash the next start replays the ledger and resumes at the last row's tick (at worst K ticks earlier than the screen showed), and says so in the popup. This is the honest cost of not writing a row per tick; the no-ghost rule is met because the only fact that lives outside the ledger is "ticks elapsed since the last row", a wall-clock fact bounded by K, and the harness proves the resumed game equals a replay.

## 13. The engine, the host and the clock

**Shape (decision D59, second part):** three programs, one rule file.
* `snake_core.c`: the PURE tick function `step(state, inputs_for_this_tick) -> state + events`, the hash fold, the food pick, the occupancy grid. Text-included (pure logic, 2 consumers: house rule) by:
* `snake.+x`: the CLI engine op: `lobby join leave start turn seat status replay check ledger_hash audit ui talk page export sim help` and `advance <n>` (headless: apply `n` ticks and append consequence rows; used by the harness and the AI trainer). It is the writer for everything except the live loop.
* `snake_driver.+x`: the HOST: single instance (flock), the editor's `GAME | driver`. It replays once at start, then steps once per `tick_ms` while `running=1`, reads `action.txt` and the key relay by byte cursors, appends rows with the SAME append function, and publishes `game_ui.txt` and the figures' `events.txt` (atomic rename). **Why the host includes the pure core instead of forking the engine per tick:** a fork per tick at 6.7 ticks/s replays the ledger every time (Teropoly forks per pet move at 1.4 s, FACT; at 150 ms that is 10 times the rate on a weak machine). The alternative (a long-lived child behind a pipe) adds a stateful process for one consumer. Including the pure function means there is ONE rule file; the harness replays the host's ledger with the CLI (a different binary built from the same file plus an independent `ckpt` hash check), and a mutant of the core must make both fail.

**The in-memory state is a cache, not an authority:** every `ckpt_every` ticks the host compares its state hash with a fresh replay of the ledger (cheap, see replay cost); a mismatch stops the game with a message and a log line, never a silent repair.

**The step order is data in the ruleset README and fixed in `snake_core.c` (PROPOSAL):** (1) apply the `turn` rows stamped for this tick, in seat order; (2) compute every new head; (3) deaths: wall (if `walls=solid`), any body cell that is still occupied after the tails move (a tail leaving the cell this tick does not block, unless that snake grows this tick), two heads on one cell or swapping cells (`headon`), a head on another snake's body; (4) eats; (5) growth; (6) spawn food; (7) rows; (8) `ckpt` if due. All snakes move simultaneously so seat order never gives an advantage in a collision.

**Ruleset options (`rulesets/classic/rules.pdl`, hashed like Teropoly's; OPEN defaults for the owner):**

| RULE | default | meaning |
|---|---|---|
| `cols`, `rows` | 20, 20 | grid size (page cap 64x64 cells, spec 10.5; with block 2 a 20x20 grid is a 40x40 page) |
| `walls` | solid | `solid` = a wall ends the run; `wrap` = the board is a torus |
| `start_len` | 3 | |
| `grow` | 1 | segments gained per food |
| `foods` | 1 | foods on the board at once (OPEN: equal to the number of snakes in multiplayer) |
| `tick_ms` | 150 | base pace (a window preference; the core ignores it) |
| `speed_curve` | every 5 foods, minus 10 ms, floor 70 ms | pace as a function of the replayed score |
| `score_food` | 10 | points per food (plus `score_len` per segment: OPEN) |
| `on_death` | end | `end` or `respawn` (with `respawn_delay` ticks, `respawn_len`, `lives`) |
| `reverse` | refuse | a reversal into the neck is refused |
| `food_tie` | lowest | two heads on the same food on one tick: `lowest` seat, `both`, or `none` |
| `ckpt_every` | 64 | ticks between checkpoint rows |
| `max_ticks`, `max_rows` | 200000, 20000 | caps |
| `obstacles` | none | a list of wall cells in `board.pdl` (data) |

## 14. Entities, steering, rendering

**Segments and food as entities.** In Teropoly the figures are driver-written `events.txt` rows, not pieces with uids (FACT, section 2). Snake does the same: one row per body segment and one for each food, `x y r g b <charset> <idx> <dir> <pat> <glyph>`; the colour fields tint a snake, the head uses a different glyph letter (`A` for p1's head, segments `a`; the glyph must be a letter, FACT). The rows are rewritten atomically each tick the board changes (at most 6.7 times a second, a file of a few kilobytes); the 3D engine already re-reads `events.txt` every frame (FACT), so no engine change. The floor page is written once at game start (walls, a checker, obstacles) and NOT per tick: Teropoly learned that rebuilding the board picture per tick cost engine launches (FACT: `deskopoly_driver.c` comment at line 156). **16x24 frames:** `deskopoly_frames` makes missing `People1` frames; a snake wants simple coloured blocks, not walking people (OPEN: a "block" charset with tint; if none exists it is a small NEW asset).

**Why not real pieces (`place` / `remove` events per segment):** the editor has events `place`, `remove`, `control`, `newpal`, `delpal` (FACT: `@.apps/rpg-edit-hq/events`; `place`, `remove`, `control` are verb pages calling `rpg_edit_hq_verb`). Each is a process chain (`play_event.sh` -> `event.pal` -> `cmd_1.sh` -> an op: one process per page step, FACT in `16.game/RPG-SPACE-BLUEPRINT-SYSTEMS.md` 5d and the blueprint guide) and growth on every food means a fork per segment. At length 100 that is the wrong cost; a projection row costs nothing.

**Steering: "possession of an entity = controlling it" and why it does not apply directly.** FACT: possession in the editor is the xelector's `possessed_id` (one authority; Enter possesses a pet or Harold; `9` releases; Tab selects; Esc releases first and then leaves In: `02-architecture/AGENT-TOOLBOX-AND-CONTROLS.md`), and in INT mode the arrow keys arrive as codes 1000 to 1003 in `state/interact_relay.txt` and walk the selected pet (`petslike_core.c` header, `@.apps/rpg-edit-hq/keybinds.pdl`). A Snake head is a projection row, not a piece, so there is nothing to possess. Steering routes, in order of cost:

1. *Popup buttons* `Left` `Right` `Up` `Down` through `action.txt` (works today, same as Teropoly's buttons; clumsy for an action game).
2. *Chat commands* via `GAME | chat` (`/l`, `/r`): works today, worse.
3. **NEW (recommended): a key hook for games.** I found NO existing hook that gives a game's driver the arrow keys (FACT by search: `GAME | driver`, `chat`, `clock` are the only `game.pdl` rows; the key relay is read by the core only). Proposal: a row `GAME | keys | claim` in `game.pdl` makes the core, while a game with that row is live and In: is ON, hand the arrow codes 1000 to 1003 to the driver's `action.txt` as `turn L|R|U|D` lines and NOT move pets. It is a small addition to the shared `petslike_core.c` (a heavy-collision file: one tiny branch, own commit, same caution as the taskbar manager in D58). The camera keys (`0`-`4`, FACT keybinds) stay with the engine. OPEN: whether INT mode needs to be on for key delivery; whether arrow-to-cursor behaviour (decision "arrows move xelector, not camera", memory) conflicts. Test with `relay_drive` before trusting anything (`relay` may mask real focus bugs, memory note).
4. *Real possession later,* if snake segments ever become pieces (it would fall out of the pieces/uid work, memory note on pieces); not needed now.

**AI seats steer by rows:** the AI decides each tick from the replayed state; a `turn` row is appended only when the direction CHANGES (so an AI snake that goes straight writes nothing). The decision is a deterministic function of state and seed, but it is still RECORDED as a row: if the policy code changes, old ledgers still replay (the policy is not part of the replay), and the rows are the training data. A policy is a named, versioned entry (`policy=bfs1`) in the `join` row.

**2D and 3D parity.** The board page is a normal desk page (`cells.txt`, `map.png`, `map.txt` for 2D, key `0` toggles 3D/2D, FACT in the memory note). The 2D view is the natural Snake view and costs less than the GPU ray-march daemon (memory note: about 17 fps live, GPU; the 3D engine is the dominant cost in any board window, to be measured). **Camera risk (FACT from memory notes):** an overhead camera was UNRESOLVED on 2026-11-08 (`cam_home.txt` differs per session, so a fixed key sequence is not reproducible); the recipe `board_ctl camset ... cam_pitch=-89 cam_pan_x=16 ... then 20 'c' presses` worked once. So Snake ships 2D-first and treats a 3D top-down camera as its own milestone.

## 15. CPU budget and ask-first work

(PROPOSAL; every figure below must be MEASURED with the Teropoly method before any claim: children plus wakeups, the producer's state written next to the number, bursts not averages, a null result needs a positive control: `03-pitfalls/CPU-OPTIMIZATION-PITFALLS.md` items 23 to 32; AGENTS.md "Our programs run alongside normal user activity".)

| State | Proposal |
|---|---|
| Stopped, paused, or game over | **idle cost zero:** the host exits on `Stop` after writing a `ckpt` (the core's `play` verb starts the driver again, and the watchdog restarts it when `running=1` with no live feed: FACT, `petslike_core.c` lines near 1647 and 1883). A stopped game is no process. OPEN: Teropoly's driver naps 80 ms between loops and stays alive (FACT: header "idle = a stat() and an 80 ms nap per loop"); for Snake zero processes is the goal, to be verified by counting children and wakeups |
| Running | one wake per `tick_ms` (about 7 a second at 150 ms), one `events.txt` rewrite per tick, one feed rewrite only when a displayed value changed (score, length, status), one append per input and per consequence row, NO fork per tick. Compare Teropoly's pace of one engine fork per 1.4 s |
| Event pages | pages fire on TRANSITIONS only (die, game over, new high score), never per tick or per eat when length is large: FACT, footprint rule 4 in `RPG-SPACE-BLUEPRINT-SYSTEMS.md` ("pages fire on transitions ... no step may raise the idle fork rate"; a death costs about 6 forks there) |
| Heavy things | compile, engine start/relaunch, screenshot loops and the AI training run behind `cpu_gate <house> run <label> -- <cmd>` (with `--stop-engines` when a 3D engine of this house runs); one heavy job at a time; AI training OFF this machine (debil 12 cores or the Mac, FACT in the data-hq plan and memory note on remote offload) |
| The world clock | `GAME \| clock \| pause` (FACT, `@.apps/deskopoly/game.pdl`): while the feed says `gm_on=1` the editor's world clock is paused and resumes by itself when the game ends. Snake needs the row for the same reason Teropoly does (the clock burned about 4 percent of a core with In: OFF, 35 to 43 percent bursts, measured 2026-11-08/09) and because Snake must not take its tick from the clock (section 19) |
| Footprint line in every commit | "idle: N processes / M wakeups a second; running: ...; with a browser open" |

**Cap on pace:** `tick_ms` has a floor (OPEN: 50 ms) and the host never runs more than one step per loop, so a stall cannot become a burst of catch-up steps.

## 16. Multiplayer, pets, scoring, death, data

**Multi-snake:** 2 to 4 snakes on one board, one `foods` count shared (OPEN: equal to the number of snakes). They move simultaneously (section 13). Collisions: a head into another snake's body kills the mover (`other:pM`); two heads on one cell, or swapping cells, kill both (`headon:pM`) by default. The round ends when at most one snake is alive (the last alive wins) or at `max_ticks` (highest score wins). Humans share the keyboard? One human steers from the real keyboard; a second human seat needs a second input route (OPEN: the shared relay has one keyboard; a second human means the network chat route or popup buttons).

**Pets = AI snakes (PROPOSAL):** policy ladder, each a named versioned policy in the `join` row: (1) `random_safe` (never moves into an immediate death), (2) `greedy` (reduces the distance to the food, avoids death), (3) `bfs` (shortest path to the food avoiding bodies, with a check that a path to its own tail remains after eating), (4) `trained1` (a table or small model trained off machine, loaded as DATA like Teropoly's "the brain is data" decision D48: weights in a `.pdl`, a deterministic evaluator in C). Hamiltonian-cycle play (never dies, slow) is a possible later policy. OPEN: pet personalities as policy parameters (caution, greed) as spec section 11 does for Teropoly.

**Scoring and speed:** `score_food` per food; optional bonus for length; the pace shortens with the score (section 12). **High score** is a projection: the best of the archived finished games' `over` rows, read for DISPLAY only (a projection read for display is fine; it is never an input to a rule).

**Death and respawn:** `on_death=end` ends a solo run; multiplayer continues until one is left. `respawn` brings a dead snake back after `respawn_delay` ticks at a free cell with `respawn_len` (a `spawn` row, location derived from `mix(seed, tick, pN)`), with a life count. Both are data.

**What the data studio gets (tables the exporter writes, projections):**

| Table | Rows | Columns |
|---|---|---|
| `games` | one per game | `game, seed, rules_hash, cols, rows, walls, players, ticks, winner, cause_end, class (played, ai_vs_ai, trainer)` |
| `snakes` | one per snake and game | `game, seat, driver, policy, score, max_len, death_tick, cause, foods_eaten, turns` |
| `foods` | one per food | `game, k, spawn_tick, eat_tick, eater, ticks_to_eat, manhattan_at_spawn, path_len_taken, efficiency` |
| `turns` | one per `turn` row (this is the TRAINING table) | `game, tick, seat, dir, kind (human, ai), head_x, head_y, food_dx, food_dy, danger_ahead, danger_left, danger_right, len, outcome_next_32_ticks (ate, died, neither)` features computed by REPLAY at that tick |
| `heat` | one per cell and class | `x, y, head_visits, deaths` |
| `ckpt` | one per checkpoint | `game, tick, hash` (the proof it replays) |
| `index` | one per archived game | the data-hq history row |

**Questions it answers:** the learning curve of a person across sessions (score and survival ticks by session number: a trend line); death causes by class (wall, self, other); reaction time (ticks between a hazard becoming adjacent and a turn); where on the board runs end (the heat map); the effect of speed-up on survival; AI policy comparison (random_safe vs greedy vs bfs vs trained: mean score and survival with confidence intervals, from simulated games with fixed seeds); food efficiency (path length versus the straight-line distance); whether the board size or wrap rule changes the score; the single-snake multiplayer interaction (how often the second snake is the cause of death).

**Training data for a snake AI offloaded to debil or the Mac:** the `turns` table is the supervised/imitation table; self-play is generated by the engine's headless `sim` (the same `snake_core.c`, so training and play cannot disagree about the rules). Training runs OFF this machine (FACT: owner 2026-10-13 offload rule and the data-hq plan P6; debil has 12 cores and no GPU seen; whether debil has a C compiler is **OPEN**, the Mac is unchecked). Method proposal: tabular Q-learning or a tiny policy on a compact state (danger in three directions, food direction, length bucket), episodes from `sim` with fixed seeds; the result is a `.pdl` weights file; the in-house evaluator loads it; the metrics come back as a dataset into the studio. Human-seat games enter training only on an explicit flag (the data-hq plan, open decision 3). No LLM is needed; the free-models-only rule is trivially met (the house AI backend is Ollama via the Mac, FACT in the memory notes, not needed here). **Honest limit:** I have not measured how many ticks a second `sim` runs; "millions per minute" is a guess for a C loop.

---

# PART C. WHICH HOUSE EVENTS WILL BE USED AND WHY

## 17. What the event system is (FACT, read today)

* An **event package** is `events/<name>/event_pkg/pages/page_N/` with `condition.pdl` (`COND | trigger | <t>`), `event.ir.pdl` (the editable node list), `event.pal` (compiled), and a `cmd_N.sh` per command; `play_event.sh` picks the page and starts the prisc VM; the wrapper runs a C op (`02-architecture/BLUEPRINT-EVENT-PAGES-GUIDE.md` sections 1 to 3).
* The command vocabulary is `#.ref/menu/event_commands.registry.pdl`: **170 `COMMAND` blocks** counted today (the guide said 135 on 2026-10-20), each with a `TEMPLATE exec ...`.
* The **trigger values** used in the 2026-11-09 tree (a count of `COND | trigger | X` over every `condition.pdl`; most are RPG-Maker map pages): `on-click` (about 2,620), `on-touch` (about 2,290), `ui` (198), `parallel` (84), `clock` (50), `setup`, `endturn`, `combat`, `move`, `autoturn`, `Autorun` (17), `roll`, `player-touch`, `on-spawn`, `turnstart`, `land`, `feed`, `block`, `cast_*`, `finrule`, `castcheck`. The semantics I READ: `ui` (a window button; verb pages), `clock` (fired by the clock daemon through `common:<event>`), `parallel` and `Autorun` (the common-events manager). The semantics of `on-touch`, `on-spawn`, `land`, `roll`, `endturn` I did NOT read.
* **Common events** are house-level packages (`common_events/`), run for any caller whose trigger matches; `Autorun` fires once on a switch going 0 to 1, `Parallel` repeatedly while the switch is on, at most once a second per event, at a 60 Hz poll, and **all automatic firings are held while a clock is paused** (`&.widgits/events-hq/ops/common_events_manager.c`, read: the `check_common_events` loop). I did not verify that this manager runs in the live house, or what exactly `any_clock_paused()` reads beyond the `#.desktop/clocks/*.pdl` scan.
* **The clock:** `time.pdl` `AT | event | HH:MM or +N<m\|h\|d> | every:<n><min\|hour\|day\|week> | text` rows become reminders in the house `lc_clock`; when one fires it runs `rpg_edit_hq.+x common:<event>`, and `event_main()` plays the events-hq page `events/<event>` with trigger `clock`, falling back to a built-in behaviour (`@.apps/rpg-edit-hq/time.pdl`; `petslike_core.c` lines about 1739-1744). Rows shipped today: `phase_dawn/day/dusk/night`, `day_tick` (00:00 daily), and opt-in `world_flow`, `world_rain`, `world_fire`, `needs_tick`, `pet_say` (shipped OFF).
* **World-ops pages** (rpg-space): `death`, `respawn`, `destroyed`, `collision`, `col_hit`, `col_breach`, `env_hit`, `environment`, `eat`, all trigger `parallel`, chained by `wo_when`, `wo_each`, `wo_each_actor`, `wo_if` (`@.apps/rpg-space/events/*/event_pkg/pages/page_1/event.ir.pdl`, read). They act through the `wo` op (`wo_kv`, `wo_drop`, `wo_place`, `wo_log`, `wo_event`, `wo_calc`, `wo_load`, `wo_run`) on **plain state files** such as `life_:actor:.txt` (keys `hp`, `dead`, `deaths`).
* **The editor's own pages** (`@.apps/rpg-edit-hq/events`, IR read for `play`, `stop`, `day_tick`, `place`, `remove`, `control`, `newgame`): `play` = `control_switch running=1` then `rpg_edit_hq_verb sync`, trigger `ui`; `stop` the same with `0`; `day_tick` = trigger `clock`, `rpg_edit_hq_verb day_tick`.
* **Teropoly itself uses NO event pages today:** `verbs.pdl` says "page: no for all of these in pass 1 (no event pages generated yet; S3 of the plan fires them through pages)" (FACT). What Teropoly uses is the game.pdl hooks (driver, chat, clock) and the Play/Stop switch.

**A conflict the event pages create for a ledger game (FACT-based, my reading):** the rpg-space `death`/`respawn` pages WRITE `life_<actor>.txt` (`hp=0`, `dead=1`). In a game whose authority is a ledger that file would be a state file carrying a fact the ledger lacks, which is ghost data (AGENTS.md). So the new games must not use the world-ops state-writing nodes for game outcomes. They may use event pages for PRESENTATION and notifications (a sound, a text, a log row) fired AFTER the ledger row exists.

## 18. Events table: Chutes and Ladders

| Event (real name or trigger) | What fires it (FACT, source) | What the game does with it | NEW work? |
|---|---|---|---|
| Editor event `play` (trigger `ui`: `control_switch running=1`, then `rpg_edit_hq_verb sync`) | the Player > Play button; `@.apps/rpg-edit-hq/events/play/.../event.ir.pdl`, `petslike_core.c` `play` verb | nothing of its own: Play starts the book's driver via `book_driver()` | no |
| Editor event `stop` (`running=0`) | Player > Stop | the driver stops ticking (Teropoly header: "ticks only while running=1") | no |
| `GAME \| driver` row | read by `book_driver()` in `petslike_core.c` when Play runs; Teropoly's `game.pdl` | `chutes_driver` is started, publishes the feed, spins for pet seats | the driver C (small, built on `game_driver_core.c`) |
| `GAME \| chat` hook | `game_chat_hook()` (exit 3 = not a command) | `spin`, `/spin` and `/roll` typed in the chat box become the engine's `spin` for the human seat | a small chat op (shape of `deskopoly_chat.c`, 436 lines, much less needed) |
| `GAME \| clock \| pause` | the clock hold in `petslike_core.c`; Teropoly's `game.pdl` | pause the world clock while a game is live: the game is turn based and needs no clock | no (one data row) |
| `ui` trigger pages for the verbs (`fire <verb>`, events-hq `verbgen`) | `BLUEPRINT-EVENT-PAGES-GUIDE.md` section 5: `VERB \| name \| yes\|native\|no` | OPTIONAL: Chutes has so few verbs (`spin`, `join`, `start`) that it is the cheapest game to make `spin` a visible page (`page: yes`) so the blueprint view shows the turn; the leaf still writes the ledger row | optional: `verbgen gen` plus a case |
| `clock` trigger / `time.pdl` `AT` rows | the clock daemon; `time.pdl` header | not used for turns. The finest repeat in `time.pdl` is `every:<n>min` of GAME time (FACT, header), far too coarse and not needed for a turn-based game | no |
| `pet_say` common event (opt-in, shipped OFF) | `time.pdl` `AT \| pet_say \| +30m` | OPTIONAL flavour only: not used | no |
| Seeded-dice commands `random_roll` (seed, counter, sides) and `roll_dice` (seeded 2d6) | registry commands (`rand_roll` in `footrace-fu`, `dice_roll` in `ring-board-rmmv`) | **NOT used.** The engine's own `die()` is the one authority for every spin (one authority per fact; a second RNG in a page would be a second definition) | no |
| Event commands `show_text`, `play_se` | registry (`mr_show_text`, `mr_world play_se`) | OPTIONAL: a win jingle or a text on `over` as a page fired after the row; not needed for the MVP | optional |
| Entity creation/removal (`place`, `remove`, `control`, `newpal`, `delpal` pages) | `@.apps/rpg-edit-hq/events` | not used: tokens are `events.txt` projection rows written by the driver, as Teropoly's figures are | no |
| Possession (xelector `possessed_id`, `lockhero`) | `AGENT-TOOLBOX-AND-CONTROLS.md`; xelector state | not used for control (the spin is a button). The camera snap respects it: if the cursor possesses another entity the snap refuses (D57 behaviour) | no |
| `on-touch` / landing triggers | RPG-Maker map pages (semantics not read) | **Not used.** The engine decides landing, ladder and chute; a touch trigger would be a second authority | no |
| world-ops `death` / `respawn` / `destroyed` / `collision` | rpg-space pages | **Not applicable:** nothing dies or collides; and their state-file writes would be ghost data here | no |
| `GAME \| export \| <program>` (data-hq plan) | proposed in D58 / the data-hq plan, **not built** | the exporter row that makes `chutes export` a provider | NEW (in the studio plan P0) |

## 19. Events table: Snake

| Event (real name or trigger) | What fires it (FACT, source) | What the game does with it | NEW work? |
|---|---|---|---|
| Editor `play` / `stop` events | as above | Play starts the host; Stop makes it write a `ckpt` row and exit (zero idle) | the exit-on-stop behaviour is NEW in the driver |
| `GAME \| driver`, `chat`, `clock \| pause` | as above | host; chat commands `/pause`, `/restart`, `/speed`; clock held while a game is live | the host and a chat op (NEW); the rows are data |
| Game tick | **there is no real-time event to hang it on.** The clock rows (`AT ... every:<n>min`) are minutes of game time, and the clock daemon at a fast rate burned 364 percent of a core (memory note on the clock burn, 2026-11-08); the common-events `Parallel` trigger is limited to once a second per event (FACT, `PARALLEL_COOLDOWN 1000000`) and is held while a clock is paused | the tick is the HOST's own loop, a nap of `tick_ms` between steps (Teropoly's driver has the same shape with an 80 ms nap) | the host loop (NEW) |
| `Autorun` common event (edge on a switch 0 to 1) | `common_events_manager.c` | OPTIONAL: a "game started" announcement; not needed | optional |
| `Parallel` common event | same manager | **not used** for ticks (1 s cooldown, held while the world clock is paused, which Snake's own `GAME \| clock \| pause` row causes: using it would stop the page exactly while the game runs; I did not verify what the manager reads, section 17) | no |
| `day_tick` (trigger `clock`, 00:00 daily) | `@.apps/rpg-edit-hq/time.pdl` | OPTIONAL "daily challenge": the seed of the day is a function of the date; the seed still goes in the `start` row. Needs the clock to be running; and Snake pauses the clock while live (the seed is read at start, so fine) | optional, NEW |
| Arrow codes 1000-1003 in `state/interact_relay.txt` | `keybinds.pdl`, `petslike_core.c` `poll_keys` | steering; today they walk the selected pet, not a game | **NEW key hook** `GAME \| keys \| claim` in the core (section 14); fallback popup buttons |
| Possession (`possessed_id`, Enter/`9`/Esc/Tab) | `AGENT-TOOLBOX-AND-CONTROLS.md` | the convention "possessing an entity controls it" is honoured by making the INPUT go to the head of the possessing seat; a head is a projection row, not a piece, so possession proper is OPEN (needs the head to be a piece) | OPEN |
| Spawn food (entity creation) | none: the engine derives it | `food = free[mix(seed*1000003 + k*16 + 7) % nfree]` appended as part of the `eat` row's `next`; the figure row appears in `events.txt` | NEW (core) |
| Pickup / eat | the engine, in the tick | the `eat` row; **not** the rpg-space `eat` page (that page is `rpg_space_verb fridge eat @actor`, hunger and a fridge, a different thing) | NEW (core) |
| Score / game-over | the engine | `eat` carries the score; `over` carries the final hash and winner | NEW (core) |
| Death / collision / respawn | the engine, in the tick | the `die` and `spawn` rows. The rpg-space world-ops pages `death`, `respawn`, `collision` have the right NAMES and a reusable node vocabulary (`wo_each_actor`, `wo_if`, `wo_log`), but write `life_<actor>.txt`, i.e. a state file: ghost data here. Reuse only the PRESENTATION nodes in new pages `snake_death`, `snake_over` (trigger `parallel`, fired by the host AFTER the row, on transitions only, never per tick, since a page step is a process: about 6 forks for a death, FACT in the blueprint doc) | optional pages (NEW) |
| Event commands `play_se`, `show_text`, `pause_clock`, `resume_clock`, `set_clock_rate` | registry | sounds on eat and die (OPTIONAL); the clock commands are covered by `GAME \| clock \| pause` | optional |
| Entity creation/removal pages `place` / `remove` | `@.apps/rpg-edit-hq/events` | **not used per segment** (a process chain per page step; a fork per growth is the wrong cost) | no |
| `random_roll` / `roll_dice` | registry | not used (one authority for randomness: `mix`) | no |
| `GAME \| export \| <program>` | data-hq plan (not built) | `snake export` for the studio | NEW (studio P0) |

## 20. What I could NOT find or verify (so nothing above reads as more than it is)

1. Any real-time or sub-second event trigger (a game tick event). The finest FACT is the clock's minute and the 60 Hz common-events poll with a 1 s cooldown.
2. Any existing way for a game driver to receive arrow keys (only the core reads `interact_relay.txt`).
3. The semantics of the triggers `on-touch`, `on-spawn`, `land`, `roll`, `endturn`, `combat`; the semantics of the registry commands `start_game`, `stop_game`, `play_se` beyond their `TEMPLATE` lines (they exec ops in `piececraft-hq` and `mr_world`); I read the templates, not the ops.
4. Whether `common_events_manager` runs in the live house, and exactly what its clock-pause gate reads.
5. How a NEW book appears in the editor's book list (Teropoly's registration path); whether the atlas loader has a cap that 100 labelled tiles plus line overlays would hit; whether a "block" charset for snake segments exists.
6. The official Chutes and Ladders board, the finish rule of any edition, and the "39 turns" figure: all MEMORY.
7. How many ticks a second a headless C `sim` runs on this machine, replay times, idle and active CPU of a 150 ms host: all unmeasured.

---

# PART D. EFFORT, RISKS, DECISIONS

## 21. Snake milestones (each ends with a case that can fail, a PNG opened, a scoped commit)

| M | Deliverable | Case | A human must SEE (PNG) |
|---|---|---|---|
| S0 | app skeleton, `rules.pdl`, `board.pdl`, lobby verbs, audit, the pure core's data types | `snk_data` | none (text) |
| S1 | `snake_core.c`: step order, collisions, food derivation, hash; verbs `turn advance replay check`; ledger rows (d) | `snk_core` (hand-solved 4x4 boards, each collision kind), `snk_replay` (pinned fixture: rows and hash), `snk_hand_edit` (an edited `turn` row is refused at the next consequence/ckpt row), `snk_ghost` | none |
| S2 | multi-snake, head-on, food ties, `respawn`, `wrap` | `snk_multi` | none |
| S3 | the host loop headless with a fake clock (steps as fast as it can), crash/resume, row counts | `snk_tick`, `snk_resume`, `snk_rows` (rows per simulated hour against the table in section 12), `snk_speed` (replay time) | none |
| S4 | page, `events.txt` figures, 2D and 3D | `snk_page` (probes at the wall and obstacle cells, figure rows) | the 2D board with a 5-segment snake and a food; the 3D view if the camera allows |
| S5 | driver, Play, Stop with `ckpt` and exit, `GAME \| clock \| pause`, popup, steering by buttons | `snk_driver`, live `snk-open-play.pdl`, `snk-steer-buttons.pdl` | the editor window mid-game with score and length |
| S6 | key hook `GAME \| keys \| claim` in the core | `snk_keys` (positive control: a pet is NOT moved while the claim is live; negative: without the row it is) | arrow keys steering the head |
| S7 | AI seats: `random_safe`, `greedy`, `bfs` | `snk_ai` (each policy plays 20 fixed-seed games; bfs beats random_safe by a computed margin; a mutant policy loses) | an AI snake chasing food |
| S8 | scoring, speed-up, death/respawn, high score projection | `snk_rules` | the game-over popup with the high score |
| S9 | CPU: idle zero when stopped, running footprint with a browser open | `snk_cpu` (children and wakeups with the producer state beside the number; a positive control that detects a spinning host) | none; the numbers in the commit |
| S10 | exporter and studio provider; training table | `snk_export` | the dashboard: heat map and policy comparison |
| S11 | train off machine, load `trained1` as data | `snk_policy_load` (a weights file changes the choice; a corrupted file is refused) | the trained snake playing |

## 22. Effort ranking and order

1. **Chutes and Ladders first, smallest.** One verb that matters, three phases, a pure-data board, no AI, no real time, a page that is a rectangle. It exercises the extraction of `game_rng`, `game_ledger`, `page_labels`, `game_driver_core`, `game_camera` once, with Teropoly's pinned cases as the safety net. It is a perfect data-studio provider (P7 "a second provider with no studio code change", D58 open decision 5: my recommendation). Size guess (PROPOSAL, a guess): the engine about 400 to 700 lines of C (Teropoly's whole engine is 1,566), the driver and chat hook small once the shared pieces exist.
2. **Snake second, larger.** New: the pure tick core, the host loop with its CPU budget, the key hook in the shared core, the AI policies, the checkpoint discipline, a 3D top-down camera problem, off-machine training. Each of these is its own milestone; S0 to S5 are a playable human game, S6 to S11 are the extras.

Dependency: Snake S5 reuses `game_driver_core.c`, so Chutes C2 and C4 come first.

## 23. Risks (honest)

* **Extraction risk:** moving code out of `deskopoly.c` and its driver can break pinned Teropoly cases. Mitigation in section 3: extract at the second consumer with the existing cases as the gate; if a case goes red, the extraction is reverted, not the pin edited.
* **Shared hot file:** the `GAME | keys` hook touches `petslike_core.c`, a high-collision file; one tiny branch, own commit, harness first.
* **Real time on a weak machine:** a 150 ms host plus the 3D engine may not coexist with a browser; the budget in section 15 is a hypothesis until measured; 2D-first is the fallback; a larger `tick_ms` is a data change.
* **Camera:** the overhead 3D camera was unresolved on 2026-11-08 (memory note); Snake ships 2D-first.
* **Trademark and names:** "Chutes and Ladders" and, possibly, other product names; the owner decides the title; internal ids can stay.
* **Data from memory:** the jump table and the finish rule are MEMORY and must be checked against an official board before the ruleset claims to be "classic".
* **Hand-edit protection is not security:** section 12's limit; ranked play needs signing, out of scope.
* **Scope creep in the studio:** the chi-square op, the heat map and the training export are studio features (D58 P3 to P6), not these games'. The games only publish tables.
* **Atlas size:** 100 labelled tiles plus line overlays may exceed an atlas or loader cap I did not read (section 8).
* **Reading of "same game and events":** if the owner meant literally the same event pages, sections 17 to 19 explain why that would force ghost data (state files written by world-ops pages) and why the plan uses events for presentation only.

## 24. Open decisions for the owner

1. **Names:** the player-visible title of each game (Chutes and Ladders is a trademark; Snake is generic). Token art: pets as tokens, or generic figures?
2. **Chutes board and variants:** confirm or replace the MEMORY jump table; default finish rule (`exact_stay`, `bounce`, `reach`); `six_again` on or off; 2 to 4 players; who goes first (join order, or a spin-off).
3. **Chutes page:** is the data-only marker version (labels and tile colours) enough for the first release, or are drawn lines required? Slide animation along the line or a jump?
4. **Snake grid and walls:** 20x20 or other; solid walls or wrap-around (torus); obstacles; foods on the board at once.
5. **Snake rules:** head-on deaths for both or the shorter loses; respawn and lives or one run; score formula; speed-up curve; the floor of `tick_ms`.
6. **Snake input:** accept the shared-core `GAME | keys | claim` hook (a small change to `petslike_core.c`), or start with popup buttons only? Second human seat: how?
7. **Snake pets:** which AI ladder, and may a trained policy ship as data?
8. **Ledger discipline for real-time games:** approve option (d) (inputs, consequence rows, checkpoint rows; no row per tick; logical time) as the house pattern for any future real-time game, and `ckpt_every` (64 proposed).
9. **Training:** debil or the Mac for the snake trainer (compiler availability unchecked); human-seat games in training only on a flag (the data-hq open decision 3).
10. **Order:** Chutes first (recommended), or Snake first.

## 25. References

Repo (house root `x0.parent-level-dev-env-04.04/yz.muchiverse/44.xyz.01.00/`): `@.apps/deskopoly/` (`game.pdl`, `page.pdl`, `rulesets/classic/*.pdl`, `ops/deskopoly.c`, `ops/deskopoly_driver.c`, `ops/deskopoly_chat.c`, `state.pdl`, `verbs.pdl`, `tuning.pdl`, `tests/`), `@.apps/rpg-edit-hq/` (`time.pdl`, `keybinds.pdl`, `events/play|stop|day_tick|place|remove|control`), `@.apps/rpg-space/events/` (`death`, `respawn`, `destroyed`, `collision`, `col_hit`, `eat`), `&.widgits/_shared-lib/petslike_core.c` (`book_driver`, `game_chat_hook`, clock hold, watchdog), `&.widgits/_shared-lib/game_events.c` (the S1 event line), `&.widgits/events-hq/ops/common_events_manager.c`, `#.ref/menu/event_commands.registry.pdl`, `&.widgits/_shared-lib/harness/` (README, `cases/`), `&.widgits/_shared-lib/ops/+x/cpu_gate.+x`, `commit_scoped.+x`, `xo_export.+x`.

BOOK: `16.game/DESKOPOLY-CLASSIC-HOW-TO-PLAY-AND-DRIVE.md`, `08-roadmap/design-docs/DESIGN-DESKOPOLY-CLASSIC-MONOPOLY-BUILD-SPEC-2026-11-08.md` (5.2 to 5.6, 10, 11, 13), `08-roadmap/design-docs/DATA-HQ-DATA-STUDIO-PLAN-2026-11-09.md`, `02-architecture/NO-GHOST-DATA-THE-LEDGER-IS-THE-AUTHORITY-2026-11-07.md`, `02-architecture/DESIGN-CHOICE-TEST-AND-DECISION-LOG.md` (D53, D55 to D59), `02-architecture/BLUEPRINT-EVENT-PAGES-GUIDE.md`, `02-architecture/EVENT-ARCHITECTURE-ONBOARDING-2026-11-05.md`, `02-architecture/AGENT-TOOLBOX-AND-CONTROLS.md`, `16.game/RPG-SPACE-BLUEPRINT-SYSTEMS.md`, `16.game/RPG-SPACE-COLLISION-AND-DESTRUCTION-DESIGN.md`, `03-pitfalls/RUNTIME-FILE-GROWTH-STANDARD.md`, `03-pitfalls/CPU-OPTIMIZATION-PITFALLS.md`, `06-testing/WHAT-TO-LOOK-FOR-ACCEPTANCE-COMPENDIUM-PER-PROJECT.md`; class `1-1.HARNECIENT.SMOL/DAY_28_TWO_SMALL_GAMES.txt`.

Outside references (none fetched; the game facts are MEMORY): the traditional Indian game Vaikuntapaali / Moksha Patam is the origin of "Snakes and Ladders"; the Milton Bradley layout is the board remembered in section 4; the 1976 arcade game Blockade and the 1997 Nokia phones are the usual ancestors of Snake. All three are background from memory, not used by any rule.
