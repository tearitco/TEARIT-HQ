# Tasks as event-type data (`.pdl`), draggable, usable by RPG Maker MV — design

Written 2026-10-06 (claude, hai manager). DESIGN ONLY: nothing in this document is built except where it says "exists". It **revises** `GRAVEYARD-GHOSTS-DESIGN.md` §6c and
`02-architecture/DRAG-AND-DROP-BETWEEN-MENUS.md` §5 after two owner corrections (verbatim):

> "we would just drag and drop the items in tasks, not the entities themselves. unless we wanted to store ghost in headstone. this should be information in a .pdl (event type data) that rmmv can also use."
> "also, we can add "assign" to assign tasks to available ghost, and "work" to take task from available headstone"

Earlier text that proposed a "quest card entity" (Option A) is superseded: the owner wants the **task item itself** to be what is dragged, and the task to be **event-shaped `.pdl` data**, not a markdown table row only.

## 1. Principles

1. **One record format for tasks, ghost jobs, and game events.** A task is event-type data in the house's existing event `.pdl` shape, so the same file can be edited in events-hq, shown on a board, run/loaded by the game engine, and mapped to RPG Maker MV data.
2. **The headstone owns the authoritative record.** Dragging, assigning and working never delete the headstone's record of a task; they create copies/pointers and append history (append-only, re-auditable).
3. **Statuses and ownership are data, not prose.** `QUEST.md` stays as the human-readable brief; the machine state lives in the `.pdl`. `quests/INDEX.md` becomes a *view* generated from the pdl records (today it is hand-edited; the board reads it).
4. **Reuse before inventing**: the event page layout, the command registry, the drop handler, the phone events, the ledger. The only genuinely new renderer capability is "a list item can start a drag" (§5).

## 2. What exists today that this builds on (read in the repo)

