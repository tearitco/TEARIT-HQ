# File-mediated I/O architecture (input AND frame output) - why it exists, and why it matters for AI

Written per direct question: "are we writing kbd input to file and reading
from it (and doing same for frame render composition, like 1.tpmos does?
this is important that we are cuz its how simple ai can reliably emulate
human behavior)." Short answer: **yes, confirmed directly against both
mutaclsym's own source and real 1.TPMOS's own equivalent files, not
assumed.** This doc explains the mechanism and why it's the actual reason
this whole session's testing methodology ([[feedback_test_through_playable_interface]]
in memory terms) works at all.

## 1. The core pattern

Every process in mutaclsym's runtime talks to every OTHER process **only
through plain text files on disk** - never a pipe, socket, shared memory
segment, or direct function call across a process boundary. Concretely,
there are two independent file-mediated channels:

```
                    INPUT CHANNEL                         OUTPUT CHANNEL

  keyboard_input.c                                   prisc+x (compose_frame.c,
  (raw termios,           --appends-->                compose_rgb_frame.c ops)
   decodes arrow keys)     integer keycodes                    |
        |                  to a plain text                     | writes
        | OR: any script/  file, one per line                  v
        |     AI/test              |                pieces/display/current_frame.txt
        v                          v                pieces/display/rgb_frame.raw
  pieces/apps/player_app/   prisc+x reads it        pieces/display/rgb_frame.receipt.txt
    history.txt             (fseek'd byte cursor,             |
                             read_history opcode)              | + pulse marker growth
                                                                v
                                                     pieces/display/frame_changed.txt
                                                                |
                                                                | watched by size (not mtime!)
                                                                v
                                                     renderer.c (prints ASCII to stdout)
                                                     gl_mirror.c (blits RGBA to a GLUT window)
```

Both channels are **plain files, read/written with ordinary `fopen`/
`fgets`/`fwrite`** - nothing GL-specific, nothing termios-specific, on
either side of the boundary. That's the whole trick.

## 2. Input side: `pieces/apps/player_app/history.txt`

- `system/keyboard_input.c` puts the real terminal into raw mode, decodes
  ANSI arrow-key escape sequences itself, and **appends** one bare decimal
  integer per line to `history.txt` - e.g. `119` for `'w'`, `1002` for
  `ARROW_UP`, `105` for `'i'`. See that file's own header comment.
- `system/prisc+x.c`'s `pal/main_loop.pal` script calls the `read_history`
  opcode every tick, which does an **incremental read**: it keeps a byte
  offset (`x1` register in the pal script) into `history.txt` and only
  consumes new bytes appended since the last read - never re-reads from
  the start, never blocks waiting for new input (returns `0` = "nothing
  new" if there's nothing past the cursor, and the main loop just
  `sleep`s and loops).
- **`keyboard_input.c` is not privileged in any way.** It is just one
  writer among possible writers to `history.txt`. Anything else that can
  append an integer followed by a newline to that same file - a shell
  script (`printf "119\n" >> history.txt`), a Python test harness, an AI
  agent deciding "move up" - produces **an indistinguishable event** from
  `prisc+x`'s point of view. This is not a side effect of the design,
  it's the literal reason this session tests mutaclsym by appending
  keycodes to `history.txt` directly instead of driving a real terminal
  (see `dox/00-HANDOFF.md`'s own testing conventions, and
  [[feedback_test_through_playable_interface]] in the persistent memory
  system) - it's the SAME mechanism a human's keypress goes through, not
  a parallel/fake path.

## 3. Output side: `pieces/display/current_frame.txt` + a pulse file

- `ops/compose_frame.c` runs every tick (right after `move_player`/
  `choice` process whatever key was read), reads every relevant state
  file (`hero/state.txt`, the current map's `map.txt`/`furniture.txt`,
  `items/`, `monsters/`, `message_log.txt`, `piece.pdl`), and writes a
  **complete plain-text rendering** of the current frame to
  `pieces/display/current_frame.txt` - the full ASCII map viewport, HUD
  stats, message log tail, and the action-bar/footer text.
- A **separate, independent process**, `system/renderer.c`, is the only
  thing that ever prints to the actual terminal. It does not compute
  anything about game state itself - it just watches
  `pieces/display/frame_changed.txt` (a pulse file `prisc+x`'s
  `OP_HIT_FRAME` opcode grows by appending `"X\n"` every tick, right
  after `compose_frame`/`compose_rgb_frame` run) and, whenever that
  file's **size** has grown since last checked, re-reads and re-prints
  `current_frame.txt`. Size, deliberately, not mtime - `stat()`'s
  `st_mtime` only has 1-second resolution, so multiple frames written
  within the same wall-clock second (normal during rapid keypresses)
  would be silently missed if mtime were used instead; a monotonically-
  growing pulse file's size never has that problem. This exact bug
  (`gl_mirror.c` originally polling `rgb_frame.raw`'s own `stat()`
  instead of the shared pulse file) was found and fixed this same
  session - see `dox/00-HANDOFF.md`'s checklist, item 3 under the GL
  auto-launch entry.
