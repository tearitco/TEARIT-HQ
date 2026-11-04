# Presentation video pipeline — PNG dumps → paced MP4 with TTS narration

**Status: working.** Doc added 2026-09-10 (it was missing from the book
until today). The pipeline turns dated PNG snapshots + a plain-text
manifest into a single paced, narrated MP4 so a human can *watch* proof
that a feature actually works instead of reading a wall of terminal text.

Owner instruction this is built to (2026-08-25): "be very careful
deliberate and make examples and test them and prove harnesses as we
go... presentations being made when we are done of proof each major
feature is working... make sure the video is paced so a human can watch,
not at light speed."

## Two tools, one pipeline

1. `&.widgits/_shared-lib/ops/+x/dump_frame_png_op.+x` — standalone op.
   Usage: `dump_frame_png_op <window_id_hex> <out.png>` (or `--root`
   for a full-screen dump). Captures a window's real on-screen pixels
   via XGetImage and prints a geometry receipt to stderr (root-relative
   position via XTranslateCoordinates, display size, explicit OFF-SCREEN
   flag when the window runs past the display edge).
   **This is the correct capture channel.** Verified live 2026-09-10 on
   the network browser: 960×640 PNG, 4364 distinct colors, real content.
   Editing caveat: `scrot`/plain root XGetImage of the same setup came
   back pure black for composited/GL windows — never template `scrot`
   into this pipeline, always use the op.

2. `make_presentation_video.py` — canonical copy at the piecemark
   media-archive root; an identical copy lives at
   `livedesk/pals/cursword/presentations/make_presentation_video.py`.
   Requires on PATH: `ffmpeg`, Pillow, `pydub`. Speech is the pet voice,
   `@.apps/pet-trainer/ops/pet_tts_local.py` (Kokoro, offline). The
   `--voice` name is an edge-style name from `voices.pdl` (`en-US-AnaNeural`
   maps to `af_heart`). Edge speech itself has answered HTTP 403 since
   2026-10-12 and is not used. The caption is also burned onto the frame.

## Where the videos live

The archive is outside the house git tree, next to the other Cursword
media. Do not put the mp4s in the game app folders or in `xyzfs/users`.

    /home/no/Desktop/🧩️Piecemark-IT/中.SP_00.00/🗡️.crswrd.media-archive

`make_presentation_video.py` at that root is the canonical script.
A second copy lives in the livedesk Cursword pal at
`xyzfs/users/<id>/home/livedesk/pals/cursword/presentations/make_presentation_video.py`.
Run the archive copy.

One folder per proof, under the month-day the frames were taken:

    October-20/game-dev-hq-progress-20261020/
    October-20/rpg-pvp-hq-progress-20261020/

Same shape as `August-26/events-hq-task2-test-20260826-211501` and
`August-27/marketing-20260827`. A later "after" video, once a feature
lands, is a new folder. It does not overwrite the progress cut.

## Directory shape (one dir per feature/proof)

Media archive is organized by date (`August-26/<feature>/`,
`August-27/<feature>/`, `September-10/<feature>/`, ...) :

    <feature>/
      snapshots/01_<state>.png ...
      manifest.txt          <- the script's real input
      REPRODUCE.md          <- plain-English steps to redo the test
      presentation.mp4       <- OUTPUT
      <feature>-yt-summary.txt   <- OUTPUT, YouTube-ready summary

## manifest.txt format

One row per snapshot, in playback order:

    <snapshot_filename> | <seconds_to_hold> | <caption text>

- `snapshot_filename` is relative to the `snapshots/` dir next to the
  manifest.
- `seconds_to_hold` is a MINIMUM. With TTS (default) the real hold time
  is `max(seconds_to_hold, narration_audio_length + 0.6s)`, so a long
  caption is never cut off mid-sentence. Pick 5–8s for one sentence.
- `caption text`: one line, plain text (no `|`). Wrapped and burned onto
  the frame bottom AND spoken via TTS while the frame is on screen.
- Lines starting with `#` or blank lines are ignored.

## Run

    make_presentation_video.py <feature_dir> [--fps 30] [--width 1280]
      [--no-tts] [--voice <edge_tts voice>] [--title "YouTube title"]

Scales frames to `--width` (aspect preserved). Writes `presentation.mp4`
and `-yt-summary.txt` into the feature dir.

## Why it is reliable

- ffmpeg concat demuxer always does a REAL re-encode — never `-c:v
  copy`; stream-copy silently drops most frames (hit once, 2026-08-27).
- One ffmpeg invocation joins scaled images + narration audio together.
- Proof frames can be co-captured with the app's own receipt/state files
  at the same moment, so the video never overclaims.
- Proven end-to-end: `August-27/marketing-20260827` (real live-desktop
  footage via the PipeWire/portal pipeline) and the `August-26/` batch
  (e.g. `events-hq-task2-test-20260826-211501`: snapshots/ +
  manifest.txt + mp4 + yt-summary).

