# How to play the games as a human, and what Save / Load really do (2026-11-10)

Owner request, 2026-11-10: instructions for each working game on how to play it as a human (if it is playable), with the menu options, save and load. The owner has not human-tested any of these games. **Nothing below was done by a human on a real screen**: every control comes from files I read (labels in `nav.pdl`, `pchq-board.xhtpm`, driver sources) or from scripted harness runs in a scratch house. Labels: **READ** (a file says so), **RAN** (a harness or scratch run did it), **UNTESTED** (nobody has tried it as a person). Report what you see by writing the result under "Your results" at the end of each game's checklist (or tell the agent); a failed step is a finding, not a mistake.

## 0. Which games can a human play today?

| Game | Human playable? | How it is shown | Status |
|---|---|---|---|
| **Teropoly** (classic Monopoly, internal `deskopoly`) | **Yes** (the only one with the full human path) | 3D/2D board in the rpg-edit window, popup buttons, players bar, table talk | READ + RAN (scripted drives); never played by a human |
| **Chutes and Ladders** (`@.apps/chutes`) | **Yes, in principle**: a Roll button for the human seat and a numbered 10 x 10 serpentine board (2D and 3D renderers) with figures on the squares | board picture, popup, table talk | RAN in a scratch editor with the real renderers (2026-11-10, screenshots opened); UNTESTED in your live editor and by a person |
| **Snake** (`@.apps/snake`) | **No**: the rules and the 30 Hz loop exist, but no board picture, no figures, one snake only | a harness could steer it with arrow keys; a window shows nothing useful | RAN by harness only |
| **Teario-Make / Teario-Play** (level editor and platformer) | **No**: only the shared level file and the publish rule exist; no physics, no UI | none | RAN by harness only |
| **Data Studio** (`data-hq`, not a game) | **Yes** as a viewer, after a taskbar rebuild and desktop relaunch | window under the `db` menu | RAN in a scratch X server |
| Older games (rpg-space, df-hq, mcl-hq, cdda-hq, digipet and more) | exist; **not covered here** | see their own docs: `16.game/DF-HQ-DIG-HOWTO.md`, `RULES-df-hq.md`, `RULES-mcl-hq.md`, skill `rpg-space-board-driving` | not re-checked today |

## 1. The editor you play inside (all games)

The games run **inside rpg-edit-hq**, the editor window (the "Play" button is the games' only front door).

**Top bar.** `In: ON/off` toggle (READ `nav.pdl`): **In: ON pauses the game clock** (the day/night world clock; Teropoly ships a row that holds it anyway) and sends number keys to the game world; **In: off** is for pressing menu items. Esc backs out one level (releases possession first, then leaves In). The `Player` tab shows "Player: playing/stopped" and opens the **Player menu**:

| Player menu row | What it does (READ `pchq-board.xhtpm`, `petslike_core.c`) |
|---|---|
| INT on/off (arrow keys drive a pet) | arrows move the selected pet (not a game piece) |
| mode label | the editor's edit/play mode |
| **Play** | starts the book's driver (the game's loop) |
| **Stop** | freezes the game; the ledger stops growing |
| blueprintz | opens the book/page editor |
| **Save (slot 1)** / **Load (slot 1)** | copies of the editor's **state files** (pets, trainer, ledger, relations, chat, flags, clock time) into `state/slots/<n>/`, n 1 to 9; slot 0 is the automatic copy that **New game** makes of the editor world |
| Synch (desk to game) / (game to desk) | copies the desk (map) and the game data in either direction |
| - cancel - | closes the menu |

**Important about Save / Load:** these rows save the **editor's world**, not a particular board game. Whether a slot also contains a game's own ledger folder (`state/games/<game>/`) is **UNTESTED** (the code comment says "state files"; I did not verify the game subfolder is included). Do not rely on slot 1 to save a Teropoly game: use the game's own persistence (below).

**Opening a game.** Open the editor, pick the game's **book** in the Books dropdown, then **Player > Play**. Teropoly's book is `deskopoly-pchq` (READ). The new books (`chutes`, `snake`, `teario-make`, `teario-play`) are linked by the board launcher **at the next board relaunch** (READ `open_board.sh`, per the generator guide): until you relaunch the board they will not be in the dropdown. I did not relaunch your live board.

