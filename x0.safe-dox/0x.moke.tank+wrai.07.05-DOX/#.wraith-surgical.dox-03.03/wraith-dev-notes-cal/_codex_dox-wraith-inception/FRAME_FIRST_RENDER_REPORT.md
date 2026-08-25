# Frame-First RGB Render Report

## Executive Summary

This project is a prototype for rendering through an auditable intermediate frame before the final visual surface. The important idea is not "OpenGL draws the world." The important idea is "the world becomes a frame artifact first, then a renderer presents that artifact."

The current pipeline is:

1. CLI/input code mutates `state.txt`.
2. State is converted into semantic `primitive ...` and `text ...` records.
3. Primitive records are projected into `rgb.txt` as explicit `pixel x y r g b` records.
4. GL code reads `rgb.txt` and displays it.

The remake should keep that architecture, but make it stricter: a frame should be the primary product of the engine. GL and CLI should both be render backends for the same frame. For the new project, the ultimate intention is to turn the GL screen into an ASCII-addressable frame so it can be rendered in the CLI, accept CLI mouse/keyboard interaction, and give AI/debug tooling a plain-text, inspectable version of every frame.

## Cited File Key

Line citations below refer to these exact project files:

- `wraith.rgbbypas🪄️🕶️CD6/locations.txt`
- `wraith.rgbbypas🪄️🕶️CD6/state.txt`
- `wraith.rgbbypas🪄️🕶️CD6/primitive.txt`
- `wraith.rgbbypas🪄️🕶️CD6/rgb.txt`
- `wraith.rgbbypas🪄️🕶️CD6/^.move.0_pipe]🗃️🟨️]h5.c`
- `wraith.rgbbypas🪄️🕶️CD6/103.bypass_gl]a1]OPT🪄️🔮️.c`
- `wraith.rgbbypas🪄️🕶️CD6/104.read_primative]3d]d4?🪄️🔮️.c`
- `wraith.rgbbypas🪄️🕶️CD6/105.rgb_gl]2d]a0🪄️🔮️.c`
- `wraith.rgbbypas🪄️🕶️CD6/105.rgb_gl]3d]a0🪄️🔮️.c`
- `wraith.rgbbypas🪄️🕶️CD6/105.rgb_gl]3d]b1-mute🪄️🔮️.c`

## Evidence From The Current Code

### Shared File Routing

The project already uses named file channels. `locations.txt` defines the active primitive, log, and RGB files:

- `wraith.rgbbypas🪄️🕶️CD6/locations.txt:1` maps `primitive` to `primitive.txt`.
- `wraith.rgbbypas🪄️🕶️CD6/locations.txt:2` maps `log` to `log.txt`.
- `wraith.rgbbypas🪄️🕶️CD6/locations.txt:3` maps `rgb` to `rgb.txt`.

Both the primitive-to-RGB writer and RGB-to-GL readers parse this file instead of hard-coding all paths. In `104.read_primative]3d]d4?...c`, `read_locations()` handles `primitive`, `rgb`, and `log` keys at lines 79-105. In `105.rgb_gl]2d]a0...c`, `read_locations()` handles `rgb` and `log` at lines 47-70.

### CLI/Input Mutates State

`^.move.0_pipe]...h5.c` is the current terminal input layer. It disables canonical terminal input and echo at lines 13-20, loads `state.txt` at lines 61-96, and writes state changes back at lines 98-123.

The input loop reads keyboard events without blocking, including arrow-key escape sequences at lines 140-169. It supports two control modes: player mode mutates `x/y/z`, and cursor mode mutates `cursor_x/cursor_y/cursor_z`. The relevant branch is at lines 153-167. Depth movement uses `z` and `x` keys at lines 171-179. Camera movement mutates `cam_x`, `cam_y`, and `cam_z` at lines 180-185.

This is directly relevant to the remake: the CLI should not be a passive text dump. It should be an interactive frontend that mutates the same state/event stream as GL, including mouse-style cursor movement.

