# screen-rec Architecture

A minimal OBS/Twitch-style screen recorder: negotiate a screen-capture
session with the compositor (GNOME on Wayland, via xdg-desktop-portal +
PipeWire), preview it live in a GL window, and record to `.mp4` on demand
via libx264.

This is the "hello world" of the eventual streamer app -- the simplest
possible working end-to-end recorder, built the same way the rest of this
tree is: self-contained ops, file-based receipts, no shared `.h` files.

## Why portal + PipeWire, not X11 `x11grab`

The desktop here runs Wayland (GNOME). Classic X11 screen-grab APIs
(`XGetImage`/ffmpeg's `x11grab`) are blocked by the compositor for security
-- there is no "just read the framebuffer" call anymore. The real mechanism
(what OBS itself uses on Wayland) is: request a `ScreenCast` session over
D-Bus from `org.freedesktop.portal.Desktop`, let the user pick a
monitor/window in the compositor's own picker dialog, then receive the
video as a PipeWire stream. Confirmed working against GNOME 42 /
xdg-desktop-portal-gnome on Ubuntu 22.04.

## Directory structure

```
151.screen-rec/
  button.sh                  # deps / compile / run / kill verbs
  system/
    screen_rec.c              # capture + encode daemon (~600 lines)
    screen_rec_gui.c           # preview window + controls (~450 lines)
  pieces/
    display/
      rgb_frame.raw            # live preview frame, raw RGBA32 (screen_rec.c writes)
      rgb_frame.receipt.txt    # frame_w/frame_h/frame_seq/checksum for the frame above
      recorder_state.receipt.txt  # recording=0/1, output_path, frames_encoded
      gui_display.receipt.txt # what the GUI actually loaded + checksum match
      thumbs/                 # cached thumbnail .raw files, one per recording
    control/
      record_command.txt      # GUI writes "start"/"stop", screen_rec.c consumes it
  recordings/
    rec_<epoch>.mp4            # finished recordings
  test-harn/
    ops/
      tk_screenshot.c          # X11 screenshot of a window found by title
      tk_click.c                # XSendEvent click at a point on a window found by title
      +x/                       # compiled binaries
    scenarios/
      test_record_flow.sh       # end-to-end: click RECORD, wait, click STOP, verify output
  dox/
    architecture.md             # this file
```

Two binaries in `system/`: `screen_rec` (the daemon, no GL) and
`screen_rec_gui` (the window, no capture logic of its own). Everything else
is data -- receipts, control files, recordings.

## screen_rec.c -- the daemon

No window, no GL. One process, three jobs that all need to stay tightly
coupled (this is the one deliberate departure from the "one op, one job"
convention elsewhere in this tree -- splitting capture and encode into
separate processes with a raw-frame-file handoff was tried and rejected:
a 1080p/30fps stream is ~8MB/frame, ~240MB/s of disk churn just for the
handoff, which chokes before it reaches the encoder):

1. **Portal negotiation** (`portal_request_screencast`, via GDBus): calls
   `CreateSession` -> `SelectSources` -> `Start` -> `OpenPipeWireRemote` on
   `org.freedesktop.portal.ScreenCast`. `Start` is what pops the GNOME
   monitor picker. This happens **once per process lifetime**, not once per
   recording -- the whole point of running as a long-lived background
   daemon is that hitting record/stop never re-prompts the dialog.

2. **PipeWire capture**: connects to the fd `OpenPipeWireRemote` returned,
   negotiates a raw video format (BGRx/RGBx/BGRA/RGBA, whichever the
   compositor offers), and gets a frame callback (`on_process`) for every
   captured frame.

3. **Preview + encode**, both driven from that same callback:
   - `update_preview()` downscales the frame (via swscale) to at most
     `PREVIEW_MAX_DIM` (480px) on its longest side, throttled to ~8fps
     regardless of capture rate, writes it to `pieces/display/rgb_frame.raw`,
     then writes `rgb_frame.receipt.txt` with `frame_w`/`frame_h`/
     `frame_seq`/an FNV-1a-64 checksum of the exact bytes just written.
   - `encode_frame()` only runs while `g_recording` is set. Converts to
     YUV420P via swscale, encodes with `libx264` (ultrafast/zerolatency),
     muxes into `recordings/rec_<epoch>.mp4` via libavformat.
   - A 200ms PipeWire timer polls `pieces/control/record_command.txt` for
     `start`/`stop`, calling `start_recording()`/`stop_recording()` and
     consuming (deleting) the command file, matching the close-request-file
     convention used elsewhere in this tree.

## screen_rec_gui.c -- the preview window

Ported from `gl_mirror.c`'s own shape almost directly: `timer()` polls
`rgb_frame.raw`'s size via `stat()`, reloads the texture on change,
`display()` blits one textured quad. What's added on top:

- **Correctness receipt** (`gui_display.receipt.txt`): every `load_texture()`
  call computes its own FNV-1a-64 checksum of the bytes it just read and
  compares it against `rgb_frame.receipt.txt`'s `frame_checksum_fnv1a64`.
  `checksum_match=1` means the GUI is provably showing the exact bytes
  `screen_rec.c` produced -- this can be confirmed by reading two text
  files, without ever looking at the window (same principle `gl_mirror.c`'s
  own `write_gl_display_receipt()` is built around).
- **Record/Stop button**: a plain colored GL rectangle, hit-tested in
  `mouse()` (freeglut `glutMouseFunc`). Same effect as pressing `r`: writes
  `pieces/control/record_command.txt`.
- **Red border** while `recorder_state.receipt.txt` says `recording=1`.
- **Thumbnail strip**: `scan_recordings()` lists `recordings/*.mp4` by
  mtime (skipping whichever file is currently being recorded, since an
  in-progress `.mp4` has no `moov` atom yet and isn't seekable). For each
  new file, `request_thumbnail()` shells out to `ffmpeg` in the background
  to grab one frame as a fixed-size raw RGBA buffer -- reusing the exact
  same "raw RGBA bytes -> `glTexImage2D`" trick the live preview already
  uses, so no image-decoding library is needed. Click a thumbnail to
  `xdg-open` it in the default video player.

## Data flow

```
screen_rec.c                              screen_rec_gui.c
     |                                          |
     | portal negotiation (once)                |
     | -> GNOME picker dialog                   |
     |                                           |
     | PipeWire on_process() per frame           |
     |  -> update_preview() -----> rgb_frame.raw + .receipt.txt (w/h/seq/checksum)
     |                                           | timer() polls file size,
     |                                           | load_texture(), checksum-verifies
     |                                           | against the receipt above,
     |                                           | writes gui_display.receipt.txt
     |                                           |
     |                              click RECORD / press r
     |                                           | -> writes record_command.txt
     | 200ms timer polls record_command.txt <----+
     | -> start_recording() / stop_recording()
     |  -> recordings/rec_<epoch>.mp4
     |  -> recorder_state.receipt.txt (recording=0/1, output_path, frames_encoded)
     |                                           |
     |                                           | timer() reads recorder_state,
     |                                           | draws red border, scans
     |                                           | recordings/ for new files,
     |                                           | requests+polls thumbnails
```

## Compilation

```sh
./button.sh deps     # apt install pipewire/spa/x264/glib/freeglut dev headers (idempotent)
./button.sh compile  # builds system/screen_rec and system/screen_rec_gui
./button.sh run      # launches both; GNOME picker appears once
./button.sh kill      # stops both
```

`SCREENREC_PROJECT_ROOT` (set by `button.sh`) is the `resolve_root()`
env var both binaries read, falling back to `getcwd()` -- same convention
as `import_pet.c`/`gl_mirror.c`.

## Known gaps / later work

- No audio capture yet (mic/desktop audio via PulseAudio simple API).
- No RTMP/Twitch streaming output -- `recordings/*.mp4` only. Adding an
  RTMP target is mostly a second `avformat_alloc_output_context2` call with
  `rtmp://...` instead of a file path; the capture/encode core doesn't change.
- Thumbnail `.raw` files in `pieces/display/thumbs/` are never cleaned up.
- No persisted portal restore-token, so a fresh `screen_rec` run always
  re-prompts the picker (acceptable since it only prompts once per daemon
  lifetime, not once per recording).