## Proof: file-explorer Grid View (2026-09-18)

Widget: `44.xyz.01.00/&.widgits/file-explorer/`.
Dir: `&.widgits/file-explorer/presentations/file-explorer-grid-20260918/`.

Relay-only (k9 order: history file, then `'p'` / code 112). No python
capture, no xdotool.

1. `sh button.sh run` — relaunch so the new `khtpm_core_render.+x` is
   the process (rebuilds do not hot-reload).
2. PID from `pgrep -f 'khtpm_core_render.+x .*file-explorer-pal.xhtpm'`.
3. Append to `#.desktop/entity_menu_history/<pid>.txt` (never truncate):
   - `KEY_PRESSED: 112` → `/tmp/entity-menu-frame.png` (list). Copied to
     `snapshots/03_relay_p.png`.
   - Digits `49` `51` then `13` (nav **13** = Grid View on that frame)
     then `112` again → `snapshots/04_grid_after_nav13.png`.
4. Text receipts (trust these if a PNG looks stale, k9 § PNG last):
   `file_explorer_ui.txt` `is_grid_view=1` `n_grid_entries=96`;
   `/tmp/entity-menu-frame.png.receipt.txt` `nav=13`;
   `.frame.txt` `item|gentry0|swatch|📁  #.DOX |...|176|40`.

**What 04 proves:** named 176×40 swatch cells (folder emoji + name),
toggle reads **List View** (we are in grid). Dual chrome X (`[ ]1` and
`[ ]100`) still on screen — not fixed this pass.

**Not this proof:** the desk **placement overlay** (solid yellow while
dragging an entity). That should become a tic-tac-toe wireframe layout;
it is not the file-explorer grid.

REPRODUCE.md lives next to the snapshots.

## Media capability note (2026-09-10)

The network browser is TEXT-only: no `<img>` / `<video>` decode or
render anywhere in the worker DOM, wire, or renderer. A `go:https://
youtube.com` renders the page's text DOM only (title / headings / links;
no thumbnails or video playback). Image support still needs, in order:
a decoder in the worker (stb_image.h — the house already ships
stb_image_write.h in the same shared lib), an `img` element + fetch in
`nb_dom.c`, a binary `IMG` wire frame, and a draw-image op + clip in the
renderer. Video needs ffmpeg demux/decode + timed frame push — a real
upgrade; plan `<img>` first, video last.
## Scripted drive harness: place things several ways, move them, give them an event (2026-11-04)

