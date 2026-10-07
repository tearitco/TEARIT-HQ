# Play modes (build / play / play-test) and which maps a player can reach

Written 2026-10-06 (claude) from the owner's words. **Design only, nothing built.** Related: `XELECTOR-CURSWORD-POSSESSION-DESIGN.md` (starting position, possession stack),
`SAVE-SLOTS-DESIGN.md` (what a slot holds), `PLAY-MODE-ENTITY-HARNESS-DESIGN.md` (older Play Mode doc: Move/Inventory/Ops/Stats/STOP menu).

## 1. The owner's rules (2026-10-06)

> "when game is being played, only maps available to user are accessible, thru teleports (either on screen, or thru menus). This is set thru .pdl (what is available during both 'play' and 'play-test'). We will also add a 'play test' option. It allows events to play but also editing to occur, and be saved in real game events, not just to save file."

So three states, not two:

| Mode | Events run | Editing | Maps reachable | Where changes go |
|---|---|---|---|---|
| **build** (normal, the "debug" state) | no (authoring) | yes, everything | all | the real game files |
| **play** | yes | no | **only the maps the .pdl lists as available** | the save slot only (game data + game time) |
| **play-test** | yes | **yes** | same available-maps rule as play | **the real game events/data** (edits), AND play state still goes to the save slot |

Starting any play mode begins at the game's single starting-position entity and restarts in-game vars/events (see the possession doc, section 5).

## 2. What exists today (read, not run)

- Play Mode is a **two-valued** house-wide flag: `#.desktop/khtpm_play_mode.state.txt`, `mode=on|off`. Readers/writers seen: taskbar manager (`khtpm_taskbar_manager.c`), pc-hq projector (`pchq_board_projector.c`, label `Player: ON/OFF`), `pchq_board_action.sh` (`player` verb), the cursword harness script. A third value must not break them: keep `on`/`off` meaning what they mean now and add the new value, then check each reader handles an unknown value as `off`.
- **Teleport already exists as a Common Event**: the door entity's `cmd_1.sh` is a "transfer player" command that runs `mr_transfer_desk.+x` (moves the player to another desk/map by the same mechanics as `livedesk_switch_desk()`). So "teleport on screen" = an entity with that event; "teleport through menus" = a menu row running the same op.
- Maps are desks/pages in the user's session (`sessions/<name>/desks/<desk>.pdl`); the taskbar's book/page tabs let a builder switch to any of them, which is exactly what play must NOT allow.

## 3. Design

1. **Mode value.** `mode=off` (build) / `on` (play) / `playtest` in the same file. One place decides; every UI that shows `Player: ON/OFF` shows the third state too.
2. **Available maps (.pdl).** A game-level `.pdl` lists the maps (desks/pages) reachable in play and in play-test, one line each, e.g. `MAP | <desk id> | available | 1`. Anything not listed is unreachable while a play mode is on. The list is **authored data** (build-state edit), not game state, and it is read each time a map change is requested (no restart needed).
3. **Enforcement point.** Both teleport paths funnel into the same desk switch (`mr_transfer_desk.+x` for events; the taskbar/pc-hq book and page tabs for menus). The check belongs there, once: in a play mode, refuse a target not in the list and refuse the free book/page tabs entirely (or show only the listed maps). Refusals are logged, not silent.
4. **Teleports on screen and through menus.** On screen: an entity whose event is transfer-player (exists). Through menus: a menu that lists only available maps; same op underneath.
5. **Play-test saving rule.** Two layers, kept apart:
   - **Authored definition** (maps, entity designs, events, the available-maps list, the starting position): edited in build and in play-test; play-test edits are written **to the real files**, so they are real game events/data.
   - **Game state** (game data + game time): changes during play and play-test go to the **save slot**, never into the authored files.
   Play-test is the only mode where the authored layer is writable *while* state is changing.
6. **Interaction rule for play-test edits.** If the player edits an entity whose state is also in the current save state, the **definition changes immediately and the live state keeps its values** unless the field is new; on the next game start/load the definition wins for anything not in the slot. (Needs the owner's confirmation, section 5.)
7. **Audit.** Mode changes, refused teleports and play-test edits are appended to the append-only ledger (house rule), so a run can be re-audited.

## 4. Build list (none done)

1. Third mode value + readers (taskbar, pc-hq projector/action, harness) tolerate it; UI label.
2. `available maps` .pdl format + loader (shared text-include if two readers).
3. The check in the one desk-switch path (+ a test: listed map allowed, unlisted refused, build mode unrestricted).
4. Hide/limit the book/page tabs in play modes.
5. Play-test: edit permissions on, save routing (authored -> real files, state -> slot), ledger rows.
6. Interaction with the starting-position entity and game-data scope (above docs).

Feasibility: yes. Teleport, desk switch, the mode flag and the ledger all exist; the new work is the third mode, the .pdl, one enforcement check, and the two-layer save rule (the only part with real design risk).

## 5. Owner answers (2026-10-06/07) and what is still open

- **A "db session" IS the game.** So "per game" and "per db session" are the same thing: the available-maps list, the starting-position entity and the designated body all belong to the db session. (This also simplifies the possession doc, which listed them as two places.)
- **Play-test editing = whatever is marked.** Not everything: each editable thing carries a marker for "editable in play-test" (the exact word is open, `playtest` / `edit`), set when it is authored. No marker, no edit in play-test.
- **Rule 6 confirmed:** an edit during play-test changes the **definition immediately**, the live state keeps its values.
- **Menus in play are gated by a user-defined setup from a .pdl** (or similar): the game's author decides which menus (db, plugins, ...) exist during play and play-test. A game may also **add its own cells and menus** (the owner's example: a "game title" menu). So the top-bar headers are **data-driven per game**, not a fixed list. This is the same mechanism as the queued pc-hq extra headers (possession doc, section 6): one per-game menu `.pdl` should drive the livedesk taskbar headers and the pc-hq top bar, in every mode (in build it lists everything).

Consequences for the build list (section 4): add (7) a per-game **menu setup .pdl** (header cells: id, label, which modes show it, what it opens, optional author-defined cells) read by both taskbars; (8) the **edit marker** on authored items and its check in play-test.

**Still open**

1. Does a refused teleport show a message to the player, or is the option just not offered? (Not answered yet.)
2. The edit marker: its name, and where it lives (per item in its `meta.pdl`, or one list in the game's setup `.pdl`).
3. The menu setup `.pdl`: one file per game, and how a game-added cell names the action it runs (an event, a shell op?).