| Existing | Where | Shape |
|---|---|---|
| Event page data | `<entity>/event_pkg/pages/page_N/` | `condition.pdl` (`COND \| trigger \| Autorun`), `event.ir.pdl` (`SECTION\|KEY\|VALUE` header, `META \| piece_id`, `STATE`, then `NODE \| id=N type=<command> \| key=value...` lines), generated `cmd_N.sh` |
| Command vocabulary | `#.ref/menu/event_commands.registry.pdl` (`COMMAND/LABEL/FIELD1/FIELD2/PARAMS/TEMPLATE/END`) | adding a simple command = editing this file only, no recompile |
| Hierarchy | 🎬️ clacker (an entity's event set, `inventory/event_clacker_N`) -> ⚙️ page -> 🧩 reserved for single events (`EVENT-MODULARITY-AND-BUILD-SPEED.md`) | a pal and an inventory item are the same object |
| Global events | `<house>/common_events/<name>/` (e.g. `greet_player`, `pc_hq_trigger_caller`) | RPG-Maker-style Common Events, game-wide |
| Drop of an event | `&.widgits/events-hq/ops/event_drop_handler.sh` | dropping a 🎬️/⚙️ pal (marked `event_object \| 1` in `meta.pdl`) onto an entity's events-hq window pushes a new numbered `event_clacker_N` onto that entity |
| **Drop is a MOVE** | same handler, comment dated 2026-09-28: "drop deletes other location and mv should do the same, same as cli" | after copying all pages it `rm -rf`s the dropped source |
| Phone events | `HAI-ROBOTS-PHONES-SERVER-DESIGN.md`, `ops/phone_send_op.c`, `server_route_op.c` | kinds `say task ask-human answer status spawn done fail command lease release result`; server is the only inbox writer; ledger |
| RMMV analysis | `RMMV-EVENT-ARCHITECTURE-LEARNINGS.md` (cites `rpg_objects.js` lines) | trigger is a plain integer on the page; self switches are keyed `[mapId, eventId, 'A'..'D']`; `$gameSwitches` / `$gameVariables` are the persistent state |

## 3. The task record

A task is a **folder** (it already is one: `^.grave/quests/Qnnn-slug/`). Add event-shaped files next to `QUEST.md`; the folder is the unit that is stored, moved, dragged and dropped.

```
^.grave/quests/Q010-fix-the-build-gate/
  QUEST.md            human brief (unchanged)
  task.pdl            the machine record (below)
  event_pkg/pages/page_1/{condition.pdl, event.ir.pdl}   OPTIONAL: what the task DOES when run (commands), same as any event page
  history.txt         append-only: <epoch_ms>|<who>|<verb>|<detail>   (create, assign, lease, pending, approve, deny, done...)
```

`task.pdl` (proposed; `SECTION | KEY | VALUE` like every other pdl, one record per line):

```
SECTION | KEY | VALUE
----------------------------------------
META   | piece_id   | Q010
META   | name       | Fix the build gate
COND   | trigger    | Parallel           # the same word set as condition.pdl (None / Autorun / Parallel ...)
COND   | switch     | grave_open         # optional: only live while this switch is ON
STATE  | status     | open               # open | pending | claimed | active | review | done | failed | abandoned
STATE  | assignee   | -                  # a ghost's entity id, or its phone number
STATE  | holder     | -                  # who currently holds the lease (work) - may differ from assignee while pending
NOTE   | tier       | worker             # free tags: anything the engine does not interpret
NOTE   | size       | S
NOTE   | board      | ^.grave/quests
NODE   | id=1 type=comment | text=Mission: ...one sentence from QUEST.md
```
Rules: unknown `NOTE` keys must be preserved by every reader (that is what makes it a safe extension point); `STATE` is the only part that changes after posting, and every change appends a `history.txt` line AND, when it
crosses ghosts, a phone event.

## 4. RPG Maker MV mapping (so the same pdl is usable there)

What the house's own doc confirms (`RMMV-EVENT-ARCHITECTURE-LEARNINGS.md` §1,§3): page `trigger` is an integer; self switches are letters A-D keyed by map+event; `$gameSwitches` / `$gameVariables` hold persistent state.
What follows about RMMV's **data files** (field and command-code names) is from the author's knowledge of the MV format. **No MV `CommonEvents.json` / `MapNNN.json` is in the repo, so these are UNVERIFIED against a real file: check on first import.**

| task.pdl | RMMV | Note |
|---|---|---|
| `META name` | Common Event `name` (or Event `name`) | |
| `META piece_id` | `id` (integer) | RMMV ids are integers: keep a map `Q010 -> id` in a generated index; the `Qnnn` string goes in `note` as `<id:Q010>` |
| `COND trigger` | Common Event `trigger` (0 none, 1 autorun, 2 parallel) / page `trigger` (0 action, 1 player touch, 2 event touch, 3 autorun, 4 parallel) | |
| `COND switch` | `switchId` (Common Event) / page `conditions.switch1Id` | switch ids are integers; names live in `System.json` |
| `STATE status` | a **Game Variable** `task_Q010_status` (integer: 0 open, 1 pending, 2 claimed, 3 active, 4 review, 5 done, ...) read with the page condition `variableId` + `variableValue` ("variable >= N") | there are only four self switches (A-D), too few for the status flow; RMMV cannot compare strings in a condition, so the integer is the portable form. The house keeps the readable word and derives the integer |
| `STATE assignee` / `holder` | `note` tag `<assignee:ghost-a>` | RMMV has no string variables outside scripts; the `note` field is MV's standard extension point and plugins read tags from it |
| `NOTE *` | `note` tags `<tier:worker>` | the engine ignores them; plugins and this house read them |
| `NODE type=comment` | command code 108 (+408 continuation) | |
| `NODE type=control_switch` / `control_variable` | 121 / 122 | the house registry already has commands of these kinds |
| `NODE type=show_text` | 101 (+401 lines) | |
| `NODE type=script` | 355 (+655) | |
| page list | `list: [{code, indent, parameters}]` | `event.ir.pdl` NODE order = list order; `indent` from nesting |

Consequences: (1) a game can show the grave as an in-game quest log (tasks = common events + variables); (2) a ghost's own quests are just event pages on the ghost, editable in events-hq; (3) an exporter
`pdl -> CommonEvents.json` and an importer are small and testable once one real MV file is available (do that first; build the exporter as a verifier-checked op, not by hand).

## 5. Dragging the task item

### 5.1 What is dragged
The **task row/item** in a board (the existing generic board, `@.apps/board-hq`), not an entity. Its payload is **the task's own folder** (the XDND payload in this house is a folder path in `$DROP_PATH` — see
`DRAG-AND-DROP-BETWEEN-MENUS.md` §2), so the existing target side (`drop_action`, `xdnd_handle_selection`, the CLI `mv <nav#> <nav#>` equivalent, the hover and zone machinery) is reused unchanged.

### 5.2 The one new renderer capability
An opt-in `drag_payload="<folder>"` on an `<item>`: press and move more than a few pixels starts a **transient drag window** that stands for the item (the glyph/label under the pointer) and then follows the **existing** path
(`kh_write_drag_hover`, `kh_drag_stack_above`, XDND offer, `xdnd_handle_selection`). Today only a placed entity process can be a drag source (`tp_main` drag loop); this is the missing piece, and it is a *shared-renderer* change:
follow the renderer house rules (generic, no per-project global, clip never translate, idempotent layout), build to a scratch binary first and score it headless. Keyboard first, always: §7 step 1 needs no renderer change.

