# The per-game setup `.pdl` (game cells, available maps, editable marks)

Written 2026-10-07 (claude). **Design only; the file does not exist yet.** The owner said the game cell `.pdl` files "don't exist but can be inferred": this is that inference, built from the `.pdl` conventions the house already uses, so it can be corrected cheaply before anything reads it.
Context: `PLAY-MODES-AND-MAP-ACCESS-DESIGN.md` (modes, map access), `XELECTOR-CURSWORD-POSSESSION-DESIGN.md` (starting position, designated body), `SAVE-SLOTS-DESIGN.md` (game data).

## 1. What the conventions are (read in the repo)

- Rows are **pipe-delimited, one fact per line**: `TYPE | key | value` (`#.desktop/livedesk_taskbar.pdl`: `SECTION | hq_menu_1_label | ...`; `board.pdl`: `BOARD | title | ...`, `SOURCE | md-table | path | cols=0,4`; entity `meta.pdl`: `META | piece_id | cursword`, `STATE | z_priority | 100`). `#` starts a comment. Extra columns are allowed when a row needs them.
- A menu action is a **string**: `livedesk:<verb>` (built into the taskbar), `sh <path>` (a script, run from the house root), or a bare verb such as `quit`.
- Events are programmed in the event system (`event_pkg`, Common Events, `#.ref/menu/event_commands.registry.pdl`); the door's "transfer player" command is the existing teleport.

## 2. The inferred file: `game.pdl`, one per game (= one per db session)

Lives in the db session's folder (a db session IS the game). All rows are **authored data**: edited in build mode (and in play-test where marked), never changed by play.

```
GAME   | title        | My Game
GAME   | start        | <entity id>                 # THE starting-position entity (exactly one per game)
GAME   | body         | hero_01                     # designated body for layer 2 of the possession stack; absent = cursword only
MAP    | <desk id>    | available | 1               # reachable during play and play-test (build mode: every map, this list is ignored)
CELL   | <cell id>    | <label> | modes=<list> | where=<list> | order=<n> | cmd=<action>
ROW    | <cell id>    | <n> | <label> | cmd=<action>   # dropdown rows of a cell, same shape as ai_menu_N_label/_cmd
EDIT   | <entity id or path> | playtest | 1          # editable in play-test (everything else is read-only there)
```

- **`CELL`** is one header on a top bar (the livedesk taskbar and/or the pc-hq top bar). `modes` = any of `build`, `play`, `playtest`; `where` = `livedesk`, `pchq`. A cell with no `ROW`s runs `cmd` when pressed; with `ROW`s it opens a dropdown.
  The built-in headers (db, plugins, book, page, player, ...) are the same kind of row: their `CELL` lines say in which modes they show. **Build mode ignores the list and shows everything**, so an author can never lock themselves out. A game's own cells (the owner's example: a "game title" menu) are just more `CELL` rows.
- **`cmd` / action** (inferred, three forms, same family as today):
  `livedesk:<verb>` built-in verb; `sh <path>` script; **`event:<id>`** run a Common Event of the game (new, because a game cell most naturally runs authored events; resolved through the same `play_event.sh` path other event commands use).
- **`MAP`** rows are the whole access rule: a map change (teleport event, menu row, book/page tab) in play or play-test is allowed only if the target desk is listed. Everything else is refused at the one desk-switch point.
- **`EDIT`** rows are the play-test marker. Inference from the existing flags: pieces already use `possessable`/`de_possessible` in `piece.pdl` and event objects use `event_object | 1` in `meta.pdl`, i.e. a per-item flag. The game-level `EDIT` list is the same idea kept in one place so the author sees every editable thing together; an item's own `meta.pdl` may also carry `STATE | playtest_edit | 1` (either source grants edit; the game list wins on conflict). Both are inference, to confirm.

## 3. Readers

One shared parser, text-included (house rule: pure logic with two or more consumers is a text-include, never header + link): `khtpm_game_setup.c` in `&.widgits/_shared-lib/`. Consumers: the livedesk taskbar manager (cells, rows), the pc-hq projector/action (cells, rows, `Player` label), the desk-switch op (`MAP`), the edit gate (`EDIT`), play start (`start`, `body`). Reads are cheap file reads at the moment of use (no restart; same as the taskbar's `ai_menu` rows today).

## 4. Teleport refusal feedback (owner answer: "depends on the event programming")

The engine **refuses and logs** (ledger row: mode, target, who asked), nothing more. What the player *sees* is decided by the event that asked: the transfer-player Common Event may follow its teleport command with its own Show Text (or a conditional branch on the result). The desk-switch op returns a nonzero exit code and a one-line reason on stdout so an event can branch on it. No engine-side popup. (Menus that list only available maps never offer a refused target, so only free-form teleports can be refused.)

## 5. Build order (nothing done)

1. `khtpm_game_setup.c` parser + a test with a scratch `game.pdl` (every row type, comments, unknown rows ignored, missing file = build defaults).
2. `MAP` check in `mr_transfer_desk` and the taskbar desk switch (+ refusal code/reason, ledger).
3. The third mode value (`playtest`) in `khtpm_play_mode.state.txt` and its readers.
4. `CELL`/`ROW` -> livedesk taskbar headers, then the pc-hq top bar (db, plugins first).
5. `EDIT` gate and the play-test save routing.
6. `event:<id>` action form.

## 6. Still open (small)

1. Is `game.pdl` the right name and place (in the db session folder)? Cheap to rename now.
2. May a game cell hide a **built-in** header only in play/play-test (inferred: yes), or can it also rename it?
3. Do `where=` and `modes=` need to combine per cell (a cell visible in pc-hq play-test only), as written, or is one enough?