**The 3D picture.** The 3D engine **stops by design after about 270 seconds with no input** (READ memory note, `/tmp/bv_engine_exit.log`). A frozen picture while waiting for your turn is not a crash: the next key or click respawns it (new engine pid). Camera keys with the window focused (READ `keybinds.pdl`): `0` toggles 2D/3D, `1` to `4` are the first-person/other POV modes, `8` snaps to the controlled hero (not to Teropoly player 1).

## 2. Teropoly (classic Monopoly, shown to players as "Teropoly")

Rules are **from memory of the standard edition and not yet checked against the official sheet** (READ `DESKOPOLY-CLASSIC-HOW-TO-PLAY-AND-DRIVE.md` section 3). Tile names are placeholders (spiral flavoured), not final lore names.

**Start a game (READ `nav.pdl` popup).**
1. In the editor choose the Teropoly book, then Player > Play. A popup shows the **setup screen**.
2. Buttons, top to bottom: **players -**, **players +** (2 to 4 in the setup I read: 3 by default), **you play: none | p1 | p2 | p3** (pick the seat you control; `none` = all pets), **seed N (next)** (same seed = same dice), **speed: fast | normal | slow**, **START GAME**.
3. Press **START GAME**. The left **players bar** shows `> p1 A $1190` (cash; `>` marks whose turn), the footer shows net worth for everyone (`>p1 1540  p2 1490`), the right **table talk** shows the last events as sentences.

**Your turn (READ popup labels).** The popup offers only what is legal now: **Roll**, then (when you land on an unowned tile) **Buy <tile label> ($price)** or **Skip**, then **End turn**. **Skip sends the tile to auction**: the pets bid, and you can bid in the table-talk box. In jail the popup offers the jail options (pay, use a card, roll). Build houses, mortgage, sell and pay-debt are verbs of the engine; whether each has a popup button is **UNTESTED** (type them in the box if there is none).

**Trading (RAN, scratch).** Two ways: the **trade screen** (four panes; press tiles in and out; no drag and drop), or **type in the table-talk box** while the game runs: `offer p2 give Orange3 get 20` (a tile by id **or by the name printed on the board**), `counter 3 ...`, `accept`, `reject`, `cancel`, `bid 60`. A line the game cannot read is refused with a reason and nothing is written. Offers do not block the game; an open offer expires at the end of the turn; at most 3 new offers per turn by default.

**Camera.** A popup button **Find me** puts the follow camera back on your piece (an explicit press takes the camera back from a free camera; the automatic per-turn snap respects a free camera).

