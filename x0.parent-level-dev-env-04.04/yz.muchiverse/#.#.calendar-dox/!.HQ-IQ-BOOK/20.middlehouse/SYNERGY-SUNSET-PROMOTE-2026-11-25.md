# TPMOS, middle house, LyfeDesk: share, sunset, promote

2026-11-25. Deeper pass after the cursory house inventory. The 30-app list is still `middle-house-conversion.txt` (driven 2026-10-24). This note did not relaunch any of those apps. What was checked today is named below.

Three generations, three jobs:

| Generation | Job | Keep | Do not keep |
|---|---|---|---|
| TPMOS | 2D layout, redraw when a marker file grows. No 3D. | The marker rule: input writes a file, the painter reads it. | Pointer-move as a reason to redraw. A `pkill` that names `system/renderer` with no session cwd. |
| Middle house | A terminal frame, a pal loop, sometimes a GL mirror of that frame, plus a pile of real game ops. | The ops that change a portfolio, a case, a corp. The sim-key hook on WSR. | The five processes (`keyboard_input`, `renderer`, `prisc`, `chtpm_parser_pal`, `gl_mirror`) as the product. The GL mirror. gl-os is not a fourth product. |
| LyfeDesk / rpg-edit | One `khtpm_core_render` window, books as data, shared `game_core`. | The window, the book list, the ledger rule, harness cases. | A second copy of the middle-house stack hiding under a converted board. |

Owner note `44.xyz.01.00/!.!.tpmos-first.txt`: do not only fix WSR. The target is one turn ledger. A player and an NPC append the same kind of row. A human and a model are controllers, not a second ruleset.

## What was checked on 2026-11-25

No `button.sh run`. No GL. No keys.

- 42 `button.sh` files still match `pkill`. WSR's own `kill` still runs `pkill -f system/renderer` and the same for `keyboard_input`, `prisc+x`, `chtpm_parser_pal`, `chtpm_rgb_render`, and `gl_mirror` (lines 45-50 of its `button.sh`). That pattern matches every middle-house process on the machine, including a board that is open.
- Group B launchers still start `system/orchestrator` beside a window: aomorai-editor, civ-txt, tactics-txt, piececraft-xyz, TSC_ELO. civ-txt and tactics-txt still exec `gl_mirror` when the binary is present.
- These folders are still on disk: pal-forum, muchi-pals, wsr-pal, mutaclysm 19, my-lawyer, my-chara-txt, my-biotech.
- WSR `ops/` has 39 `.c` files. The trade verb is `player_trade.c`. It writes `pieces/player_you/holdings.txt` (`ticker|shares`, negative means short). It does not append a ledger row. Price moves in `corp_update_price.c` on End Turn, not on the trade. That split is written in the file header.
- my-lawyer has its own `system/renderer.c`, `keyboard_input.c`, `orchestrator.c`, `chtpm_parser_pal.c`, `chtpm_rgb_render.c`, plus `mylawyer_case_worker.c` and `mylawyer_judge_worker.c`. The case worker says it copies `gemma_ask()` from my-biotech on purpose, and it appends argument text to `data/cases/<id>/<side>_case.txt`. That file is the authority today. It is not a ledger.
- `@.apps/rta/ops` has 12 `.c` files and already matches the later shape (one op, a grid, a desk). It does not use the middle-house tty stack.

## Codebase differences that matter

Middle house looks like one template. The difference that decides the work is where the fact lives.

