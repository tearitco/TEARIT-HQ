# video-player-hq

An in-house player for testing videos (first slice). Drop a video file onto the window (the renderer's generic XDND `drop_action`, first dropped path) or add one with the `add` row; it plays IN the window (ffmpeg raw frames fitted to the centre panel and blitted by the generic `<canvas>`; sound by an `ffplay -nodisp` child), with a queue, pause/resume/stop/next/prev/clear as numbered rows (relay-drivable), and a **file check** that says what is in the file: streams, size, length, average frame rate, and a warning for sparse variable-frame-rate files (the first Eden presentation had 3 video frames over 23 s and would not play in VLC).

- Launch: `sh open_video_player_hq.sh <house_root>`. Backend `ops/video_player_manager.c` (publishes `video_player_ui.txt`, reads `video_player_action.txt` by cursor); `ops/video_player_op.c` writes the command rows. Build: `ops/build_video_player.sh`.
- Harness hook: `video_player_manager --probe <file>` prints the file check.
- Layout: queue, status and file check on the left; the picture fills the centre; the transport (play, pause, resume, stop, back/fwd 10 s, prev, next, clear) is a bottom footer. A `<footer>` breaks a flex-row page, so this is the plain sidebar + panel layout. Region widths in a flex page must be written one property per line, in px.
- Opens from the Toys menu (`toy.pdl`, launcher walks up to the house root when given `run`).
- Cost: the renderer used about 30% of a core while a 980x680 picture played; lower `BOXW`/`BOXH` in the manager on a weak machine. Pause and seek restart ffmpeg and ffplay at the remembered second.
- Not done: no seek bar (only +-10 s rows); no taskbar entry besides Toys; the XDND drop was not exercised with a real drag by the author (the renderer's drop path is the same one File Explorer and events-hq use). Other video code: `103.media-studio/103.vid-edit` (a timeline editor with a preview and ffmpeg export).
