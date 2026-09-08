# screen-rec-hq — conversion skeleton (2026-09-08)

The "streaming" / screen-recorder toy, being moved onto house x11-HQ
spec so it appears in the **HQ → toys** menu and is testable like every
other khtpm window (relay injection + a `_ui.txt` read).

## Style: WRAP the engine, don't rewrite it

This is the **forum/irc conversion style**, NOT the media-editor style.
The hard part (Wayland `xdg-desktop-portal` + PipeWire ScreenCast →
libx264 `.mp4`) is already built and stays **unchanged**:

```
44.xyz.01.00/151.screen-rec+01.02/
  system/screen_rec          # capture + encode daemon  <- the ENGINE, keep as-is
  system/screen_rec_gui      # old GLUT preview window   <- this is what we replace
  pieces/display/recorder_state.receipt.txt   # recording=0|1, output_path=, frames_encoded=
  pieces/display/rgb_frame.raw / .receipt.txt # live RGBA32 preview frame
  pieces/control/record_command.txt           # "start" / "stop"  (engine consumes)
  dox/architecture.md        # READ THIS - the real portal/PipeWire mechanism
```

The converted toy = a thin manager + a static template:

```
@.apps/screen-rec-hq/
  button.sh                       # house-spec launcher (from @.apps/music-player-hq/button.sh)
  screen-rec-hq.xhtpm / .css      # Start/Stop, state, recordings list, <canvas> preview
  toy.pdl                         # title + launch=button.sh  -> shows in HQ toys
  ops/screen_rec_manager.c        # thin manager: spawn engine, poll action file,
  ops/build_screen_rec_manager.sh #   translate START/STOP -> record_command.txt,
  ops/srec_action.sh              #   read recorder_state receipt -> publish screen_rec_ui.txt
```

Actions dispatch through `ops/srec_action.sh` (writes `seq`/`cmd` to
`state/screen_rec_action.txt`) — **no per-app verb in
`khtpm_core_render.c`** (CENTROID_GOLD_STD).

## Status — what works in the skeleton

- ✅ compiles (`ops/build_screen_rec_manager.sh`), runs, publishes
  `state/screen_rec_ui.txt` (engine_up / recording / state / frame
  count / recordings list).
- ✅ `START` / `STOP` / `TOGGLE` write `pieces/control/record_command.txt`;
  `ENGINE_START` spawns the engine.
- ✅ `toy.pdl` present → appears in HQ toys once `@.apps/` is scanned.

## TODO (grok / whoever continues)

1. **`daemon-only` launch verb.** `151.screen-rec+01.02/button.sh`'s
   `run` verb `exec`s the old GLUT GUI too. Add a `daemon-only` verb
   (spawn `system/screen_rec`, do NOT exec `screen_rec_gui`) and point
   `engine_spawn()` in `screen_rec_manager.c` at it.
2. **Live preview.** Wire the `<canvas id="preview">` to the engine's
   `pieces/display/rgb_frame.raw` (+ its `.receipt.txt` for w/h). The
   renderer's `kh_draw_canvas()` already blits a live RGBA framebuffer
   for the piececraft board — same mechanism, likely just a
   `sprite="…/rgb_frame.raw"` on the canvas or a small manager-side
   symlink/copy into the pkg dir.
3. **Portal first-run.** On Wayland the compositor shows a
   screen-picker dialog the first time the engine starts. Confirm it
   works headless-ish, or document that the user must pick once.
4. **Recordings list polish** — durations, thumbnails
   (`pieces/display/thumbs/`), click-to-open in a player.
5. **Elapsed time** while recording; a size/fps readout.
6. Retire `151.screen-rec+01.02/system/screen_rec_gui` once (2) works;
   leave a one-line pointer from `151.screen-rec+01.02/` to here.

## Test

```
# from repo:
bash 44.xyz.01.00/@.apps/screen-rec-hq/button.sh run
# then relay-inject into the window's #.desktop/entity_menu_history/<pid>.txt
# and read state/screen_rec_ui.txt to confirm START/STOP took.
```