**Plan (done in this order):** (1) pick only input paths already proven on the real window; (2) put the steps in a scenario text file, not in code; (3) every step runs behind `cpu_gate`; (4) every claim is an `expect` against a state file (negative-tested: a wrong expectation stops the run and says FAIL with the file's last line); (5) the live game is saved to slot 9 first and loaded back after, `house.txt` and `voxel_log.txt` put back; (6) frames and manifest feed the existing `make_presentation_video.py`.

**Saved, reusable harness:** `&.widgits/_shared-lib/presentation/present_harness.py` + scenarios in `&.widgits/_shared-lib/presentation/scenarios/`. Needs the editor window open (`board_ctl recover <app>`; an unmapped window is mapped by the harness).

    python3 &.widgits/_shared-lib/presentation/present_harness.py \
        &.widgits/_shared-lib/presentation/scenarios/place_and_move.txt [--no-video] [--no-tts] [--dry]

Output goes to the media archive, `<Month-DD>/<scenario>-<stamp>/` (`snapshots/`, `manifest.txt`, `REPRODUCE.md` with the full step log and OK/FAIL, the mp4 and the yt summary). Step vocabulary is in the script's docstring: `book`, `verb`, `drive`, `armtile`, `xclick`, `clickplace`, `keys`, `event`, `wait`, `expect`, `shot`, `note` (`\p` = a literal `|` in a field).

**Scenario `place_and_move.txt` (first run 2026-11-04, status OK, 6 frames, ~42 s with narration):**
1. *Way one, palette + mouse:* arm the CDDA fire barrel from the palettes bar, a **real X click** on a floor cell, Enter through `bv_keys`; `expect` the log line `placed 30gal_firebarr as #1`. The click retries neighbouring pixels when the editor answers "not placed: someone is standing there" (the camera follows the xelector, so one pixel is never the same cell twice).
2. *Way two, verb, no mouse:* `place 1 9 9` (item number, new x, y) moves the placed barrel; `expect` the `MOVE|1|` row and `|9|9` in `house.txt`.
3. *Way three, drive the character:* `goto bank` sends the controlled character (here the pet `robot`) to the Bank; `mark`/`moved` assert its room,x,y changed (`store,5,10 -> bank,6,4`). (`walk` is NOT used for the visible move: a blocked pet only bumps its step counter; this was found because the `moved` assertion failed.)
4. *Event:* `events/demo_automove` (new, one `goto store` node, trigger `ui`) played through `events-hq/play_event.sh` sends it back with nobody driving (`bank,6,4 -> store,6,4`).

**Daylight:** the first runs were filmed at 03:00 and were nearly black; the scenario now starts with `clock advance 9h` and asserts `daylight.txt` says day. The load at the end puts the clock back (`allow_settime_back`, section 15 of the sprint doc).

**Honest limits:** relay clicks do not reach the 3D canvas (only a real X click), so the harness uses `xdotool` for that one step; the step that presses Enter goes through `bv_keys` (xdotool Return does not reach the engine); `place <id> x y` only moves an item that already exists; cleanup uses the in-game `remove <id>` first, because the daemon keeps its own item list and writes `house.txt` back after a plain load.
Verified 2026-11-04: dry run, 10 real runs (several failed on purpose or by bug and each fix is in the harness); the last two runs: status OK, desk restored (no `house.txt`, slot 9 removed, clock back), video with narration (about 42 s, 6 frames); frames viewed: bright store, barrel placed, barrel moved, Bank, back in the Store.

### Scenario `lamps_at_dusk.txt` (2026-11-04): lamps lit by the dusk event
Design (one generator, not one event per lamp): `@.apps/rpg-edit-hq/lamps.pdl` (`LAMP | item_key | r | g | b | radius | strength | z`) + the placed items of a room become that room's engine `lights.pdl` (LIGHT rows, max 8) while `state/lamps_on.txt` is 1; the verb `lamps on|off` writes the flag and re-exports the 3D book; `phase_dusk` has a new node `lamps on`, `phase_dawn` a node `lamps off`. The `lamp` catalog item uses a placeholder tile (a white pillar of Inside_B; there is no lamp tile in that sheet). New harness steps: `clockto`, `buyplace`, `brighter`; every `shot` nudges the idle engine first (it only redraws when woken).
Verified: night 23:00, lamp bought and placed, dusk page fires -> `lamps_on=1`, canvas mean brightness 14.7 -> 18.0 (x1.23) and the frame shows a warm pool of light around the lamp, dawn page -> `lamps_on=0`, third frame dark again; desk restored. Not done: a lit 2D view (3D only), a real lamp tile, lights on rooms other than the exported editor rooms.

### Mouse alignment test: pointer marker + 3D raycast round trip (2026-11-04)
Owner rule: every mouse test must agree with the pointer marker (`show_pointer=1` in `#.desktop/hq_ui.pdl`) and with the 3D raycast. `xclick` in `present_harness.py` now checks, for every real click: (1) the pixel the window published (`canvas_click.txt`) equals the commanded pixel minus a canvas origin that is identical for all clicks of the run (here 208,106); (2) the engine writes `pieces/display/click_proj.txt` for each pick: `pixel -> ray -> cell -> pixel`, the picked cell's 8 corners projected back through the same camera (`cell_box`), `inside=1` only when the click lies in that box (`cell_px`/`err_px` give the centre offset). Both click paths of the engine write it (frame loop and the idle poll; the idle poll was a second caller that the first patch missed).
Scenario `scenarios/mouse_align.txt`: 6 real clicks spread over the canvas: published origin constant (208,106) for all six; 4 picks inside their projected cell box (centre offsets 12-37 px, growing toward the corner), 2 clicks missed the map (reported, not failed). Negative test: `PRESENT_NEG_SKEW=5` makes the same run FAIL on the second click; without it the run is OK.
Open: the centre offset grows toward the screen corner (11.8 px in the middle, 37 px at the bottom-left): normal for a perspective cell (the box is bigger there) but a proper calibration needs your hand clicks at known cells; the camera follows the xelector after a click.

### Pinned camera + mouse calibration (2026-11-04)
`pincam|yaw|pitch|dist` writes the engine's debug camera (`pieces/system/cam_look.txt`, `look_on=1`, target = the xelector's cell, no `look_sel`) so the view stays put when a click moves the xelector; `unpin` (also run by the restore) deletes it. `calgrid|cols|rows|x0|y0|x1|y1` clicks a grid of real pixels, each checked like `xclick`, and writes `calibration.txt`. `collect|n|seconds` waits for n clicks by a human with the real pointer and records each pick (pixel, cell, inside the projected box, centre offset).
Scenarios: `mouse_calibrate.txt` (automatic) and `mouse_calibrate_human.txt` (YOUR clicks). Verified (automatic): camera pinned, 12 real clicks, the picture before and after is identical (view did not move), published origin constant, 7 picks inside their projected cell box (centre offsets 10-22 px), 5 clicks above the room missed the map. The human run has not been done yet.
