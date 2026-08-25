# TSC_ELO — QUICK STANDARDS (PAL module + WIDGIT cheat sheet)

Concise, no-excuses rules for building this project's screens and widgets
correctly the first time. Source of truth: `&.widgits/WIDGIT_BIBLE.md`,
`#.haiku+/!.xyzos-standards+1.txt`, and a working trace of
`@.apps/my-chara-txt`. When these conflict with a doc, the working trace wins.

## 1. Two programs, never one

- **Host** = app profile: owns the TTY, `keyboard_input` foreground,
  `system/renderer` (ASCII) + `gl_mirror` (GL) both render.
- **WIDGIT** = widget profile: GL-only. NO `keyboard_input`, NO ASCII
  renderer, stdout -> `/dev/null`. Launched by the host with:
  `setsid env RUN_PROFILE=widget bash '<w>/button.sh' run-widget '<host_real_root>' >/dev/null 2>&1 < /dev/null &`
- Each program has its OWN session dir, its OWN `system/` binaries, its
  OWN `ops/`, its OWN PAL loop, its OWN GL window. Communication is
  FILE-MEDIATED only (inbox/status files, shared ledger). Never
  subprocess a widget from the host (except that one setsid launch).

## 2. Pipeline (one render)

```
compose op            -> pieces/apps/player_app/view.txt      (ONE WRITER RULE)
chtpm_parser_pal      -> pieces/display/current_frame.txt     (MUST cd to session dir: getcwd!)
chtpm_rgb_render      -> pieces/display/rgb_frame.raw         (1,966,080 bytes, 640x768 RGBA)
gl_mirror             -> GL window + writes interact_relay.txt
```

- `chtpm_parser_pal` spawns `prisc+x` itself from `state.txt`'s
  `module_path=system/prisc+x pal/main_loop_chtpm.pal` (persistent module).
- Launch order guard (PITFALL 54): wait for `pieces/display/current_frame.txt`
  to be non-empty BEFORE starting `chtpm_rgb_render`, or the window is
  permanently black.
- Glyphs are a LOCAL COPY per project (`pieces/registry/fonts/ascii/`, 95
  files 32-126). Missing = invisible text, no error (PITFALL 52).
- `gl_mirror` MUST be the 014.wsr-pal version (writes `interact_relay.txt`).
  The mutaclysm/045 copy does NOT relay GL keys -> widget seems frozen.
- Two gl_mirror instances (host + widget) land on the SAME glut default
  spot -> widget window hidden behind the host. wsr-pal's gl_mirror.c now
  honors `GL_MIRROR_X`/`GL_MIRROR_Y`; the widget sets them (default 680/90)
  so its window opens BESIDE the host's. Compile it from wsr-pal SOURCE
  (widget build.sh) so the feature + relay are both present.

## 3. Input paths

- TTY (host): `keyboard_input` -> `pieces/keyboard/history.txt` ->
  parser relays -> `pieces/apps/player_app/interact_relay.txt` ->
  prisc+x `read_history`.
- GL (widget): `gl_mirror` writes `pieces/apps/player_app/interact_relay.txt`
  DIRECTLY. No parser in the loop.
