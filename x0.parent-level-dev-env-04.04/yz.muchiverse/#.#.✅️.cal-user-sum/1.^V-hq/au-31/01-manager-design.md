# au-31/01 — real per-app manager design (irc-chat-hq first)

## Real naming clarity, direct instruction ("they will need new 'khtpm'
   style layouts for 'x11' right? those can go in a subdir so u dont
   confuse the 2")

Each of the 3 real apps ALREADY has its own real `.chtpm` layouts under
`<app>/pieces/chtpm/layouts/` (e.g. `044.pal-chat-irc👥️+2/pieces/
chtpm/layouts/room.chtpm`) - these belong to a COMPLETELY DIFFERENT
engine, `chtpm_parser_pal` (the ASCII/terminal renderer: tags like
`<panel time_reactive="true">`, `<module>system/prisc+x ...</module>`,
`<interact src="...">`, `<cli_io>`, `href="..."` page navigation - real,
already-working, already-designed screens: chat has `login`/`room_list`/
`room`; forum has `login`/`feed`/`post_compose`/`follow`/`dms`; chain
has `login`/`signup`/`wallet_main`/`send_screen`/`receive_screen`/
`mining_status`). Genuinely useful as real reference for what fields/
flow already exist and are considered "done" for each app - NOT
something to copy tag-for-tag, since the vocabulary is entirely
different from the X11 khtpm family (`<window class="...">`,
`<sidebar>`, `<panel>`, `<button onClick="...">`).

The NEW X11 khtpm-style layouts this design is about are physically
separate already - `&.hq-apps/<name>-hq/<name>-hq.chtpm`, outside each
app's own `pieces/chtpm/layouts/` entirely, zero file-path collision
risk. Naming convention going forward, made explicit here so nobody
confuses the two by name alone: this design's files are always called
**"khtpm layouts"** (X11, `&.hq-apps/`); each app's own existing
screens stay **"chtpm layouts"** (ASCII, `pieces/chtpm/layouts/`) -
one letter of difference in the family name, deliberately mirroring
the real `khtpm_*` vs `chtpm_*` binary-name split already established
house-wide (`khtpm_taskbar_manager.c` vs `chtpm_parser_pal.c`, etc.).

Design pass only, per `00-todo.md` item 1 - no code yet. Grounded
directly in `khtpm_hq_manager.c` (db-hq's own real, working manager,
178 lines, read in full before writing this) as the exact real
precedent to copy, not invent fresh.

## The real shape being copied (khtpm_hq_manager.c's own contract)

- One standalone C binary, `argv[1] = house_root`, a plain `for (;;)`
  loop with a fixed `usleep()` poll interval (400ms there).
- **Publish**: scans its own real source of truth, writes a plain-text
  state file the shell reads - atomic (`fopen(path.tmp, "w")` ->
  `fclose()` -> `rename(path.tmp, path)`, never a direct in-place
  write the shell could read half-written).
- **Consume**: polls one plain "action request" file the shell writes
  into when the user does something (`open:<name>`); on seeing a
  non-empty line, does the real work (here: a real `system()` spawn),
  then truncates the file back to empty so it doesn't re-fire.
- Manager and shell share ZERO code and ZERO memory - the state/action
  files under `#.desktop/` are the entire real contract between them.