- The GL/RGB mirror is the exact same pattern, one layer further: `ops/
  compose_rgb_frame.c` computes a full RGBA32 framebuffer in plain,
  portable C (zero GL calls - see that file's own GOVERNING CONSTRAINT
  citation) and writes it to `pieces/display/rgb_frame.raw`, plus a
  `pieces/display/rgb_frame.receipt.txt` (dimensions, byte count, an
  FNV-1a-64 checksum) so correctness can be verified by reading a text
  file, never by looking at a window. `system/gl_mirror.c` - the ONLY
  file in this whole project allowed to call a GL primitive - watches
  the SAME `frame_changed.txt` pulse file's size and, on growth, reads
  `rgb_frame.raw` and blits it as a texture. Confirmed directly in this
  file (`frame_pulse`/`last_pulse_size` fields, `system/gl_mirror.c`
  lines ~342-344).

So: **reading what's currently on screen never requires OCR, screen-
scraping, or GL pixel access.** `current_frame.txt` already IS the exact
plain-text representation a human would visually parse - reading it with
an ordinary file read is the whole "how does an AI or test harness see
what's happening" answer, for both the ASCII and (via the receipt file's
checksum) the GL path.

## 4. Confirmed against the real 1.TPMOS precedent, not just internally consistent

This is not a mutaclsym-only convention invented in isolation - it's a
direct port of how real 1.TPMOS's own reference implementation works,
confirmed by reading the actual source, not assumed from mutaclsym's own
comments citing it:

- **Input**: `1.TPMOS_c_+rmmp.0102.0028/pieces/system/input_dispatcher/
  plugins/input_capture.c` - raw termios capture, decodes the same arrow-
  key escape sequences, and `writeCommand()` (line 104) appends to
  `pieces/keyboard/history.txt` (real 1.TPMOS's own separate `apps`-style
  history path; mutaclsym's own `keyboard_input.c` header comment
  explicitly notes it deliberately uses the simpler bare-decimal format,
  not real 1.TPMOS's own timestamped `[ts] KEY_PRESSED: N` line format,
  to match what `prisc+x`'s `read_history` opcode expects via `fscanf`).
- **Output**: `1.TPMOS_c_+rmmp.0102.0028/pieces/chtpm/plugins/
  chtpm_parser.c`'s `compose_frame()` function - confirmed by direct
  read, ends with (line ~2444 onward):
  ```c
  char* cur_f = build_path_malloc("pieces/display/current_frame.txt");
  FILE *out_f = fopen(cur_f, "w");
  if (out_f) {
      fprintf(out_f, "%s", frame);
      fclose(out_f);
      char* renderer_pulse = build_path_malloc("pieces/display/renderer_pulse.txt");
      FILE *marker = fopen(renderer_pulse, "a"); if (marker) { fprintf(marker, "P\n"); fclose(marker); }
      free(renderer_pulse);
  }
  ```
  Literally the same `pieces/display/current_frame.txt` path, and the
  same "write the frame, then append a marker line to a separate pulse
  file so a renderer knows something changed" shape mutaclsym's own
  `frame_changed.txt`/`OP_HIT_FRAME` implements - mutaclsym's version
  just grows the pulse file by a fixed 2 bytes (`"X\n"`) each tick
  instead of one line per real frame-composition event; the SIZE-based
  watch-for-growth mechanism on the consuming end is identical in spirit.

## 5. Practical implications (why this matters beyond "it's neat")

1. **A human player, a test script, and an AI agent are the same kind of
   actor from the engine's point of view** - all three interact with the
   game through the exact same two files (append to `history.txt`, read
   `current_frame.txt`). There is no special "AI mode" or "test mode"
   API surface to build or keep in sync with the real game - reliable AI
   control of mutaclsym (or emulating a human at the file level) already
   requires zero new engine work, only a process that appends the right
   integers and reads the resulting text.
2. This is also exactly why this session's own testing convention
   ([[feedback_test_through_playable_interface]]) insists on driving the
   game via `history.txt` + reading `current_frame.txt`/`hero/state.txt`
   rather than calling ops directly on the command line - a CLI-only test
   can pass while the actual input->render pipeline a human/AI would use
   is silently broken (this caught 2 real bugs earlier this project's
   history, per that memory's own citation).
3. **Nothing about this requires the GL window to be focused, mapped, or
   even running.** The ASCII channel (`history.txt` in, `current_frame.txt`
   out) is fully sufficient for any external actor to play a complete game
   session headlessly - the GL mirror is a genuinely separate, optional
   observer of the same underlying state, not a required part of the
   control loop.