**Save and load.**
| Want | How | Status |
|---|---|---|
| keep the game when I stop | **Player > Stop**; the **ledger** (`state/games/deskopoly/game/ledger.txt`, append-only) IS the save; **Play** again continues from it | READ (nav: "Stop freezes the game, the ledger stops growing"); resume UNTESTED by a person |
| a fresh game | popup **New game**: the driver **archives** the last game (the last 3 are kept in `state/games/deskopoly/archive/game-<time>/`, older ones deleted) then shows setup | READ (`deskopoly_driver.c archive_game`) |
| load an old game | **no menu row exists**. An archived game folder can be replayed/inspected by tools (`deskopoly replay`, the Data Studio lists them read-only) | READ; no "Load game" feature |
| editor Save/Load (slot 1) | saves the **editor world**, probably not the game folder | UNTESTED, see section 1 |
| undo a New game | `Load slot 0` (the editor world copy made by the editor's `newgame`), not the Teropoly game | READ for the editor only |

**What to look for (a human checklist; tick and write what you saw):**
- [ ] The board shows 40 named tiles, not an empty grass sheet (a past failed test had an empty screen).
- [ ] Setup buttons are all there; START GAME starts; your seat is the one you chose.
- [ ] Roll moves YOUR piece the number shown in the table talk; Buy deducts the price from your cash in the left bar.
- [ ] Skip starts an auction and pets bid; the winner pays and owns the tile.
- [ ] Stop, close the editor, reopen, Play: the same game continues (round, cash).
- [ ] New game archives the old game (check the Data Studio list or the archive folder).
- [ ] CPU stays calm while you wait for your own turn (the game clock is held).
Your results: ______________________________

## 3. Chutes and Ladders (foundation; no board picture yet)

**What exists (READ the plan Status block, RAN in a scratch editor):** the full rules engine (a 100-square board as DATA with 9 ladders and 10 chutes **from memory, not checked against an official board**, exact-finish rule, a seeded spinner), table-talk sentences (`B rolled 2 and climbed the ladder from 80 to 100 and wins`), 2 to 4 players, pets that spin by themselves, and a **Roll** button for the human seat. **The numbered board is built (2026-11-10):** 100 squares in serpentine order (1 bottom-left, 10 bottom-right, 11 above 10, 100 top-left), ladders drawn blue with `^` and the target, chutes orange with `v` and the target, the other ends marked with the source, and each player as a figure on their square (4 players share the same square when they land together). **Not built:** the follow camera, a walking animation (a ladder or chute teleports the figure), drawn ladder/chute lines (the ends are only coloured and labelled); the overhead 3D camera view showed sky in the scratch run.

**To try it (UNTESTED live):** relaunch the board so the `chutes` book appears, choose it in Books, Player > Play, set **you play: p1**, press **START GAME**, then press **Roll** on your turn. The game ends at square 100 (with exact finish by default: overshoot stays where you are).

**Save/load:** same as Teropoly: its ledger is the save; no Load menu row. The Data Studio can list its games.

**What to look for:** [ ] the board shows 1 at the bottom-left and 100 at the top-left with the numbers readable; [ ] your figure moves to the square the table talk names; [ ] the popup offers Roll only on your turn; [ ] the sentence in the table talk matches the roll; [ ] a ladder moves you up and a chute down as the sentence says; [ ] the game ends with a winner sentence. Your results: ______________________________

## 4. Snake and Teario (not playable by a human yet)

- **Snake:** the rules (head, trail, seeded food, wall and self collision, growth) and an in-process driver that can step at 30 Hz exist and are tested by harness (case `snake`). Arrow keys reach the game as codes 1000 to 1003 through the editor's key file **only while Interact is armed**; there is no board picture or figures. **Do not expect to play it yet.**
- **Teario-Make / Teario-Play:** a shared 20 by 12 level file, a verified-clear publish rule and an inputs-only ledger exist; no physics, no editor UI. **Not playable.**

## 5. The Data Studio (a viewer, not a game)

After the owner rebuilds the taskbar (`sh '$.crypts/button.sh' build`) and relaunches the desktop, the **db** toolbar cell has a row **data-hq**. It opens the Data Studio window: a left **SESSIONS** list (the live game, archived games, fixture tables; **PRIVATE** is shown on games a human played), top buttons `Dashboard | Table | prev | next | rows up/down | cols < > | clear | refresh`, a chart or table in the middle. Tick two games and press `Dashboard` to see charts per game. It **only reads** game data. (RAN in a scratch X server with the real archived Teropoly games; never opened from the real db menu.)

## 6. Traps a human will hit

- A frozen 3D picture = the engine idled out; press a key.
- Pressing a menu item needs **In: off**; number keys go to the game world when In is ON.
- A blind press on the Player tab **closes** an open Player menu.
- The 3D engine and the screenshot tools are heavy: the machine is weak; close extra windows before a long session.
- Tile names are placeholders; rules are from memory; the pets use a simple fixed policy (not personalities).

## 7. External references

BOOK: `16.game/DESKOPOLY-CLASSIC-HOW-TO-PLAY-AND-DRIVE.md`, `16.game/TEROPOLY-ONBOARDING-HARNESSES-DRIVING-AND-CPU-LESSONS-2026-11-08.md`, `16.game/CHUTES-LADDERS-AND-SNAKE-GAME-PLANS-2026-11-09.md`, `16.game/TEARIO-MAKE-PLAY-LEVEL-EDITOR-AND-PLATFORMER-PLAN-2026-11-09.md`, `08-roadmap/design-docs/DATA-HQ-DATA-STUDIO-PLAN-2026-11-09.md`, `06-testing/WHAT-TO-LOOK-FOR-ACCEPTANCE-COMPENDIUM-PER-PROJECT.md`. Repo (house root `x0.parent-level-dev-env-04.04/yz.muchiverse/44.xyz.01.00/`): `@.apps/deskopoly/nav.pdl`, `@.apps/rpg-edit-hq/board/pchq-board.xhtpm`, `&.widgits/_shared-lib/petslike_core.c` (Player tab, slots), `@.apps/deskopoly/ops/deskopoly_driver.c` (archive and new game).
