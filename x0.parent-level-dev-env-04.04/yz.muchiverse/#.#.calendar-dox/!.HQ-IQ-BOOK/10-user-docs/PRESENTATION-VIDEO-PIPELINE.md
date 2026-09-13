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
   Requires on PATH: `ffmpeg`, Pillow, `edge_tts`, `pydub` (all
   confirmed present 2026-09-10). Each caption is synthesized with
   edge_tts and burned onto the bottom of its frame.

## Directory shape (one dir per feature/proof)

Media archive is organized by date (`August-26/<feature>/`,
`August-27/<feature>/`, ...) or under `presentations/<feature>/`:

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