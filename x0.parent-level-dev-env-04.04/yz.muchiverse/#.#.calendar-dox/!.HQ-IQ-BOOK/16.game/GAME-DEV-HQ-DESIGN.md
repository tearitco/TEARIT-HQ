# game-dev-hq

A studio sim in the same shape as df-hq, cdda-hq, and mcl-hq: one daemon, one board, one ledger. The game it copies is Kairosoft's Game Dev Story. The old CLI under the share drive (`kairosoft-grok`, the `gds` C files) is a sketch only. Do not compile it and do not import its stock, bond, or four-company rules. Those were a later pivot. This page is the studio.

Built as `@.apps/game-dev-hq`. The menu window is `sh @.apps/game-dev-hq/button.sh run`. The office picture is `sh @.apps/game-dev-hq/board/open_board.sh`. Three staff start, so one hire fits in an office of 4.

## The week

Time is a week. One player verb ends the week. Staff on a job also tick that week. Funds and fame are two numbers in `state/studio.txt`.

```
STUDIO | funds | fame | office | week
```

Office 1 holds 4 people. A bigger office is **later**. It only raises the cap.

## People

A person is a row and a sprite in the office. Jobs: `writer designer programmer artist sound director`. Stats, each 0–500: `scenario graphics programming sound creativity`. Plus `energy` 0–100 and `salary` per week.

```
STAFF | id | name | job | scenario | graphics | programming | sound | creativity | energy | salary
STAFF | s1 | Rex | programmer | 40 | 30 | 80 | 20 | 50 | 80 | 2000
```

Hire costs a flat fee and adds a row. Train spends the week and adds points to one stat of one person, and spends energy. Rest spends the week and fills energy. Job change is **later**. Pay every salary at week end. Energy at 0 means that person adds nothing that week.

## One game

A contract is a genre and a type. Genre examples: `action rpg puzzle`. Type examples: `sim adventure shooter`. The pair is the whole pitch.

Phases, in order: `proposal production debug review`. Each phase is a week with staff assigned. The four scores start at 0. Each assigned person adds their matching stat, times energy/100, into the score that job feeds:

| job | score |
| --- | --- |
| writer | fun |
| designer | creativity |
| programmer | polish |
| artist | graphics |
| sound | sound |
| director | a small add to all five |

```
GAME | id | title | genre | type | phase | fun | creativity | polish | graphics | sound | assigned
GAME | g1 | Pip Quest | rpg | adventure | production | 0 | 0 | 0 | 0 | 0 | s1,s2
```

`assigned` is staff ids, comma-separated, no spaces. Debug is the same add, smaller. Review does not add. It turns the five scores into one rank and a sale.

Ship: rank is the average of the five scores, 0–100 after a cap. Sales are rank times 1000, plus fame times 10, plus hype times 100. Funds increase by sales. Fame increases by rank / 10. Hype clears. Then the game row is kept as history and the studio may start another. Sequels, console licenses, contests, and the hall of fame are **later**. They read this same row.

Advertise spends cash for hype and does not end the week. Small is 5000 for 5 hype, mid is 15000 for 15, big is 40000 for 40. Fame also moves by hype / 5.

## Verbs

Same ledger as the other games (`state/events.txt`, `EVENT|ts|actor|verb|target|at|queued`). New verbs wait until the hook exists. For the first slice the daemon can append these and resolve them itself:

| verb | target | result |
| --- | --- | --- |
| hire | job id | a new STAFF row, or `broke` |
| train | `staff:stat` | that stat +10, energy −20, or `tired` |
| rest | staff id | energy 100 |
| start | `genre:type` | a GAME row in proposal, or `busy` if one is already open |
| assign | `staff` | that id joins the open game |
| advance | `-` | one phase, scores move, week +1, salaries paid |
| ship | `-` | rank, sales, fame. Only legal in review |
| advertise | small, mid, or big | cash down, hype up. Does not end the week |
| int | `-` | take the floor. Staff stop talking |
| select | staff id | the footer name. Train and assign use that person |
| play / stop | `-` | staff chatter on or off |

`at` stays `-` or `office`. People stand at a desk while the week is open. A week-ending verb (train, rest, advance) pays salaries and sends them out the door. The resident brings them back in for the next day. The door is the same `!Door1` crop the other RPG rooms use, on the wall row at column 6, with the mat on the cell in front. A `DOOR` row in `rooms.pdl` records that.

## What the window is

Open the live window with `sh @.apps/game-dev-hq/button.sh run`. The picture is one interior, `office`, painted from the house sheets (`Inside_A5` floor and wall, `Inside_B` desks, `!Door1`, the character sheets `party.pdl` names). `exportbook` also writes the pc-hq book `maps/game-dev-hq`.

Top bar: INT, Book, Page, Player, Clock, Menu. Those last three are the common entries the other games use. Player is play, stop, save, load, and new game (the old studio is slot 0). Clock sets a speed name and skip +1 week, which is the same week-end as train or advance. Menu shows or hides the studio and the chat, clears the chat, and resets with that same backup.

Left pane: cash, fame, hype, week, the open game, then the main rows Hire, Train, Contract, Advertise, Play, Stop. Hire, Train, Contract, and Advertise open their choices in the sidebar. Staff names are the footer buttons only. The right pane is the chat. Both panes have a `_` that hides them. Studio text is `#111111` on the cream pane. Chat text stays light on the dark strip.

The playthrough is `&.widgits/_shared-lib/harness/play/phases/game_dev_hq.phases.pdl`, same runner as df, cdda, and mcl.

## First build

1. `studio.txt`, `staff.pdl`, `games.pdl`, and the seven verbs.
2. Four starting staff, office 1, funds enough for one hire.
3. One genre list and one type list, three each.
4. Ship once and show rank and sales on the log.

Do not port the share-drive C. Do not add a market. Do not call `pc_generate_chunk`.