- `open_db_hq.sh` launches both (shell directly; shell's own `<module
  src="...">` tag fork+execv's the manager) and kill-before-relaunch
  guards both binary names.

## irc-chat-hq's real manager (`irc_chat_hq_manager.c`)

Real source of truth: `044.pal-chat-irc👥️+2/data/master_ledger.txt`,
rows `MSG|<msg_id>|<room>|<user>|<ts>|<text>` (real schema, confirmed
live by reading actual rows in that file).

**Publishes**, atomically, every poll tick:
- `#.desktop/irc_chat_hq_rooms.state.txt` - one room name per line,
  the distinct `room` values seen in the ledger so far, most-recently-
  active first (same real "scan a dir/file, sort, atomic publish"
  shape as `publish_common_events()`, just a distinct-values scan
  instead of a directory listing).
- `#.desktop/irc_chat_hq_messages.state.txt` - the real messages for
  whichever room is CURRENTLY ACTIVE (see the action file below),
  newest N lines, plain `<user>: <text>` per line (already-formatted
  for direct display, matching `publish_common_events()`'s own "the
  state file is display-ready, not raw data the shell has to re-
  parse" convention).

**Consumes** `#.desktop/irc_chat_hq_action.txt`, one pending line at a
time, cleared after handling:
- `room:<name>` - switch which room's messages get published above.
  Real, in-memory-only manager state (`g_active_room`) - no file needed
  for this, it's re-derivable (defaults to the first/most-recent room
  on manager restart, same real "stateless-on-restart is fine" shape
  `g_current_page` defaulting to "main" already establishes elsewhere).
- `send:<user>|<text>` - real action: shells out to the app's own real,
  already-existing op, `chat_post_message.+x <user> <active_room>
  <text>` (exact real argv contract, verified against that op's own
  usage string) - the SAME real "manager does the actual spawn, shell
  never does" split `handle_action_request()` already establishes for
  db-hq's "Open in Editor" action.

**Real, deliberately NOT done in this manager**: user create/switch
(`chat_create_user.c`/`chat_switch_user.c`) - v1 scope is read+send
only, matching "reasonable guis opening" (today's real ask); user
identity can be a real, later action line (`switch-user:<name>`) added
the same way once the display side is proven.

## The shared-file side (what THIS pass adds to
   `khtpm_entity_menu_render.c` - real, minimal, no per-app business
   logic there)

A new `g_is_irc_chat` mode flag (`<window class="irc-chat-window">`),
whose ENTIRE real job is: read the two state files above via
`reusable_slot()` into the sidebar/panel Elem tree (same generic
injection mechanism `dbhq_inject_list_sidebar()`/
`dbhq_inject_list_panel()` already use for Common Events' own list),
and on a real click/Enter on a room row or the compose field, write
the matching `room:`/`send:` line into `irc_chat_hq_action.txt`. No
ledger parsing, no room-scanning, no `chat_post_message` argv-building
- all of that stays in the manager. This is the real, minimal
"generic fallback" style addition discussed earlier today, scoped down
to exactly one app's real fields instead of a fully generic any-.chtpm
renderer - smaller, safer, and provably matches an already-working
real precedent (Common Events) line for line.

## Real build order (matches `00-todo.md`'s own item 3)

1. Write `irc_chat_hq_manager.c` for real, standalone-testable BEFORE
   touching the shared file at all - run it by hand, watch the two
   state files populate/update from real `master_ledger.txt` growth,
   confirm `send:`/`room:` action lines produce the real expected
   effect (a new real ledger row via `chat_post_message.+x`, a
   different room's messages appearing) - all provable via plain `cat`/
   `diff` on the state files, zero X11/rendering involved yet.
2. Only once step 1 is proven live: add `g_is_irc_chat` + its real
   `<window class="irc-chat-window">` chtpm to
   `khtpm_entity_menu_render.c`, wire the sidebar/panel injection +
   action-file writes, rebuild, verify live (real screenshot/window-
   image read, per `HOUSE_CODE_PITFALLS.md` #4).
3. Retire `&.hq-apps/irc-chat-hq/`'s current placeholder `.chtpm`
   (today's wrong-assumption scratch file) once the real one replaces
   it; `open_irc_chat_hq.sh` (already written today) needs no change -
   it already launches the shared binary against whatever `.chtpm`
   lives at that path.
4. Repeat 1-3 for `forum-hq`, then `chain-hq`, each its own manager
   reading ITS real schema/ops (per `NETWORK-CELL-HQ-WINDOWS-
   DESIGN.md` §12's own real per-app breakdown) - not started until
   irc-chat-hq is fully proven end to end.

Not started yet: nothing past this design has real code written
against it.