### 5.3 Drop = move, but the headstone keeps its record
The house rule (`event_drop_handler.sh`, 2026-09-28) is that a drop **moves**: the source disappears after a successful copy. A task must not vanish from the headstone when it is handed to a ghost (owner: it "can stay in headstone as pending").
Resolution (decision needed, recommended): **the dragged payload is a working copy** created at drag start (a transient folder holding `task.pdl` + `event_pkg/` with `STATE origin | Q010`). The drop handler consumes that copy
(consistent with "drop = move"), and:
1. the **headstone's** `task.pdl` is set `status = pending`, `holder = <ghost>`, a `history.txt` line is appended;
2. the **ghost** receives the task as a new event page in its own clacker (the same copy-pages code path as `event_drop_handler.sh`), marked `origin | Q010`, so it appears in the ghost's events-hq as one of ITS quests;
3. a phone `task` message (owner's phone -> ghost's number, `ref = Q010`) is the auditable record in the server ledger.
Nothing is deleted from the headstone; if the ghost is later denied, `pending` returns to `open` and the ghost's copy is removed by the approval step, not by the drag.

### 5.4 Ghost into headstone
Only when you want to **store a ghost in a headstone** is an entity dragged: a ghost *entity* dropped on a headstone window (a window with `drop_action`) — this already works with the existing entity-drag machinery, no new renderer code.
The handler records the ghost as resting in that headstone's roster (`ghosts/<ghost>/`), i.e. "unplaced". (And placing a ghost back out is the existing placer, `GRAVEYARD-GHOSTS-DESIGN.md` §6b.)

### 5.5 Owner clarification (2026-10-06, later): the dragged task item IS an entity (a "doc")

> "the drag item technically is an entity - a 'doc' stored in the headstone's dir somewhere. if it was dropped on desk, the 'task/doc.txt or w/e' would be physically 'on desk'. that's great. feature not a bug."

This supersedes the "transient working copy" idea in §5.3 and **removes the need for a new renderer drag source (§5.2)**: a task doc is a normal house object ("a pal and an inventory item are the same object", `EVENT-MODULARITY-AND-BUILD-SPEED.md` §1)
that lives in the headstone's inventory. The existing entity drag (X11, reparent-stack, XDND, `drop_action`) already moves entities; a doc shown on the desk is draggable like any other pal.
Consequences, all consistent with the house's "drop = move" rule (`event_drop_handler.sh`, 2026-09-28):
- **Physical location is state.** `headstone/inventory/` = the task is in the headstone; dropped on a desk cell = the doc is physically on the desk (visible, owner-visible, a feature); dropped on a ghost window = the doc is in that ghost's inventory (its own quests).
  `task.pdl STATE status/holder` records the same fact in data so it is auditable, and a reconciler can flag a doc whose location and `holder` disagree.
- **"Stays in the headstone as pending"** becomes a place, not a copy: a `pending/` bin inside the headstone (docs awaiting the judge and the official approval) next to `open/`. Assigning = move from `open/` to the ghost's inventory **through** `pending/` (the headstone holds it until approved, then the move completes; denied = returns to `open/`). Only the approval step moves it out of the headstone for good.
- **Storing a ghost in a headstone** is the same gesture the other way (ghost entity dropped on the headstone window = resting in its roster).
- **Remaining need**: a board must be able to *show* a headstone's docs as draggable things: either the doc entities are placed on the desk/hotbar (existing placer, works today), or the board lists them and "place on desk" is an action that uses the existing placer.
  A row-level drag (`drag_payload=`) is now only a convenience for dragging straight out of a list, not a prerequisite. Steps 1-4 of §7 are unchanged; step 5 shrinks to "board action: place this doc on the desk".
- The doc is still event-shaped `.pdl` (§3), still maps to RMMV (§4). Where §3 says "a folder", read "a doc entity folder with `meta.pdl` `event_object | 1` and `task_object | 1`".

## 5.6 pc-hq (what translates and what does not) — answer to "will these carry over to pc-hq menus?"