### State Becomes Semantic Primitives

`103.bypass_gl]a1]OPT...c` is a state-to-primitive bridge. It reads `state.txt` at lines 80-117, extracting player and cursor coordinates at lines 92-99. It then writes semantic records to `primitive.txt` at lines 119-134:

- A blue player sphere is written at line 125.
- A yellow cursor cube is written at line 126.
- Gray block cubes are written at lines 127-129.
- HUD text is written at lines 130-131.

The current checked-in `primitive.txt` shows the resulting format: lines 1-80 are `primitive cube ... type=block` records, with normalized color fields and alpha-like opacity values.

This format is useful because it is human-readable and AI-readable. For the remake, this layer should probably remain as a semantic debug artifact, but it should not be the final renderer contract. The final renderer contract should be the frame.

### Semantic Primitives Become An RGB Frame

`104.read_primative]3d]d4?...c` is the clearest implementation of the frame-first idea.

It defines a fixed frame grid:

- `GRID_WIDTH 640` at line 13.
- `GRID_HEIGHT 480` at line 14.
- 8x13 bitmap font parameters at lines 15-17.

It parses primitive records at lines 173-243. The parser recognizes `primitive` records at lines 190-223 and `text` records at lines 224-241.

The key frame construction happens after parsing:

- A full RGB framebuffer is allocated as `unsigned char pixel_grid[GRID_HEIGHT][GRID_WIDTH][3]` at line 246.
- World units are mapped to screen pixels using `scale = 64.0f` at line 247.
- Primitive type order is explicitly chosen at lines 248-249.
- Each primitive is projected into pixel coordinates at lines 255-256.
- Cube/sphere size is rasterized as square pixel coverage at lines 257-268.
- RGB values are written into the framebuffer at lines 263-265.

Finally, the code serializes only non-black pixels to `rgb.txt`:

- Opens the output file at lines 272-279.
- Emits `pixel x y r g b` lines at lines 281-289.
- Projects text glyphs into pixels at line 292.
- Also preserves text records for GL/HUD readers at lines 294-297.

The checked-in `rgb.txt` confirms the artifact format. Lines 1-80 are explicit pixel records such as `pixel 288 208 127 127 127`.

This is the strongest part of the project. It proves the core concept: a renderable frame can exist before GL gets involved.

### Text Is Also Frame Data

`104.read_primative]3d]d4?...c` includes a tiny bitmap-font path. `font_8x13` is declared at lines 36-67. `project_text_to_pixels()` walks text lines, glyph rows, and glyph bits at lines 149-171. Lit glyph pixels are emitted as white `pixel` records at line 161.

This matters for the CLI/AI direction. Text overlays should not be a separate GL-only HUD. If text becomes frame pixels, it can be audited, converted to ASCII, and compared frame-to-frame just like geometry.

### RGB Frame Becomes GL Output

The best GL consumer for a frame-first remake is `105.rgb_gl]2d]a0...c`, because it treats `rgb.txt` as a pixel frame instead of trying to reinterpret it as 3D geometry.

Evidence:

- It declares `Pixel pixel_buffer[GRID_HEIGHT][GRID_WIDTH]` at line 20.
- It clears the buffer at lines 72-80.
- It parses `pixel x y r g b` records at lines 90-102.
- It uploads the buffer to a GL texture via `glTexImage2D(... GL_RGB ... pixel_buffer)` at lines 106-110.
- It draws that texture as a full-screen quad at lines 144-157.
- It also contains a point-cloud debug alternative at lines 159-171.

That is the model to copy. GL should be a presentation target for the frame, not the owner of frame truth.

### 3D Reconstruction Exists But Is Lossy

`105.rgb_gl]3d]b1-mute...c` tries to reconstruct primitives from `rgb.txt`. It parses pixels at lines 170-181, converts pixel coordinates back into world coordinates at lines 187-188, guesses object type from color at lines 193-213, deduplicates by approximate position at lines 214-221, and then renders guessed primitives in GL at lines 341-353.

