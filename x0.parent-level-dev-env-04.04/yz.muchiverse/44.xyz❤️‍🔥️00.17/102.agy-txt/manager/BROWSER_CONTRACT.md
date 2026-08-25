# agy-txt file browser - shared C/PAL manager contract

Written 2026-07-30 per `#.haiku+/jul30-house-refactor.txt` §6 step 2 -
this is the real, concrete shape BOTH the native C manager and the
future PAL-driven manager must produce/consume identically. Neither
manager invents its own state shape; both target this document.

## 0. The real, already-active house mechanism this rides on

CORRECTED 2026-07-30 (live-test caught the earlier version of this
section was wrong - see manager/agy_browser_manager.c's own
gui_state_path() comment for the full trace): `chtpm_parser_pal.c`'s
own `load_vars()` does NOT unconditionally auto-load
`pieces/apps/player_app/manager/gui_state.txt`. That path is only used
when `project_id` is EMPTY (the `else if (is_modern_layout(...))`
branch). agy-txt's own `pieces/apps/player_app/state.txt`
(button.sh) sets `project_id=agy-txt`, so `load_vars()` instead takes
the `load_project_gui_state("agy-txt")` branch, which checks (in
order, first match wins, confirmed via direct read):
```
projects/agy-txt/manager/gui_state.txt
pieces/apps/agy-txt/manager/gui_state.txt   <- the one that exists/is used
pieces/apps/agy-txt/loader/gui_state.txt
pieces/apps/agy-txt/session/gui_state.txt
```
Both managers must write to `pieces/apps/agy-txt/manager/gui_state.txt`
(session-relative, created by button.sh). Every `key=value` line in
that file becomes a real `${key}` substitution in the `.chtpm` layout -
no custom `${game_map}`-single-blob scheme needed (that was an earlier
mistake, now superseded). NOTE: TPMOS's own `agy-text-editor_manager.c`
writes to its OWN project's equivalent of this same per-project path
(not literally `player_app/`) - the earlier claim that both write to
one identical literal path was itself part of the same misreading,
now corrected.

Also real, also live-caught: `pulse_frame_marker()` must touch BOTH
`pieces/display/frame_changed.txt` (sets the render loop's dirty flag)
AND `pieces/apps/player_app/state_changed.txt` (forces a fresh
`parse_chtm()`, which is what actually re-substitutes raw `${var}`-as-
markup lines like `${directory_browser_markup}` into real `<button>`
elements - confirmed via direct read of chtpm_parser_pal.c's main
loop and matched against mutaclysm's own `compose_frame.c`, which
pulses this exact second file for this exact reason). Pulsing only the
first marker re-renders the ALREADY-parsed (stale) element tree.

## 1. gui_state.txt keys (written by whichever manager is active)

```
browser_mode_header=MODE: SAVE FILE   (or MODE: LOAD FILE)
browser_current_dir_line=DIR: <real browse_dir>
search_query_val=<current SEARCH field content>
file_path_input_val=<current FILE field content>
active_project=<current editor_state.txt's own file_path, for the
                 "ACTIVE:" display line - matches TPMOS's own
                 active_project var>
directory_browser_markup=<real, dynamically-generated <button> rows -
                           see §2>
browser_action_buttons_markup=<real, dynamically-generated CONFIRM +
                                CANCEL <button> rows - see §2>
editor_response_line=<status_line equivalent - last command result>
```

No `cursor_pos`/`field_active`/hand-tracked focus state anywhere in
this file - per PITFALL 65, the PARSER owns all of that via real
`<cli_io>`/`<button>` element state. Neither manager should write or
read a hand-rolled cursor position.

## 2. Dynamic markup generation (real `<button>`, not hand-drawn text)

`directory_browser_markup` - one real button per BACK + real directory
entry (reuses `agy_scan_dir.+x <dir> [filter]`, already generic,
unchanged):
```
<button label="<- BACK" onClick="KEY:1" /><br/>
<button label="[DIR] layouts/" onClick="KEY:2" /><br/>
<button label="[FIL] hi.txt (26B)" onClick="KEY:3" /><br/>
```
`browser_action_buttons_markup`:
```
<button label="SAVE FILE" onClick="KEY:8" /><br/>
<button label="CANCEL" href="pieces/chtpm/layouts/file_menu.chtpm" /><br/>
```

KEY:n IS the real, temporary encoding (matching mutaclysm's own
already-proven, currently-shipped adaptation - `prisc+x`'s own
`OP_READ_HISTORY` is integer-only, confirmed via direct source read,
see jul30-house-refactor.txt §3) - NOT the final form. Internally,
BOTH managers must dispatch on a STRING action name
(`"back"`/`"select_2"`/`"save"`/etc), with KEY:n today acting as a
thin, swappable encoding layer over that string dispatch - so that
once prisc+x gains real string-command relay (Tier 2,
jul30-house-refactor.txt §5a Path B), only the ENCODING layer changes
(KEY:n -> SET_/OP: prefixes), not the manager's own real dispatch
logic. Do not hardwire "key == '2' means descend into directory slot
1" anywhere outside one clearly-marked encode/decode boundary.

SEARCH/FILE fields use real `<cli_io id="search_query"
target_id="search_query" label="${search_query_val}" />` /
`<cli_io id="file_path_input" ...>` - real, house-native, Parser-owned
focus and Enter-submit semantics (confirmed supported by
chtpm_parser_pal.c this same session) - NOT a hand-rolled
`onClick="INTERACT"` button, and NOT hand-tracked `field_active`.

## 3. Command relay IN (browser -> manager)

Real key/cli_io activity lands in `pieces/apps/player_app/
cli_buffers.txt` (cli_io's own real per-keystroke sync, confirmed
mechanism) and `pieces/keyboard/history.txt` (real button KEY:n
presses, chtpm-native). The manager polls both, same real shape as
`game_manager.c`'s own `poll_history()` (fseek to last position, fgets
loop, `sscanf`/`strstr` per line) - not re-derived, copy that
function's own real structure.

## 4. Ops reused unchanged (already correct, already manager-agnostic)

- `102.agy-txt/ops/agy_scan_dir.+x <dir> [filter]` - real, generic,
  already proven this session. No changes.
- `resolve_save_path()`/`resolve_xyzfs_home()` (currently living in
  `agy_widget_cmds.c`) - the real xyzfs jail + documents/ default.
  Reused as-is by the new manager (call the compiled op, or link the
  same logic directly - implementation detail, the CONTRACT is: same
  real jail behavior, same real resolution chain, not reinvented).
- `agy_widget_cmds.+x` inbox/drain contract
  (`pieces/system/widget_cmds/inbox.txt` -> `SAVE_AS:<path>`/
  `LOAD:<path>` commands) - unchanged, real, already proven.

## 5. What differs between the C manager and the future PAL manager

Nothing, by design. Same gui_state.txt keys, same markup shape, same
ops, same inbox contract. The ONLY difference is which process reads
real input and writes gui_state.txt - a pthread-based native daemon
(C, see `game_manager.c`'s own shape) vs a `prisc+x`-interpreted `.pal`
script (once Tier 2's own string-command extension exists). A real
flag (`RUN_MANAGER=c` / `RUN_MANAGER=pal`) selects which one
`button.sh` launches for a given session - both are real, both stay
maintained, neither is a stub kept around for show.