pc-hq (`@.apps/piececraft-hq/pchq-board.xhtpm`, verbs in `ops/pchq_board_action.sh`) has its **own hand-written dropdowns** (tabs: Book / Page / Menu / Player / clock) kept in sync with the taskbar **by hand**; nothing in the taskbar's `khtpm_taskbar_manager.c` reaches it.
The tb **Player** menu is the house-wide Play Mode control and pc-hq's Player writes the same flag (`#.desktop/khtpm_play_mode.state.txt`), so the *logic* is shared; the *rows* are not.
| Feature | In pc-hq | Work needed |
|---|---|---|
| save-game / load-game slots | logic shared (`game_slot_op`, takes any root); UI NOT present | add two rows to pc-hq's Player dropdown + verbs `savegame`/`loadgame` in `pchq_board_action.sh`; pc-hq has no "swap to a 16-slot sub-list" (the taskbar does it with menu ids), so use a vars flag + `show=` rows with a back row. **Decide what a slot covers when played from pc-hq**: today's manifest is the DESK user's entity tree only; pc-hq's game state is its session (`pieces/`); the op's root must be extended (a scope list) |
| h-ai menu boards | no h-ai tab in pc-hq | add rows to pc-hq's Menu dropdown calling `open_board_menu.sh <name>` (one line each) |
| confirm popup (Backspace) | automatic: same renderer | add `confirm=` to any pc-hq item that has a `backspace_action` (none today) |
| drag and drop of docs | does NOT carry: pc-hq pieces are canvas objects, not X windows (renderer comment: "click-to-place, NOT drag/drop") | the equivalent is placing a doc piece on the cell of a ghost piece; therefore build `assign` / `work` / `ghost_drop` as **pure ops taking (doc folder, target entity)** in `_shared-lib` so both front-ends call the same op (house rule: desk is the functional parent, lift pure logic into `_shared-lib`) |
Nothing here is built for pc-hq yet.

## 6. assign, work, pending, judged (unchanged from §6c, restated on the new data)

| Verb | Starts at | Effect on `task.pdl` | Phone event |
|---|---|---|---|
| **assign** | owner/headstone: drag a task item onto a ghost window (or button + pick) | `status open -> pending`, `holder = ghost` | `task` to the ghost |
| **work** | the ghost's menu action (or dragging nothing: the ghost lists the headstone's open tasks at or below its tier) | `status open -> pending`, `holder = ghost` | `lease` to the headstone (`release` to give back) |
| **approve** | the official LLM or the owner (the headstone judge only RECOMMENDS: verdict + reason in `history.txt`) | `pending -> claimed` | `status` |
| **deny** | same | `pending -> open`, `holder = -`, reason logged | `status` |

The server grants a lease to one ghost only (single writer). Available ghost = idle; available task = `open`; tier filter on `work`. All tunables (judge thresholds, tier rules) in the server's `tunables.conf`.

## 7. Build order (each step has a deterministic verifier BEFORE it counts)

1. **Data first (no renderer change).** `task.pdl` + `history.txt` in each quest folder; `^.grave/ops` scripts own all writes (`quest_new.sh` extended; new `quest_status.sh`); `INDEX.md` generated from the pdl records; verifier: round trip, hostile input, append-only history. Migrate Q001-Q010 by script, keep `QUEST.md`.
2. **Keyboard assign/work**: `assign`/`work` actions on the board and in the ghost window; ghost `drop_action` + `ghost_drop.sh` tested through the existing CLI `mv <nav#> <nav#>` equivalent; phone `task` / `lease` events; ledger rows asserted; idempotent on a repeated drop; silent exit for a wrong payload.
3. **`pending` + rule-based judge + approve/deny** (LLM slot left open; provider config Q002 is not done).
4. **h-ai placer entries** for graves and ghosts (`GRAVEYARD-GHOSTS-DESIGN.md` §6b).
5. **Row drag** (`drag_payload=`): scratch renderer, headless score, then the owner's screen. Ghost-entity-into-headstone handler (works with the existing entity drag).
6. **RMMV exporter/importer** against one real MV data file (obtain from the owner or a sample project), mapping table §4 corrected where wrong.

## 8. Open questions for the owner

1. (answered 2026-10-06, see §5.5: the doc is an entity; location is state; `pending/` is a bin in the headstone) Remaining: should a doc left on the desk count as `holder = desk` (visible, unassigned) or stay `open`? Original §5.3: confirm "drag creates a working copy; the headstone keeps the pending record" (the alternative, a true move that leaves only an index pointer in the headstone, makes the ghost the owner of the task data).
2. Where does a ghost's own quest list live: as event pages in its clacker (my recommendation: events-hq already edits, shows and exports them), or in a separate `quests/` folder on the ghost?
3. Status as an integer variable for RMMV (my mapping) vs also exporting the four self switches for the first four states: any preference?
4. Is a real RPG Maker MV project (or a `CommonEvents.json`) available to verify the mapping against?
5. Should the headstone judge ever auto-approve (tier fits, ghost idle, no conflict), or does every `pending` wait for the official LLM / the owner?