This is useful as an experiment, but it is not a reliable remake foundation. It loses information:

- Shape/type are inferred from color, not encoded structurally.
- Z/depth is mostly guessed.
- White text pixels are skipped at lines 207-208.
- Duplicate suppression collapses nearby data.

For the remake, keep reconstruction only as a debug mode. The main contract should be explicit frame data plus optional semantic metadata.

### One Variant Does Not Match The RGB Pixel Contract

`105.rgb_gl]3d]a0...c` reads from `rgb.txt`, but its parser expects `primitive ... type=...` records at lines 152-189. That does not match the pixel records written by `104.read_primative]3d]d4?...c` at lines 281-289.

This variant is useful historically, but it should not be treated as the current frame-first GL consumer. The 2D RGB texture reader is the better reference implementation.

## Architecture To Reuse In The Remake

The remake should formalize this pipeline:

```text
input events
  -> world state
  -> semantic scene/debug records
  -> Frame { width, height, rgb/rgba cells, depth?, ids?, metadata? }
  -> GL backend
  -> CLI ASCII backend
  -> audit logs / AI-readable snapshots
```

The key difference from a normal GL app is the order. GL should not be where the first complete picture appears. The frame should exist first. Then GL, CLI, tests, and AI tools all consume the same frame.

## CLI/ASCII Target

For the next project, the CLI renderer should convert the frame into character cells:

- Group pixels into terminal cells, for example 2x4, 4x8, or font-sized blocks.
- Choose a glyph from luminance, color, edge density, object id, or semantic layer.
- Preserve RGB as ANSI foreground/background color where useful.
- Maintain a cursor/mouse coordinate transform from terminal cell to frame pixel to world position.
- Emit a text snapshot that an AI can inspect without screenshots.

Mouse abilities in CLI should feed the same input/event layer as GL. A click in terminal cell `(cx, cy)` should map back to frame coordinates, then into whatever world/camera coordinate system the engine uses. This gives CLI parity with GL for debugging and interaction.

## Recommended Contract

Use a real in-memory frame object in the remake, then serialize it for audit:

```c
typedef struct {
    int width;
    int height;
    unsigned char *rgba;
    float *depth;
    unsigned int *object_id;
    unsigned int frame_index;
} Frame;
```

Then build renderers around the same object:

```text
frame_render_gl(Frame *)
frame_render_cli_ascii(Frame *, CliInputState *)
frame_write_rgb_txt(Frame *, path)
frame_write_ascii_txt(Frame *, path)
frame_write_metadata(Frame *, path)
```

The current `rgb.txt` sparse-pixel format is good for human audit, but the remake may also want a dense binary or structured format for speed. Keep the text format anyway because it is the AI/debugging win.

## Main Risks To Fix

1. The current code mixes semantic records, pixel records, and GL rendering variants. The remake should name these contracts separately: `scene`, `frame`, `presenter`.
2. The current RGB frame has color but no object identity, depth, normals, or event hit map. CLI mouse support will be much easier if the frame includes an `object_id` or hit-test layer.
3. The current 3D reconstruction path guesses semantics from colors. This should be avoided as a primary mechanism.
4. Text/HUD should be frame-owned, not GL-owned, so CLI and AI snapshots see the same overlays as GL.
5. Frame diffs should be first-class. A future audit tool should be able to compare frame N and frame N+1 as text, pixels, object ids, and input events.

## Bottom Line

This project already demonstrates the core pattern: build a frame before GL renders it. The most important source file for that idea is `104.read_primative]3d]d4?...c`, which converts primitives into explicit RGB pixels. The best GL reference is `105.rgb_gl]2d]a0...c`, which loads those pixels into a texture and presents them.

The remake should make this intentional and central: every visual frame should be produced as inspectable data first. GL becomes one viewer. CLI/ASCII becomes another viewer. AI debugging becomes practical because it can read the same frame as text, inspect mouse/cursor state, and reason about rendering without depending on opaque screenshots.
