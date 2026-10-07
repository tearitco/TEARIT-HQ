# Drag and drop between windows / menus — how it works today, and how to extend it

Written 2026-10-06 (claude, hai manager) after the owner said: *"render supports dragging 'items' somewhat between menus so we will have to use or extend that"*
(for dragging a quest into a ghost's menu, see `08-roadmap/design-docs/GRAVEYARD-GHOSTS-DESIGN.md` §6c).
Everything under "How it works today" was **read in the code** (file and function names below, line numbers as of 2026-10-06 in
`_.monads/_.livedesk-taskbar/ops/khtpm_core_render.c`, `R` below). Nothing here was exercised with a real mouse drag by the author; anything not read is marked **UNVERIFIED**.

## 1. The model in one paragraph

A **drag source** is a placed *entity* (a pal on the desktop or in pc-hq: it is its own small X window). A **drop target** is any *window* that opted in with
`<window drop_action="...">` (or the File Explorer class). While you drag an entity, the entity's own process polls the published drop zones, tells the target which entity hovers
over it, stacks itself above the target, and on release the target receives a real X11 **XDND** drop whose payload is a **directory path** (the dragged entity's folder). The target runs its
`drop_action` shell command with that path in `$DROP_PATH`. The payload is therefore "a folder"; the *meaning* of the drop is entirely in the handler script.

## 2. The pieces (all of them)

| Piece | Where | What it does |
|---|---|---|
| `drop_action` attribute | `R` ~1057-1080 (`g_drop_action`), parsed ~1270 | Opts a `<window>` into being a drop target. Decoded like `action=` (`&quot;` `&amp;`). Windows without it never advertise XDND: byte-for-byte unchanged. Documented in `XHTPM-PARSER-REFERENCE.md` (attribute table). |
| Zone publication | `kh_write_drop_zone()` ~12302, `kh_is_drop_target_window()` ~12297 | A drop-target window writes `#.desktop/khtpm_drop_zones/<pid>.txt`: `pid= win=0x.. x= y= w= h= dest= color=`. `dest` is the File Explorer's current dir (empty for other windows). Removed by `kh_drop_zone_unregister()` ~12342. |
| Zone lookup | `kh_drop_zone_hit()` ~12387 | Walks the X window chain under the pointer and matches it to a zone's `win=` (not the often stale x/y/w/h). Falls back to the rect only if no window id. |
| Hover signal | `kh_write_drag_hover(pid, name)` ~12349, reader above it (`drag_hover_pid.txt`, `g_drop_hover_name`) | The dragged entity writes `#.desktop/drag_hover_pid.txt` (`pid=`, `name=`) so the **target** can highlight and show which entity hovers it. |
| Drag loop (source side) | `R` ~18560-18610 (desktop-entity mode, `tp_main`) | While `dragging`: `XQueryPointer`, find the zone, `kh_write_drag_hover`, `kh_drag_stack_above(...)`, and write `#.desktop/drop_poll.txt` (debug trace: `rx= ry= zpid= name= dest=`). |
| Stacking | `kh_drag_stack_above()` ~13039, `g_drag_reparented`, `g_drag_host`, `g_drag_host_pid` | The dragged entity window is **reparented into** the host window so it stays visible above it (a plain stacking order does not hold under the window manager); on leaving the host rect it is reparented back to root. |
| Real drop | `xdnd_handle_selection()` ~12222 | On XDND `SelectionNotify`: reads `text/uri-list`, takes the **first existing directory** (else the first existing path), `setenv("DROP_PATH", path)`, runs `g_drop_action` as `<action> '<package_dir-or-arg3_dir>' '<house_root>'`, background, **does not quit the window**, always answers `XdndFinished`. |
| Keyboard equivalent | `kh_cliio_exec()` ~3657 | In a window with a `<cli_io>`: `mv <src nav#> <dst nav#>` / `cp ...` resolves the **source nav number** to a real path through the live nav-claim pool (`kh_claimed_tab_path`) and fires the SAME `drop_action` with the same env. For a non-File-Explorer window the destination number is ignored (the target is "this whole window"). `mv` and `cp` are equivalent there. Result text goes to `cliio_result.txt` in the instance dir. **This is how a drop can be driven and tested without a mouse.** |

Handler argument convention (same as every action in the house): `$1` = per-instance dir (`g_arg3_dir` if set, else `g_package_dir`), `$2` = house root, env `DROP_PATH` = dropped folder.
**Bug already fixed, do not reintroduce (2026-09-28):** for a *shared* template (e.g. `events-hq.xhtpm` used by every entity) `g_package_dir` is the template's folder, never per-entity;
the real instance is `g_arg3_dir`. Both XDND paths now prefer `g_arg3_dir`.

## 3. Existing consumers (use these as the pattern)

1. **bookmarks** (2026-08-24, first consumer): drag a directory onto the window to add it.
2. **File Explorer** (`file-explorer-pal` class, `fe_drop.sh`): drop moves/copies into the listed directory (`dest=` in its zone file). Its click-to-place short-circuit `FE_PLACE_CLICK` is also reused by Move.
3. **events-hq** (`&.widgits/events-hq/ops/event_drop_handler.sh`, design `EVENT-MODULARITY-AND-BUILD-SPEED.md` §1): drop a 🎬️ clacker or a ⚙️ page (a pal whose `meta.pdl` has `event_object | 1`) onto an
   entity's events-hq window and it pushes a new numbered `event_clacker_N` onto that entity (copy; every page of a clacker). Conventions worth copying:
   - **Silent `exit 0` for any drop that is not meant for this target** (not an error: "not every drop target wants every drop").
   - **Identify the payload by a marker in its `meta.pdl`** (`event_object | 1`) and its `glyph.txt`, never by the path name.
   - Multi-item payloads are enumerated and **sorted numerically** (a real bug was losing page order because `find` order is not numeric).
   - The handler deletes its own source on success (the house "mv" drop convention); a target that must keep the source says so explicitly.

## 4. Limits (why "somewhat")

- The only drag **source** is an *entity window*. A **row inside a list** (a quest in the board, a slot in the hotbar) cannot be picked up and dragged out; the hotbar *slides* as a window, it does not export an item.
- The payload is a **folder path** and nothing else: no kind, no id, no text. Kind must be read from files inside the folder.
- A drop target is the **whole window**. There is no per-row drop target (dropping "onto the third row" does not exist; the CLI `mv a b` ignores `b` for non-File-Explorer windows).
- One `drop_action` per window.
- The X11 path needs a real pointer and XDND; the test mouse path is flaky (house rule: use the relay, `xdotool` last resort; see `INPUT-RELAY-PIPELINE.md`). Use the CLI `mv` form for deterministic tests.
- **UNVERIFIED by me:** the exact moment a released entity sends the XDND offer to the host (only the receiving side `xdnd_handle_selection` and the polling side were read); whether the highlight colour works for non-File-Explorer targets;
  behaviour with two targets stacked; Windows/macOS (XDND is X11 only).

## 5. Extending it for quests, ghosts and headstones (design, nothing built)

Goal (owner, 2026-10-06): drag a **quest** into a **ghost's** menu to assign it; a ghost can take work from a **headstone**; the quest stays `pending` in the headstone until judged. Full flow in `GRAVEYARD-GHOSTS-DESIGN.md` §6c.

### Option A — quest card as an entity (recommended first; ZERO renderer change)
A quest is currently a row + folder, not an entity. Make a **quest card**: a tiny pal (glyph 📜) whose `meta.pdl` has `quest_object | 1` and `quest_id | Q010`, `board | <grave folder>`. The board's **"assign"** action (and later a drag) places/uses one card.
- Ghost menu window gets `drop_action="'<path>/ghost_drop.sh'"`. `ghost_drop.sh` (modelled on `event_drop_handler.sh`):
  1. exit 0 unless `$DROP_PATH/meta.pdl` has `quest_object | 1` (silent no-op for anything else);
  2. read `quest_id`, validate `^Q[0-9A-Za-z]+$`, and that `$DROP_PATH` is under the house (the path comes from X11: treat it as **untrusted**; never `rm -rf` an unchecked path);
  3. send a `task` message from the **owner's phone** to the ghost's phone number with `ref = quest_id` (`phone_send_op.+x`, kinds listed in `HAI-ROBOTS-PHONES-SERVER-DESIGN.md`), i.e. an auditable ledger row, not a file move;
  4. set the quest `pending` (see §6c) via the quest scripts (`^.grave/ops`), never by editing INDEX.md by hand;
  5. consume the card (it is only a pointer; the quest data stays in the grave).
  It must be **idempotent**: a double drop must not create two assignments (check for an existing `pending`/`claimed` row for that quest and ghost first).
- Reverse direction **work**: drag a **ghost** entity onto a **headstone** window (`drop_action` on the board window) → handler sends `lease` to the headstone for the first `open` quest at or below the ghost's tier; `release` gives it back.
- Cost: two small shell handlers + two quest-card/ghost metas. The board window needs the `drop_action` attribute (it is a template: `board.xhtpm`, re-stamp instances with `ops/new_board.sh`).

### Option B — rows as drag sources (extension of the renderer; later)
Add an opt-in `drag_payload="..."` on `<item>`: press and move more than a few pixels on such an item spawns a transient drag entity carrying a folder (a temp card) so the **existing** hover/stack/XDND path
handles the rest. Reuses everything in §2; the new code is only "item press-move starts a transient card". Must follow the renderer house rules: **clip, never translate; no per-project globals; every layout mutation idempotent**
(`khtpm-house-standards` skill, `khtpm-shared-layout-caution` memory). Needs the owner's go-ahead; estimate: moderate, touches the shared renderer, so build to a scratch binary first (as done for `confirm=`).

### Option C — keyboard assign (works today, no drag)
Select a quest row, press an **assign** action that asks for a ghost (nav number) and runs the same handler. In a ghost window with `drop_action`, typing `mv <quest-card nav#> 0` in its `cli_io` already fires it
(§2 "Keyboard equivalent"). This is the form to build and test first; drag is sugar over the same `task` event.

### Recommended order
C (verifiable now, deterministic) → A (real drag using existing machinery) → B (only if dragging a plain row is wanted).

## 6. How to test a drop handler (house method)

1. Unit: run the handler directly with `DROP_PATH=<scratch card> sh handler.sh <instance_dir> <house>`; assert the ledger row, the quest status, idempotence on a second run, and **silent exit 0 for a wrong payload**.
2. Integration, no mouse: open the target window, find its pid (`#.desktop/khtpm_drop_zones/<pid>.txt` proves it published a zone), arm its `cli_io` through the relay and type `mv <nav#> 0`; read `cliio_result.txt` (`ok: dropped via this window's own drop_action`).
3. Real drag (last resort, owner's screen): watch `#.desktop/drop_poll.txt` (`zpid=` non-zero when the pointer is over a zone) and `#.desktop/drag_hover_pid.txt`.
4. Never drive tests on the owner's windows; launch and close your own (record positions first, restore only to the recorded value: memory `feedback-dont-move-owner-windows`).

## 7. Open questions for the owner

1. Is a quest card (a 📜 entity on the desktop/pc-hq) acceptable as the draggable thing, or should the quest row itself be draggable (Option B)?
2. On drop, should the card be consumed (my default) or stay on the desktop as a visible "assigned" marker?
3. Should dropping a ghost onto a headstone mean `work` (lease), or is `work` only a button inside the ghost's menu?
4. May the headstone judge auto-approve a drop whose tier fits, or does every `pending` wait for the official LLM / the owner?