- Arrow key codes: LEFT=1000 RIGHT=1001 UP=1002 DOWN=1003 (from
  keyboard_input.c's escape-sequence combining). Digits are ASCII '1'-'9'.

## 4. PAL loop shape (main_loop_chtpm.pal)

```
li x1, 0
li x9, 0 ; <op>_input x9          # idle pre-sync (first frame correct)
<compose_op> ; hit_frame ; read_pos x7, "..._changed.txt"
loop:
  <op>_input x9
  <referee op>                    # only if you tick on idle
  read_pos x8, "..._changed.txt"
  beq x7, x8, check_key
  addi x7, x8, 0 ; j render
check_key:
  read_history pieces/apps/player_app/interact_relay.txt x2, x1
  beq x2, x0, no_key
  <op>_input x2                   # dispatch the relayed key
  j render
no_key:
  sleep 16667 ; j loop
render:
  <compose_op> ; hit_frame ; sleep 16667 ; j loop
```

`hit_frame` in app mode writes `frame_changed.txt`; in widget mode the
real render trigger is `renderer_pulse.txt` written by the parser. Don't
debug an empty `frame_changed.txt` in a widget.

## 5. Menus: `${piece_methods}` + piece.pdl (index nav with arrows)

- Layout includes `<text label="${piece_methods}" />`.
- `projects/<proj>/pieces/<screen>/piece.pdl` defines METHOD rows
  (`METHOD | Label | COMMAND`).
- The PARSER renders the rows as a focusable list (`[ ] 1. ...` /
  `[>] 2. ...`), owns arrow/Enter focus navigation, and `?`-style
  highlighting.
- Digit keys ALSO flow to the PAL loop: the `<op>_menu_input` op maps
  `'0'-'9'` -> item index -> executes the METHOD's command. The op NEVER
  renders the menu list or tracks focus (parser's job) — it only
  executes commands and mutates config/ledger.

## 6. One-writer rule

- Exactly one op writes `view.txt` (the compose op). It appends to its
  own screen-changed marker and to `frame_changed.txt`.
- Only `tsc_setup` drains `pieces/system/widget_cmds/inbox.txt` (host).
- Only `setup_enqueue_cmd` writes the host inbox (widget).
- Only `tsc_input` writes `player_action.txt`; only `tsc_deal` consumes it.

## 7. Widget cmd bus (widget -> host)

```
widget op: setup_enqueue_cmd <widget_state_dir> CMD[:args]
  -> reads <widget_state_dir>/focus.txt  (inbox_path=<host>/pieces/system/widget_cmds/inbox.txt)
  -> appends "CMD" to that host inbox
host drainer (background): ops/+x/tsc_setup.+x 8  every ~0.2s
  -> reads inbox, applies to config.txt, truncates inbox, writes ACK
     to <host>/pieces/system/widget_cmds/status.txt
  -> MUST persist MATCH/RATING/PLAYER to <host>/pieces/system/widget_cmds/
     pending.txt after each command: the drainer's 0.2s tick can split the
     widget's separate enqueue calls across invocations, and each invocation
     is stateless. Without pending.txt, MATCH/RATING that land in an earlier
     tick than START are silently dropped (live-caught: started a HvH match
     when the widget had set CvC). START applies the accumulated pending
     values, then resets pending.txt to defaults.
widget reads status.txt to show ACK.
```

Host sets the widget's focus by passing its REAL project root as
`run-widget <root>`; the widget's button.sh writes `focus.txt`.

## 8. Session isolation (both programs)

- Session dir under `pieces/sessions/<ts>-<pid>/`.
- SYMLINK static assets: `system/ ops/ pal/ default_op.txt pieces/chtpm/
  pieces/registry/ data/`.
- REAL files in session: markers, relay, history, `house_root.txt`,
  `state.txt`. `config.txt` is symlinked from the REAL project dir
  (persistent across sessions).
- Cleanup trap: kill daemons -> kill own prisc+x by cwd match ->
  `rm -rf "$SESSION_DIR"`.

## 9. Launch/build do's and don'ts

- Do `cd "$SESSION_DIR"` before starting `chtpm_parser_pal`.
- Do export `PRISC_PROJECT_ROOT="$SESSION_DIR"` for every op/system binary.
- Do `echo "$HOUSE_DIR" > pieces/system/house_root.txt` in every session.
- Don't start `keyboard_input` in widget profile.
- Don't set `NO_GL=1` and expect a visible GL window.
- Don't symlink glyphs across projects.

## 10. ELO / ratings

- Ratings persist at `<house>/<xyzfs>/home/games/tsc_ratings.txt`
  (`tsc_elo resolve` prints the path). Empty login falls back to the
  first xyzfs user; no users -> 1000 default, not persisted.
