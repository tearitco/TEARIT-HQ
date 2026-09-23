# 2026-09-23 — Actors list, relay, frame history

Desk pals Ember, Glacine, Murmur, Solvent, Asa, Ava, and Cursword are
actor rows 5–11. The live Database window listed them (`Actors (11)`
in `db-hq-pal/state/ui.txt`). Rows were written into `actors.pdl` and
`db_hq_actors.state.txt`. Not fixed below; written down so the next
pass can pick them up.

## Frame history (done this pass, TPMOS pattern)

`1.TPMOS_c_+rmmp.0103.0001/pieces/display/renderer.c` keeps two files.
`pieces/display/current_frame.txt` is the live snapshot. On each real
render it appends that snapshot to
`pieces/debug/frames/session_frame_history.txt` under a
`--- FRAME UPDATE at <time> ---` line. A new process truncates that
log once and writes `=== NEW SESSION at <time> ===`. It does not
truncate on every frame.

db-hq-pal now does the same for its text frame. `state/ui.txt` stays
the live snapshot. `ops/frame_history.sh`, started by `button.sh`,
wipes `debug/frames/session_frame_history.txt` once per launch and
appends only when `ui.txt` changes. The key mailbox
`#.desktop/entity_menu_history/<pid>.txt` is still a different file
and the renderer still truncates that one on startup.

## Still open

1. **No add-actor control.** `dashboard.xhtpm` can show a row and edit
   a field. It cannot create an actor. The seven rows exist because
   the data files were edited. There is no GUI gesture for IRL to
   learn that step from.

2. **Click count is a setting, not something to infer.**
   `#.desktop/hq_ui.pdl` has `click_two_step=1`: first mouse click
   focuses, second click activates. `0` would be one click. Keyboard
   Enter does not read that key; one Return calls `activate_focused()`.
   Eight Downs and one Enter opened States without a two-step click
   on Actors. That is not a list-arrow bug, and it is not a reason to
   edit tab code. The k9 addendum that said one activation locks the
   sidebar was wrong and is corrected in the same file.

## Meta AI does not click. The FSM does.

The click checks above are the kind of repeated action that should
leave this chat. The architecture is already written. Do not start a
second one.

- New C is allowed only for AI event-command types in
  `44.xyz.01.00/#.ref/menu/event_commands.registry.pdl`:
  `ai_describe` (describe only, never classify), `ai_fsm_transition`,
  `ai_goap_plan`. That permission is kilo's, in
  `13.agent-coms/KILO/claude-2-kilo-9.17.md` §2b. The registry still
  has zero `ai_*` commands. A behavior composed of those commands is
  an event page, not a new `.c`.
- tomom is the learner (school, corpus, return path into gameplay).
  It is not the thing that issues the click. NIGHT_20 through NIGHT_23
  in `08-roadmap/00-INDEX.md`. The return path is still design-only.
- A harness player is the flag in that same kilo note: the turn is
  driven by `ai_fsm_transition` / `ai_goap_plan` instead of a human
  relay. The human-vs-harness flag can be scaffolded. The IRL
  start/stop control is later, in hai-lab's game tab.
- What the FSM would own for the Database window, once those
  primitives exist: read `ascii_frames/<pid>.frame.txt` for the
  `[>]` row and `state/ui.txt` for `detail_title`. If the marker is
  not on the tab, click that tab's box from
  `entity_menu_frame_<pid>.txt`. With `click_two_step=1`, the first
  click only focuses, so the transition is "click the same box
  again," then read the frame again. Down is a separate transition,
  and it currently walks detail fields, not the actor list. The meta
  layer may describe that frame. It does not emit the click, and it
  does not decide that the GUI is broken.

Checked 2026-09-23, not delegated yet: two clicks on the Actors tab
box left `[>]` on a detail field and `sel` 0 (Harold). Escape moved
`[>]` to `17. 1. Harold`. A click inside that row's published box
moved `[>]` to the blank detail field. Down never changed the
selected actor.

3. **`db_hq_history.txt` does not drive this window.** The harness
   `nav.sh` appends there for the old `g_is_db_hq` path.
   `class="db-hq-pal"` leaves that path dormant. Keys for this window
   go to `entity_menu_history/<pid>.txt`, and that file is truncated
   when the renderer starts. The frame log above is the review copy.