- **Shell copies.** `system/*.c` is the same five programs, copied per app. Changing keyboard handling in my-lawyer does not change WSR. Synergy is one shell, or no shell. Sunset every extra copy after the LyfeDesk window for that app is proven.
- **Verb copies.** WSR's corp, gov, realestate, and pop each have `decide`, `tick_idle`, and sometimes `trade`. Same shape, four domains. Synergy is one tick op with a ruleset name. Do not merge the C by hand into a god file.
- **Fact files vs ledger.** WSR holdings, my-lawyer case text, and the October frame file are projections that became the authority. The later house rule says the opposite. Promoting an app means the verb appends a row first. The txt and the window are rewritten from that row.
- **Async workers.** my-lawyer's case worker is a pid file plus a status file under `data/`, so it survives quitting. That is a real idea. The house version is `proc_reg_op` and a ledger row, not a private pid file.
- **Partial windows.** Group B already calls board-viewer `run-widget` and still starts the orchestrator. Promoting them is finishing the window and deleting the pair, not adding a third renderer.
- **Already promoted.** Centon, Bay slice, and the four city books are LyfeDesk books. Their Play button still starts generic `game_core` and does not call `rta_city_op`. The window moved. The verb did not.

## Synergy (one implementation, many callers)

- Office verbs WSR already has: trade, loan (`bank_loan_op.c`), IPO, merger, stake, news, real estate. my-lawyer already has a case build and a judge. Civ and a startup sim should call those, with different RULE numbers (tax rate, fee, pay period).
- The turn shape in `!.!.tpmos-first.txt` is the same shape as Teropoly's ledger and the hai party row. One append. Next entity reads the tail.
- sim-key on WSR is the middle-house version of a harness step. Keep the idea (a key with no tty). Point it at an op, not at `prisc pal/main_loop.pal`.
- Marker-file redraw from TPMOS and the board's dirty frame are the same idea. One implementation in the renderer. Apps do not each poll the mouse.

## Sunset (stop spending)

- `gl_mirror` on every app. It was not started in the 2026-10-24 run. It is not the product window.
- Unscoped `pkill -f system/renderer`. Replace with a kill whose `/proc/<pid>/cwd` is that session, the pattern already named in the conversion list. Do not mass-edit the 42 scripts in a drive-by.
- Duplicate `system/*.c` and duplicate `json_parser.c` (WSR and my-lawyer each have one).
- Group A games whose job is now a LyfeDesk book: LPNS word game, zoo, ttg-tactics, rpg-xyz, rtp-xyz, the agy editors, START_BUTTON. Read them for a rule worth an op. Do not port the launcher.
- Mutaclysm 18 once 19's interact mode is the reference the owner named. 19 stays until that keyboard bug is a harness, not a memory.

## Promote (the thing that should become the house)

- WSR's verb list, not WSR's window. First op to wrap: `player_trade` writing a TRADE row, holdings.txt regenerated from it. A scratch house. An agent calls the same binary a key would.
- my-lawyer's case rounds (select angle, search corpus, write a point, one closing pass) as event pages. The worker pid file goes away when the proc registry owns the process.
- my-chara as the sheet template for my-biotech and my-lawyer only after the sheet is rows. October showed the screen can change (main to farm). That is a UI proof, not a data proof.
- TSC_ELO's duel and Elo write-back, already called out as unfinished in the Tear-it roadmap. It is group B. Do not open it on the live desktop to "see". A harness seat is the promotion.
- RTA city books: promote the painter and the stand-up, which exist. Sunset the idea that Play already plays the city.

## Focus for the next session that has quota

One app. WSR. One verb. Trade. Ledger row, then the old holdings file as a projection. No GL. Kill scoped to that session. Do not start the other 29. Do not open a second rpg-edit window to show the city books.

## External references

- List and the October run: `20.middlehouse/middle-house-conversion.txt`
- How to drive without the tty: `20.middlehouse/HOW-TO-RELAY-DRIVE-MIDDLE-HOUSE.md`
- Owner scan note: `44.xyz.01.00/!.!.tpmos-first.txt`
- Trade that writes a text file: `44.xyz.01.00/014.wsr-pal/ops/player_trade.c` (folder name has emoji on disk)
- Case worker that copies gemma_ask: `44.xyz.01.00/@.apps/my-lawyer/ops/mylawyer_case_worker.c`
- Cursory inventory this extends: `XO/31.Nov/nov-25/0.house-inventory-cursory.md`
